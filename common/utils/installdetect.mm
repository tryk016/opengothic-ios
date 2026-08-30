#include "installdetect.h"

#if __has_feature(objc_arc)
#error "Objective C++ ARC is not supported"
#endif

#include <Tempest/TextCodec>

#import <Foundation/Foundation.h>

#if defined(OSX)
#import <Cocoa/Cocoa.h>
#endif

std::u16string InstallDetect::applicationSupportDirectory() {
  std::string ret;

  // NOTE (MRC): both `paths` (from NSSearchPathForDirectoriesInDomains) and
  // `app` (from stringByAppendingPathComponent:) are AUTORELEASED — we do not
  // own them, so we must NOT release them. The previous code sent them an extra
  // -release, which over-released the objects; the eventual autorelease-pool
  // drain (inside iOSApi::implProcessEvents) then messaged freed memory and
  // aborted with SIGABRT. Wrap in a pool so the temporaries are reclaimed
  // promptly without any manual release.
  @autoreleasepool {
#if defined(__OSX__)
    NSArray* paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
#else
    NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
#endif
    if(paths!=nil && [paths count]>0) {
#if defined(__OSX__)
      NSString* app = [[paths firstObject] stringByAppendingPathComponent:@"OpenGothic"];
#else
      NSString* app = [paths firstObject];
#endif
      if(app!=nil) {
        const char* c = [app cStringUsingEncoding:NSUTF8StringEncoding];
        if(c!=nullptr)
          ret = c;
        }
      }
    }

  return Tempest::TextCodec::toUtf16(ret);
  }
