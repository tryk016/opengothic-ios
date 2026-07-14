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

## M2 (deferred, not part of M1 — captured for later milestones)

- Virtual on-screen gamepad / touch movement controls.
- Real controller + haptics support via the Game Controller Library.
- Radial/quick-select rings adapted for touch.
- World-load memory tuning for 4 GB devices.
- `android/DEVELOPMENT.md`, `android/README-android.md` (Task 6 docs).
