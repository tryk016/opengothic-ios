#include "gamepad.h"

#include <Tempest/Platform>

#include <mutex>

#if defined(__IOS__)

#import <GameController/GameController.h>
#import <UIKit/UIKit.h>
#import <dispatch/dispatch.h>

namespace Gamepad {

namespace {

std::mutex       snapshotSync;
GamepadState     snapshot;
std::once_flag   initializeOnce;
dispatch_queue_t handlerQueue = nullptr;
GCController*    activeController = nil;
bool             applicationActive = false;
uint64_t         controllerGeneration = 0;

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

void publish(GamepadState state) {
  state.generation = controllerGeneration;
  std::lock_guard<std::mutex> guard(snapshotSync);
  snapshot = state;
  }

void deactivateController() {
  if(activeController!=nil) {
    activeController.extendedGamepad.valueChangedHandler = nil;
#if !__has_feature(objc_arc)
    [activeController release];
#endif
    activeController = nil;
    }
  ++controllerGeneration;
  publish(GamepadState{});
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
  gamepad.valueChangedHandler = ^(GCExtendedGamepad* changed,
                                  GCControllerElement* element) {
    (void)element;
    if(activeController==nil || activeController.extendedGamepad!=changed)
      return;
    publish(readState(changed));
    };

  // valueChangedHandler only reports future changes. Seed the snapshot with
  // the complete current state so the first poll is already coherent.
  publish(readState(gamepad));
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
  publish(GamepadState{});
  }

void initialize() {
  dispatch_queue_attr_t attributes =
    dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,
                                            QOS_CLASS_USER_INTERACTIVE, 0);
  attributes = dispatch_queue_attr_make_with_autorelease_frequency(
    attributes, DISPATCH_AUTORELEASE_FREQUENCY_WORK_ITEM);
  handlerQueue = dispatch_queue_create("org.opengothic.gamecontroller", attributes);

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
    dispatch_sync(handlerQueue, ^{
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
  dispatch_sync(handlerQueue, ^{
    applicationActive = (state==UIApplicationStateActive);
    activateFirstController();
    });
  }

}

GamepadState poll() {
  std::call_once(initializeOnce, initialize);
  std::lock_guard<std::mutex> guard(snapshotSync);
  return snapshot;
  }

}

#endif
