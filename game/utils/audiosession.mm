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
  if(![s setActive:YES error:&err])
    Tempest::Log::e("AVAudioSession: setActive(YES) failed");
  }

#endif
