#pragma once

#include <cstdint>

// Cross-platform gamepad snapshot. On iOS this is filled from
// GameController.framework (see gamepad.mm); on other platforms poll()
// returns a disconnected state (see gamepad.cpp).

struct GamepadState {
  bool  connected = false;
  uint64_t generation = 0;  // epoch changed by controller/lifecycle resets

  float lx = 0, ly = 0;   // left  stick, -1..1 (ly>0 == up/forward)
  float rx = 0, ry = 0;   // right stick, -1..1
  float lt = 0, rt = 0;   // triggers, 0..1

  bool  a = false, b = false, x = false, y = false;
  bool  lb = false, rb = false;              // shoulders
  bool  l3 = false, r3 = false;              // stick presses
  bool  dup = false, ddown = false, dleft = false, dright = false;
  bool  menu = false, options = false;       // menu (☰) / view
  };

namespace Gamepad {
GamepadState poll();
}
