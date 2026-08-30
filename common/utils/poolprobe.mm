#include "poolprobe.h"

#include <Tempest/Platform>

#if defined(__IOS__)

#include <cstdio>

// Private libobjc debug helper: prints the current thread's autorelease-pool
// stack (pages, boundaries, contents) to stderr. Weak import — if a future OS
// drops it, the probe silently degrades to just the tag line.
extern "C" void _objc_autoreleasePoolPrint(void) __attribute__((weak_import));

void PoolProbe::dump(const char* tag) {
  std::fprintf(stderr, "[pool] ---- %s ----\n", tag!=nullptr ? tag : "?");
  if(&_objc_autoreleasePoolPrint != nullptr)
    _objc_autoreleasePoolPrint();
  std::fflush(stderr);
  }

#endif
