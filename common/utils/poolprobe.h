#pragma once

// Diagnostic for the iOS save-crash investigation: dump the Objective-C
// autorelease-pool stack (via libobjc's _objc_autoreleasePoolPrint) to stderr,
// tagged, so stderr.log shows the pool-stack shape around the crash window.
// No-op on non-iOS platforms (poolprobe.cpp) and when the private symbol is
// unavailable at runtime (weak import).
struct PoolProbe {
  static void dump(const char* tag);
  };
