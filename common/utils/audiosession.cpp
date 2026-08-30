#include "audiosession.h"

#include <Tempest/Platform>

// iOS provides AudioSession::activate in audiosession.mm. Every other platform
// needs no audio-session setup. game/*.mm is compiled only on Apple targets, so
// on iOS this body is compiled out and the .mm wins; on desktop macOS __IOS__ is
// undefined, so this body is used and the .mm compiles to nothing.
#if !defined(__IOS__)

void AudioSession::activate() {
  }

#endif
