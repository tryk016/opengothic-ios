#include "gamepad.h"

#include <Tempest/Platform>

// Non-iOS platforms have no GameController backend here; the .mm file provides
// the real implementation on iOS. Guarded so the two never define poll() twice
// (game/*.mm is compiled only on Apple targets).
#if !defined(__IOS__)

namespace Gamepad {
GamepadState poll() {
  return GamepadState{};
  }

GamepadInputFrame consume() {
  return GamepadInputFrame{};
  }
}

#endif
