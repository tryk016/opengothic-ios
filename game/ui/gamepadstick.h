#pragma once

#include <algorithm>
#include <cmath>

struct GamepadStick final {
  float x = 0.f;
  float y = 0.f;
  };

// Applies one circular dead zone to the whole stick and rescales the remaining
// magnitude back to 0..1. The function owns no state, so returning to the
// center always produces zero on the very next game tick.
inline GamepadStick gamepadRadialDeadZone(float x, float y, float deadZone) {
  if(!std::isfinite(x) || !std::isfinite(y))
    return {};

  x = std::clamp(x,-1.f,1.f);
  y = std::clamp(y,-1.f,1.f);
  deadZone = std::clamp(deadZone,0.f,0.95f);

  const float magnitude = std::sqrt(x*x + y*y);
  if(!(magnitude>deadZone))
    return {};

  const float saturated = std::min(magnitude,1.f);
  const float scaled    = (saturated-deadZone)/(1.f-deadZone);
  const float factor    = scaled/magnitude;
  return {x*factor,y*factor};
  }
