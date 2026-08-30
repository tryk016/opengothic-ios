#include "systemmsg.h"

#include <Tempest/Platform>

// iOS provides SystemMsg::fatal in systemmsg.mm. This .cpp covers every other
// platform. (game/*.mm is compiled only on Apple targets, so on iOS this body
// is compiled out and the .mm wins; on desktop macOS __IOS__ is undefined, so
// this body is used and the .mm compiles to nothing.)
#if !defined(__IOS__)

#include <Tempest/Log>

void SystemMsg::fatal(const char* title, const char* message) {
  Tempest::Log::e(title ? title : "", ": ", message ? message : "");
  }

#endif
