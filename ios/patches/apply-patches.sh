#!/usr/bin/env bash
set -euo pipefail

# Applies local fixes to submodules that are fetched fresh from upstream.
# Idempotent and CRLF-tolerant. Fails loudly if a patch does not apply, so CI
# never silently produces a broken binary. Both CI and the Mac build call this.

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

VC="$ROOT/lib/Tempest/Engine/system/api/iosapi.mm"

# Fix: ViewController -init must call [super init]. Without it the view
# controller has no initial trait collection and iOS 17/18 throws
# UIViewControllerMissingInitialTraitCollection during
# -[AppDelegate application:didFinishLaunchingWithOptions:], crashing on launch.
if [ ! -f "$VC" ]; then
  echo "ERROR: not found: $VC" >&2
  exit 1
fi

if grep -q 'self = \[super init\]' "$VC"; then
  echo "skip: iosapi.mm ViewController [super init] (already patched)"
else
  perl -0777 -pi -e \
    's/(-\(id\)init \{\r?\n)(\s*)(fullScreen = true;)/${1}${2}self = [super init];\n${2}${3}/' \
    "$VC"
  if grep -q 'self = \[super init\]' "$VC"; then
    echo "patched: iosapi.mm ViewController [super init]"
  else
    echo "ERROR: failed to patch iosapi.mm ViewController (pattern not found)" >&2
    exit 1
  fi
fi

# Fix (review B4): handle -touchesCancelled. Without it a system touch cancel
# (Control Center, incoming call) never produces a MouseUp -> stuck movement
# keys + a leaked touch-id slot. Forward it to the existing -touchesEnded.
if grep -q 'touchesCancelled' "$VC"; then
  echo "skip: iosapi.mm touchesCancelled (already patched)"
else
  perl -0777 -pi -e 's/(curentEvent = Event::MouseUp;\r?\n\s*swapContext\(\);\r?\n\s*\}\r?\n\s*\}\r?\n)\@end/$1\n- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)ex {\n  [self touchesEnded:touches withEvent:ex];\n  }\n\@end/s' \
    "$VC"
  if grep -q 'touchesCancelled' "$VC"; then
    echo "patched: iosapi.mm touchesCancelled"
  else
    echo "ERROR: failed to patch iosapi.mm touchesCancelled (pattern not found)" >&2
    exit 1
  fi
fi

# Fix (review N3): the whole engine + Daedalus VM run on this hand-swapped fiber
# stack. 1 MB has no guard page, so deep call chains (VM recursion, world vob
# trees) can silently overflow into corruption. Enlarge to 8 MB.
if grep -q 'appleStack\[8\*1024\*1024\]' "$VC"; then
  echo "skip: iosapi.mm fiber stack size (already patched)"
else
  perl -0777 -pi -e 's/appleStack\[1\*1024\*1024\]/appleStack[8*1024*1024]/' "$VC"
  if grep -q 'appleStack\[8\*1024\*1024\]' "$VC"; then
    echo "patched: iosapi.mm fiber stack -> 8 MB"
  else
    echo "ERROR: failed to patch iosapi.mm fiber stack (pattern not found)" >&2
    exit 1
  fi
fi

# Fix (review N2): implDestroyWindow was empty, leaving a live CADisplayLink and
# a dangling owner after teardown. Invalidate the link and null the pointers.
if grep -q 'displayLink invalidate' "$VC"; then
  echo "skip: iosapi.mm implDestroyWindow (already patched)"
else
  perl -0777 -pi -e 's/(void iOSApi::implDestroyWindow\(SystemApi::Window \*w\) \{\r?\n)\s*\}/${1}  auto wx = reinterpret_cast<TempestWindow*>(w);\n  if(wx==nullptr)\n    return;\n  [wx->displayLink invalidate];\n  wx->displayLink = nil;\n  wx->owner = nullptr;\n  }/s' \
    "$VC"
  if grep -q 'displayLink invalidate' "$VC"; then
    echo "patched: iosapi.mm implDestroyWindow"
  else
    echo "ERROR: failed to patch iosapi.mm implDestroyWindow (pattern not found)" >&2
    exit 1
  fi
fi

# Change: lock the game to landscape (matches Info.plist). Both the view
# controller and the app delegate advertise all orientations; restrict both to
# landscape so the 3D world never flips to portrait. shouldAutorotate stays YES,
# so it still rotates between LandscapeLeft/Right.
if grep -q 'UIInterfaceOrientationMaskLandscape' "$VC"; then
  echo "skip: iosapi.mm landscape lock (already patched)"
else
  perl -0777 -pi -e 's/UIInterfaceOrientationMaskAll/UIInterfaceOrientationMaskLandscape/g' "$VC"
  if grep -q 'UIInterfaceOrientationMaskLandscape' "$VC"; then
    echo "patched: iosapi.mm landscape lock"
  else
    echo "ERROR: failed to patch iosapi.mm landscape lock (pattern not found)" >&2
    exit 1
  fi
fi

# Change: keep the screen awake during play. Without this iOS dims and locks the
# display on its idle timer while the player isn't touching the screen (e.g.
# using a gamepad). Re-assert it every time the app becomes active.
if grep -q 'idleTimerDisabled' "$VC"; then
  echo "skip: iosapi.mm idle timer (already patched)"
else
  perl -0777 -pi -e 's/(- \(void\)applicationDidBecomeActive:\(UIApplication \*\)application\s*\{\r?\n\s*\(void\)application;\r?\n)/${1}  application.idleTimerDisabled = YES;\n/s' "$VC"
  if grep -q 'idleTimerDisabled' "$VC"; then
    echo "patched: iosapi.mm idle timer (keep screen awake)"
  else
    echo "ERROR: failed to patch iosapi.mm idle timer (pattern not found)" >&2
    exit 1
  fi
fi
