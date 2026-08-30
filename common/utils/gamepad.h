#pragma once

#include <cstdint>
#include <vector>

enum class GamepadButton : uint8_t {
  A,
  B,
  X,
  Y,
  LB,
  RB,
  LT,
  RT,
  L3,
  R3,
  DpadUp,
  DpadDown,
  DpadLeft,
  DpadRight,
  Menu,
  Options,
  };

struct GamepadButtonEvent {
  GamepadButton button   = GamepadButton::A;
  bool          pressed  = false;
  uint64_t      sequence = 0;
  };

// Cross-platform gamepad snapshot. On iOS this is filled from
// GameController.framework (see gamepad.mm); on other platforms poll()
// returns a disconnected state (see gamepad.cpp).

struct GamepadState {
  bool  connected = false;
  uint64_t generation = 0;  // epoch changed by controller/lifecycle resets
  uint64_t sampleSequence = 0; // backend publication order for diagnostics

  float lx = 0, ly = 0;   // left  stick, -1..1 (ly>0 == up/forward)
  float rx = 0, ry = 0;   // right stick, -1..1
  float lt = 0, rt = 0;   // triggers, 0..1

  bool  a = false, b = false, x = false, y = false;
  bool  lb = false, rb = false;              // shoulders
  bool  ltPressed = false, rtPressed = false;// backend digital trigger state
  bool  l3 = false, r3 = false;              // stick presses
  bool  dup = false, ddown = false, dleft = false, dright = false;
  bool  menu = false, options = false;       // menu (☰) / view
  };

// consume() returns every queued digital transition together with the freshest
// analog state. poll() remains a non-consuming state peek for touch/UI callers.
struct GamepadInputFrame {
  GamepadState                    state;
  std::vector<GamepadButtonEvent> events;
  uint64_t                        droppedEvents = 0; // FIFO overflows since consume()
  };

namespace Gamepad {
GamepadState poll();
GamepadInputFrame consume();
}
