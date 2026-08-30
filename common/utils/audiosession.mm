#include "audiosession.h"

#include <Tempest/Platform>

#if defined(__IOS__)

#import  <AVFoundation/AVFoundation.h>
#include <Tempest/Log>

void AudioSession::activate() {
  NSError*        err = nil;
  AVAudioSession* s   = [AVAudioSession sharedInstance];
  // Playback category: game audio is the point of the session, so keep it
  // audible with the mute switch on and don't mix-duck by default.
  if(![s setCategory:AVAudioSessionCategoryPlayback error:&err])
    Tempest::Log::e("AVAudioSession: setCategory(Playback) failed");
  err = nil;
#if defined(__IPHONE_27_0)
  if(@available(iOS 27.0, *)) {
    // Session activation can wait for other system audio services. Starting
    // with iOS 27 Apple explicitly warns against doing that synchronously on
    // UIKit's main thread, which is also the Tempest engine thread on iOS.
    [s activateWithOptions:AVAudioSessionActivationOptionNone
         completionHandler:^(BOOL activated, NSError* activationError) {
      if(!activated)
        Tempest::Log::e("AVAudioSession: asynchronous activation failed: ",
                        activationError.localizedDescription.UTF8String);
    }];
    return;
  }
#endif
  if(![s setActive:YES error:&err])
    Tempest::Log::e("AVAudioSession: setActive(YES) failed");
  }

#endif
