#include "gamepadinput.h"

#include <Tempest/Platform>

// The whole pad dispatcher exists only on mobile: MainWindow instantiates it
// (and defines the pad* bridges it calls) under __MOBILE_PLATFORM__ only, so
// desktop builds compile this TU empty.
#if defined(__MOBILE_PLATFORM__)

#include <Tempest/Application>
#include <Tempest/Event>
#include <cmath>
#include <algorithm>
#include <initializer_list>
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
#include "camera.h"

using A = KeyCodec::Action;
using M = KeyCodec::Mapping;
using Tempest::Event;

namespace {
// Sloped axial dead-zone: a dominant component raises the activation threshold
// of the perpendicular component. This keeps imperfect cardinal stick motion
// from becoming a full second digital action while preserving true diagonals.
constexpr float slopedAxisThreshold(float deadZone, float crossAxis,
                                    float crossAxisGuard) {
  const float magnitude = crossAxis<0.f ? -crossAxis : crossAxis;
  const float clamped    = magnitude>1.f ? 1.f : magnitude;
  const float threshold  = deadZone + crossAxisGuard*clamped;
  return threshold>1.f ? 1.f : threshold;
  }

static_assert(slopedAxisThreshold(0.25f, 0.960f, 0.12f)>0.269f);
static_assert(slopedAxisThreshold(0.25f,-0.948f, 0.12f)>0.304f);
static_assert(slopedAxisThreshold(0.25f, 0.269f, 0.12f)<0.960f);
}

GamepadInput::GamepadInput(MainWindow& owner, PlayerControl& ctrl)
  : owner(owner), ctrl(ctrl) {
  loadConfig();
  observedInputGen = ctrl.inputGeneration();
  }

void GamepadInput::loadConfig() {
  auto f = [](const char* n, float d){
    const float v = Gothic::settingsGetF("GAMEPAD", n);
    return v>0.f ? v : d;
    };
  deadZone   = std::clamp(f("deadZone", 0.25f), 0.05f, 0.95f);
  releaseZone= std::clamp(f("releaseZone", 0.15f), 0.01f,
                          std::max(0.01f, deadZone-0.01f));
  crossAxisGuard = std::clamp(Gothic::settingsGetF("GAMEPAD","crossAxisGuard"),
                              0.f,0.50f);
  trigThresh = f("triggerThreshold", 0.50f);
  lookSens   = f("lookSensitivity",  0.20f);
  invertY    = Gothic::settingsGetI("GAMEPAD","invertY")!=0;
  stuckProtect = (Gothic::settingsGetI("GAMEPAD","noStuckProtect")==0); // opt-out
  }

void GamepadInput::openMap() {
  auto& g = Gothic::inst();
  auto* w = g.world();
  if(owner.padContext()!=PadCtx::World || w==nullptr || w->player()==nullptr ||
     !g.isInGameAndAlive() || g.isPause() || w->isCutsceneLock())
    return;
  // The engine intentionally opens the scripted map on KeyCodec::Map release.
  ctrl.onKeyPressed (A::Map, Event::K_NoKey, M::Primary);
  ctrl.onKeyReleased(A::Map, M::Primary);
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

bool GamepadInput::ringOpen() const {
  return ringWeapons.isOpen() || ringItems.isOpen();
  }

void GamepadInput::openWeaponsRing() { openRing(ringWeapons); }
void GamepadInput::openItemRing()    { openRing(ringItems); }

void GamepadInput::ringAim(float nx, float ny) {
  QuickRing* r = ringWeapons.isOpen() ? &ringWeapons : (ringItems.isOpen() ? &ringItems : nullptr);
  if(r!=nullptr) {
    r->updateSelection(nx, ny);
    owner.update();
    }
  }

void GamepadInput::pulseWorldAction(A action) {
  ctrl.onKeyPressed(action, Event::K_NoKey, M::Primary);
  ctrl.onKeyReleased(action, M::Primary);
  }

void GamepadInput::activateRingSelection(QuickRing& r) {
  if(auto pl = worldPlayer()) {
    if(auto action = r.commit(*pl))
      pulseWorldAction(*action);
    Haptics::impact(Haptics::Light);
    }
  else
    r.close();
  owner.update();
  }

void GamepadInput::ringCommit() {
  QuickRing* r = ringWeapons.isOpen() ? &ringWeapons : (ringItems.isOpen() ? &ringItems : nullptr);
  if(r==nullptr)
    return;
  activateRingSelection(*r);
  }

void GamepadInput::ringCancel() {
  if(ringWeapons.isOpen())
    ringWeapons.close();
  if(ringItems.isOpen())
    ringItems.close();
  owner.update();
  }

void GamepadInput::openRing(QuickRing& r) {
  auto& g = Gothic::inst();
  auto* w = g.world();
  if(owner.padContext()!=PadCtx::World || w==nullptr || w->isCutsceneLock() ||
     !g.isInGameAndAlive() || g.isPause())
    return;
  if(auto pl = w->player()) {
    releaseAllWorld();               // stop moving/attacking while the ring is up
    ringWeapons.close();
    ringItems.close();
    r.open(*pl);
    owner.update();
    }
  }

void GamepadInput::openItemAssignmentRing() {
  auto& g = Gothic::inst();
  auto* w = g.world();
  if(owner.padContext()!=PadCtx::Inventory || w==nullptr ||
     !g.isInGameAndAlive() || g.checkLoading()!=Gothic::LoadState::Idle)
    return;
  const auto selected = owner.padInventorySelectedItem();
  auto* pl = w->player();
  if(!selected || pl==nullptr)
    return;

  ringWeapons.close();
  ringItems.close();
  if(ringItems.openEdit(*pl,*selected)) {
    Haptics::impact(Haptics::Light);
    owner.update();
    }
  }

void GamepadInput::tickRing(
    const GamepadState& s, const std::vector<GamepadButtonEvent>& events) {
  QuickRing& r = ringWeapons.isOpen() ? ringWeapons : ringItems;
  r.updateSelection(s.rx, s.ry);
  owner.update();

  auto pressed = [&](GamepadButton button) {
    return std::any_of(events.begin(),events.end(),[&](const auto& event) {
      return event.button==button && event.pressed;
      });
    };
  if(pressed(GamepadButton::B) || (s.b && !prev.b)) {
    ringCancel();
    return;
    }
  if(r.isEditing()) {
    const bool assign = s.rt>trigThresh && prev.rt<=trigThresh;
    const bool clear  = s.lt>trigThresh && prev.lt<=trigThresh;
    // Simultaneous trigger crossings are ambiguous: wait for a deliberate
    // single-trigger press instead of assigning and immediately deleting.
    if(assign==clear)
      return;
    if(auto* pl=worldPlayer()) {
      const bool changed = assign ? r.assignSelection(*pl)
                                  : r.clearSelection(*pl);
      if(changed)
        Haptics::impact(Haptics::Light);
      owner.update();
      }
    return;
    }
  if(pressed(GamepadButton::DpadUp) || (s.dup && !prev.dup)) {
    openRing(ringItems);
    return;
    }
  if(pressed(GamepadButton::DpadDown) || (s.ddown && !prev.ddown)) {
    openRing(ringWeapons);
    return;
    }
  if(pressed(GamepadButton::A) || (s.a && !prev.a) ||
     (s.rt>trigThresh && prev.rt<=trigThresh)) {
    activateRingSelection(r);
    return;
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

void GamepadInput::setWorldHeld(A a, bool held) {
  auto& current = worldHeld[size_t(a)];
  if(!held)
    worldPulseRelease[size_t(a)] = false;
  if(current==held)
    return;
  current = held;
  if(held)
    ctrl.onKeyPressed(a, Event::K_NoKey, M::Primary);
  else
    ctrl.onKeyReleased(a, M::Primary);
  }

void GamepadInput::setWorldButton(GamepadButton button, bool physicalHeld,
                                  A action,
                                  const std::vector<GamepadButtonEvent>& events) {
  // A complete press+release can occur between two game ticks. Keep such a
  // tap logically pressed for one simulation tick so Gothic can observe it.
  if(worldPulseRelease[size_t(action)]) {
    worldPulseRelease[size_t(action)] = false;
    setWorldHeld(action, false);
    }

  size_t remaining = 0;
  for(const auto& event : events)
    if(event.button==button)
      ++remaining;

  bool freshPress  = false;
  bool deferRelease= false;
  for(const auto& event : events) {
    if(event.button!=button)
      continue;
    --remaining;
    if(event.pressed) {
      if(!worldHeld[size_t(action)])
        freshPress = true;
      setWorldHeld(action, true);
      }
    else if(worldHeld[size_t(action)]) {
      // Only the final release of a newly pressed tap is deferred. Earlier
      // releases are delivered immediately, preserving release->press when a
      // second tap or hold also arrived in this batch.
      if(remaining==0 && !physicalHeld && freshPress)
        deferRelease = true;
      else {
        setWorldHeld(action, false);
        freshPress = false;
        }
      }
    }

  // A final held state supersedes a deferred release (for example a quick tap
  // followed by a new hold in the same frame). The fresh backend sample is
  // also a fallback if an old controller does not emit a callback edge.
  if(physicalHeld)
    deferRelease = false;
  if(!deferRelease)
    setWorldHeld(action, physicalHeld);
  else
    worldPulseRelease[size_t(action)] = true;
  }

void GamepadInput::setWorldAxis(A negative, bool negativeHeld,
                                A positive, bool positiveHeld) {
  const bool hadNegative = worldHeld[size_t(negative)];
  const bool hadPositive = worldHeld[size_t(positive)];

  // PlayerControl::onKeyReleased clears transient combat actions. On a direct
  // axis reversal all old directions therefore have to be released before the
  // new direction is pressed.
  if(hadNegative && !negativeHeld)
    setWorldHeld(negative, false);
  if(hadPositive && !positiveHeld)
    setWorldHeld(positive, false);
  if(!hadNegative && negativeHeld)
    setWorldHeld(negative, true);
  if(!hadPositive && positiveHeld)
    setWorldHeld(positive, true);
  }

void GamepadInput::key(bool now, bool before, Event::KeyType k) {
  if(now && !before) {
    Tempest::KeyEvent ev(k);
    owner.dispatchKey(ev);
    }
  }

void GamepadInput::keyTap(Event::KeyType k, PadCtx,
                          const GamepadButtonEvent&,
                          const GamepadState&) {
  Tempest::KeyEvent ev(k);
  owner.dispatchKey(ev);
  }

void GamepadInput::tickWorldSystemButtons(
    const GamepadState& s, const std::vector<GamepadButtonEvent>& events) {
  using B = PadSystemGesture::Button;
  using E = PadSystemGesture::Effect;
  const uint64_t now = Tempest::Application::tickCount();

  auto dispatch = [&](E effect) {
    if(effect==E::None)
      return false;

    // Cancel every pending short/long variant before an effect changes the
    // routing context. Buttons still held become suppressed until release.
    systemGesture.reset(s.options,s.menu);
    switch(effect) {
      case E::Inventory:  owner.uiAction(A::Inventory); break;
      case E::GameMenu:   owner.uiAction(A::Escape);    break;
      case E::None:                                     break;
      }
    return true;
    };

  auto feed = [&](B button, bool pressed) {
    return dispatch(systemGesture.onButton(button,pressed,now));
    };

  // Queue order preserves taps entirely contained between two game frames.
  for(const auto& event:events) {
    bool fired = false;
    switch(event.button) {
      case GamepadButton::Options: fired = feed(B::View,event.pressed); break;
      case GamepadButton::Menu:    fired = feed(B::Menu,event.pressed); break;
      default:                                                           break;
      }
    if(fired)
      return;
    }

  // Snapshot reconciliation covers controllers that omit a callback.
  if(systemGesture.down(B::View)!=s.options && feed(B::View,s.options))
    return;
  if(systemGesture.down(B::Menu)!=s.menu && feed(B::Menu,s.menu))
    return;
  dispatch(systemGesture.tick(now));
  }

void GamepadInput::suppressCarriedWorldInput() {
  suppressMoveUntilNeutral = true;
  suppressTurnUntilNeutral = true;
  suppressAUntilRelease    = true;
  suppressBUntilRelease    = true;
  suppressXUntilRelease    = true;
  suppressLbUntilRelease   = true;
  suppressRbUntilRelease   = true;
  suppressLtUntilRelease   = true;
  suppressRtUntilRelease   = true;
  systemGesture.reset(true,true);
  }

void GamepadInput::releaseAllWorld() {
  for(size_t i=0; i<worldHeld.size(); ++i)
    if(worldHeld[i])
      setWorldHeld(A(i), false);
  moveAxis.reset();
  turnAxis.reset();
  worldPulseRelease.fill(false);
  ltSemanticLatched = false;
  ltLatchedSemantic = A::Idle;
  if(gamepadWalkHeld) {
    ctrl.setGamepadWalk(false);
    gamepadWalkHeld = false;
    }
  ctrl.setGamepadTurn(0.f);
  suppressCarriedWorldInput();
  }

void GamepadInput::tick(uint64_t dt) {
  GamepadInputFrame input = Gamepad::consume();
  const GamepadState& s = input.state;
  // Once the bounded queue overflows its remaining prefix may begin with a
  // release whose matching press was discarded. Drop that batch and reconcile
  // held world controls from the fresh final snapshot instead.
  if(input.droppedEvents!=0) {
    input.events.clear();
    systemGesture.reset(s.options,s.menu);
    }

  if(s.generation!=observedControllerGen) {
    // A disconnect/suspend can happen entirely between rendered frames. The
    // generation survives a fast reconnect so a stale held action is still
    // released even when no disconnected snapshot was rendered.
    releaseAllWorld();
    ringCancel();
    observedControllerGen = s.generation;
    }

  const uint64_t inputGen = ctrl.inputGeneration();
  if(inputGen!=observedInputGen) {
    // PlayerControl can clear itself from interactions, cutscenes or loading.
    // It already discarded the input, so do not synthesize releases here.
    worldHeld.fill(false);
    worldPulseRelease.fill(false);
    ltSemanticLatched = false;
    ltLatchedSemantic = A::Idle;
    moveAxis.reset();
    turnAxis.reset();
    gamepadWalkHeld = false;
    ctrl.setGamepadTurn(0.f);
    suppressCarriedWorldInput();
    observedInputGen = inputGen;
    }
  if(!s.connected) {                 // pad vanished mid-hold -> release everything (B5)
    if(prev.connected) {
      releaseAllWorld();
      ringCancel();
      }
    else if(ringOpen() && owner.padContext()!=PadCtx::World) {
      // Touch uses the same rings while no physical pad is connected. Keep a
      // gameplay ring alive, but never let it retain items from an old world
      // or draw above a menu/loading screen.
      ringCancel();
      }
    prev    = GamepadState{};
    prevCtx = PadCtx::Loading;
    return;
    }

  // An open radial panel captures all input until confirm or cancel. The
  // normal panels own World; the assignment editor deliberately owns the
  // still-open Inventory behind it.
  if(ringWeapons.isOpen() || ringItems.isOpen()) {
    QuickRing& ring = ringWeapons.isOpen() ? ringWeapons : ringItems;
    const PadCtx ringCtx = owner.padContext();
    const bool ownsContext = ring.isEditing()
                           ? ringCtx==PadCtx::Inventory
                           : ringCtx==PadCtx::World;
    if(ownsContext) {
      tickRing(s,input.events);
      prev = s;
      return;
      }
    ringCancel();
    }

  const PadCtx ctx = owner.padContext();

  // Leaving gameplay releases only stateful actions that this pad owns.
  if(ctx!=PadCtx::World && prevCtx==PadCtx::World)
    releaseAllWorld();
  // Entering gameplay synchronizes one-shot edges with the physical state and
  // requires continuous controls to return to neutral before they can re-arm.
  if(ctx==PadCtx::World && prevCtx!=PadCtx::World) {
    const bool carriedView = prev.connected && prev.options;
    const bool carriedMenu = prev.connected && prev.menu;
    moveAxis.reset();
    turnAxis.reset();
    ctrl.setGamepadTurn(0.f);
    if(prev.connected) {
      // Gate only controls which were already held in the previous context.
      // A new press/deflection that arrived between UI and this World tick is
      // fresh input and must not be discarded as if it had leaked from UI.
      suppressMoveUntilNeutral = std::abs(prev.ly)>deadZone;
      suppressTurnUntilNeutral = std::abs(prev.lx)>deadZone;
      suppressAUntilRelease    = prev.a;
      suppressBUntilRelease    = prev.b;
      suppressXUntilRelease    = prev.x;
      suppressLbUntilRelease   = prev.lb;
      suppressRbUntilRelease   = prev.rb;
      suppressLtUntilRelease   = prev.lt>trigThresh;
      suppressRtUntilRelease   = prev.rt>trigThresh;
      }
    else {
      suppressCarriedWorldInput();
      }
    systemGesture.reset(carriedView,carriedMenu);
    prev = s;
    }

  switch(ctx) {
    case PadCtx::World:     tickWorld(dt, s, input.events);  break;
    case PadCtx::Dialog:    tickDialog(s, input.events);     break;
    case PadCtx::Inventory: tickInvent(dt, s, input.events); break;
    case PadCtx::Menu:      tickMenu(s, input.events);       break;
    case PadCtx::Loading:                      break;
    }

  prevCtx = ctx;
  prev    = s;
  }

void GamepadInput::tickWorld(uint64_t dt, const GamepadState& s,
                             const std::vector<GamepadButtonEvent>& events) {
  auto pressed = [&](GamepadButton button) {
    return std::any_of(events.begin(),events.end(),[&](const auto& event) {
      return event.button==button && event.pressed;
      });
    };
  auto pressedOrEdge = [&](GamepadButton button, bool now, bool before) {
    return pressed(button) || (now && !before);
    };

  // Two independent modal quick-rings. Once opened, tickRing captures every
  // input until A/RT confirms or B cancels.
  if(pressedOrEdge(GamepadButton::DpadUp,s.dup,prev.dup)) {
    openRing(ringItems);
    return;
    }
  if(pressedOrEdge(GamepadButton::DpadDown,s.ddown,prev.ddown)) {
    openRing(ringWeapons);
    return;
    }

  // Each carried control rearms independently. A slightly noisy stick must
  // never block A/B/RT, and one axis must not disable the other.
  if(suppressMoveUntilNeutral && std::abs(s.ly)<=releaseZone)
    suppressMoveUntilNeutral = false;
  if(suppressTurnUntilNeutral && std::abs(s.lx)<=releaseZone)
    suppressTurnUntilNeutral = false;
  const bool aReleased = std::any_of(events.begin(),events.end(),[](const auto& event) {
    return event.button==GamepadButton::A && !event.pressed;
    });
  const bool bReleased = std::any_of(events.begin(),events.end(),[](const auto& event) {
    return event.button==GamepadButton::B && !event.pressed;
    });
  const bool xReleased = std::any_of(events.begin(),events.end(),[](const auto& event) {
    return event.button==GamepadButton::X && !event.pressed;
    });
  const bool lbReleased = std::any_of(events.begin(),events.end(),[](const auto& event) {
    return event.button==GamepadButton::LB && !event.pressed;
    });
  const bool rbReleased = std::any_of(events.begin(),events.end(),[](const auto& event) {
    return event.button==GamepadButton::RB && !event.pressed;
    });
  const bool ltReleased = std::any_of(events.begin(),events.end(),[](const auto& event) {
    return event.button==GamepadButton::LT && !event.pressed;
    });
  const bool rtReleased = std::any_of(events.begin(),events.end(),[](const auto& event) {
    return event.button==GamepadButton::RT && !event.pressed;
    });
  if(suppressAUntilRelease && (!s.a || aReleased))
    suppressAUntilRelease = false;
  if(suppressBUntilRelease && (!s.b || bReleased))
    suppressBUntilRelease = false;
  if(suppressXUntilRelease && (!s.x || xReleased))
    suppressXUntilRelease = false;
  if(suppressLbUntilRelease && (!s.lb || lbReleased))
    suppressLbUntilRelease = false;
  if(suppressRbUntilRelease && (!s.rb || rbReleased))
    suppressRbUntilRelease = false;
  if(suppressLtUntilRelease && (s.lt<=trigThresh || ltReleased))
    suppressLtUntilRelease = false;
  if(suppressRtUntilRelease && (s.rt<=trigThresh || rtReleased))
    suppressRtUntilRelease = false;

  const float moveThreshold = slopedAxisThreshold(deadZone, s.lx,
                                                  crossAxisGuard);
  const float turnThreshold = slopedAxisThreshold(deadZone, s.ly,
                                                  crossAxisGuard);

  if(!suppressMoveUntilNeutral) {
    // Y keeps Gothic's animation-driven start/stop movement. The guarded
    // threshold starts a direction; the fixed deadZone releases it, while
    // releaseZone only rearms after threshold chatter.
    moveAxis.update(s.ly, moveThreshold, deadZone, releaseZone);
    setWorldAxis(A::Back,    moveAxis.negative(),
                 A::Forward, moveAxis.positive());
    }

  if(!suppressTurnUntilNeutral) {
    // X is genuinely analog: guard only activation, then remove the fixed
    // inner dead-zone and scale the classic turn rate by the remaining -1..1.
    turnAxis.update(s.lx, turnThreshold, deadZone, releaseZone);
    const float turn = turnAxis.scaled(s.lx, deadZone);
    ctrl.setGamepadTurn(turn);
    // Keep RotateL/RotateR edge semantics for lockpicking, classic combat and
    // rotate+jump side-steps. PlayerControl prefers gamepadTurn for speed.
    setWorldAxis(A::RotateL, turnAxis.negative(),
                 A::RotateR, turnAxis.positive());
    }
  else {
    ctrl.setGamepadTurn(0.f);
    }

  auto* pl = worldPlayer();
  const WeaponState ws = pl!=nullptr ? pl->weaponState() : WeaponState::NoWeapon;
  const bool melee  = ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H;
  const bool ranged = ws==WeaponState::Bow || ws==WeaponState::CBow;
  const bool armed  = ws!=WeaponState::NoWeapon;

  const A ltSemantic = !armed ? A::WeaponBow :
                       melee  ? A::Parade    :
                       ranged ? A::PadAim    : A::Idle;
  const A rtSemantic = !armed ? A::WeaponMele : A::PadAttack;
  const A lbSemantic = melee ? A::PadAttackLeft : A::Walk; // Walk is a latch sentinel
  const A rbSemantic = melee ? A::PadAttackRight : A::LookBack;
  auto semanticChanged = [&](A desired, std::initializer_list<A> choices) {
    for(A action:choices)
      if(action!=desired && worldHeld[size_t(action)])
        return true;
    return false;
    };
  auto releaseSemantic = [&](std::initializer_list<A> choices) {
    for(A action:choices)
      setWorldHeld(action,false);
    };

  // A held contextual button keeps the meaning it had at press time. If a
  // draw/sheathe animation changes WeaponState underneath it, release the old
  // meaning and require a real button release before the new one can arm.
  const bool ltHeld = s.lt>trigThresh;
  if(!ltHeld) {
    ltSemanticLatched = false;
    ltLatchedSemantic = A::Idle;
    }
  else if(!suppressLtUntilRelease && !ltSemanticLatched) {
    ltSemanticLatched = true;
    ltLatchedSemantic = ltSemantic;
    }
  if(!suppressLtUntilRelease && ltHeld && ltSemanticLatched &&
     ltLatchedSemantic!=ltSemantic) {
    releaseSemantic({A::WeaponBow,A::Parade,A::PadAim});
    suppressLtUntilRelease = true;
    }
  if(!suppressRtUntilRelease && s.rt>trigThresh &&
     semanticChanged(rtSemantic,{A::WeaponMele,A::PadAttack})) {
    releaseSemantic({A::WeaponMele,A::PadAttack});
    suppressRtUntilRelease = true;
    }
  const bool lbChanged = gamepadWalkHeld ? lbSemantic!=A::Walk
                                        : semanticChanged(lbSemantic,{A::PadAttackLeft});
  if(!suppressLbUntilRelease && s.lb && lbChanged) {
    releaseSemantic({A::PadAttackLeft});
    if(gamepadWalkHeld) {
      ctrl.setGamepadWalk(false);
      gamepadWalkHeld = false;
      }
    suppressLbUntilRelease = true;
    }
  if(!suppressRbUntilRelease && s.rb &&
     semanticChanged(rbSemantic,{A::LookBack,A::PadAttackRight})) {
    releaseSemantic({A::LookBack,A::PadAttackRight});
    suppressRbUntilRelease = true;
    }

  // zGamePad-style context mapping. Internal Pad* actions deliberately bypass
  // Gothic 1/2 keyboard preset differences in PlayerControl.
  if(!suppressAUntilRelease && !armed)
    setWorldButton(GamepadButton::A,s.a,A::ActionGeneric,events);
  else
    setWorldHeld(A::ActionGeneric,false);

  if(!suppressBUntilRelease && melee)
    setWorldButton(GamepadButton::B,s.b,A::PadSpecial,events);
  else
    setWorldHeld(A::PadSpecial,false);

  if(!suppressXUntilRelease)
    setWorldButton(GamepadButton::X,s.x,A::Jump,events);
  else
    setWorldHeld(A::Jump,false);
  if(pressedOrEdge(GamepadButton::Y,s.y,prev.y))
    pulseWorldAction(A::Weapon);

  if(!suppressLtUntilRelease) {
    if(!armed) {
      setWorldHeld(A::Parade,false);
      setWorldHeld(A::PadAim,false);
      setWorldButton(GamepadButton::LT,s.lt>trigThresh,A::WeaponBow,events);
      }
    else if(melee) {
      setWorldHeld(A::WeaponBow,false);
      setWorldHeld(A::PadAim,false);
      setWorldButton(GamepadButton::LT,s.lt>trigThresh,A::Parade,events);
      }
    else if(ranged) {
      setWorldHeld(A::WeaponBow,false);
      setWorldHeld(A::Parade,false);
      setWorldButton(GamepadButton::LT,s.lt>trigThresh,A::PadAim,events);
      }
    else {
      setWorldHeld(A::WeaponBow,false);
      setWorldHeld(A::Parade,false);
      setWorldHeld(A::PadAim,false);
      }
    }

  if(!suppressRtUntilRelease) {
    if(!armed) {
      setWorldHeld(A::PadAttack,false);
      setWorldButton(GamepadButton::RT,s.rt>trigThresh,A::WeaponMele,events);
      }
    else {
      setWorldHeld(A::WeaponMele,false);
      setWorldButton(GamepadButton::RT,s.rt>trigThresh,A::PadAttack,events);
      }
    }

  if(!suppressLbUntilRelease && melee) {
    if(gamepadWalkHeld) {
      ctrl.setGamepadWalk(false);
      gamepadWalkHeld = false;
      }
    setWorldButton(GamepadButton::LB,s.lb,A::PadAttackLeft,events);
    }
  else if(!suppressLbUntilRelease) {
    setWorldHeld(A::PadAttackLeft,false);
    const bool walk = s.lb;
    if(gamepadWalkHeld!=walk) {
      ctrl.setGamepadWalk(walk);
      gamepadWalkHeld = walk;
      }
    }
  else {
    setWorldHeld(A::PadAttackLeft,false);
    if(gamepadWalkHeld) {
      ctrl.setGamepadWalk(false);
      gamepadWalkHeld = false;
      }
    }

  if(!suppressRbUntilRelease && melee) {
    setWorldHeld(A::LookBack,false);
    setWorldButton(GamepadButton::RB,s.rb,A::PadAttackRight,events);
    }
  else if(!suppressRbUntilRelease) {
    setWorldHeld(A::PadAttackRight,false);
    setWorldButton(GamepadButton::RB,s.rb,A::LookBack,events);
    }
  else {
    setWorldHeld(A::PadAttackRight,false);
    setWorldHeld(A::LookBack,false);
    }

  // Right stick -> analog camera look. PlayerControl consumes yaw, but normal
  // gameplay deliberately ignores Npc::setDirectionY; feed Camera as well so
  // pitch works outside swimming/climbing. Apply the dead-zone per axis to
  // avoid turning an X-only look into vertical drift.
  const float rx = std::abs(s.rx)>deadZone ? s.rx : 0.f;
  const float ry = std::abs(s.ry)>deadZone ? s.ry : 0.f;
  if(rx!=0.f || ry!=0.f) {
    const float scale = float(std::min<uint64_t>(dt,50)) * lookSens;
    const float yDir  = invertY ? -1.f : 1.f;
    const float yaw   = -rx * scale;
    const float pitch =  ry * scale * yDir;

    // Match MainWindow::tickMouse: camera receives {-pitch,yaw}, while the
    // player receives {yaw,pitch}. gamepad.tick runs before the global dt
    // clamp, hence the local 50 ms cap above.
    if(auto* camera = Gothic::inst().camera();
       camera!=nullptr && !camera->isCutscene() && !Gothic::inst().isPause()) {
      camera->onRotateMouse(Tempest::PointF(-pitch,yaw));
      ctrl.onRotateMouse(yaw,pitch);
      }
    }

  // L3 toggles sneak; R3 toggles target lock.
  if(pressedOrEdge(GamepadButton::L3,s.l3,prev.l3))
    pulseWorldAction(A::Sneak);
  if(pressedOrEdge(GamepadButton::R3,s.r3,prev.r3)) {
    ctrl.toggleTargetLock();
    Haptics::impact(Haptics::Light);
    }

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

  // D-pad left/right changes target focus while locked. Otherwise Left opens
  // the quest journal and Right opens the scripted map.
  if(pressedOrEdge(GamepadButton::DpadLeft,s.dleft,prev.dleft)) {
    if(ctrl.isTargetLocked()) {
      ctrl.focusLeft();
      Haptics::impact(Haptics::Light);
      }
    else {
      owner.uiAction(A::Log);
      return;
      }
    }
  if(pressedOrEdge(GamepadButton::DpadRight,s.dright,prev.dright)) {
    if(ctrl.isTargetLocked()) {
      ctrl.focusRight();
      Haptics::impact(Haptics::Light);
      }
    else {
      openMap();
      return;
      }
    }

  // View = inventory; Menu = the game menu.
  tickWorldSystemButtons(s,events);
  }

void GamepadInput::tickDialog(const GamepadState& s,
                              const std::vector<GamepadButtonEvent>& events) {
  key(s.ly >  deadZone, prev.ly >  deadZone, Event::K_Up);
  key(s.ly < -deadZone, prev.ly < -deadZone, Event::K_Down);
  for(const auto& event : events) {
    if(!event.pressed)
      continue;
    switch(event.button) {
      case GamepadButton::DpadUp:   keyTap(Event::K_Up,     PadCtx::Dialog, event, s); break;
      case GamepadButton::DpadDown: keyTap(Event::K_Down,   PadCtx::Dialog, event, s); break;
      case GamepadButton::A:        keyTap(Event::K_Return, PadCtx::Dialog, event, s); break;
      case GamepadButton::B:        keyTap(Event::K_ESCAPE, PadCtx::Dialog, event, s); break;
      default: break;
      }
    }
  }

void GamepadInput::tickMenu(const GamepadState& s,
                            const std::vector<GamepadButtonEvent>& events) {
  // Character pages deliberately use the D-pad only. This avoids accidental
  // category/quest changes from a stick that was still settling after play.
  if(!owner.padCharacterNavigationActive()) {
    key(s.ly >  deadZone, prev.ly >  deadZone, Event::K_Up);
    key(s.ly < -deadZone, prev.ly < -deadZone, Event::K_Down);
    key(s.lx < -deadZone, prev.lx < -deadZone, Event::K_Left);
    key(s.lx >  deadZone, prev.lx >  deadZone, Event::K_Right);
    }
  for(const auto& event : events) {
    if(!event.pressed)
      continue;
    if(owner.padCharacterPageActive()) {
      if(event.button==GamepadButton::LB) {
        owner.padCycleCharacterPage(-1);
        return;
        }
      if(event.button==GamepadButton::RB) {
        owner.padCycleCharacterPage(1);
        return;
        }
      }
    switch(event.button) {
      case GamepadButton::DpadUp:    keyTap(Event::K_Up,     PadCtx::Menu, event, s); break;
      case GamepadButton::DpadDown:  keyTap(Event::K_Down,   PadCtx::Menu, event, s); break;
      case GamepadButton::DpadLeft:  keyTap(Event::K_Left,   PadCtx::Menu, event, s); break;
      case GamepadButton::DpadRight: keyTap(Event::K_Right,  PadCtx::Menu, event, s); break;
      case GamepadButton::A:         keyTap(Event::K_Return, PadCtx::Menu, event, s); break;
      case GamepadButton::B:
      case GamepadButton::Menu:      keyTap(Event::K_ESCAPE, PadCtx::Menu, event, s); break;
      default: break;
      }
    }
  }

void GamepadInput::tickInvent(uint64_t dt, const GamepadState& s,
                              const std::vector<GamepadButtonEvent>& events) {
  (void)dt;
  const bool openAssignment = (s.r3 && !prev.r3) ||
    std::any_of(events.begin(),events.end(),[](const auto& event) {
      return event.button==GamepadButton::R3 && event.pressed;
      });
  if(openAssignment) {
    openItemAssignmentRing();
    return;
    }

  // Grid navigation like a menu; LB/RB jump between sorted item categories.
  for(const auto& event : events) {
    if(!event.pressed)
      continue;
    if(event.button==GamepadButton::LB)
      owner.padInventoryCategory(-1);
    else if(event.button==GamepadButton::RB)
      owner.padInventoryCategory(1);
    }

  const bool up    = s.ly >  deadZone;
  const bool down  = s.ly < -deadZone;
  const bool left  = s.lx < -deadZone;
  const bool right = s.lx >  deadZone;
  key(up,    prev.ly >  deadZone,  Event::K_Up);
  key(down,  prev.ly < -deadZone,  Event::K_Down);
  key(left,  (prev.lx < -deadZone), Event::K_Left);
  key(right, (prev.lx >  deadZone), Event::K_Right);
  for(const auto& event : events) {
    if(!event.pressed)
      continue;
    switch(event.button) {
      case GamepadButton::DpadUp:   keyTap(Event::K_Up,     PadCtx::Inventory, event, s); break;
      case GamepadButton::DpadDown: keyTap(Event::K_Down,   PadCtx::Inventory, event, s); break;
      case GamepadButton::DpadLeft: keyTap(Event::K_Left,   PadCtx::Inventory, event, s); break;
      case GamepadButton::DpadRight:keyTap(Event::K_Right,  PadCtx::Inventory, event, s); break;
      case GamepadButton::A:       keyTap(Event::K_Return, PadCtx::Inventory, event, s); break;
      case GamepadButton::B:
      case GamepadButton::Menu:
      case GamepadButton::Options: keyTap(Event::K_ESCAPE, PadCtx::Inventory, event, s); break;
      default: break;
      }
    }
  }

#endif
