# Android port — TODO / status

Tracks M1 (boot-to-menu) task-by-task status. See
`docs/superpowers/plans/2026-07-14-android-port-m1.md` for the full plan and
`.superpowers/sdd/progress.md` for the cross-task ledger.

## Task 1: First light — scaffolding, CI, clear-screen APK

Status: implemented, **UNVERIFIED** (no local NDK/Gradle toolchain on this
machine — real verification is CI green + on-device dark-red screen).

Done:
- [x] Gradle project skeleton (`android/settings.gradle`, `android/build.gradle`,
      `android/gradle.properties`, `android/app/build.gradle`).
- [x] Manifest + `MainActivity` (GameActivity, `MANAGE_EXTERNAL_STORAGE`,
      landscape lock, loads `libGothic2Notr.so`) + `strings.xml`.
- [x] Root `CMakeLists.txt`: `elseif(ANDROID)` branch — shared library target,
      `find_package(game-activity)`, link `game-activity`/`android`/`log`,
      every pre-existing `if(UNIX)`/`elseif(UNIX)` audited and guarded with
      `AND NOT ANDROID` / preceding `elseif(ANDROID)` branches.
- [x] `android/patches/apply-patches.sh` (idempotent): adds `AndroidApi` to
      `SystemApi::inst()` dispatch, `VK_KHR_android_surface` in
      `vulkanapi.cpp` + `vswapchain.cpp` (surface create + present-support),
      Android Vulkan/game-activity link section in Tempest's `CMakeLists.txt`,
      and emits the new `androidapi.h`/`androidapi.cpp` Tempest backend files.
- [x] `game/main_android.cpp` — temporary clear-screen (`#3a0000`) `android_main`,
      whole-file guarded by `#if defined(__ANDROID__)`.
- [x] `.github/workflows/android.yml` — CI: checkout w/ submodules, JDK 17,
      Android SDK, glslang-tools, apply patches, `./gradlew assembleRelease`,
      publish to GitHub Release `latest-android`, upload build artifact.
- [x] `android/build-android.sh` — local/manual reference build script.
- [ ] `android/keystore/opengothic.jks` — **NOT generated** (`keytool` not on
      PATH on this dev machine, no JDK installed locally). Needs to be
      generated once (see command in the plan / apply-patches sibling docs)
      before `assembleRelease` can produce a *signed* APK. This blocks CI's
      signing step until it exists in the repo.

Known concerns / expected CI iteration (flagged for the controller):
- `AndroidApi`'s window-availability handshake: `Tempest::Window` (and its
  `Swapchain`) is constructed synchronously in `android_main` before
  `ANativeWindow*` necessarily exists. `AndroidApi`'s constructor blocks,
  pumping `ALooper_pollAll`, until `app->window` is non-null, so that the
  `SystemApi::Window*` handle (== `ANativeWindow*`) captured once by
  `VSwapchain` at construction time is already valid. This is a reasonable
  reading of the "hwnd IS the ANativeWindow*" contract but is exactly the kind
  of thing that may need adjustment once real `android_native_app_glue` /
  `game-activity` compiler output is available.
- `game-activity`'s exact `android_app` struct fields (`window`, `cmd`,
  `onAppCmd`, `cmdPollSource`, `destroyRequested`, ...) were written from the
  well-known upstream `android_native_app_glue.h` contract (the header
  `game-activity` ships is a close derivative). If AGP/CI pulls a
  `game-activity` version with renamed/reshaped fields, `androidapi.cpp` will
  need small adjustments — this is explicitly called out as CI-iterative in
  the task brief.
- `#include "system/api/androidapi.h"` in `game/main_android.cpp` requires
  `lib/Tempest/Engine` (not just `Engine/include`) on the include path for the
  `${PROJECT_NAME}` target; added `include_directories(lib/Tempest/Engine)`
  inside the new `elseif(ANDROID)` CMake branch for this reason (a deliberate
  addition beyond the literal brief text, needed for the given include to
  resolve).
- CI's `gradle wrapper --gradle-version 8.7 || true` bootstrap step is
  unverified since it has never run; per the brief, this is expected to take
  a few CI iterations to shake out.

## Task 2: Boot the real engine (replace the clear-screen stub)

Status: implemented, **UNVERIFIED** (no local NDK/Gradle toolchain on this
machine — real verification is CI green + logcat showing a controlled
data-load failure, not the dark-red clear screen).

Done:
- [x] `game/main_android.cpp` — replaced `ClearWindow`/`android_main` body with
      the real bootstrap mirrored from `game/main.cpp`'s mobile path: log
      setup (`Tempest::WFile` + `Log::setOutputCallback`), `CrashLog::setup()`,
      `zenkit::Logger`/`Dm_setLogger` callbacks, `Workers::setThreadName`,
      `AudioSession::activate()`, `CommandLine` → `VulkanApi` → `Device` →
      `Resources` → `Gothic` → `GameMusic` → `gothic.setupGlobalScripts()` →
      `MainWindow` → `Tempest::Application::exec()`, wrapped in the same
      `catch(GothicNotFoundException&)` / `catch(std::exception&)` pair as
      `main.cpp`. `g_androidApp`/`AndroidApi::setAndroidApp(app)` stay first,
      before any of the above, preserving the Task 1 ordering invariant
      (`AndroidApi`'s ctor blocks until `app->window != nullptr`, so it must be
      wired before the first `Window`/`MainWindow` gets constructed).
- [x] `android_main` has no OS-supplied `argc`/`argv`; synthesized a
      single-element `argv = {"opengothic"}` (`argc=1`) so `CommandLine` takes
      the same "no flags" path a normal GUI process with no arguments takes —
      this is the same path `main.cpp`'s `int main(int argc, const char**
      argv)` already takes on iOS (there is no separate iOS arg-synthesis;
      `main()` there just receives the platform's normal empty/1-arg argv).
- [x] Data root left DEFAULT (Task 3 wires `/sdcard/OpenGothic`). Since
      Android's `InstallDetect::detectG2()` has no platform branch (falls to
      `return u"";`), `CommandLine`'s ctor throws `GothicNotFoundException`
      before any `Device`/`VulkanApi`/`Window` is constructed — expected,
      controlled failure for this task, logged via `Tempest::Log::e` /
      `SystemMsg::fatal` (both route through `__android_log_print`, tag
      `"app"`, visible via `adb logcat | grep -i gothic`).

Known concerns / expected CI iteration (flagged for the controller):
- `Tempest::WFile logFile("log.txt")` uses a relative path; Android's process
  CWD for a native-activity glue thread is unspecified and this write may
  fail. That's already wrapped in the same `try/catch(...)` main.cpp uses, so
  it falls back to console/`__android_log_print` only — not fatal, but worth
  checking in logcat that the "unable to setup logfile" fallback message (or
  its absence) matches expectations.
- Because the failure happens inside `CommandLine`'s constructor (before
  `VulkanApi`/`Device`/`MainWindow` exist), the Task 1 `AndroidApi`
  window-blocking handshake is never exercised by this run. That handshake
  still needs on-device verification once Task 3 wires real data and the
  bootstrap proceeds far enough to construct `MainWindow`.
- Selected a single-arg `Tempest::Device(AbstractGraphicsApi&)` construction
  path plus a locally-duplicated `selectDevice()` helper (mirroring
  `main.cpp`'s free function, which is file-local there and not exposed via
  any header) rather than sharing code — acceptable duplication given the
  "don't touch Tempest" constraint and small size of the helper.

## Task 3: Storage data root — point the engine at /sdcard/OpenGothic/Gothic2

Status: implemented, **UNVERIFIED** (no local NDK/Gradle toolchain on this
machine — real verification is CI green + logcat showing the engine load
`Gothic.ini`/VDFs from the device and reach the **main menu render**).

Done:
- [x] `game/main_android.cpp` — added `::chdir("/sdcard/OpenGothic")`
      (`<unistd.h>`) right after `Tempest::AndroidApi::setAndroidApp(app)`
      and before the log-file / engine bootstrap, so relative `log.txt`,
      `Gothic.ini`, and saves resolve under a writable, known directory
      instead of an unspecified native-glue-thread CWD. Return value is
      checked and logged via `Tempest::Log::e` on failure rather than
      silently ignored (`-Wall -Wconversion -Werror` is on for this target).
- [x] `game/main_android.cpp` — the synthesized argv now passes the Gothic II
      data root explicitly: `{"opengothic", "-g", "/sdcard/OpenGothic/Gothic2"}`,
      `argc=3`. Confirmed against `game/commandline.cpp` (`CommandLine::CommandLine`,
      ~line 62): `-g` takes the next argv entry verbatim into `gpath`, the
      exact field `rootPath()`/`nestedPath()`/`validateGothicPath()` all use
      as the game-data root (`validateGothicPath()` checks `<gpath>/Data` and
      `<gpath>/_work/Data` both exist). This is the same flag named in the
      Task 2 logcat evidence — `commandline.cpp:197`'s
      `"Gothic path is not provided. Please use command line argument -g <path>"`.
- [x] Updated the stale Task-2 comment on the `GothicNotFoundException` catch
      block: it no longer describes an expected/default outcome, since data
      is now wired — a controller note explains what a genuine catch there
      would now mean (data not copied yet, storage permission not granted,
      or a relative-path lookup that doesn't honor the absolute `-g` root).

Explicitly not done (out of scope per controller brief, left for the
controller to request only if on-device testing shows they're needed):
- Storage-permission gating/polling (`Environment.isExternalStorageManager()`)
  before bootstrap — relies on the test device already having
  `MANAGE_EXTERNAL_STORAGE` granted and `MainActivity.kt`'s existing
  Settings-redirect/relaunch flow (Task 1).
- Tempest `io/rfile` CWD-first relative-path patch (the iOS-style fix) — only
  to be added if logcat still shows file-open failures for `Gothic.ini`/VDFs
  after this change, since ZenKit is expected to open them by the absolute
  `-g` path already.
- `lib/Tempest` (submodule) was not touched.

Known concerns / expected CI iteration (flagged for the controller):
- If logcat shows the engine getting past `CommandLine` (no more
  `GothicNotFoundException`) but still failing later on a relative open
  (e.g. `Gothic.ini` under `system/`, or a save file), that is exactly the
  RFile CWD-first patch scenario called out above — watch for "Unable to
  open file" after the "Gothic path is not provided" line disappears.
- `chdir("/sdcard/OpenGothic")` assumes that path exists and is
  writable/readable by the app's UID by the time `android_main` runs (i.e.
  storage permission is already granted at first launch, per this task's
  stated assumption). No JNI polling/gating was added; a grant-then-relaunch
  flow is accepted for M1 per the brief.
- Did not add a trailing slash to `/sdcard/OpenGothic/Gothic2`;
  `commandline.cpp` appends one itself if missing
  (`if(gpath.size()>0 && gpath.back()!='/') gpath.push_back('/');`), so the
  no-trailing-slash literal used here is fine as-is.

## Task 4: Touch input — tap dispatched as mouse click for menu nav

Status: implemented, **UNVERIFIED** (no local NDK/Gradle toolchain on this
machine — real verification is CI green + on-device taps highlighting/
activating main-menu items via `adb shell input tap`).

Done:
- [x] `android/patches/apply-patches.sh` (`androidapi.cpp` heredoc) —
      `implProcessEvents` now drains `game-activity`'s per-frame motion-event
      buffer after cmd handling and before `dispatchRender`:
      `android_app_swap_input_buffers(g_app)` → for each
      `GameActivityMotionEvent`, mask `m.action & AMOTION_EVENT_ACTION_MASK`
      and map `AMOTION_EVENT_ACTION_DOWN/UP/MOVE` (and `CANCEL`, treated as an
      `UP`) to `Event::MouseDown/MouseUp/MouseMove`, read the primary pointer's
      position via `GameActivityPointerAxes_getX/Y(&m.pointers[0])`, build a
      `Tempest::MouseEvent` with the **exact constructor signature copied from
      `iosapi.mm`** (`x, y, Event::ButtonLeft, Event::M_NoModifier, 0,
      mouseID, type`) and dispatch it via `dispatchMouseDown/Up/Move(*g_owner,
      e)` — `g_owner` is the same `Tempest::Window*` handle the existing
      `dispatchResize`/`dispatchRender` calls already use. Always
      `android_app_clear_motion_events(ib)` after the loop, even if there was
      no owner yet, so stale events never queue up and replay as a burst
      later.
      Event order mirrors `iosapi.mm` exactly: `touchesBegan/Moved/Ended` each
      synthesize a single `MouseEvent` at the touch position and dispatch it
      directly — there is **no** separate hover/move sent before a down — so
      no synthetic move-before-down was added on the Android side either.
- [x] Added `#include <android/input.h>` to `androidapi.cpp` for the
      `AMOTION_EVENT_ACTION_*` constants (previously relied only on the
      transitive include from `android_native_app_glue.h`).
- [x] Renamed the pointer-id local to `pid` to avoid shadowing the
      pre-existing outer `int id` (the `ALooper_pollAll` result) already in
      `implProcessEvents`'s scope.

Explicitly not done (out of scope for M1's "tap = click" goal):
- Multi-touch: only the primary pointer (`m.pointers[0]`) is read. A second
  finger touching down mid-gesture emits `AMOTION_EVENT_ACTION_POINTER_DOWN/
  _UP` (distinct from plain `DOWN`/`UP` once masked), which this code ignores
  by design — deferred to M2's virtual gamepad/touch-controls work.
- No coordinate scaling is applied (unlike iOS, which multiplies by
  `contentScaleFactor`): `GameActivityPointerAxes_getX/Y` already report
  surface pixels, the same space as `ANativeWindow_getWidth/Height`.

Known concerns / expected CI iteration (flagged for the controller):
- **Coordinate alignment**: relies on the Activity being truly edge-to-edge
  with no letterboxing/inset offset between the touch coordinate space and
  the rendered surface (the existing `implSetAsFullscreen` comment says the
  theme is already fullscreen/edge-to-edge). If on-device taps land visually
  offset from the highlighted item (e.g. a status/nav-bar inset not accounted
  for), that points here first — not something a Windows-only, no-NDK
  environment can verify by reading source alone.
- `GameActivityMotionEvent`/`GameActivityPointerAxes_getX/Y`/
  `android_input_buffer`/`android_app_swap_input_buffers`/
  `android_app_clear_motion_events` are assumed reachable transitively via the
  already-included `<game-activity/native_app_glue/android_native_app_glue.h>`
  (this is the standard, documented AGDK GameActivity input-handling API —
  same shape as Google's own sample code) — first real compiler confirmation
  happens in CI, consistent with Task 1's note that `game-activity`'s exact
  header shape is CI-iterative.
- Did not special-case `AMOTION_EVENT_ACTION_POINTER_DOWN/_UP` (see "not done"
  above) — if on-device testing shows a stuck "button held" menu state after
  a brief accidental multi-touch, that is the mechanism to revisit.

## Task 5: Lifecycle hardening — surface loss on background/resume

Status: implemented, **UNVERIFIED** (no local NDK/Gradle/build toolchain on
this machine — real verification is CI green + on-device: press Home, wait,
return to foreground, confirm no SIGSEGV and rendering resumes).

Confirmed bug (reproduced on-device before this task): backgrounding (Home)
survives, but returning to foreground SIGSEGVs the game thread. Root cause:
`APP_CMD_TERM_WINDOW` destroys the `ANativeWindow`, but the engine's
`VSwapchain` (built from that window's `VkSurfaceKHR`) is never torn down or
rebuilt; `APP_CMD_INIT_WINDOW` on resume hands back a **different**
`ANativeWindow*`, and the next frame renders against the stale, freed
surface. `Swapchain::reset()` (the existing resize path) only rebuilds the
swapchain **images**, not the surface (confirmed by reading
`vswapchain.cpp`: `reset()` calls `cleanupSwapchain()` +`createSwapchain()`
only; `surface`/`hwnd` are private members only touched by the constructor
and `cleanup()`) — so it cannot recover a changed window handle.

Done:
- [x] Chose **approach (b)** from the brief (full `Swapchain` reconstruction)
      over (a) (teaching `vswapchain.cpp` to read a "live" window and
      self-heal in `reset()`). `Tempest::Device::swapchain(SystemApi::Window*)`
      already exists and does exactly this — `Device::swapchain(w)` →
      `api.createSwapchain(w,dev)` → `new Detail::VSwapchain(*dx, w)`, the
      same call `Swapchain(Device&, SystemApi::Window*)`'s constructor uses
      — so a full, independent surface+swapchain rebuild against a new
      window is already a first-class, already-battle-tested operation.
      This needed **zero changes** to `vswapchain.cpp`/`vulkanapi.cpp`/
      `device.cpp` — confirmed by reading all three.
- [x] `android/patches/apply-patches.sh` (`androidapi.h`/`androidapi.cpp`
      heredocs) — added `AndroidApi::setSurfaceCallbacks(onSurfaceDestroyed,
      onSurfaceCreated)`, a one-time registration point for two
      `std::function` callbacks:
      - `handleAppCmd`'s `APP_CMD_TERM_WINDOW` case now calls
        `onSurfaceDestroyed()` **synchronously**, before returning — not via
        the existing deferred `g_windowChanged` flag mechanism. This matters:
        `android_native_app_glue`'s post-exec for `TERM_WINDOW` nulls
        `android_app::window` and signals a condvar *after* this callback
        returns, and the platform's `onNativeWindowDestroyed` (UI thread)
        blocks on that condvar before actually releasing the
        `ANativeWindow`. That makes this handler returning the actual
        synchronization point gating real teardown — deferring would race
        the OS's teardown of the buffer queue while GPU work might still
        reference it (exactly what the observed "Surface: freeAllBuffers: 1
        buffers were freed while being dequeued!" logcat line describes).
      - `implProcessEvents`'s existing window-changed check now compares
        `g_app->window` against a remembered `g_lastWindow`: unchanged
        (a same-window resize/orientation change) still takes the original,
        cheap `dispatchResize` → `swapchain.reset()` path, untouched; an
        actually **different** window (background/resume) instead calls
        `onSurfaceCreated(newWindow)` and updates the baseline. This avoids
        both a wasteful double-reset on genuine resizes and a wasteful
        redundant recreate on the very first boot (the callback isn't even
        registered yet at that point — `setSurfaceCallbacks` also seeds
        `g_lastWindow` to the boot window and clears any pending stale
        flag, so registration itself can't misfire a spurious recreate).
      - Both callback invocations are wrapped in `try/catch`
        (`std::exception` + `catch(...)`, logged via `Tempest::Log::e`,
        which double-writes to logcat tag `"app"` **and** the `log.txt`
        sink `main_android.cpp` installs) so a failure can't unwind a C++
        exception across the glue's C callback boundary
        (`handleAppCmd`/`source->process`), which would otherwise be
        undefined behavior / `std::terminate`.
- [x] `game/mainwindow.h`/`mainwindow.cpp` (main repo, not the submodule —
      edited directly) — two new `#if defined(__ANDROID__)` public methods:
      - `onSurfaceDestroyed()`: `device.waitIdle()` (wrapped in
        `try{}catch(...){}`), draining any in-flight GPU work before the
        window dies. `dispatchRender` is already guarded off while windowless
        (pre-existing `g_app->window!=nullptr` check), so nothing new gets
        submitted after this.
      - `onSurfaceCreated(Tempest::SystemApi::Window* w)`:
        `device.waitIdle(); swapchain = device.swapchain(w);
        renderer.resetSwapchain(); camera->setViewport(...)` — the exact
        same recovery shape already used by the pre-existing
        `catch(const Tempest::SwapchainSuboptimal&)` block in `tick()`
        (`device.waitIdle(); swapchain.reset(); renderer.resetSwapchain();`),
        just with a full `Swapchain` replace instead of `.reset()` since the
        window handle itself changed. `Renderer::resetSwapchain()` already
        starts with its own `device.waitIdle()` and fully re-derives
        attachments — confirmed by reading `renderer.cpp` — so this is not
        new, untested machinery.
- [x] `game/main_android.cpp` — registers the two callbacks right after
      `MainWindow wx(device);` and before `Tempest::Application app;
      app.exec();`, as two lambdas capturing `wx` by reference (valid for
      `wx`'s whole remaining lifetime, which spans the entire `app.exec()`
      call — the only place these callbacks can fire from).

Explicitly not done:
- [ ] **Audio-init try/catch guard** — skipped. Read `main_android.cpp` in
      full: it does not construct a `Tempest::SoundDevice` directly at all;
      that construction is nested inside `Resources`'s member-initializer
      list (`resources.h:232`, `Tempest::SoundDevice sound;`), a class
      shared by every platform, and 5 other places
      (`gothic.h`/`gamemusic.h`/`gamesession.h`/`dialogmenu.h`/
      `videowidget.cpp`) each construct their own `SoundDevice` too. There is
      no cheap, local, Android-only wrap point in `main_android.cpp` itself;
      doing this properly means changing `Tempest::SoundDevice`'s
      constructor (submodule, affects all platforms) or restructuring
      `Resources`'s construction order — both bigger than "wrap in
      main_android.cpp" and explicitly called out as skippable in the brief
      ("Do NOT spend long on this... Skip if it would expand scope"), and
      audio is already confirmed working on-device (Task 4 note: "engine
      outputs PCM; emulator HAL chokes harmlessly"). Any existing exception
      from this path is already caught by `main_android.cpp`'s pre-existing
      outer `catch(const std::exception&)` around the whole bootstrap
      (logs + `SystemMsg::fatal`, not a silent crash) — so boot-time
      robustness here is degraded (a real init failure still aborts boot)
      but not worse than before, and not a raw crash either.

Known concerns / expected CI iteration (flagged for the controller):
- **This is the core, hardest fix of M1 and is unverified on real hardware.**
  Confidence is high (the swapchain reconstruction reuses the exact code
  path the constructor and the existing `SwapchainSuboptimal` recovery
  already exercise; the synchronous-teardown timing is derived from the
  documented `android_native_app_glue` command-ack protocol, not a guess),
  but this could not be built or run locally. On resume, watch logcat for:
  - No `SIGSEGV` / `Force removing ActivityRecord ... app died` after
    `adb shell input keyevent KEYCODE_HOME` then re-foregrounding.
  - Whether "Surface: freeAllBuffers: N buffers were freed while being
    dequeued!" still appears at backgrounding time. It may still print once
    (it can fire from the platform side regardless of how fast our
    `onSurfaceDestroyed` runs, depending on exactly how much GPU work was
    in flight the instant `TERM_WINDOW` arrived) — the important signal is
    whether it's still followed by a crash, not whether the line itself is
    fully eliminated.
  - Any `AndroidApi: onSurfaceDestroyed:` / `AndroidApi: onSurfaceCreated:`
    warning lines (tag `"app"`, from the new `Log::e` catch blocks) — these
    mean `device.waitIdle()` or `device.swapchain(w)` threw, which would
    point at a real device/driver-level problem worth its own investigation
    rather than a silent hang/crash.
  - That rendering visibly resumes (menu or in-game frame appears) rather
    than the app sitting on a black/frozen frame after resume — a black
    frame with no crash would suggest the new `swapchain` came up but
    `renderer.resetSwapchain()`/attachment recreation has its own issue.
- Multiple rapid Home/resume cycles, and backgrounding during a big
  blocking load (e.g. right after starting a new game), are untested code
  paths worth a device pass beyond the single background/resume test named
  in the brief.
- `Device::waitIdle()` waits on **all** device queues
  (`waitIdleSync`/`vkDeviceWaitIdle`, confirmed in `vdevice.cpp`), a
  stronger guarantee than `VSwapchain::cleanupSwapchain()`'s own internal
  `presentQueue->waitIdle()` — deliberately used instead of/in addition to
  that for the safety-critical background/resume path.

## M2 (deferred, not part of M1 — captured for later milestones)

- Virtual on-screen gamepad / touch movement controls.
- Real controller + haptics support via the Game Controller Library.
- Radial/quick-select rings adapted for touch.
- World-load memory tuning for 4 GB devices.
- `android/DEVELOPMENT.md`, `android/README-android.md` (Task 6 docs).
