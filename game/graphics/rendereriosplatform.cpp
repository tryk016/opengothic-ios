#include "rendereriosplatform.h"

#include <Tempest/Platform>

#include <cstdio>

#if !defined(__IOS__)

RendererIOSPlatformInfo rendererIOSPlatformInfo() noexcept {
  RendererIOSPlatformInfo info;
  std::snprintf(info.osVersion.data(),info.osVersion.size(),"non-iOS");
  std::snprintf(info.deviceFamily.data(),info.deviceFamily.size(),"non-iOS");
  return info;
  }

#endif
