#include "systemmsg.h"

#include <Tempest/Platform>

#if defined(__IOS__)

#import <UIKit/UIKit.h>

static UIViewController* topViewController() {
  UIViewController* root = nil;
  for(UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    if([scene isKindOfClass:[UIWindowScene class]]) {
      UIWindowScene* ws = (UIWindowScene*)scene;
      for(UIWindow* w in ws.windows)
        if(w.isKeyWindow) { root = w.rootViewController; break; }
      }
    if(root!=nil)
      break;
    }
  return root;
  }

void SystemMsg::fatal(const char* title, const char* message) {
  NSString* t = [NSString stringWithUTF8String:(title   ? title   : "")];
  NSString* m = [NSString stringWithUTF8String:(message ? message : "")];
  dispatch_async(dispatch_get_main_queue(), ^{
    UIViewController* root = topViewController();
    if(root==nil)
      return;
    UIAlertController* a =
      [UIAlertController alertControllerWithTitle:t
                                          message:m
                                   preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"OK"
                                          style:UIAlertActionStyleDefault
                                        handler:nil]];
    [root presentViewController:a animated:YES completion:nil];
    });
  }

#endif
