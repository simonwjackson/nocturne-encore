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
  int margin = 64;  // extra PS1 pixels on each side of the 256-wide view
};

Config& Settings();

// Install the guest-function overrides. Returns a log line.
std::string Install(rex::Runtime* runtime);

// Called once per main-loop iteration, before the original runs.
void BeforeFrame();

}  // namespace expand
