#include "padglyph.h"

#include <Tempest/Platform>

#if defined(__IOS__)

#import  <Foundation/Foundation.h>
#include <Tempest/Texture2d>
#include <Tempest/Pixmap>
#include <array>

#include "resources.h"

using namespace Tempest;

static const char* glyphFile(PadGlyph::Btn b) {
  switch(b) {
    case PadGlyph::A:         return "XboxSeriesX_A";
    case PadGlyph::B:         return "XboxSeriesX_B";
    case PadGlyph::X:         return "XboxSeriesX_X";
    case PadGlyph::Y:         return "XboxSeriesX_Y";
    case PadGlyph::LB:        return "XboxSeriesX_LB";
    case PadGlyph::RB:        return "XboxSeriesX_RB";
    case PadGlyph::LT:        return "XboxSeriesX_LT";
    case PadGlyph::RT:        return "XboxSeriesX_RT";
    case PadGlyph::L3:        return "XboxSeriesX_Left_Stick_Click";
    case PadGlyph::R3:        return "XboxSeriesX_Right_Stick_Click";
    case PadGlyph::LStick:    return "XboxSeriesX_Left_Stick";
    case PadGlyph::RStick:    return "XboxSeriesX_Right_Stick";
    case PadGlyph::DPadUp:    return "Arrow_Up_Key_Dark";
    case PadGlyph::DPadDown:  return "Arrow_Down_Key_Dark";
    case PadGlyph::DPadLeft:  return "Arrow_Left_Key_Dark";
    case PadGlyph::DPadRight: return "Arrow_Right_Key_Dark";
    case PadGlyph::Menu:      return "XboxSeriesX_Menu";
    case PadGlyph::View:      return "XboxSeriesX_View";
    }
  return nullptr;
  }

static const char* dpadFile(PadGlyph::Btn b) {
  switch(b) {
    case PadGlyph::DPadUp:    return "XboxSeriesX_Dpad_Up";
    case PadGlyph::DPadDown:  return "XboxSeriesX_Dpad_Down";
    case PadGlyph::DPadLeft:  return "XboxSeriesX_Dpad_Left";
    case PadGlyph::DPadRight: return "XboxSeriesX_Dpad_Right";
    default:                  return nullptr;
    }
  }

// Load one bundled PNG into `tex`. `state`: 0=unloaded, 1=loaded, 2=missing.
static void loadBundled(const char* nm, Texture2d& tex, uint8_t& state) {
  if(state!=0)
    return;
  state = 2;
  if(nm==nullptr)
    return;
  @autoreleasepool {
    NSString* n   = [NSString stringWithUTF8String:nm];
    NSString* pth = [[NSBundle mainBundle] pathForResource:n ofType:@"png"];
    if(pth!=nil) {
      try {
        Pixmap pm(pth.UTF8String);
        if(!pm.isEmpty()) {
          tex   = Resources::loadTexturePm(pm);
          state = 1;
          }
        }
      catch(...) {
        state = 2;
        }
      }
    }
  }

const Tempest::Texture2d* PadGlyph::texture(Btn b) {
  static std::array<Texture2d,18> cache;
  static std::array<uint8_t,18>   state{};
  const size_t i = size_t(b);
  if(i>=cache.size())
    return nullptr;
  loadBundled(glyphFile(b), cache[i], state[i]);
  return state[i]==1 ? &cache[i] : nullptr;
  }

const Tempest::Texture2d* PadGlyph::dpadTexture(Btn b) {
  static std::array<Texture2d,18> cache;
  static std::array<uint8_t,18>   state{};
  const size_t i = size_t(b);
  if(i>=cache.size())
    return nullptr;
  loadBundled(dpadFile(b), cache[i], state[i]);
  return state[i]==1 ? &cache[i] : nullptr;
  }

const Tempest::Texture2d* PadGlyph::diagram() {
  static Texture2d cache;
  static uint8_t   state = 0;
  loadBundled("XboxSeriesX_Diagram", cache, state);
  return state==1 ? &cache : nullptr;
  }

#endif
