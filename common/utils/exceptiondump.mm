#include "exceptiondump.h"

#include <Tempest/Platform>

#if defined(__IOS__)

#if __has_feature(objc_arc)
#error "Objective C++ ARC is not supported"
#endif

#import <Foundation/Foundation.h>

std::string ExceptionDump::describe(std::exception_ptr p) {
  if(!p)
    return "";
  try {
    std::rethrow_exception(p);
    }
  catch(NSException* e) {
    // Objective-C exception (e.g. a Metal/UIKit validation error). On arm64 it
    // unwinds through the C++ ABI, so it can be re-caught here by type. `e` is
    // owned by the runtime until __cxa_end_catch — copy everything out inside
    // the pool, do NOT release it (MRC).
    std::string ret = "NSException";
    @autoreleasepool {
      NSString* name   = [e name];
      NSString* reason = [e reason];
      ret += "(";
      ret += (name  !=nil ? [name   UTF8String] : "?");
      ret += ": ";
      ret += (reason!=nil ? [reason UTF8String] : "?");
      ret += ")";
      NSArray* bt = [e callStackSymbols];
      for(NSString* s in bt) {
        const char* c = [s UTF8String];
        if(c!=nullptr) {
          ret += "\n  ";
          ret += c;
          }
        }
      }
    return ret;
    }
  catch(const std::exception& e) {
    return std::string("std::exception(") + e.what() + ")";
    }
  catch(...) {
    return "unknown foreign exception";
    }
  }

#endif
