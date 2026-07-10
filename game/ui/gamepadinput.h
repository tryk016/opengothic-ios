#pragma once

#include <cstdint>

#include <Tempest/Event>

#include "utils/gamepad.h"
#include "utils/keycodec.h"
#include "ui/quickring.h"

class PlayerControl;
class MainWindow;
class Npc;

// Active pad-routing context, chosen once per tick. The dispatcher sends the
// pad either to gameplay (PlayerControl) or, for any UI, to synthetic key
// events on the active widget — mirroring how the keyboard drives menus.
enum class PadCtx : uint8_t {
  World,      // normal gameplay
  Dialog,     // NPC dialogue choice
  Menu,       // main / pause / save menu, video, chapter, document
  Inventory,  // inventory / container
  Loading,    // world loading — ignore input
  };

// Maps a GamepadState snapshot to the game each tick, following the Gothic
// Classic (THQ Nordic) console scheme. Owns edge-detection and dead-zones, and
// routes per-context so the pad works in menus and dialogues, not just world.
class GamepadInput {
  public:
    GamepadInput(MainWindow& owner, PlayerControl& ctrl);

    void tick(uint64_t dt);

    // The radial quick-bar currently open (for MainWindow to draw), or nullptr.
    const QuickRing* activeRing() const;

  private:
    MainWindow&    owner;
    PlayerControl& ctrl;
    GamepadState   prev;
    PadCtx         prevCtx = PadCtx::Loading;
    bool           prevRT  = false;

    QuickRing      ringWeapons{QuickRing::Weapons};
    QuickRing      ringItems  {QuickRing::Items};

    void tickWorld (uint64_t dt, const GamepadState& s);
    void tickDialog(const GamepadState& s);
    void tickMenu  (const GamepadState& s);
    void tickInvent(const GamepadState& s);

    void edge  (bool now, bool before, KeyCodec::Action a);       // -> PlayerControl
    void uiEdge(bool now, bool before, KeyCodec::Action a);       // -> MainWindow::uiAction
    void key   (bool now, bool before, Tempest::Event::KeyType k);// synthetic KeyEvent
    void releaseAllWorld();                                       // drop held world actions

    void loadConfig();                     // read the [GAMEPAD] section
    void quickSaveRotating();              // rotating save slots (spec 6)
    void quickLoadRotating();              // load the last rotating slot

    void  tickRing(const GamepadState& s); // drive an open radial quick-bar
    void  openRing(QuickRing& r);          // fill from inventory + open
    Npc*  worldPlayer() const;             // current player npc, or nullptr
    void  stuckTeleport();                 // warp to the nearest waypoint (spec 8)

    // Tunables, overridable via Gothic.ini [GAMEPAD] (see loadConfig).
    float deadZone   = 0.25f;   // stick dead-zone
    float trigThresh = 0.50f;   // trigger press threshold
    float lookSens   = 0.20f;   // camera speed per ms
    bool  invertY    = false;   // camera Y invert (review B6)
    int   saveSlots  = 5;       // rotating quick-save slot count
    bool  stuckProtect = true;  // L3+R3 hold -> warp to nearest waypoint

    uint64_t stuckHoldMs = 0;   // how long both sticks have been held
  };
