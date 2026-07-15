#include "rendereriosplatform.h"

#include <Tempest/Platform>

#if defined(__IOS__)

#import <Foundation/Foundation.h>

#include <cstdio>
#include <sys/sysctl.h>

RendererIOSPlatformInfo rendererIOSPlatformInfo() noexcept {
  RendererIOSPlatformInfo info;
  std::snprintf(info.osVersion.data(),info.osVersion.size(),"unknown");
  std::snprintf(info.deviceFamily.data(),info.deviceFamily.size(),"unknown");

  // The game runs on Tempest's hand-swapped main-thread fiber. Do not push an
  // Objective-C autorelease pool here; UIKit's run-loop pool owns autoreleased
  // objects created by this short synchronous query.
  @try {
    const NSOperatingSystemVersion version = NSProcessInfo.processInfo.operatingSystemVersion;
    std::snprintf(info.osVersion.data(),info.osVersion.size(),"%ld.%ld.%ld",
                  long(version.majorVersion),long(version.minorVersion),long(version.patchVersion));
    }
  @catch(NSException* exception) {
    (void)exception;
    }

  size_t size = info.deviceFamily.size();
  if(sysctlbyname("hw.machine",info.deviceFamily.data(),&size,nullptr,0)!=0 || size==0u)
    std::snprintf(info.deviceFamily.data(),info.deviceFamily.size(),"unknown");
  else
    info.deviceFamily.back() = '\0';
  return info;
  }

#endif
