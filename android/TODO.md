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

## M2 (deferred, not part of M1 — captured for later milestones)

- Virtual on-screen gamepad / touch movement controls.
- Real controller + haptics support via the Game Controller Library.
- Radial/quick-select rings adapted for touch.
- World-load memory tuning for 4 GB devices.
- `android/DEVELOPMENT.md`, `android/README-android.md` (Task 6 docs).
