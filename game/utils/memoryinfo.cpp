#include "memoryinfo.h"

#include <Tempest/Platform>

#if !defined(__IOS__)

void MemoryInfo::initialize() {
  }

MemoryInfo::Snapshot MemoryInfo::snapshot() {
  return Snapshot();
  }

uint32_t MemoryInfo::consumeEvents() {
  return NoEvent;
  }

const char* MemoryInfo::thermalStateName(ThermalState state) {
  switch(state) {
    case ThermalState::Nominal:  return "nominal";
    case ThermalState::Fair:     return "fair";
    case ThermalState::Serious:  return "serious";
    case ThermalState::Critical: return "critical";
    case ThermalState::Unknown:  return "unknown";
    }
  return "unknown";
  }

#endif
