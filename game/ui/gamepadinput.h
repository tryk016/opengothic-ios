#pragma once

#include <array>
#include <cstdint>

#include <Tempest/Event>

#include "utils/gamepad.h"
#include "utils/keycodec.h"
#include "ui/gamepadaxisstate.h"
#include "ui/padsystemgesture.h"
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

// Maps a GamepadState snapshot to the game each tick, following a contextual
// zGamePad-inspired scheme. Owns edge-detection and dead-zones, and routes
// per-context so the pad works in menus and dialogues, not just the world.
class GamepadInput {
  public:
    GamepadInput(MainWindow& owner, PlayerControl& ctrl);

    void tick(uint64_t dt);

    // The radial quick-bar currently open (for MainWindow to draw), or nullptr.
    const QuickRing* activeRing() const;

    // Touch-overlay hooks (used when no pad is connected): open/aim/commit the
    // two independent modal rings.
    bool  ringOpen() const;
    void  openWeaponsRing();
    void  openItemRing();
    void  ringAim(float nx, float ny);
    void  ringCommit();
    void  ringCancel();
    void  openMap();

  private:
    MainWindow&    owner;
    PlayerControl& ctrl;
    GamepadState   prev;
    PadCtx         prevCtx = PadCtx::Loading;
    std::array<bool,KeyCodec::Last> worldHeld{};
    GamepadAxisState moveAxis;
    GamepadAxisState turnAxis;
    uint64_t         observedInputGen = 0;
    uint64_t         observedControllerGen = 0;
    bool             suppressMoveUntilNeutral = true;
    bool             suppressTurnUntilNeutral = true;
    bool             suppressAUntilRelease = true;
    bool             suppressBUntilRelease = true;
    bool             suppressXUntilRelease = true;
    bool             suppressLbUntilRelease = true;
    bool             suppressRbUntilRelease = true;
    bool             suppressLtUntilRelease = true;
    bool             suppressRtUntilRelease = true;
    bool             gamepadWalkHeld = false;
    bool             ltSemanticLatched = false;
    KeyCodec::Action ltLatchedSemantic = KeyCodec::Idle;
    PadSystemGesture systemGesture;

    QuickRing      ringWeapons{QuickRing::Weapons};
    QuickRing      ringItems{QuickRing::Items};

    void tickWorld (uint64_t dt, const GamepadState& s,
                    const std::vector<GamepadButtonEvent>& events);
    void tickDialog(const GamepadState& s,
                    const std::vector<GamepadButtonEvent>& events);
    void tickMenu  (const GamepadState& s,
                    const std::vector<GamepadButtonEvent>& events);
    void tickInvent(uint64_t dt, const GamepadState& s,
                    const std::vector<GamepadButtonEvent>& events);

    void setWorldHeld(KeyCodec::Action a, bool held);             // stateful world action
    void setWorldButton(GamepadButton button, bool physicalHeld,
                        KeyCodec::Action action,
                        const std::vector<GamepadButtonEvent>& events);
    void setWorldAxis(KeyCodec::Action negative, bool negativeHeld,
                      KeyCodec::Action positive, bool positiveHeld);
    void key   (bool now, bool before, Tempest::Event::KeyType k);// synthetic KeyEvent
    void keyTap(Tempest::Event::KeyType k, PadCtx ctx,
                const GamepadButtonEvent& source,
                const GamepadState& state);
    void tickWorldSystemButtons(const GamepadState& s,
                                const std::vector<GamepadButtonEvent>& events);
    void suppressCarriedWorldInput();                            // per-control neutral gates
    void releaseAllWorld();                                       // drop held world actions

    void loadConfig();                     // read the [GAMEPAD] section

    void  tickRing(const GamepadState& s,
                   const std::vector<GamepadButtonEvent>& events);
    void  openRing(QuickRing& r);          // fill from inventory + open
    void  openItemAssignmentRing();        // selected inventory item -> editor
    void  activateRingSelection(QuickRing& r);
    void  pulseWorldAction(KeyCodec::Action action);
    Npc*  worldPlayer() const;             // current player npc, or nullptr
    void  stuckTeleport();                 // warp to the nearest waypoint (spec 8)

    // Tunables, overridable via Gothic.ini [GAMEPAD] (see loadConfig).
    float deadZone      = 0.25f; // stick press dead-zone
    float releaseZone   = 0.15f; // inner neutral threshold for re-arming an axis
    float crossAxisGuard= 0.12f; // suppress perpendicular stick-axis drift
    float trigThresh    = 0.50f; // trigger press threshold
    float lookSens   = 0.20f;   // camera speed per ms
    bool  invertY    = false;   // camera Y invert (review B6)
    bool  stuckProtect = true;  // L3+R3 hold -> warp to nearest waypoint
    bool  debugInput = false;   // transition-only stderr diagnostics

    float debugLx = 0.f;
    float debugLy = 0.f;
    std::array<bool,KeyCodec::Last> worldPulseRelease{};

    uint64_t stuckHoldMs = 0;   // how long both sticks have been held

  };
