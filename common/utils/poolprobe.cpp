#include "poolprobe.h"

#include <Tempest/Platform>

// iOS provides PoolProbe::dump in poolprobe.mm; everywhere else it is a no-op.
// Same .mm/.cpp split as systemmsg and exceptiondump.
#if !defined(__IOS__)

void PoolProbe::dump(const char*) {
  }

#endif
