#include "gamepadinput.h"

#include <Tempest/Event>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <string>

#include "game/playercontrol.h"
#include "world/world.h"
#include "world/waypoint.h"
#include "world/objects/npc.h"
#include "utils/haptics.h"
#include "mainwindow.h"
#include "gothic.h"

using A = KeyCodec::Action;
using M = KeyCodec::Mapping;
using Tempest::Event;

GamepadInput::GamepadInput(MainWindow& owner, PlayerControl& ctrl)
  : owner(owner), ctrl(ctrl) {
  loadConfig();
  }

void GamepadInput::loadConfig() {
  auto f = [](const char* n, float d){
    const float v = Gothic::settingsGetF("GAMEPAD", n);
    return v>0.f ? v : d;
    };
  deadZone   = f("deadZone",         0.25f);
  trigThresh = f("triggerThreshold", 0.50f);
  lookSens   = f("lookSensitivity",  0.20f);
  invertY    = Gothic::settingsGetI("GAMEPAD","invertY")!=0;
  const int slots = Gothic::settingsGetI("GAMEPAD","saveSlots");
  saveSlots  = slots>0 ? slots : 5;
  stuckProtect = (Gothic::settingsGetI("GAMEPAD","noStuckProtect")==0); // opt-out
  }

void GamepadInput::quickSaveRotating() {
  auto& g = Gothic::inst();
  const int slots = std::max(1, saveSlots);
  int idx = Gothic::settingsGetI("GAMEPAD","padQuickSlot");
  idx = (idx % slots) + 1;                         // 1..slots
  Gothic::settingsSetI("GAMEPAD","padQuickSlot", idx);
  Gothic::flushSettings();                          // survive restart
  char slot[32] = {};
  std::snprintf(slot, sizeof(slot), "save_slot_%d.sav", idx);
  std::string nm = "Quick";
  if(auto w = g.world())
    nm = "Quick - " + std::string(w->name());
  g.save(slot, nm);
  Haptics::impact(Haptics::Medium);
  }

void GamepadInput::quickLoadRotating() {
  const int idx = Gothic::settingsGetI("GAMEPAD","padQuickSlot");
  if(idx<=0) {
    Gothic::inst().quickLoad();                     // nothing rotated yet
    return;
    }
  char slot[32] = {};
  std::snprintf(slot, sizeof(slot), "save_slot_%d.sav", idx);
  Gothic::inst().load(slot);
  }

Npc* GamepadInput::worldPlayer() const {
  auto w = Gothic::inst().world();
  return w!=nullptr ? w->player() : nullptr;
  }

const QuickRing* GamepadInput::activeRing() const {
  if(ringWeapons.isOpen())
    return &ringWeapons;
  if(ringItems.isOpen())
    return &ringItems;
  return nullptr;
  }

void GamepadInput::openRing(QuickRing& r) {
  if(auto pl = worldPlayer()) {
    releaseAllWorld();               // stop moving/attacking while the ring is up
    r.open(*pl);
    }
  }

void GamepadInput::tickRing(const GamepadState& s) {
  const bool weapons = ringWeapons.isOpen();
  QuickRing& r       = weapons ? ringWeapons : ringItems;
  const bool held    = weapons ? s.rb : (s.lt > trigThresh);

  r.updateSelection(s.rx, s.ry);
  if(!held) {                        // released -> activate the aimed slice
    if(auto pl = worldPlayer()) {
      r.commit(*pl);
      Haptics::impact(Haptics::Light);
      }
    else
      r.close();
    }
  }

void GamepadInput::stuckTeleport() {
  auto* pl = worldPlayer();
  auto  w  = Gothic::inst().world();
  if(pl==nullptr || w==nullptr)
    return;
  if(auto wp = w->findWayPoint(pl->position()))
    pl->setPosition(wp->position());
  }

void GamepadInput::edge(bool now, bool before, A a) {
  if(now && !before)
    ctrl.onKeyPressed(a, Event::K_NoKey, M::Primary);
  else if(!now && before)
    ctrl.onKeyReleased(a, M::Primary);
  }

void GamepadInput::uiEdge(bool now, bool before, A a) {
  if(now && !before)
    owner.uiAction(a);
  }

void GamepadInput::key(bool now, bool before, Event::KeyType k) {
  if(now && !before) {
    Tempest::KeyEvent ev(k);
    owner.dispatchKey(ev);
    }
  }

void GamepadInput::releaseAllWorld() {
  for(A a : { A::Forward, A::Back, A::Left, A::Right,
              A::ActionGeneric, A::Jump, A::Sneak, A::Weapon,
              A::Parade, A::Walk, A::LookBack, A::Heal, A::Potion })
    ctrl.onKeyReleased(a, M::Primary);   // releasing a non-held action is harmless
  prevRT = false;
  }

void GamepadInput::tick(uint64_t dt) {
  GamepadState s = Gamepad::poll();
  if(!s.connected) {                 // pad vanished mid-hold -> release everything (B5)
    if(prev.connected)
      releaseAllWorld();
    prev    = GamepadState{};
    prevCtx = PadCtx::Loading;
    return;
    }

  // An open radial quick-bar captures all input until released.
  if(ringWeapons.isOpen() || ringItems.isOpen()) {
    tickRing(s);
    prev = s;
    return;
    }

  const PadCtx ctx = owner.padContext();

  // Leaving gameplay for any UI: release held world actions so the character
  // doesn't keep walking/attacking while a menu or dialogue is open.
  if(ctx!=PadCtx::World && prevCtx==PadCtx::World)
    releaseAllWorld();
  // Entering gameplay: neutralize prev so still-held inputs re-fire as presses.
  if(ctx==PadCtx::World && prevCtx!=PadCtx::World) {
    prev = GamepadState{};
    prev.connected = true;
    }

  switch(ctx) {
    case PadCtx::World:     tickWorld(dt, s); break;
    case PadCtx::Dialog:    tickDialog(s);    break;
    case PadCtx::Inventory: tickInvent(s);    break;
    case PadCtx::Menu:      tickMenu(s);      break;
    case PadCtx::Loading:                     break;
    }

  prevCtx = ctx;
  prev    = s;
  }

void GamepadInput::tickWorld(uint64_t dt, const GamepadState& s) {
  // Radial quick-bars first: RB opens the weapon ring, LT opens the item ring.
  // Opening one hands input to tickRing on the following frames.
  if(s.rb && !prev.rb) {
    openRing(ringWeapons);
    return;
    }
  if(s.lt>trigThresh && !(prev.lt>trigThresh)) {
    openRing(ringItems);
    return;
    }

  // Left stick -> movement (digital, dead-zoned). Forward == stick up (ly>0).
  const bool fwd   = s.ly >  deadZone, back  = s.ly < -deadZone;
  const bool left  = s.lx < -deadZone, right = s.lx >  deadZone;
  edge(fwd,   prev.ly >  deadZone, A::Forward);
  edge(back,  prev.ly < -deadZone, A::Back);
  edge(left,  prev.lx < -deadZone, A::Left);
  edge(right, prev.lx >  deadZone, A::Right);

  // Right stick -> analog camera look. Y is unified with the touch overlay
  // convention (stick up == look up); invertY flips it (review B6).
  if(std::abs(s.rx) > deadZone || std::abs(s.ry) > deadZone) {
    const float scale = float(dt) * lookSens;
    const float yDir  = invertY ? -1.f : 1.f;
    ctrl.onRotateMouse(-s.rx * scale, s.ry * scale * yDir);
    }

  // Face buttons: A = interact/attack, B = jump, X = sneak, Y = draw weapon.
  edge(s.a, prev.a, A::ActionGeneric);
  edge(s.b, prev.b, A::Jump);
  edge(s.x, prev.x, A::Sneak);
  edge(s.y, prev.y, A::Weapon);

  // Right trigger -> block/parry (analog, thresholded).
  const bool rtDown = s.rt > trigThresh;
  edge(rtDown, prevRT, A::Parade);
  prevRT = rtDown;

  // R3 = toggle target-lock (native focus); L3 = toggle walk/run.
  if(s.r3 && !prev.r3) {
    ctrl.toggleTargetLock();
    Haptics::impact(Haptics::Light);
    }
  edge(s.l3, prev.l3, A::Walk);

  // Stuck-protection: hold both sticks (L3+R3) ~2 s to warp to the nearest
  // waypoint (opt out with [GAMEPAD] noStuckProtect=1).
  if(stuckProtect && s.l3 && s.r3) {
    stuckHoldMs += dt;
    if(stuckHoldMs>=2000) {
      stuckTeleport();
      Haptics::impact(Haptics::Heavy);
      stuckHoldMs = 0;
      }
    } else {
    stuckHoldMs = 0;
    }

  // D-pad up/down -> quick items (Heal / Potion). Left/right switch the locked
  // target (no-op when not locked).
  edge(s.dup,   prev.dup,   A::Heal);
  edge(s.ddown, prev.ddown, A::Potion);
  if(s.dleft  && !prev.dleft)  ctrl.focusLeft();
  if(s.dright && !prev.dright) ctrl.focusRight();

  // LB modifier: bare View/Menu open inventory/menu (routed to the window, B1);
  // LB + View/Menu = quick load/save, mirroring the F5/F9 keyboard guards (B3).
  if(!s.lb) {
    uiEdge(s.options, prev.options, A::Inventory);
    uiEdge(s.menu,    prev.menu,    A::Escape);
    }
  else {
    const bool useQuickSaveKeys = Gothic::settingsGetI("GAME","useQuickSaveKeys")!=0;
    if(useQuickSaveKeys) {
      auto& g = Gothic::inst();
      if(s.menu    && !prev.menu    && g.isInGameAndAlive() && !g.isPause())
        quickSaveRotating();
      if(s.options && !prev.options && !g.isPause())
        quickLoadRotating();
      }
    }
  }

void GamepadInput::tickDialog(const GamepadState& s) {
  const bool up   = s.ly >  deadZone || s.dup;
  const bool down = s.ly < -deadZone || s.ddown;
  key(up,   (prev.ly >  deadZone || prev.dup),   Event::K_Up);
  key(down, (prev.ly < -deadZone || prev.ddown), Event::K_Down);
  key(s.a,  prev.a, Event::K_Return);
  key(s.b,  prev.b, Event::K_ESCAPE);
  }

void GamepadInput::tickMenu(const GamepadState& s) {
  const bool up    = s.ly >  deadZone || s.dup;
  const bool down  = s.ly < -deadZone || s.ddown;
  const bool left  = s.lx < -deadZone || s.dleft;
  const bool right = s.lx >  deadZone || s.dright;
  key(up,    (prev.ly >  deadZone || prev.dup),    Event::K_Up);
  key(down,  (prev.ly < -deadZone || prev.ddown),  Event::K_Down);
  key(left,  (prev.lx < -deadZone || prev.dleft),  Event::K_Left);
  key(right, (prev.lx >  deadZone || prev.dright), Event::K_Right);
  key(s.a,    prev.a,    Event::K_Return);
  key(s.b,    prev.b,    Event::K_ESCAPE);
  key(s.menu, prev.menu, Event::K_ESCAPE);
  }

void GamepadInput::tickInvent(const GamepadState& s) {
  // Grid navigation like a menu; View (options) also closes.
  tickMenu(s);
  key(s.options, prev.options, Event::K_ESCAPE);
  }
