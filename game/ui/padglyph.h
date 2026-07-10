#pragma once

#include <cstdint>
#include <string_view>

namespace Tempest { class Painter; }
class GthFont;

// Draws controller-style button glyphs (round A/B/X/Y in Xbox colours, shoulder
// and trigger pills, sticks, D-pad arrows, menu/view icons) with the game font
// for labels. Own vector rendering — no external glyph assets. Used by the touch
// overlay and the controls-help bar so buttons look like buttons, not squares.
namespace PadGlyph {
  enum Btn : uint8_t {
    A, B, X, Y,
    LB, RB, LT, RT,
    L3, R3,
    DPadUp, DPadDown, DPadLeft, DPadRight,
    Menu, View,
    LStick, RStick,
    };

  // Draw the glyph inside the square of side s with top-left (x,y). alpha scales opacity.
  void draw(Tempest::Painter& p, const GthFont& fnt, Btn b, int x, int y, int s, float alpha=1.f);

  // Convenience for the hint bar: glyph of side s at (x,y) followed by `label`.
  // Returns the total advance in pixels (glyph + gap + text).
  int  drawLabelled(Tempest::Painter& p, const GthFont& fnt, Btn b,
                    int x, int y, int s, std::string_view label, float alpha=1.f);
  }
