#pragma once

// Configure the process audio session so gameplay audio behaves correctly on
// iOS: it keeps playing with the ring/silent switch on and cooperates with
// interruptions (calls, Bluetooth headset). Implemented in audiosession.mm on
// iOS; a no-op elsewhere (audiosession.cpp).
struct AudioSession {
  static void activate();
  };
