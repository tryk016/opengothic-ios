#include "padglyph.h"

#include <Tempest/Platform>

// iOS loads bundled Xelu glyph textures in padglyphtex.mm. Everywhere else there
// are no bundled glyphs, so the drawn fallback in padglyph.cpp is always used.
// game/*.mm is compiled only on Apple targets, so guards prevent double-definition.
#if !defined(__IOS__)

const Tempest::Texture2d* PadGlyph::texture(Btn) {
  return nullptr;
  }

#endif
