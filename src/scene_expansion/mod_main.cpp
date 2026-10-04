// scene_probe: throwaway research mod for the scene-expansion proof.
//
// Control directory: $SCENE_PROBE_DIR (default /tmp/nocturne-expand/ctl).
//   buttons   one line "<hex buttons>" applied to pad 0 while the file exists
//   cmd       one command per line; consumed (deleted) at the next frame
//   log       append-only probe log
// Commands:
//   dump <guestaddr hex> <len dec> <file>   raw guest bytes to file
//   poke16|poke32|poke8 <guestaddr hex> <value hex>
//   frames                                   log the frame counter
//   vram <file>                              raw 1024x512 big-endian VRAM
#include <rex/system/mod_plugin.h>

#include <rex/input/input.h>
#include <rex/input/input_driver.h>
#include <rex/input/input_system.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>
#include <rex/system/xmemory.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "expand.h"

namespace fs = std::filesystem;

namespace {
using namespace rex;
using namespace rex::input;

constexpr uint32_t kMainLoopIterFn = 0x8224DCE8u;  // stage main-loop iteration
constexpr uint32_t kGpuObjectPtr = 0x83133B00u;     // software PS1 GPU object
constexpr uint32_t kVramOffset = 4;                 // VRAM inside the object

rex::Runtime* g_runtime = nullptr;

// Game-state snapshot (research only): the PS1-game globals region.
uint32_t g_snap_base = 0x83000000u;
uint32_t g_snap_end = 0x831DA600u;
std::vector<uint8_t> g_snapshot;

// Frame-indexed input script and per-frame activation trace.
constexpr uint32_t kEntities = 0x8314EEC0u;  // g_Entities (player first)
constexpr uint32_t kEntityStride = 252;
constexpr int kEntityCount = 256;
constexpr uint32_t kTilemapScroll = 0x8316AF88u;  // g_Tilemap.scrollX (s32 16.16)
std::vector<std::pair<int, uint16_t>> g_script;  // (frames, buttons)
std::vector<uint16_t> g_script_frames;
size_t g_script_pos = 0;
bool g_script_active = false;
uint16_t g_script_buttons = 0;
fs::path g_trace;
PPCFunc* g_original_main_iter = nullptr;
std::atomic<uint64_t> g_frame{0};
fs::path g_dir;

void Log(const std::string& line) {
  std::ofstream out(g_dir / "log", std::ios::app);
  out << "[" << g_frame.load() << "] " << line << "\n";
}

uint8_t* Guest(uint32_t addr) {
  return g_runtime->memory()->TranslateVirtual<uint8_t*>(addr);
}

uint32_t Read32(uint32_t addr) {
  uint8_t* p = Guest(addr);
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

void WriteFile(const fs::path& path, const uint8_t* data, size_t len) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(data), std::streamsize(len));
}

void RunCommand(const std::string& line) {
  std::istringstream in(line);
  std::string op;
  in >> op;
  if (op == "dump") {
    std::string a, file;
    size_t len = 0;
    in >> a >> len >> file;
    WriteFile(file, Guest(uint32_t(std::stoul(a, nullptr, 16))), len);
    Log("dump " + a + " " + std::to_string(len) + " -> " + file);
  } else if (op == "vram") {
    std::string file;
    in >> file;
    uint32_t obj = Read32(kGpuObjectPtr);
    WriteFile(file, Guest(obj + kVramOffset), 1024 * 512 * 2);
    char buf[64];
    std::snprintf(buf, sizeof buf, "vram obj=%08X -> ", obj);
    Log(buf + file);
  } else if (op == "poke8" || op == "poke16" || op == "poke32") {
    std::string a, v;
    in >> a >> v;
    uint8_t* p = Guest(uint32_t(std::stoul(a, nullptr, 16)));
    uint32_t value = uint32_t(std::stoul(v, nullptr, 16));
    int n = op == "poke8" ? 1 : op == "poke16" ? 2 : 4;
    for (int i = 0; i < n; ++i) {
      p[i] = uint8_t(value >> (8 * (n - 1 - i)));
    }
    Log(line);
  } else if (op == "expand") {
    std::string v;
    in >> v;
    expand::Settings().enabled = v == "on";
    Log(line);
  } else if (op == "freeze") {
    std::string v;
    in >> v;
    expand::Settings().frozen = v == "on";
    Log(line);
  } else if (op == "margin") {
    in >> expand::Settings().margin;
    Log(line);
  } else if (op == "snapshot") {
    std::string v, a, b;
    in >> v >> a >> b;
    if (v == "save" && !b.empty()) {
      g_snap_base = uint32_t(std::stoul(a, nullptr, 16));
      g_snap_end = uint32_t(std::stoul(b, nullptr, 16));
    }
    uint8_t* p = Guest(g_snap_base);
    if (v == "save") {
      g_snapshot.assign(p, p + (g_snap_end - g_snap_base));
    } else if (v == "load" && !g_snapshot.empty()) {
      std::memcpy(p, g_snapshot.data(), g_snapshot.size());
    }
    Log(line);
  } else if (op == "script") {
    std::string file, trace;
    in >> file >> trace;
    g_script_frames.clear();
    std::ifstream s(file);
    int frames;
    std::string hex;
    while (s >> frames >> hex) {
      uint16_t b = uint16_t(std::stoul(hex, nullptr, 16));
      for (int i = 0; i < frames; ++i) g_script_frames.push_back(b);
    }
    g_trace = trace;
    std::ofstream(g_trace, std::ios::trunc);
    g_script_pos = 0;
    g_script_active = !g_script_frames.empty();
    expand::Settings().frozen = false;  // start in the same frame as the load
    Log(line + " (" + std::to_string(g_script_frames.size()) + " frames)");
  } else if (op == "frames") {
    Log("frames");
  } else if (!op.empty()) {
    Log("unknown command: " + line);
  }
}

void PollCommands() {
  fs::path cmd = g_dir / "cmd";
  std::error_code ec;
  if (!fs::exists(cmd, ec)) {
    return;
  }
  fs::path taken = g_dir / "cmd.taken";
  fs::rename(cmd, taken, ec);
  if (ec) {
    return;
  }
  std::ifstream in(taken);
  std::string line;
  while (std::getline(in, line)) {
    RunCommand(line);
  }
  fs::remove(taken, ec);
}

uint64_t Fnv(const uint8_t* p, size_t n) {
  uint64_t h = 1469598103934665603ull;
  for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ull;
  return h;
}

void TraceFrame() {
  const uint8_t* e = Guest(kEntities);
  int live = 0;
  std::string ids;
  for (int i = 0; i < kEntityCount; ++i) {
    const uint8_t* s = e + i * kEntityStride;
    bool any = false;
    for (uint32_t k = 0; k < kEntityStride && !any; ++k) any = s[k] != 0;
    if (any) {
      ++live;
      char h[32];
      std::snprintf(h, sizeof h, " %d:%08x", i, uint32_t(Fnv(s, kEntityStride)));
      ids += h;
    }
  }
  uint32_t scroll = Read32(kTilemapScroll);
  int px = int16_t((Read32(kEntities) >> 16)), py = int16_t((Read32(kEntities + 4) >> 16));
  int sy = int32_t(Read32(kTilemapScroll + 4)) >> 16;
  char buf[160];
  std::snprintf(buf, sizeof buf, "%zu btn=%04x scroll=%d,%d player=%d,%d live=%d hash=%016llx", g_script_pos,
                g_script_buttons, int32_t(scroll) >> 16, sy, px, py, live,
                (unsigned long long)Fnv(e, kEntityCount * kEntityStride));
  std::ofstream(g_trace, std::ios::app) << buf << " slots:" << ids << "\n";
}

extern "C" void SceneProbe_MainIter(PPCContext& ctx, uint8_t* base) {
  PollCommands();
  if (g_script_active) {
    g_script_buttons = g_script_frames[g_script_pos];
  }
  expand::BeforeFrame();
  g_original_main_iter(ctx, base);
  if (g_script_active) {
    TraceFrame();
    if (++g_script_pos >= g_script_frames.size()) {
      g_script_active = false;
      g_script_buttons = 0;
      expand::Settings().frozen = true;
      Log("script done; frozen");
    }
  }
  g_frame.fetch_add(1);
}

class ScriptedPad : public rex::input::InputDriver {
 public:
  ScriptedPad() : InputDriver(nullptr, 0) {}
  X_STATUS Setup() override { return X_STATUS_SUCCESS; }
  bool is_physical_device() const override { return false; }
  X_RESULT GetCapabilities(uint32_t user_index, uint32_t, X_INPUT_CAPABILITIES* caps) override {
    if (user_index != 0) return X_ERROR_DEVICE_NOT_CONNECTED;
    if (caps) {
      std::memset(caps, 0, sizeof(*caps));
      caps->type = 0x01;
      caps->sub_type = 0x01;
      caps->gamepad.buttons = 0xFFFF;
    }
    return X_ERROR_SUCCESS;
  }
  X_RESULT GetState(uint32_t user_index, X_INPUT_STATE* out) override {
    if (user_index != 0) return X_ERROR_DEVICE_NOT_CONNECTED;
    uint16_t buttons = 0;
    if (g_script_active) {
      buttons = g_script_buttons;
    } else {
      std::ifstream in(g_dir / "buttons");
      std::string hex;
      if (in >> hex) {
        buttons = uint16_t(std::stoul(hex, nullptr, 16));
      }
    }
    if (out) {
      std::memset(out, 0, sizeof(*out));
      out->packet_number = ++packet_;
      out->gamepad.buttons = buttons;
    }
    return X_ERROR_SUCCESS;
  }
  X_RESULT SetState(uint32_t, X_INPUT_VIBRATION*) override { return X_ERROR_SUCCESS; }
  X_RESULT GetKeystroke(uint32_t, uint32_t, X_INPUT_KEYSTROKE*) override { return X_ERROR_EMPTY; }

 private:
  uint32_t packet_ = 0;
};

class SceneProbe : public rex::system::IModPlugin {
 public:
  explicit SceneProbe(rex::Runtime* runtime) { g_runtime = runtime; }

  void OnCreateDialogs(rex::ui::ImGuiDrawer*) override {
    auto* input = static_cast<rex::input::InputSystem*>(g_runtime->input_system());
    input->AddDriver(std::make_unique<ScriptedPad>());
    Log("scripted pad added");
  }

  void OnModuleLaunched() override {
    if (!g_runtime->function_dispatcher()->OverrideFunction(
            kMainLoopIterFn, &SceneProbe_MainIter, &g_original_main_iter)) {
      Log("override main iter FAILED");
      return;
    }
    Log("override main iter ok");
    Log(expand::Install(g_runtime));
  }
};

}  // namespace

extern "C" REX_MOD_PLUGIN_EXPORT uint32_t rex_mod_abi_version(void) {
  return rex::system::kModPluginAbiVersion;
}

extern "C" REX_MOD_PLUGIN_EXPORT rex::system::IModPlugin* rex_mod_create(
    uint32_t abi_version, const rex::system::ModHostContext* ctx) {
  if (abi_version != rex::system::kModPluginAbiVersion || !ctx) {
    return nullptr;
  }
  const char* dir = std::getenv("SCENE_PROBE_DIR");
  g_dir = dir ? dir : "/tmp/nocturne-expand/ctl";
  fs::create_directories(g_dir);
  return new SceneProbe(ctx->runtime);
}
