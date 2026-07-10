#include "haptics.h"

#include <Tempest/Platform>

// iOS provides Haptics::impact in haptics.mm. Every other platform is a no-op.
// game/*.mm is compiled only on Apple targets, so on iOS this body is compiled
// out and the .mm wins; on desktop macOS __IOS__ is undefined, so this body is
// used and the .mm compiles to nothing.
#if !defined(__IOS__)

void Haptics::impact(Kind) {
  }

#endif
