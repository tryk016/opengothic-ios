#pragma once

#include <cstdint>

enum class ScriptLang : int32_t;

// Text owned by the native iOS overlays. These strings deliberately do not
// come from MENU.DAT: stock and modded menu instances must remain untouched.
namespace IosUiLocalization {

struct DeviceSettingsText {
  const char* title;
  const char* frameRate;
  const char* off;
  const char* controlled;
  const char* back;
  };

struct PadDiagramText {
  const char* title;
  const char* ltAction;
  const char* lbAction;
  const char* move;
  const char* sneak;
  const char* itemRing;
  const char* statusPrev;
  const char* questNext;
  const char* weaponMagicRing;
  const char* rtAction;
  const char* rbAction;
  const char* weapon;
  const char* special;
  const char* jump;
  const char* action;
  const char* camera;
  const char* targetLock;
  const char* inventory;
  const char* gameMenu;
  };

// Read GAME/language once at each paint/opening boundary. NONE and unknown
// values intentionally fall back to English.
ScriptLang currentLanguage();

const DeviceSettingsText& deviceSettings(ScriptLang language);
const PadDiagramText&      padDiagram(ScriptLang language);

}
