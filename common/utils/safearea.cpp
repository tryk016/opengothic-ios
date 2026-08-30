#include "safearea.h"

#include <Tempest/Platform>

// iOS provides SafeArea::insets in safearea.mm. Other displays have no
// cutouts, so the HUD may use the full framebuffer. game/*.mm is compiled only
// on Apple targets; on iOS this body is compiled out and the .mm wins, on
// desktop macOS __IOS__ is undefined, so this body is used and the .mm
// compiles to nothing.
#if !defined(__IOS__)

SafeArea::Insets SafeArea::insets() {
  return Insets();
  }

#endif
