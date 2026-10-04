// Scene expansion research code (SotN XBLA, vanilla v1.4.5 image).
//
// Model (same as ar-recomp PR #2): the gameplay camera never moves. The
// game keeps drawing its original 256-pixel view; this code shifts every
// primitive right by `margin` through the PS1 draw offset, widens the draw
// clip and the displayed VRAM rectangle, adds tile columns outside the
// original view, and widens the front-end destination rectangle by the same
// factor so pixels keep their aspect. Nothing writes camera, scroll or
// entity state.
#include "expand.h"

#include <rex/ppc/context.h>
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>
#include <rex/system/xmemory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace expand {
namespace {

// Guest addresses for the vanilla v1.4.5 image. Found in the recompiled
// code and live memory; see research/scene_expansion/README.md.
constexpr uint32_t kRenderTilemapFn = 0x82258190u;
constexpr uint32_t kUpdateGameFn = 0x822521C0u;
constexpr uint32_t kGpuBuffer0 = 0x8318F660u;
constexpr uint32_t kGpuBuffer1 = 0x831AB158u;
constexpr uint32_t kCameraOffsetX = 0x8316AF48u;  // s32
constexpr uint32_t kCameraOffsetY = 0x83173FA4u;  // s32
constexpr uint32_t kClutIds = 0x83173FC0u;        // u16[]
constexpr uint32_t kCurrentBuffer = 0x8316AF40u;  // GpuBuffer*
constexpr uint32_t kTilemap = 0x8316AF80u;
constexpr uint32_t kBgLayers = 0x831CECA0u;
constexpr uint32_t kBgLayerStride = 48;
constexpr int kBgLayerCount = 16;
constexpr uint32_t kOtOffset = 1208;              // GpuBuffer::ot (8-byte entries)
constexpr uint32_t kStretchRect = 0x82882C68u;    // s32 LTRB, front-end pixels
constexpr uint32_t kRenderPrimitivesFn = 0x8223B798u;
constexpr uint32_t kPrimBuf = 0x831751E0u;        // Primitive[0x500], 56 bytes each
constexpr int kPrimCount = 0x500;
constexpr uint32_t kPrimSize = 56;
constexpr uint16_t kDrawHide = 0x8, kDrawAbsPos = 0x2000;
constexpr uint32_t kStretchRectMax = 0x82882C98u;  // s32 LTRB: the whole front-end frame
// Player HUD record (DrawRichterHud, sub_8225BBC8, writes it at r31): +4 and
// +8 hold the first g_PrimBuf index of the two HUD primitive chains.
constexpr uint32_t kPlayerHud = 0x82E86BF0u;
// Primitive record fields (56 bytes in this build).
constexpr uint32_t kPrimType = 5, kPrimX0 = 12, kPrimY0 = 14, kPrimU0 = 16, kPrimV0 = 17,
                   kPrimR0 = 8, kPrimX1 = 24, kPrimU1 = 28, kPrimY1 = 26, kPrimR1 = 20, kPrimX2 = 36,
                   kPrimY2 = 38, kPrimR2 = 32, kPrimPriority = 42, kPrimX3 = 48, kPrimY3 = 50,
                   kPrimR3 = 44, kPrimDrawMode = 54;
// The HUD draws at ordering-table priorities 0x1EE-0x1F0 (Richter and Alucard).
constexpr uint16_t kHudPriorityLow = 0x1EE, kHudPriorityHigh = 0x1F0;
constexpr int kMaxMargin = 128;  // 256 + 2*128 = 512, the start of the texture pages

// Tile layer record. Read from the guest tile-layer routine sub_82258190:
// the foreground record is at kTilemap (r24 in that routine) and the 16
// background records follow kBgLayers with a stride of 48 (r15 - 40).
constexpr uint32_t kLayerTiles = 0;      // lwz 0(r24): u16 tile index grid
constexpr uint32_t kLayerTileDef = 4;    // lwz 4(r24): -> {page[], gfx[], clut[]}
constexpr uint32_t kLayerScrollX = 8;    // lhz 8(r24): integer part, s16
constexpr uint32_t kLayerScrollY = 12;   // lhz 12(r24)
constexpr uint32_t kLayerOrder = 24;     // lwz 24(r24): ordering-table base slot
constexpr uint32_t kLayerFlags = 28;     // lwz 28(r24)
constexpr uint32_t kLayerW = 32;         // lwz 32(r24): width in 16-tile blocks
constexpr uint32_t kLayerH = 36;         // lwz 36(r24)
constexpr uint32_t kLayerHideTimer = 40; // lwz 40(r24): skip while non-zero
// Flag bits tested by that routine (rlwinm masks in parentheses).
constexpr uint32_t kFlagVisible = 0x1;      // clrlwi 31
constexpr uint32_t kFlagBlend = 0x80;       // rlwinm 0,24,24: semi-transparent sprites
constexpr uint32_t kFlagAltPalette = 0x200; // rlwinm 0,22,22: palette index + 0x100
constexpr uint32_t kFlagRepeat = 0x1000;    // rlwinm 0,19,19: background layers only

constexpr int kStageWidth = 256;
constexpr int kPoolSprites = 4096;
constexpr uint32_t kSpriteSize = 20;

rex::Runtime* g_rt = nullptr;
PPCFunc* g_render_tilemap = nullptr;
PPCFunc* g_update_game = nullptr;
PPCFunc* g_render_primitives = nullptr;
Config g_cfg;
uint32_t g_pool = 0;  // guest address of the extra-sprite pool
int g_pool_used = 0;

// Stretch rectangle bookkeeping.
bool g_rect_widened = false;
int32_t g_rect_base[4] = {};
int32_t g_rect_ours[4] = {};
int g_margin = 0;          // margin in effect this frame (0 = not expanded)
int g_applied_margin = 0;  // margin last written to the stage buffers

// Primitive counters for the last expanded frame.
struct UiCounts {
  int docked_left = 0, docked_right = 0, overlays = 0, hidden = 0;
} g_ui;

uint8_t* P(uint32_t a) { return g_rt->memory()->TranslateVirtual<uint8_t*>(a); }
uint32_t R32(uint32_t a) {
  uint8_t* p = P(a);
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
uint16_t R16(uint32_t a) {
  uint8_t* p = P(a);
  return uint16_t((p[0] << 8) | p[1]);
}
uint8_t R8(uint32_t a) { return *P(a); }
void W32(uint32_t a, uint32_t v) {
  uint8_t* p = P(a);
  p[0] = uint8_t(v >> 24), p[1] = uint8_t(v >> 16), p[2] = uint8_t(v >> 8), p[3] = uint8_t(v);
}
void W16(uint32_t a, uint16_t v) {
  uint8_t* p = P(a);
  p[0] = uint8_t(v >> 8), p[1] = uint8_t(v);
}

// --- Display environments -------------------------------------------------

struct Env {
  int16_t clip_x, clip_w, ofs_x, disp_x, disp_w;
};

Env ReadEnv(uint32_t buf) {
  return Env{int16_t(R16(buf + 4 + 0)), int16_t(R16(buf + 4 + 4)), int16_t(R16(buf + 4 + 8)),
             int16_t(R16(buf + 100 + 0)), int16_t(R16(buf + 100 + 4))};
}

void WriteEnv(uint32_t buf, const Env& e) {
  W16(buf + 4 + 0, uint16_t(e.clip_x));
  W16(buf + 4 + 4, uint16_t(e.clip_w));
  W16(buf + 4 + 8, uint16_t(e.ofs_x));
  W16(buf + 100 + 0, uint16_t(e.disp_x));
  W16(buf + 100 + 4, uint16_t(e.disp_w));
}

bool Same(const Env& a, const Env& b) {
  return std::memcmp(&a, &b, sizeof a) == 0;
}

// Stage layout written by SetStageDisplayBuffer: two side-by-side buffers.
const Env kStage0{0, kStageWidth, 0, kStageWidth, kStageWidth};
const Env kStage1{kStageWidth, kStageWidth, kStageWidth, 0, kStageWidth};

Env Expanded(int margin) {
  // One buffer at VRAM x=0. Width 256 + 2*margin must stay <= 512: VRAM
  // x 512+ holds texture pages and y 256+ holds sprite sheets.
  int16_t w = int16_t(kStageWidth + 2 * margin);
  return Env{0, w, int16_t(margin), 0, w};
}

// Writes the stage buffers for `margin` (0 restores the game's layout).
// Returns true when the stage buffers carry an expanded layout this frame.
bool ApplyEnvs(int margin) {
  Env e0 = ReadEnv(kGpuBuffer0), e1 = ReadEnv(kGpuBuffer1);
  bool stage = Same(e0, kStage0) && Same(e1, kStage1);
  Env ours_env = Expanded(g_applied_margin);
  bool ours = g_applied_margin > 0 && Same(e0, ours_env) && Same(e1, ours_env);
  if (margin > 0 && (stage || ours)) {
    if (!ours || margin != g_applied_margin) {
      Env want = Expanded(margin);
      WriteEnv(kGpuBuffer0, want);
      WriteEnv(kGpuBuffer1, want);
      g_applied_margin = margin;
    }
    return true;
  }
  if (ours) {
    WriteEnv(kGpuBuffer0, kStage0);
    WriteEnv(kGpuBuffer1, kStage1);
  }
  g_applied_margin = 0;
  return false;
}

// The stretch rectangle the game or the player chose, ignoring our widening.
void BaseRect(int32_t out[4]) {
  int32_t cur[4];
  for (int i = 0; i < 4; ++i) cur[i] = int32_t(R32(kStretchRect + 4 * i));
  bool is_ours = g_rect_widened && std::memcmp(cur, g_rect_ours, sizeof cur) == 0;
  std::memcpy(out, is_ours ? g_rect_base : cur, sizeof cur);
}

// Auto margin: the widest picture, at the same pixel scale, whose sides stay
// inside the front-end frame. The frame and the base rectangle are read every
// frame, so a new stretch preset or resolution changes the margin live.
int ResolveMargin() {
  if (!g_cfg.enabled) return 0;
  if (g_cfg.margin >= 0) return std::min(g_cfg.margin, kMaxMargin);
  int32_t base[4], frame[4];
  BaseRect(base);
  for (int i = 0; i < 4; ++i) frame[i] = int32_t(R32(kStretchRectMax + 4 * i));
  int64_t width = int64_t(base[2]) - base[0];
  if (width <= 0) return 0;
  // Half the room on the narrower side, in PS1 pixels: room * 256 / width.
  int64_t centre2 = int64_t(base[2]) + base[0];
  int64_t room2 = std::min(centre2 - 2 * int64_t(frame[0]), 2 * int64_t(frame[2]) - centre2);
  int64_t half_total = room2 * kStageWidth / (2 * width);  // PS1 px from centre to frame edge
  int64_t margin = half_total - kStageWidth / 2;
  return int(std::clamp<int64_t>(margin, 0, kMaxMargin));
}

void ApplyStretch(bool expanded) {
  int32_t cur[4];
  for (int i = 0; i < 4; ++i) cur[i] = int32_t(R32(kStretchRect + 4 * i));
  bool is_ours = g_rect_widened && std::memcmp(cur, g_rect_ours, sizeof cur) == 0;
  if (!expanded) {
    if (is_ours) {
      for (int i = 0; i < 4; ++i) W32(kStretchRect + 4 * i, uint32_t(g_rect_base[i]));
    }
    g_rect_widened = false;
    return;
  }
  if (!is_ours) {
    // New base (first frame, or the player or game changed the rectangle).
    std::memcpy(g_rect_base, cur, sizeof cur);
  }
  const int32_t* b = g_rect_base;
  int64_t width = int64_t(b[2]) - b[0];
  int64_t centre2 = int64_t(b[2]) + b[0];  // 2 * centre
  int64_t total = int64_t(kStageWidth + 2 * g_margin);
  int64_t new_width = width * total / kStageWidth;
  g_rect_ours[0] = int32_t((centre2 - new_width) / 2);
  g_rect_ours[2] = int32_t(g_rect_ours[0] + new_width);
  g_rect_ours[1] = b[1];
  g_rect_ours[3] = b[3];
  for (int i = 0; i < 4; ++i) W32(kStretchRect + 4 * i, uint32_t(g_rect_ours[i]));
  g_rect_widened = true;
}

// --- Extra tile columns ---------------------------------------------------

void AddPrim(uint32_t ot, uint32_t slot, uint32_t prim) {
  uint32_t entry = ot + slot * 8;
  W32(prim, R32(entry));
  W32(entry, prim);
}

void EmitSprite(uint32_t ot, uint32_t slot, int x0, int y0, uint8_t u, uint8_t v, uint16_t clut,
                bool semi) {
  if (g_pool_used >= kPoolSprites) return;
  uint32_t s = g_pool + uint32_t(g_pool_used++) * kSpriteSize;
  std::memset(P(s), 0, kSpriteSize);
  P(s)[4] = 20;                                   // length, as the game writes it
  P(s)[5] = uint8_t(0x7C | 1 | (semi ? 2 : 0));  // SPRT_16, raw texture
  W16(s + 12, uint16_t(int16_t(x0)));
  W16(s + 14, uint16_t(int16_t(y0)));
  P(s)[16] = u;
  P(s)[17] = v;
  W16(s + 18, clut);
  AddPrim(ot, slot, s);
}

// Adds the 16x16 tiles of one layer that fall in the margins.
//
// The game's routine sub_82258190 covers 16 rows and 17 columns per layer,
// starting at the tile under the scroll position, offset by the camera
// (stage x at 0x8316AF48, y at 0x83173FA4) minus the sub-tile scroll. This
// function walks the same grid but only the columns left of column 0 and
// right of column 16, so the original columns are never drawn twice. Each
// tile becomes the same 20-byte textured-sprite packet the routine builds
// (code 0x7C, raw texture, blend bit from the layer) and is linked into the
// same ordering-table slot (layer base + texture page of the tile).
void AddMarginTiles(uint32_t layer, bool background, int cam_x, int cam_y, uint32_t ot,
                    int margin) {
  uint32_t flags = R32(layer + kLayerFlags) & 0xFFFF;
  if (!(flags & kFlagVisible) || R32(layer + kLayerHideTimer) != 0) return;
  uint32_t grid = R32(layer + kLayerTiles);
  uint32_t def = R32(layer + kLayerTileDef);
  if (!grid || !def) return;
  uint32_t pages = R32(def + 0), cells = R32(def + 4), palettes = R32(def + 8);
  int slot_base = int(R32(layer + kLayerOrder) & 0xFFFF);
  int cols = int(R32(layer + kLayerW)) * 16;
  int rows = int(R32(layer + kLayerH)) * 16;
  int scroll_x = int16_t(R16(layer + kLayerScrollX));
  int scroll_y = int16_t(R16(layer + kLayerScrollY));
  bool repeat = background && (flags & kFlagRepeat);
  bool blend = flags & kFlagBlend;
  uint32_t palette_bias = (flags & kFlagAltPalette) ? 0x100 : 0;
  int first_col = scroll_x >> 4, first_row = scroll_y >> 4;
  if (repeat) {
    first_col &= 15;
    first_row &= 15;
  }
  int left = cam_x - (scroll_x & 15);
  int top = cam_y - (scroll_y & 15);
  int margin_cols = (margin + 15) / 16 + 1;
  for (int r = 0; r < 16; ++r) {
    int row = first_row + r;
    if (repeat) row &= 15;
    if (row < 0) continue;
    if (row >= rows) break;
    for (int c = -margin_cols; c < 17 + margin_cols; ++c) {
      if (c == 0) c = 17;  // columns 0..16 belong to the game's own pass
      int col = first_col + c;
      if (repeat) col &= 15;
      if (col < 0 || col >= cols) continue;
      uint16_t tile = R16(grid + uint32_t(row * cols + col) * 2);
      if (tile == 0) continue;
      uint8_t cell = R8(cells + tile);  // texture cell: low nibble u/16, high nibble v
      uint16_t clut = R16(kClutIds + (R8(palettes + tile) + palette_bias) * 2);
      EmitSprite(ot, uint32_t(slot_base + R8(pages + tile)), left + 16 * c, top + 16 * r,
                 uint8_t(cell << 4), uint8_t(cell & 0xF0), clut, blend);
    }
  }
}

bool g_expanded_this_frame = false;

extern "C" void Expand_RenderTilemap(PPCContext& ctx, uint8_t* base) {
  if (g_expanded_this_frame) {
    g_pool_used = 0;
    int cam_x = int(int32_t(R32(kCameraOffsetX)));
    int cam_y = int(int32_t(R32(kCameraOffsetY)));
    uint32_t ot = R32(kCurrentBuffer) + kOtOffset;
    // Before the original call: the original then inserts its DR_MODE
    // (texture page) primitives ahead of these sprites in every OT slot.
    AddMarginTiles(kTilemap, false, cam_x, cam_y, ot, g_margin);
    for (int l = 0; l < kBgLayerCount; ++l) {
      AddMarginTiles(kBgLayers + uint32_t(l) * kBgLayerStride, true, cam_x, cam_y, ot,
                     g_margin);
    }
  }
  g_render_tilemap(ctx, base);
}

// --- Screen-space UI ------------------------------------------------------
//
// Screen-space primitives (DRAW_ABSPOS) use original-view coordinates, 0-255.
// Three rules apply to them while the scene is expanded, in this order:
//
// 1. Dock. Primitives on the player-HUD chains split at the view centre, as
//    ar-recomp splits its action HUD: the left group moves to the left edge
//    of the widened picture (x - margin) and the right group to the right
//    edge (x + margin). Parked parts stay outside the picture after the move,
//    so the boss gauge still slides in from the right edge.
// 2. Overlays. Untextured tiles and quads that cover the whole original
//    width (fades, flashes) are widened to cover the margins too.
// 3. Fail closed. Any other screen-space primitive wholly outside the
//    original view stays hidden, as the original clip hid it.
//
// Centred UI such as dialogue keeps its position. Every change is made to a
// copy-on-write record: the bytes are restored right after the original
// call, so no game state changes.

struct Saved {
  int index;
  uint8_t bytes[kPrimSize];
};
Saved g_saved[kPrimCount];
bool g_is_hud[kPrimCount];

void MarkHudChain(int32_t first) {
  int32_t i = first;
  for (int guard = 0; guard < 64 && i >= 0 && i < kPrimCount; ++guard) {
    g_is_hud[i] = true;
    uint32_t next = R32(kPrimBuf + uint32_t(i) * kPrimSize);
    if (next < kPrimBuf || next >= kPrimBuf + kPrimCount * kPrimSize) break;
    i = int32_t((next - kPrimBuf) / kPrimSize);
  }
}

int VertexCount(int type) {
  switch (type) {
    case 2: return 2;  // LINE_G2
    case 3:            // G4
    case 4: return 4;  // GT4
    case 5: return 3;  // GT3
    default: return 0;
  }
}

constexpr uint32_t kXs[4] = {kPrimX0, kPrimX1, kPrimX2, kPrimX3};

void ShiftX(uint32_t p, int n, int dx) {
  for (int k = 0; k < std::max(n, 1); ++k) {
    W16(p + kXs[k], uint16_t(int16_t(int16_t(R16(p + kXs[k])) + dx)));
  }
}

extern "C" void Expand_RenderPrimitives(PPCContext& ctx, uint8_t* base) {
  int saved = 0;
  if (g_expanded_this_frame) {
    const int m = g_margin;
    std::memset(g_is_hud, 0, sizeof g_is_hud);
    if (g_cfg.dock) {
      MarkHudChain(int32_t(R32(kPlayerHud + 4)));
      MarkHudChain(int32_t(R32(kPlayerHud + 8)));
    }
    for (int i = 0; i < kPrimCount; ++i) {
      uint32_t p = kPrimBuf + uint32_t(i) * kPrimSize;
      uint16_t mode = R16(p + kPrimDrawMode);
      if (!(mode & kDrawAbsPos) || (mode & kDrawHide)) continue;
      int type = R8(p + kPrimType) & 0x0F;
      int n = VertexCount(type);
      int lo, hi;
      if (type == 1 || type == 6) {  // TILE (width in u0) or SPRT (width in u1)
        lo = int16_t(R16(p + kPrimX0));
        hi = lo + R8(p + (type == 1 ? kPrimU0 : kPrimU1));
      } else if (n) {
        lo = hi = int16_t(R16(p + kPrimX0));
        for (int k = 1; k < n; ++k) {
          int x = int16_t(R16(p + kXs[k]));
          lo = std::min(lo, x), hi = std::max(hi, x);
        }
      } else {
        continue;
      }
      uint16_t priority = R16(p + kPrimPriority);
      bool hud = g_is_hud[i] && priority >= kHudPriorityLow && priority <= kHudPriorityHigh;
      bool overlay = (type == 1 || type == 3) && lo <= 0 && hi >= kStageWidth - 1;
      bool outside = hi <= 0 || lo >= kStageWidth;
      if (!hud && !overlay && !outside) continue;

      Saved& s = g_saved[saved++];
      s.index = i;
      std::memcpy(s.bytes, P(p), kPrimSize);
      if (hud) {
        bool left = lo + hi < kStageWidth;  // centre left of the view centre
        ShiftX(p, n, left ? -m : m);
        ++(left ? g_ui.docked_left : g_ui.docked_right);
      } else if (overlay) {
        if (type == 1) {
          // A tile is at most 255 wide; draw it as a flat quad instead.
          int y0 = int16_t(R16(p + kPrimY0)), y1 = y0 + R8(p + kPrimV0);
          P(p)[kPrimType] = uint8_t((P(p)[kPrimType] & 0xF0) | 3);
          for (uint32_t c : {kPrimR1, kPrimR2, kPrimR3}) std::memcpy(P(p + c), P(p + kPrimR0), 3);
          W16(p + kPrimX0, uint16_t(int16_t(lo - m)));
          W16(p + kPrimX1, uint16_t(int16_t(hi + m)));
          W16(p + kPrimX2, uint16_t(int16_t(lo - m)));
          W16(p + kPrimX3, uint16_t(int16_t(hi + m)));
          W16(p + kPrimY1, uint16_t(int16_t(y0)));
          W16(p + kPrimY2, uint16_t(int16_t(y1)));
          W16(p + kPrimY3, uint16_t(int16_t(y1)));
        } else {
          for (int k = 0; k < 4; ++k) {
            int x = int16_t(R16(p + kXs[k]));
            if (x <= lo) x -= m;
            if (x >= hi) x += m;
            W16(p + kXs[k], uint16_t(int16_t(x)));
          }
        }
        ++g_ui.overlays;
      } else {
        W16(p + kPrimDrawMode, uint16_t(mode | kDrawHide));
        ++g_ui.hidden;
      }
    }
  }
  g_render_primitives(ctx, base);
  for (int k = 0; k < saved; ++k) {
    std::memcpy(P(kPrimBuf + uint32_t(g_saved[k].index) * kPrimSize), g_saved[k].bytes,
                kPrimSize);
  }
}

extern "C" void Expand_UpdateGame(PPCContext& ctx, uint8_t* base) {
  if (g_cfg.frozen) return;
  g_update_game(ctx, base);
}

}  // namespace

Config& Settings() { return g_cfg; }

std::string Install(rex::Runtime* runtime) {
  g_rt = runtime;
  g_pool = runtime->memory()->SystemHeapAlloc(kPoolSprites * kSpriteSize);
  auto* d = runtime->function_dispatcher();
  bool ok = d->OverrideFunction(kRenderTilemapFn, &Expand_RenderTilemap, &g_render_tilemap) &&
            d->OverrideFunction(kUpdateGameFn, &Expand_UpdateGame, &g_update_game) &&
            d->OverrideFunction(kRenderPrimitivesFn, &Expand_RenderPrimitives,
                                &g_render_primitives);
  return std::string("expand install ") + (ok && g_pool ? "ok" : "FAILED");
}

void BeforeFrame() {
  int margin = ResolveMargin();
  g_expanded_this_frame = ApplyEnvs(margin);
  g_margin = g_expanded_this_frame ? margin : 0;
  ApplyStretch(g_expanded_this_frame);
}

std::string Status() {
  int32_t r[4];
  for (int i = 0; i < 4; ++i) r[i] = int32_t(R32(kStretchRect + 4 * i));
  char buf[200];
  std::snprintf(buf, sizeof buf,
                "margin=%d (%s) rect=%d,%d,%d,%d dock=%s ui: left=%d right=%d overlays=%d "
                "hidden=%d",
                g_margin, g_cfg.margin < 0 ? "auto" : "fixed", r[0], r[1], r[2], r[3],
                g_cfg.dock ? "on" : "off", g_ui.docked_left, g_ui.docked_right, g_ui.overlays,
                g_ui.hidden);
  g_ui = UiCounts{};  // counters cover the frames since the previous status
  return buf;
}

}  // namespace expand
