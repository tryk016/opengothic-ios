#pragma once

// Show a fatal, user-facing message. On iOS this is a native UIAlertController
// (implemented in systemmsg.mm); elsewhere it goes to the log (systemmsg.cpp).
struct SystemMsg {
  static void fatal(const char* title, const char* message);
  };
