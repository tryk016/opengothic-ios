#include "exceptiondump.h"

#include <Tempest/Platform>

// iOS provides ExceptionDump::describe in exceptiondump.mm (NSException-aware).
// This .cpp covers every other platform, same split as systemmsg.{mm,cpp}.
#if !defined(__IOS__)

std::string ExceptionDump::describe(std::exception_ptr p) {
  if(!p)
    return "";
  try {
    std::rethrow_exception(p);
    }
  catch(const std::exception& e) {
    return std::string("std::exception(") + e.what() + ")";
    }
  catch(...) {
    return "unknown foreign exception";
    }
  }

#endif
