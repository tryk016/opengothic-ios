#pragma once

#include <cstdint>

namespace MemoryInfo {

enum class ThermalState : uint8_t {
  Unknown,
  Nominal,
  Fair,
  Serious,
  Critical,
  };

enum Event : uint32_t {
  NoEvent                 = 0,
  MemoryWarning           = 1u << 0u,
  DidEnterBackground      = 1u << 1u,
  WillEnterForeground     = 1u << 2u,
  DidBecomeActive         = 1u << 3u,
  };

struct Snapshot final {
  uint64_t     footprintBytes = 0;
  uint64_t     availableBytes = 0;
  ThermalState thermal        = ThermalState::Unknown;

  bool footprintValid                  = false;
  bool availableValid                  = false;
  bool increasedMemoryLimitRequested   = false;
  bool increasedMemoryLimitChecked     = false;
  bool increasedMemoryLimitPresent     = false;
  };

// Registers cheap lifecycle observers on iOS. Notification callbacks only set
// atomic flags; snapshots and logging stay on the game loop.
void     initialize();
Snapshot snapshot();
uint32_t consumeEvents();

const char* thermalStateName(ThermalState state);

}
