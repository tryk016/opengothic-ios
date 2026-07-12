#include "gamepad.h"

#include <Tempest/Platform>

#include <cstddef>
#include <deque>
#include <mutex>

#if defined(__IOS__)

#import <GameController/GameController.h>
#import <UIKit/UIKit.h>
#import <dispatch/dispatch.h>

namespace Gamepad {

namespace {

std::mutex       snapshotSync;
GamepadState     snapshot;
GamepadState     transitionState;
std::deque<GamepadButtonEvent> pendingEvents;
std::once_flag   initializeOnce;
dispatch_queue_t handlerQueue = nullptr;
GCController*    activeController = nil;
bool             applicationActive = false;
uint64_t         controllerGeneration = 0;
uint64_t         nextSampleSequence = 0;
uint64_t         nextEventSequence = 0;
uint64_t         pendingDroppedEvents = 0;
char             handlerQueueIdentity;

constexpr std::size_t MaxPendingEvents = 128;

GamepadState readState(GCExtendedGamepad* g) {
  GamepadState s;
  if(g==nil)
    return s;

  s.connected = true;
  s.lx = g.leftThumbstick.xAxis.value;
  s.ly = g.leftThumbstick.yAxis.value;
  s.rx = g.rightThumbstick.xAxis.value;
  s.ry = g.rightThumbstick.yAxis.value;
  s.lt = g.leftTrigger.value;
  s.rt = g.rightTrigger.value;

  s.a = g.buttonA.isPressed;
  s.b = g.buttonB.isPressed;
  s.x = g.buttonX.isPressed;
  s.y = g.buttonY.isPressed;

  s.lb = g.leftShoulder.isPressed;
  s.rb = g.rightShoulder.isPressed;
  s.l3 = g.leftThumbstickButton.isPressed;
  s.r3 = g.rightThumbstickButton.isPressed;

  s.dup    = g.dpad.up.isPressed;
  s.ddown  = g.dpad.down.isPressed;
  s.dleft  = g.dpad.left.isPressed;
  s.dright = g.dpad.right.isPressed;

  s.menu    = g.buttonMenu.isPressed;
  s.options = (g.buttonOptions!=nil) ? g.buttonOptions.isPressed : false;

  return s;
  }

void queueButtonEventLocked(GamepadButton button, bool before, bool after) {
  if(before==after)
    return;

  GamepadButtonEvent event;
  event.button   = button;
  event.pressed  = after;
  event.sequence = ++nextEventSequence;

  if(pendingEvents.size()>=MaxPendingEvents) {
    pendingEvents.pop_front();
    ++pendingDroppedEvents;
    }
  pendingEvents.push_back(event);
  }

bool* buttonField(GamepadState& state, GamepadButton button) {
  switch(button) {
    case GamepadButton::A:         return &state.a;
    case GamepadButton::B:         return &state.b;
    case GamepadButton::X:         return &state.x;
    case GamepadButton::Y:         return &state.y;
    case GamepadButton::LB:        return &state.lb;
    case GamepadButton::RB:        return &state.rb;
    case GamepadButton::L3:        return &state.l3;
    case GamepadButton::R3:        return &state.r3;
    case GamepadButton::DpadUp:    return &state.dup;
    case GamepadButton::DpadDown:  return &state.ddown;
    case GamepadButton::DpadLeft:  return &state.dleft;
    case GamepadButton::DpadRight: return &state.dright;
    case GamepadButton::Menu:      return &state.menu;
    case GamepadButton::Options:   return &state.options;
    }
  return nullptr;
  }

// Full-profile callbacks are latest-state notifications. They keep analog axes
// and the public snapshot fresh, but button history comes only from the
// pressedChangedHandler argument below.
void publish(GamepadState state, bool resetTransitions = false) {
  state.generation = controllerGeneration;
  std::lock_guard<std::mutex> guard(snapshotSync);
  state.sampleSequence = ++nextSampleSequence;

  if(resetTransitions) {
    pendingEvents.clear();
    pendingDroppedEvents = 0;
    transitionState = state;
    }
  snapshot = state;
  }

void publishButton(GamepadButton button, bool pressed) {
  std::lock_guard<std::mutex> guard(snapshotSync);
  bool* transition = buttonField(transitionState,button);
  bool* current    = buttonField(snapshot,button);
  if(transition==nullptr || current==nullptr)
    return;

  const bool before = *transition;
  *transition = pressed;
  *current    = pressed;
  snapshot.sampleSequence = ++nextSampleSequence;
  queueButtonEventLocked(button,before,pressed);
  }

void installButtonHandler(GCControllerButtonInput* input,
                          GamepadButton button) {
  if(input==nil)
    return;
  const uint64_t generation = controllerGeneration;
  input.pressedChangedHandler = ^(GCControllerButtonInput* changed,
                                  float value, BOOL pressed) {
    (void)changed;
    (void)value;
    if(activeController==nil || controllerGeneration!=generation)
      return;
    publishButton(button,pressed!=NO);
    };
  }

void installButtonHandlers(GCExtendedGamepad* gamepad) {
  installButtonHandler(gamepad.buttonA,                 GamepadButton::A);
  installButtonHandler(gamepad.buttonB,                 GamepadButton::B);
  installButtonHandler(gamepad.buttonX,                 GamepadButton::X);
  installButtonHandler(gamepad.buttonY,                 GamepadButton::Y);
  installButtonHandler(gamepad.leftShoulder,            GamepadButton::LB);
  installButtonHandler(gamepad.rightShoulder,           GamepadButton::RB);
  installButtonHandler(gamepad.leftThumbstickButton,    GamepadButton::L3);
  installButtonHandler(gamepad.rightThumbstickButton,   GamepadButton::R3);
  installButtonHandler(gamepad.dpad.up,                 GamepadButton::DpadUp);
  installButtonHandler(gamepad.dpad.down,               GamepadButton::DpadDown);
  installButtonHandler(gamepad.dpad.left,               GamepadButton::DpadLeft);
  installButtonHandler(gamepad.dpad.right,              GamepadButton::DpadRight);
  installButtonHandler(gamepad.buttonMenu,              GamepadButton::Menu);
  installButtonHandler(gamepad.buttonOptions,           GamepadButton::Options);
  }

void clearButtonHandlers(GCExtendedGamepad* gamepad) {
  if(gamepad==nil)
    return;
  gamepad.buttonA.pressedChangedHandler               = nil;
  gamepad.buttonB.pressedChangedHandler               = nil;
  gamepad.buttonX.pressedChangedHandler               = nil;
  gamepad.buttonY.pressedChangedHandler               = nil;
  gamepad.leftShoulder.pressedChangedHandler          = nil;
  gamepad.rightShoulder.pressedChangedHandler         = nil;
  gamepad.leftThumbstickButton.pressedChangedHandler  = nil;
  gamepad.rightThumbstickButton.pressedChangedHandler = nil;
  gamepad.dpad.up.pressedChangedHandler               = nil;
  gamepad.dpad.down.pressedChangedHandler             = nil;
  gamepad.dpad.left.pressedChangedHandler             = nil;
  gamepad.dpad.right.pressedChangedHandler            = nil;
  gamepad.buttonMenu.pressedChangedHandler            = nil;
  gamepad.buttonOptions.pressedChangedHandler         = nil;
  }

void synchronizeHandlerQueue(dispatch_block_t block) {
  if(handlerQueue==nullptr ||
     dispatch_get_specific(&handlerQueueIdentity)==&handlerQueueIdentity) {
    block();
    return;
    }
  dispatch_sync(handlerQueue,block);
  }

void deactivateController() {
  if(activeController!=nil) {
    GCExtendedGamepad* gamepad = activeController.extendedGamepad;
    gamepad.valueChangedHandler = nil;
    clearButtonHandlers(gamepad);
#if !__has_feature(objc_arc)
    [activeController release];
#endif
    activeController = nil;
    }
  ++controllerGeneration;
  publish(GamepadState{},true);
  }

void activateController(GCController* controller) {
  if(controller==nil || controller.extendedGamepad==nil)
    return;

  if(activeController==controller) {
    publish(readState(controller.extendedGamepad));
    return;
    }

  deactivateController();
#if __has_feature(objc_arc)
  activeController = controller;
#else
  activeController = [controller retain];
#endif

  activeController.handlerQueue = handlerQueue;
  GCExtendedGamepad* gamepad = activeController.extendedGamepad;

  // Establish the transition baseline before installing callbacks. A button
  // already held while connecting is state, not a synthetic press event.
  publish(readState(gamepad),true);
  installButtonHandlers(gamepad);
  gamepad.valueChangedHandler = ^(GCExtendedGamepad* changed,
                                  GCControllerElement* element) {
    (void)element;
    if(activeController==nil || activeController.extendedGamepad!=changed)
      return;
    publish(readState(changed));
    };
  }

void activateFirstController(GCController* ignored = nil) {
  if(!applicationActive || activeController!=nil)
    return;
  for(GCController* controller in [GCController controllers]) {
    if(controller!=ignored && controller.extendedGamepad!=nil) {
      activateController(controller);
      return;
      }
    }
  publish(GamepadState{},true);
  }

void initialize() {
  dispatch_queue_attr_t attributes =
    dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,
                                            QOS_CLASS_USER_INTERACTIVE, 0);
  attributes = dispatch_queue_attr_make_with_autorelease_frequency(
    attributes, DISPATCH_AUTORELEASE_FREQUENCY_WORK_ITEM);
  handlerQueue = dispatch_queue_create("org.opengothic.gamecontroller", attributes);
  dispatch_queue_set_specific(handlerQueue, &handlerQueueIdentity,
                              &handlerQueueIdentity, nullptr);

  NSNotificationCenter* notifications = [NSNotificationCenter defaultCenter];
  NSOperationQueue* mainQueue = [NSOperationQueue mainQueue];

  [notifications addObserverForName:GCControllerDidConnectNotification
                             object:nil
                              queue:mainQueue
                         usingBlock:^(NSNotification* note) {
    GCController* controller = (GCController*)note.object;
    dispatch_async(handlerQueue, ^{
      if(applicationActive && activeController==nil)
        activateController(controller);
      });
    }];

  [notifications addObserverForName:GCControllerDidDisconnectNotification
                             object:nil
                              queue:mainQueue
                         usingBlock:^(NSNotification* note) {
    GCController* controller = (GCController*)note.object;
    dispatch_async(handlerQueue, ^{
      if(controller==activeController) {
        deactivateController();
        activateFirstController(controller);
        }
      });
    }];

  [notifications addObserverForName:UIApplicationWillResignActiveNotification
                             object:nil
                              queue:mainQueue
                         usingBlock:^(NSNotification* note) {
    (void)note;
    // UIKit posts this notification on the main queue. Synchronizing here
    // guarantees that no stale pressed state survives application suspension.
    synchronizeHandlerQueue(^{
      applicationActive = false;
      deactivateController();
      });
    }];

  [notifications addObserverForName:UIApplicationDidBecomeActiveNotification
                             object:nil
                              queue:mainQueue
                         usingBlock:^(NSNotification* note) {
    (void)note;
    dispatch_async(handlerQueue, ^{
      applicationActive = true;
      activateFirstController();
      });
    }];

  UIApplicationState state = [UIApplication sharedApplication].applicationState;
  synchronizeHandlerQueue(^{
    applicationActive = (state==UIApplicationStateActive);
    activateFirstController();
    });
  }

void refreshControllerState() {
  synchronizeHandlerQueue(^{
    if(!applicationActive || activeController==nil)
      return;
    GCExtendedGamepad* gamepad = activeController.extendedGamepad;
    if(gamepad!=nil)
      publish(readState(gamepad));
    });
  }

}

GamepadState poll() {
  std::call_once(initializeOnce, initialize);
  std::lock_guard<std::mutex> guard(snapshotSync);
  return snapshot;
  }

GamepadInputFrame consume() {
  std::call_once(initializeOnce, initialize);
  refreshControllerState();

  GamepadInputFrame frame;
  std::lock_guard<std::mutex> guard(snapshotSync);
  frame.state = snapshot;
  frame.events.assign(pendingEvents.begin(),pendingEvents.end());
  frame.droppedEvents = pendingDroppedEvents;
  pendingEvents.clear();
  pendingDroppedEvents = 0;
  return frame;
  }

}

#endif
