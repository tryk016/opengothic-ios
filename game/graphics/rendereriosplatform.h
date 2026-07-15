#pragma once

#include <array>

struct RendererIOSPlatformInfo final {
  std::array<char,64> osVersion    = {};
  std::array<char,64> deviceFamily = {};
  };

RendererIOSPlatformInfo rendererIOSPlatformInfo() noexcept;
