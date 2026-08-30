#pragma once

#include <cstdint>

// Lightweight tactile feedback. On iOS this maps to UIImpactFeedbackGenerator
// (implemented in haptics.mm); a no-op on every other platform (haptics.cpp).
struct Haptics {
  enum Kind : uint8_t { Light, Medium, Heavy };
  static void impact(Kind k);
  };
