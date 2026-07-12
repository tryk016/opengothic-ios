#include "gamepadinput.h"

#include <Tempest/Platform>

// The whole pad dispatcher exists only on mobile: MainWindow instantiates it
// (and defines the pad* bridges it calls) under __MOBILE_PLATFORM__ only, so
// desktop builds compile this TU empty.
#if defined(__MOBILE_PLATFORM__)

#include <Tempest/Application>
#include <Tempest/Event>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <string>

#include "game/playercontrol.h"
#include "game/inventory.h"
#include "world/world.h"
#include "world/waypoint.h"
#include "world/objects/npc.h"
#include "world/objects/item.h"
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

  const int sl = Gothic::settingsGetI("GAMEPAD","quickSlotL");
  const int sr = Gothic::settingsGetI("GAMEPAD","quickSlotR");
  slotCls[0] = sl>0 ? size_t(sl) : 0;
  slotCls[1] = sr>0 ? size_t(sr) : 0;
  }

void GamepadInput::useQuickSlot(int idx) {
  auto* pl = worldPlayer();
  if(pl==nullptr || idx<0 || idx>1)
    return;
  const size_t cls = slotCls[idx];
  if(cls==0) {
    // unassigned: keep the classic quick-potion behavior (heal / mana)
    const A a = (idx==0) ? A::Heal : A::Potion;
    ctrl.onKeyPressed(a, Event::K_NoKey, M::Primary);
    ctrl.onKeyReleased(a, M::Primary);
    return;
    }
  for(auto it=pl->inventory().iterator(Inventory::T_Inventory); it.isValid(); ++it)
    if((*it).clsId()==cls) {
      pl->useItem(cls, Item::NSLOT, false);
      Haptics::impact(Haptics::Light);
      return;
      }
  Gothic::inst().onPrint("Quick slot: item not in inventory");
  }

bool GamepadInput::assignQuickSlot(int idx) {
  if(idx<0 || idx>1)
    return false;
  const size_t cls = owner.padInventorySelectedCls();
  if(cls==0)
    return false;
  slotCls[idx] = cls;
  Gothic::settingsSetI("GAMEPAD", idx==0 ? "quickSlotL" : "quickSlotR", int(cls));
  Gothic::flushSettings();
  Haptics::impact(Haptics::Medium);
  Gothic::inst().onPrint(idx==0 ? "Assigned to left quick slot"
                                : "Assigned to right quick slot");
  return true;
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
  if(auto w = g.world()) {
    const auto t = w->time();
    char buf[128] = {};
    std::snprintf(buf, sizeof(buf), "Quick - %.*s, day %d %d:%02d",
                  int(w->name().size()), w->name().data(),
                  int(t.day()), int(t.hour()), int(t.minute()));
    nm = buf;
    }
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
  if(ringMagic.isOpen())
    return &ringMagic;
  if(ringItems.isOpen())
    return &ringItems;
  return nullptr;
  }

bool GamepadInput::ringOpen() const {
  return ringMagic.isOpen() || ringItems.isOpen();
  }

void GamepadInput::openMagicRing() { openRing(ringMagic); }
void GamepadInput::openItemRing()  { openRing(ringItems); }

void GamepadInput::ringAim(float nx, float ny) {
  QuickRing* r = ringMagic.isOpen() ? &ringMagic : (ringItems.isOpen() ? &ringItems : nullptr);
  if(r!=nullptr)
    r->updateSelection(nx, ny);
  }

void GamepadInput::ringCommit() {
  QuickRing* r = ringMagic.isOpen() ? &ringMagic : (ringItems.isOpen() ? &ringItems : nullptr);
  if(r==nullptr)
    return;
  if(auto pl = worldPlayer())
    r->commit(*pl);
  else
    r->close();
  Haptics::impact(Haptics::Light);
  }

void GamepadInput::quickSave() { quickSaveRotating(); }

void GamepadInput::openRing(QuickRing& r) {
  if(auto pl = worldPlayer()) {
    releaseAllWorld();               // stop moving/attacking while the ring is up
    r.open(*pl);
    }
  }

void GamepadInput::tickRing(const GamepadState& s) {
  const bool magic = ringMagic.isOpen();
  QuickRing& r     = magic ? ringMagic : ringItems;
  const bool held  = magic ? s.rb : (s.lt > trigThresh);

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
              A::RotateL, A::RotateR,
              A::ActionGeneric, A::Jump, A::Sneak, A::Weapon,
              A::WeaponMele, A::WeaponBow,
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
  if(ringMagic.isOpen() || ringItems.isOpen()) {
    tickRing(s);
    prev = s;
    return;
    }

  const PadCtx ctx = owner.padContext();

  // Leaving gameplay for any UI: release held world actions so the character
  // doesn't keep walking/attacking while a menu or dialogue is open.
  if(ctx!=PadCtx::World && prevCtx==PadCtx::World)
    releaseAllWorld();
  // Entering gameplay: neutralize prev so still-held inputs re-fire as presses
  // — except the D-pad: its buttons are one-shots now (weapon draw, quick
  // slots), and re-firing them would e.g. drink a potion the moment the
  // inventory closes with ◀ still held after a bind.
  if(ctx==PadCtx::World && prevCtx!=PadCtx::World) {
    prev = GamepadState{};
    prev.connected = true;
    prev.dup    = s.dup;
    prev.ddown  = s.ddown;
    prev.dleft  = s.dleft;
    prev.dright = s.dright;
    }

  // Any context switch aborts a pending hold-to-bind.
  if(ctx!=prevCtx) {
    slotHoldMs[0]   = slotHoldMs[1]   = 0;
    slotHoldDone[0] = slotHoldDone[1] = false;
    }

  switch(ctx) {
    case PadCtx::World:     tickWorld(dt, s);  break;
    case PadCtx::Dialog:    tickDialog(s);     break;
    case PadCtx::Inventory: tickInvent(dt, s); break;
    case PadCtx::Menu:      tickMenu(s);       break;
    case PadCtx::Loading:                      break;
    }

  prevCtx = ctx;
  prev    = s;
  }

void GamepadInput::tickWorld(uint64_t dt, const GamepadState& s) {
  // Radial quick-bars first: RB opens the magic ring, LT opens the item ring.
  // Opening one hands input to tickRing on the following frames.
  if(s.rb && !prev.rb) {
    openRing(ringMagic);
    return;
    }
  if(s.lt>trigThresh && !(prev.lt>trigThresh)) {
    openRing(ringItems);
    return;
    }

  // Left stick -> movement (digital, dead-zoned). Forward == stick up (ly>0).
  // Stick X turns the character (Gothic-classic rotate), it does not strafe.
  const bool fwd   = s.ly >  deadZone, back  = s.ly < -deadZone;
  const bool left  = s.lx < -deadZone, right = s.lx >  deadZone;
  edge(fwd,   prev.ly >  deadZone, A::Forward);
  edge(back,  prev.ly < -deadZone, A::Back);
  edge(left,  prev.lx < -deadZone, A::RotateL);
  edge(right, prev.lx >  deadZone, A::RotateR);

  // Right stick -> analog camera look. Y is unified with the touch overlay
  // convention (stick up == look up); invertY flips it (review B6).
  if(std::abs(s.rx) > deadZone || std::abs(s.ry) > deadZone) {
    const float scale = float(dt) * lookSens;
    const float yDir  = invertY ? -1.f : 1.f;
    ctrl.onRotateMouse(-s.rx * scale, s.ry * scale * yDir);
    }

  // While locked, a hard horizontal flick of the right stick steps the locked
  // target (the D-pad hosts quick slots now). Re-arm on crossing the threshold
  // so one flick = one step; the cooldown guards against jitter re-crossings.
  if(ctrl.isTargetLocked() && std::abs(s.rx)>0.75f && std::abs(prev.rx)<=0.75f) {
    const uint64_t now = Tempest::Application::tickCount();
    if(now>=focusFlickCd) {
      if(s.rx<0.f) ctrl.focusLeft(); else ctrl.focusRight();
      focusFlickCd = now + 350;
      Haptics::impact(Haptics::Light);
      }
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

  // D-pad, Gothic-Remake style: ▲ draws the melee weapon, ▼ the bow/crossbow,
  // ◀/▶ fire the two player-assignable quick slots (assigned in the inventory).
  edge(s.dup,   prev.dup,   A::WeaponMele);
  edge(s.ddown, prev.ddown, A::WeaponBow);
  if(s.dleft  && !prev.dleft)  useQuickSlot(0);
  if(s.dright && !prev.dright) useQuickSlot(1);

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

void GamepadInput::tickInvent(uint64_t dt, const GamepadState& s) {
  // Grid navigation like a menu; View (options) also closes.
  // D-pad ◀/▶: hold ~0.6 s to bind the highlighted item to that quick slot;
  // a short press keeps its column-navigation meaning (sent on release).
  auto holdSlot = [&](bool now, int idx, Event::KeyType k){
    if(now) {
      slotHoldMs[idx] += dt;
      if(!slotHoldDone[idx] && slotHoldMs[idx]>=600) {
        // binding works only on the player's own equip page (see
        // selectedItemCls); elsewhere - chest, trade, lockpicking - the hold
        // falls through to a plain (late) navigation tap on release
        if(assignQuickSlot(idx))
          slotHoldDone[idx] = true;
        }
      }
    else {
      if(slotHoldMs[idx]>0 && !slotHoldDone[idx]) {
        key(true,  false, k);          // released early -> normal navigation tap
        key(false, true,  k);
        }
      slotHoldMs[idx]   = 0;
      slotHoldDone[idx] = false;
      }
    };
  holdSlot(s.dleft,  0, Event::K_Left);
  holdSlot(s.dright, 1, Event::K_Right);

  const bool up    = s.ly >  deadZone || s.dup;
  const bool down  = s.ly < -deadZone || s.ddown;
  const bool left  = s.lx < -deadZone;
  const bool right = s.lx >  deadZone;
  key(up,    (prev.ly >  deadZone || prev.dup),   Event::K_Up);
  key(down,  (prev.ly < -deadZone || prev.ddown), Event::K_Down);
  key(left,  (prev.lx < -deadZone), Event::K_Left);
  key(right, (prev.lx >  deadZone), Event::K_Right);
  key(s.a,       prev.a,       Event::K_Return);
  key(s.b,       prev.b,       Event::K_ESCAPE);
  key(s.menu,    prev.menu,    Event::K_ESCAPE);
  key(s.options, prev.options, Event::K_ESCAPE);
  }

#endif
