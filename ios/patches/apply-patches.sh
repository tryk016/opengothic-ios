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

# Change: unlock ProMotion refresh. The CADisplayLink is created with default
# settings, so iOS caps it at 60 Hz — and because one tick must fit sim +
# render + present, a >16.7 ms cycle drops every other vsync, pinning the game
# at a hard 30 fps. Ask for up to 120 Hz (min 30 lets the system lower the
# cadence under thermal pressure). Pairs with the
# CADisableMinimumFrameDurationOnPhone key in Info.plist.in; no effect on
# 60 Hz (non-ProMotion) displays.
if grep -q 'preferredFrameRateRange' "$VC"; then
  echo "skip: iosapi.mm preferredFrameRateRange (already patched)"
else
  perl -0777 -pi -e 's/(window->displayLink = \[CADisplayLink displayLinkWithTarget:window selector:\@selector\(drawFrame\)\];)/$1\n  if (\@available(iOS 15.0, *)) {\n    window->displayLink.preferredFrameRateRange = CAFrameRateRangeMake(30, 120, 120);\n    }/' "$VC"
  if grep -q 'preferredFrameRateRange' "$VC"; then
    echo "patched: iosapi.mm preferredFrameRateRange (30..120 Hz)"
  else
    echo "ERROR: failed to patch iosapi.mm preferredFrameRateRange (pattern not found)" >&2
    exit 1
  fi
fi

# Expose a tiny C bridge so OpenGothic can select the CADisplayLink cadence at
# runtime. The game and UIKit fibers share the main thread on iOS, therefore a
# software sleep inside MainWindow::render blocks CADisplayLink itself and adds
# another partial/full refresh interval after the requested delay.
if grep -q 'tempestIosSetPreferredFrameRate' "$VC"; then
  echo "skip: iosapi.mm runtime frame-rate bridge (already patched)"
else
  perl -0777 -pi -e 's/(static TempestWindow\* mainWindow = nullptr;\r?\n)/$1\nextern "C" void tempestIosSetPreferredFrameRate(int fps) {\n  auto* window = mainWindow;\n  if(window==nil || window->displayLink==nil)\n    return;\n  if (\@available(iOS 15.0, *)) {\n    if(fps==30)\n      window->displayLink.preferredFrameRateRange = CAFrameRateRangeMake(30, 30, 30);\n    else if(fps==60)\n      window->displayLink.preferredFrameRateRange = CAFrameRateRangeMake(60, 60, 60);\n    else\n      window->displayLink.preferredFrameRateRange = CAFrameRateRangeMake(30, 120, 120);\n    } else {\n    window->displayLink.preferredFramesPerSecond = fps>0 ? fps : 60;\n    }\n  }\n/s' "$VC"
  if grep -q 'tempestIosSetPreferredFrameRate' "$VC"; then
    echo "patched: iosapi.mm runtime CADisplayLink frame-rate bridge"
  else
    echo "ERROR: failed to patch iosapi.mm runtime frame-rate bridge (pattern not found)" >&2
    exit 1
  fi
fi

# Fix: never let a C++ exception from event/render dispatch escape into the
# fiber run-loop. Without a guard it unwinds through implProcessEvents into
# implExec/main (no handler) -> std::terminate -> SIGABRT (crash to home).
# Wrap the dispatch in try/catch so a stray throw (e.g. during save-game
# finalize) is logged and the app keeps running instead of aborting.
if grep -q 'uncaught exception in iOS event dispatch' "$VC"; then
  echo "skip: iosapi.mm implProcessEvents exception guard (already patched)"
else
  perl -0777 -pi -e 's/(\@autoreleasepool \{\r?\n)(\s*auto& wnd   = \*mainWindow->owner;)/${1}    try {\n${2}/s' "$VC"
  perl -0777 -pi -e 's/    \}(\r?\n)  swapContext\(\);(\r?\n)  \}(\r?\n\r?\n)void iOSApi::implSetWindowTitle/    }\n    catch(const std::exception& e){ Tempest::Log::e("uncaught exception in iOS event dispatch: ", e.what()); }\n    catch(NSException* e){ Tempest::Log::e("uncaught NSException in iOS event dispatch: ", (e.name!=nil ? e.name.UTF8String : "?"), ": ", (e.reason!=nil ? e.reason.UTF8String : "?")); }\n    catch(...){ Tempest::Log::e("uncaught non-std\/ObjC exception in iOS event dispatch"); }\n    }${1}  swapContext();${2}  }${3}void iOSApi::implSetWindowTitle/s' "$VC"
  if grep -q 'uncaught exception in iOS event dispatch' "$VC"; then
    echo "patched: iosapi.mm implProcessEvents exception guard"
  else
    echo "ERROR: failed to patch iosapi.mm implProcessEvents guard (pattern not found)" >&2
    exit 1
  fi
fi

# Fix (save crash, proven on device): the game runs on a manual setjmp/longjmp
# fiber that SHARES the thread's Objective-C autorelease-pool stack with the
# UIKit fiber. The @autoreleasepool pushed here got its page invalidated across
# fiber switches -> AutoreleasePoolPage::badPop -> _objc_fatal ("Invalid or
# prematurely-freed autorelease pool") -> SIGABRT on save. Do NOT push a pool
# on the game fiber at all: objects autoreleased during game dispatch drain in
# UIKit's own per-runloop-cycle pools, which is safe and bounded.
# NOTE: must run AFTER the exception-guard patch above (it anchors on 'try {').
if grep -q 'no-objc-pool' "$VC"; then
  echo "skip: iosapi.mm implProcessEvents no-objc-pool (already patched)"
else
  perl -0777 -pi -e 's|\@autoreleasepool \{(\r?\n    try \{)|{ // no-objc-pool: pool stack is per-thread and shared with the UIKit fiber; pushing here gets invalidated across swapContext (badPop/SIGABRT)${1}|s' "$VC"
  if grep -q 'no-objc-pool' "$VC"; then
    echo "patched: iosapi.mm implProcessEvents no-objc-pool"
  else
    echo "ERROR: failed to patch iosapi.mm no-objc-pool (pattern not found)" >&2
    exit 1
  fi
fi

RF="$ROOT/lib/Tempest/Engine/io/rfile.mm"

# Fix: RFile on iOS resolves RELATIVE paths against the app-bundle resource dir
# only. But savegames and Gothic.ini are written (WFile = plain fopen) into the
# CWD — the Documents folder — so reading them back always failed ("Unable to
# open file"): loading a save did nothing and save slots showed no
# header/date/thumbnail. Try the CWD first; fall back to the bundle only for
# genuinely bundled resources.
if [ ! -f "$RF" ]; then
  echo "ERROR: not found: $RF" >&2
  exit 1
fi
if grep -q 'cwd-first' "$RF"; then
  echo "skip: rfile.mm cwd-first (already patched)"
else
  perl -0777 -pi -e 's|(  \@autoreleasepool \{)|  // cwd-first: user files (saves, Gothic.ini) are written to CWD == Documents\n  if(void* ret = fopen(cstr,"rb"))\n    return ret;\n\n${1}|s' "$RF"
  if grep -q 'cwd-first' "$RF"; then
    echo "patched: rfile.mm cwd-first"
  else
    echo "ERROR: failed to patch rfile.mm cwd-first (pattern not found)" >&2
    exit 1
  fi
fi

MTS="$ROOT/lib/Tempest/Engine/gapi/metal/mtswapchain.mm"

# Change: triple-buffer the Metal swapchain. Tempest pins maximumDrawableCount
# to 2 on iOS (memory guard for 2 GB iPhones), but with double buffering the
# synchronous present path blocks inside nextDrawable()
# (allowsNextDrawableTimeout is NO) for up to a full vsync while the previous
# frame is still on glass - a fixed ~16 ms cost every frame that caps the game
# near 30 fps on 60 Hz displays and ~60 fps on ProMotion, regardless of GPU
# load. A third drawable (~15 MB at 2868x1320 BGRA) makes the acquire
# non-blocking.
if [ ! -f "$MTS" ]; then
  echo "ERROR: not found: $MTS" >&2
  exit 1
fi
if grep -Eq 'maximumDrawableCount\s+= 3' "$MTS"; then
  echo "skip: mtswapchain.mm triple buffering (already patched)"
else
  perl -0777 -pi -e 's/(lay\.maximumDrawableCount\s*=\s*)2;/${1}3;/' "$MTS"
  if grep -Eq 'maximumDrawableCount\s+= 3' "$MTS"; then
    echo "patched: mtswapchain.mm maximumDrawableCount 2 -> 3 (triple buffering)"
  else
    echo "ERROR: failed to patch mtswapchain.mm drawable count (pattern not found)" >&2
    exit 1
  fi
fi

# Validated iOS performance path: expose an opt-in implementation that renders
# the final pass directly into CAMetalDrawable. The patch is always present
# after this script; production and performance builds enable it through the
# CMake-owned TEMPEST_METAL_DIRECT_DRAWABLE definition.
DIRECT_DRAWABLE_PATCH="$ROOT/ios/patches/tempest-metal-direct-drawable-v2.patch"
if grep -q 'Direct-drawable v2 experiment' "$MTS"; then
  echo "skip: mtswapchain.mm direct drawable v2 support (already patched)"
else
  if [ ! -f "$DIRECT_DRAWABLE_PATCH" ]; then
    echo "ERROR: not found: $DIRECT_DRAWABLE_PATCH" >&2
    exit 1
  fi
  EXPECTED_TEMPEST_COMMIT="61b58f710b00f64d190fed2661f5762909397d1a"
  ACTUAL_TEMPEST_COMMIT="$(git -C "$ROOT/lib/Tempest" rev-parse HEAD)"
  if [ "$ACTUAL_TEMPEST_COMMIT" != "$EXPECTED_TEMPEST_COMMIT" ]; then
    echo "ERROR: Tempest changed ($ACTUAL_TEMPEST_COMMIT); refresh direct drawable v2 patch" >&2
    exit 1
  fi
  if git -C "$ROOT/lib/Tempest" apply --unidiff-zero --check "$DIRECT_DRAWABLE_PATCH"; then
    git -C "$ROOT/lib/Tempest" apply --unidiff-zero "$DIRECT_DRAWABLE_PATCH"
  else
    echo "ERROR: failed to apply Tempest direct drawable v2 support patch" >&2
    exit 1
  fi
  if grep -q 'Direct-drawable v2 experiment' "$MTS"; then
    echo "patched: mtswapchain.mm direct drawable v2 support"
  else
    echo "ERROR: direct drawable v2 marker missing after patch" >&2
    exit 1
  fi
fi

# Optional MetalFX Spatial backend support. The patch is always applied so the
# public Tempest API remains available to the game; the CMake option controls
# whether the iOS Metal implementation and weak framework link are compiled.
METALFX_PATCH="$ROOT/ios/patches/tempest-metalfx-spatial.patch"
METALFX_MARKER="$ROOT/lib/Tempest/Engine/gapi/metal/mtspatialscaler.mm"
if [ -f "$METALFX_MARKER" ] && grep -q 'MetalApi::createSpatialScaler' "$METALFX_MARKER"; then
  echo "skip: Tempest MetalFX Spatial support (already patched)"
else
  if [ ! -f "$METALFX_PATCH" ]; then
    echo "ERROR: not found: $METALFX_PATCH" >&2
    exit 1
  fi
  EXPECTED_TEMPEST_COMMIT="61b58f710b00f64d190fed2661f5762909397d1a"
  ACTUAL_TEMPEST_COMMIT="$(git -C "$ROOT/lib/Tempest" rev-parse HEAD)"
  if [ "$ACTUAL_TEMPEST_COMMIT" != "$EXPECTED_TEMPEST_COMMIT" ]; then
    echo "ERROR: Tempest changed ($ACTUAL_TEMPEST_COMMIT); refresh MetalFX Spatial patch" >&2
    exit 1
  fi
  if git -C "$ROOT/lib/Tempest" apply --unidiff-zero --check "$METALFX_PATCH"; then
    git -C "$ROOT/lib/Tempest" apply --unidiff-zero "$METALFX_PATCH"
  else
    echo "ERROR: failed to apply Tempest MetalFX Spatial support patch" >&2
    exit 1
  fi
  if [ -f "$METALFX_MARKER" ] && grep -q 'MetalApi::createSpatialScaler' "$METALFX_MARKER"; then
    echo "patched: Tempest MetalFX Spatial support"
  else
    echo "ERROR: Tempest MetalFX Spatial marker missing after patch" >&2
    exit 1
  fi
fi

# Optional MetalFX Temporal backend support. This patch is based on the
# already-patched Spatial Tempest tree because Temporal deliberately keeps
# Spatial as its first runtime fallback. The CMake option controls compilation.
METALFX_TEMPORAL_PATCH="$ROOT/ios/patches/tempest-metalfx-temporal.patch"
METALFX_TEMPORAL_MARKER="$ROOT/lib/Tempest/Engine/gapi/metal/mttemporalscaler.mm"
if [ -f "$METALFX_TEMPORAL_MARKER" ] && grep -q 'MetalApi::createTemporalScaler' "$METALFX_TEMPORAL_MARKER"; then
  echo "skip: Tempest MetalFX Temporal support (already patched)"
else
  if [ ! -f "$METALFX_TEMPORAL_PATCH" ]; then
    echo "ERROR: not found: $METALFX_TEMPORAL_PATCH" >&2
    exit 1
  fi
  EXPECTED_TEMPEST_COMMIT="61b58f710b00f64d190fed2661f5762909397d1a"
  ACTUAL_TEMPEST_COMMIT="$(git -C "$ROOT/lib/Tempest" rev-parse HEAD)"
  if [ "$ACTUAL_TEMPEST_COMMIT" != "$EXPECTED_TEMPEST_COMMIT" ]; then
    echo "ERROR: Tempest changed ($ACTUAL_TEMPEST_COMMIT); refresh MetalFX Temporal patch" >&2
    exit 1
  fi
  if git -C "$ROOT/lib/Tempest" apply --unidiff-zero --check "$METALFX_TEMPORAL_PATCH"; then
    git -C "$ROOT/lib/Tempest" apply --unidiff-zero "$METALFX_TEMPORAL_PATCH"
  else
    echo "ERROR: failed to apply Tempest MetalFX Temporal support patch after Spatial" >&2
    exit 1
  fi
  if [ -f "$METALFX_TEMPORAL_MARKER" ] && grep -q 'MetalApi::createTemporalScaler' "$METALFX_TEMPORAL_MARKER"; then
    echo "patched: Tempest MetalFX Temporal support"
  else
    echo "ERROR: Tempest MetalFX Temporal marker missing after patch" >&2
    exit 1
  fi
fi
