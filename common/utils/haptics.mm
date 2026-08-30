#include "haptics.h"

#include <Tempest/Platform>

#if defined(__IOS__)

#import <UIKit/UIKit.h>

void Haptics::impact(Kind k) {
  UIImpactFeedbackStyle st = UIImpactFeedbackStyleMedium;
  if(k==Haptics::Light)
    st = UIImpactFeedbackStyleLight;
  else if(k==Haptics::Heavy)
    st = UIImpactFeedbackStyleHeavy;
  // Feedback generators must be used on the main thread. MRC: balance alloc.
  dispatch_async(dispatch_get_main_queue(), ^{
    UIImpactFeedbackGenerator* g = [[UIImpactFeedbackGenerator alloc] initWithStyle:st];
    [g impactOccurred];
    [g release];
    });
  }

#endif
