#pragma once

#include <cstdint>

// Pure state reducer for one signed gamepad axis. It deliberately owns no
// callbacks: consumers compare positive()/negative() before and after update
// and decide which gameplay actions to press or release. Activation may use a
// higher threshold than an already-active direction; the lower release
// threshold only rearms a direction after the stick returns to neutral.
class GamepadAxisState final {
  public:
    constexpr void update(float value, float pressThreshold,
                          float releaseThreshold) {
      update(value, pressThreshold, pressThreshold, releaseThreshold);
      }

    // The activation threshold may be higher than the threshold which releases
    // an already active direction. This lets a caller reject perpendicular
    // stick drift without stopping a direction merely because the other axis
    // changed after it was engaged.
    constexpr void update(float value, float pressThreshold,
                          float activeThreshold, float releaseThreshold) {
      const float press   = clamp01(pressThreshold);
      const float active  = min(clamp01(activeThreshold),press);
      const float release = min(clamp01(releaseThreshold),active);
      value = clampSigned(value);

      // A direction which has just been released must reach the inner neutral
      // band before it can fire again. This prevents chatter around the press
      // threshold without prolonging the gameplay action itself.
      if(value>=-release && value<=release) {
        negativeArmed = true;
        positiveArmed = true;
        }

      switch(dir) {
        case Direction::Neutral:
          if(value>press && positiveArmed) {
            dir = Direction::Positive;
            positiveArmed = false;
            }
          else if(value<-press && negativeArmed) {
            dir = Direction::Negative;
            negativeArmed = false;
            }
          break;
        case Direction::Positive:
          // A true reversal is a new direction, not threshold chatter. Allow
          // it even when that side has not passed through the neutral band.
          if(value<-press) {
            dir = Direction::Negative;
            negativeArmed = false;
            }
          else if(value<=active)
            dir = Direction::Neutral;
          break;
        case Direction::Negative:
          if(value>press) {
            dir = Direction::Positive;
            positiveArmed = false;
            }
          else if(value>=-active)
            dir = Direction::Neutral;
          break;
        }
      }

    constexpr void reset() {
      dir = Direction::Neutral;
      negativeArmed = true;
      positiveArmed = true;
      }

    constexpr bool positive() const {
      return dir==Direction::Positive;
      }

    constexpr bool negative() const {
      return dir==Direction::Negative;
      }

    // Removes the inner dead-zone and returns an analog -1..1 value only for
    // the direction currently owned by the reducer.
    constexpr float scaled(float value, float releaseThreshold) const {
      const float release = clamp01(releaseThreshold);
      value = clampSigned(value);
      if(dir==Direction::Positive && value>release)
        return scale(value, release);
      if(dir==Direction::Negative && value<-release)
        return -scale(-value, release);
      return 0.f;
      }

  private:
    enum class Direction : int8_t {
      Negative = -1,
      Neutral  =  0,
      Positive =  1,
      };

    Direction dir = Direction::Neutral;
    bool negativeArmed = true;
    bool positiveArmed = true;

    static constexpr float min(float a, float b) {
      return a<b ? a : b;
      }

    static constexpr float clamp01(float value) {
      if(!(value>=0.f))
        return 0.f;
      return value>1.f ? 1.f : value;
      }

    static constexpr float clampSigned(float value) {
      if(!(value>=-1.f))
        return -1.f;
      return value>1.f ? 1.f : value;
      }

    static constexpr float scale(float value, float deadZone) {
      if(deadZone>=1.f)
        return 0.f;
      return clamp01((value-deadZone)/(1.f-deadZone));
      }
  };

namespace GamepadAxisStateCompileTests {
constexpr bool releaseAtPressAndRearmAtNeutral() {
  GamepadAxisState axis;
  axis.update(0.25f, 0.25f, 0.15f);
  if(axis.positive() || axis.negative())
    return false;
  axis.update(0.251f, 0.25f, 0.15f);
  if(!axis.positive() || axis.scaled(1.f, 0.15f)!=1.f)
    return false;
  axis.update(0.24f, 0.25f, 0.15f);
  if(axis.positive() || axis.negative() || axis.scaled(1.f, 0.15f)!=0.f)
    return false;
  axis.update(0.26f, 0.25f, 0.15f);
  if(axis.positive() || axis.negative())
    return false;
  axis.update(0.15f, 0.25f, 0.15f);
  axis.update(0.26f, 0.25f, 0.15f);
  return axis.positive() && !axis.negative();
  }

constexpr bool changesSignAndResets() {
  GamepadAxisState axis;
  axis.update(0.50f, 0.25f, 0.15f);
  if(!axis.positive() || axis.negative())
    return false;
  axis.update(-0.50f, 0.25f, 0.15f);
  if(!axis.negative() || axis.positive() || axis.scaled(-1.f, 0.15f)!=-1.f)
    return false;
  axis.update(0.50f, 0.25f, 0.15f);
  if(!axis.positive() || axis.negative() || axis.scaled(1.f, 0.15f)!=1.f)
    return false;
  axis.reset();
  return !axis.negative() && !axis.positive();
  }

constexpr bool ignoresThresholdChatter() {
  GamepadAxisState axis;
  axis.update(0.26f, 0.25f, 0.15f);
  axis.update(0.24f, 0.25f, 0.15f);
  axis.update(0.26f, 0.25f, 0.15f);
  axis.update(0.23f, 0.25f, 0.15f);
  return !axis.positive() && !axis.negative();
  }

constexpr bool guardedActivationKeepsActiveDirection() {
  GamepadAxisState axis;
  axis.update(0.31f, 0.25f, 0.15f);
  if(!axis.positive())
    return false;
  axis.update(0.31f, 0.322f, 0.25f, 0.15f);
  if(!axis.positive())
    return false;
  axis.update(0.25f, 0.322f, 0.25f, 0.15f);
  return !axis.positive() && !axis.negative();
  }

static_assert(releaseAtPressAndRearmAtNeutral());
static_assert(changesSignAndResets());
static_assert(ignoresThresholdChatter());
static_assert(guardedActivationKeepsActiveDirection());
}
