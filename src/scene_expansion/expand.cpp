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

// Returns true when the stage buffers currently carry the expanded layout.
bool ApplyEnvs() {
  Env e0 = ReadEnv(kGpuBuffer0), e1 = ReadEnv(kGpuBuffer1);
  Env want = Expanded(g_cfg.margin);
  bool stage = Same(e0, kStage0) && Same(e1, kStage1);
  bool ours = Same(e0, want) && Same(e1, want);
  if (g_cfg.enabled) {
    if (stage) {
      WriteEnv(kGpuBuffer0, want);
      WriteEnv(kGpuBuffer1, want);
      return true;
    }
    return ours;
  }
  if (ours) {
    WriteEnv(kGpuBuffer0, kStage0);
    WriteEnv(kGpuBuffer1, kStage1);
  }
  return false;
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
  if (is_ours) return;
  // New base (first frame, or the player/game changed the rectangle).
  std::memcpy(g_rect_base, cur, sizeof cur);
  int64_t width = int64_t(cur[2]) - cur[0];
  int64_t centre2 = int64_t(cur[2]) + cur[0];  // 2 * centre
  int64_t total = int64_t(kStageWidth + 2 * g_cfg.margin);
  int64_t new_width = width * total / kStageWidth;
  g_rect_ours[0] = int32_t((centre2 - new_width) / 2);
  g_rect_ours[2] = int32_t(g_rect_ours[0] + new_width);
  g_rect_ours[1] = cur[1];
  g_rect_ours[3] = cur[3];
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
    AddMarginTiles(kTilemap, false, cam_x, cam_y, ot, g_cfg.margin);
    for (int l = 0; l < kBgLayerCount; ++l) {
      AddMarginTiles(kBgLayers + uint32_t(l) * kBgLayerStride, true, cam_x, cam_y, ot,
                     g_cfg.margin);
    }
  }
  g_render_tilemap(ctx, base);
}

// Screen-space (DRAW_ABSPOS) primitives that lie wholly outside the original
// 256-pixel view were hidden by the authentic clip; the game parks HUD parts
// there (for example the boss gauge at x=264). Keep them hidden: the same
// fail-closed rule ar-recomp PR #1 applies to unbound layers. The drawMode
// change is undone right after the original call, so game state is kept.
int g_parked[kPrimCount];

extern "C" void Expand_RenderPrimitives(PPCContext& ctx, uint8_t* base) {
  int parked = 0;
  if (g_expanded_this_frame) {
    for (int i = 0; i < kPrimCount; ++i) {
      uint32_t p = kPrimBuf + uint32_t(i) * kPrimSize;
      uint16_t mode = R16(p + 54);
      if (!(mode & kDrawAbsPos) || (mode & kDrawHide)) continue;
      int type = R8(p + 5) & 0x0F;
      int xs[4] = {int16_t(R16(p + 12)), int16_t(R16(p + 24)), int16_t(R16(p + 36)),
                   int16_t(R16(p + 48))};
      int n = 0, lo = 0, hi = 0;
      switch (type) {
        case 2: n = 2; break;  // LINE_G2
        case 3:                // G4
        case 4: n = 4; break;  // GT4
        case 5: n = 3; break;  // GT3
        case 1:                // TILE: width in u0
          lo = xs[0], hi = xs[0] + R8(p + 16);
          break;
        default: continue;
      }
      if (n) {
        lo = hi = xs[0];
        for (int k = 1; k < n; ++k) lo = std::min(lo, xs[k]), hi = std::max(hi, xs[k]);
      }
      if (hi <= 0 || lo >= kStageWidth) {
        W16(p + 54, uint16_t(mode | kDrawHide));
        g_parked[parked++] = i;
      }
    }
  }
  g_render_primitives(ctx, base);
  for (int k = 0; k < parked; ++k) {
    uint32_t p = kPrimBuf + uint32_t(g_parked[k]) * kPrimSize;
    W16(p + 54, uint16_t(R16(p + 54) & ~kDrawHide));
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
  g_expanded_this_frame = ApplyEnvs();
  ApplyStretch(g_expanded_this_frame);
}

}  // namespace expand
