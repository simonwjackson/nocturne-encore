// Scene expansion research code (SotN XBLA, vanilla v1.4.5 image).
#pragma once
#include <cstdint>
#include <string>

namespace rex {
class Runtime;
}

namespace expand {

struct Config {
  bool enabled = false;
  bool frozen = false;
  // Extra PS1 pixels on each side of the 256-wide view. -1 is Auto: the
  // largest margin that keeps the widened picture inside the front-end frame.
  int margin = -1;
  // Pin the player HUD's left and right groups to the edges of the picture.
  bool dock = true;
};

Config& Settings();

// Install the guest-function overrides. Returns a log line.
std::string Install(rex::Runtime* runtime);

// Called once per main-loop iteration, before the original runs.
void BeforeFrame();

// One line: margin in effect, stretch rectangle, dock state.
std::string Status();

}  // namespace expand
