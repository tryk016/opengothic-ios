#include "memoryinfo.h"

#include <Tempest/Platform>

#if defined(__IOS__)

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <atomic>
#include <dlfcn.h>
#include <mach/mach.h>
#include <mutex>
#include <os/proc.h>

namespace {

std::atomic<uint32_t> pendingEvents{MemoryInfo::NoEvent};

void markEvent(MemoryInfo::Event event) {
  pendingEvents.fetch_or(uint32_t(event),std::memory_order_relaxed);
  }

}

@interface OGMemoryInfoObserver : NSObject
- (void)memoryWarning:(NSNotification*)notification;
- (void)didEnterBackground:(NSNotification*)notification;
- (void)willEnterForeground:(NSNotification*)notification;
- (void)didBecomeActive:(NSNotification*)notification;
@end

@implementation OGMemoryInfoObserver

- (id)init {
  self = [super init];
  if(self!=nil) {
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(memoryWarning:)
                   name:UIApplicationDidReceiveMemoryWarningNotification object:nil];
    [center addObserver:self selector:@selector(didEnterBackground:)
                   name:UIApplicationDidEnterBackgroundNotification object:nil];
    [center addObserver:self selector:@selector(willEnterForeground:)
                   name:UIApplicationWillEnterForegroundNotification object:nil];
    [center addObserver:self selector:@selector(didBecomeActive:)
                   name:UIApplicationDidBecomeActiveNotification object:nil];
    }
  return self;
  }

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
  }

- (void)memoryWarning:(NSNotification*)notification {
  (void)notification;
  markEvent(MemoryInfo::MemoryWarning);
  }

- (void)didEnterBackground:(NSNotification*)notification {
  (void)notification;
  markEvent(MemoryInfo::DidEnterBackground);
  }

- (void)willEnterForeground:(NSNotification*)notification {
  (void)notification;
  markEvent(MemoryInfo::WillEnterForeground);
  }

- (void)didBecomeActive:(NSNotification*)notification {
  (void)notification;
  markEvent(MemoryInfo::DidBecomeActive);
  }

@end

void MemoryInfo::initialize() {
  static std::once_flag once;
  std::call_once(once,[]() {
    // Process-lifetime observer. Tempest owns the UIApplication lifetime and
    // tears the process down immediately after the game loop exits.
    static OGMemoryInfoObserver* observer = [[OGMemoryInfoObserver alloc] init];
    (void)observer;
    });
  }

MemoryInfo::Snapshot MemoryInfo::snapshot() {
  Snapshot ret;

#if defined(OPENGOTHIC_IOS_INCREASED_MEMORY_REQUESTED)
  ret.increasedMemoryLimitRequested = true;
#endif

  task_vm_info_data_t vmInfo = {};
  mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
  const kern_return_t taskResult = task_info(mach_task_self(),TASK_VM_INFO,
                                              reinterpret_cast<task_info_t>(&vmInfo),
                                              &count);
  if(taskResult==KERN_SUCCESS) {
    ret.footprintBytes = uint64_t(vmInfo.phys_footprint);
    ret.footprintValid = true;
    }

  ret.availableBytes = uint64_t(os_proc_available_memory());
  ret.availableValid = true;

  switch([[NSProcessInfo processInfo] thermalState]) {
    case NSProcessInfoThermalStateNominal:  ret.thermal = ThermalState::Nominal;  break;
    case NSProcessInfoThermalStateFair:     ret.thermal = ThermalState::Fair;     break;
    case NSProcessInfoThermalStateSerious:  ret.thermal = ThermalState::Serious;  break;
    case NSProcessInfoThermalStateCritical: ret.thermal = ThermalState::Critical; break;
    }

  // SecTask is available at runtime on iOS but its header is not part of the
  // public iPhoneOS SDK. Resolve it lazily so newer SDKs can still compile the
  // diagnostics build, and degrade to "unchecked" if the symbols disappear.
  using SecTaskCreateFromSelfFn = CFTypeRef (*)(CFAllocatorRef);
  using SecTaskCopyValueForEntitlementFn = CFTypeRef (*)(CFTypeRef,CFStringRef,CFErrorRef*);
  const auto createTask = reinterpret_cast<SecTaskCreateFromSelfFn>(
      dlsym(RTLD_DEFAULT,"SecTaskCreateFromSelf"));
  const auto copyEntitlement = reinterpret_cast<SecTaskCopyValueForEntitlementFn>(
      dlsym(RTLD_DEFAULT,"SecTaskCopyValueForEntitlement"));

  CFTypeRef task = createTask!=nullptr ? createTask(kCFAllocatorDefault) : nullptr;
  if(task!=nullptr && copyEntitlement!=nullptr) {
    CFErrorRef error = nullptr;
    CFTypeRef value = copyEntitlement(
        task,CFSTR("com.apple.developer.kernel.increased-memory-limit"),&error);
    ret.increasedMemoryLimitChecked = (error==nullptr);
    if(value!=nullptr) {
      ret.increasedMemoryLimitChecked = true;
      if(CFGetTypeID(value)==CFBooleanGetTypeID())
        ret.increasedMemoryLimitPresent = CFBooleanGetValue(static_cast<CFBooleanRef>(value));
      CFRelease(value);
      }
    if(error!=nullptr)
      CFRelease(error);
    }
  if(task!=nullptr)
    CFRelease(task);

  return ret;
  }

uint32_t MemoryInfo::consumeEvents() {
  return pendingEvents.exchange(NoEvent,std::memory_order_relaxed);
  }

const char* MemoryInfo::thermalStateName(ThermalState state) {
  switch(state) {
    case ThermalState::Nominal:  return "nominal";
    case ThermalState::Fair:     return "fair";
    case ThermalState::Serious:  return "serious";
    case ThermalState::Critical: return "critical";
    case ThermalState::Unknown:  return "unknown";
    }
  return "unknown";
  }

#endif
