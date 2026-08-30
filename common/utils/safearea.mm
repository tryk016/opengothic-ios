#include "safearea.h"

#include <Tempest/Platform>

#if defined(__IOS__)

#import <UIKit/UIKit.h>

SafeArea::Insets SafeArea::insets() {
  // The game and UIKit share one thread (Tempest's fiber model), so reading
  // UIKit properties directly here is safe.
  UIWindow* win = nil;
  for(UIScene* sc in UIApplication.sharedApplication.connectedScenes) {
    if(![sc isKindOfClass:UIWindowScene.class])
      continue;
    for(UIWindow* w in ((UIWindowScene*)sc).windows) {
      if(win==nil)
        win = w;
      if(w.isKeyWindow) {
        win = w;
        break;
        }
      }
    }
  if(win==nil)
    return Insets();

  // safeAreaInsets are in points; widget coordinates are frame * scale
  // (see Tempest implWindowClientRect), so convert with the same factor.
  const UIEdgeInsets in = win.safeAreaInsets;
  const CGFloat      s  = win.contentScaleFactor;

  Insets r;
  r.top    = int(in.top   *s);
  r.left   = int(in.left  *s);
  r.bottom = int(in.bottom*s);
  r.right  = int(in.right *s);
  return r;
  }

#endif
