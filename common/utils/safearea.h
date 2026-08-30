#pragma once

// Display cutout margins (rounded screen corners, Dynamic Island, home
// indicator) in framebuffer PIXELS, matching the widget coordinate space.
// Content outside these margins is physically clipped or covered by hardware.
// Implemented in safearea.mm on iOS; zeros elsewhere (safearea.cpp).
struct SafeArea {
  struct Insets {
    int top=0, left=0, bottom=0, right=0;
    };
  static Insets insets();
  };
