# OpenGothic Android Port — Design (M1: boot-to-menu)

**Date:** 2026-07-14
**Branch:** `android`
**Status:** Approved (design phase)

## Goal

Add an Android build of OpenGothic (Gothic II: Night of the Raven) as a second
mobile target alongside the existing iOS port, developed entirely on the
`android` branch so `master`/iOS stays untouched. The engine core (Vulkan
renderer, script VM, physics, audio decoders) is shared; only the platform
window/lifecycle/input backend, the Vulkan surface path, the build system, and
distribution are new.

**Milestone 1 (this spec):** engine boots on Android, creates a Vulkan surface
on `ANativeWindow`, renders the **main menu**, and accepts basic touch (tap =
click/select) to navigate it. This proves the shared core runs on the target
hardware. Everything richer (virtual gamepad, real controllers, radial rings,
lock-on) is deferred to later milestones.

**Target test device:** Mediatek Helio G99, Mali-G57 MC2 (Vulkan 1.1+), 4 GB
RAM, arm64. Verdict: viable. GPU is ample for a 2003 game; 4 GB is the real
constraint (world load will be borderline — an M2 concern, not M1).

## Non-goals (deferred to M2+)

- Full on-screen virtual gamepad (iOS UX logic is portable C++; the `.mm`
  system bridges are rewritten as Android/C++).
- Real controllers via the Android Game Controller Library + haptics + button
  glyph/layout detection.
- Radial rings, lock-on overlays, focus UX (logic already exists, ported after M1).
- Save-name text input via GameTextInput (reuse existing cross-platform
  auto-naming for now).
- Loading a world / entering gameplay (M2 — where the 4 GB limit bites).

## Key decisions

1. **App framework: GameActivity** (Android Game Development Kit / Jetpack),
   not NativeActivity. Rationale: NativeActivity is frozen/legacy; GameActivity
   is Google's recommended path, renders into a `SurfaceView`, has cleaner input
   (`android_input_buffer`), `GameTextInput`, and — decisively — works with the
   **Game Controller Library** which provides gamepad layout/label detection and
   haptics out of the box. We already know the roadmap needs controllers and a
   rich input layer (built on iOS), so GameActivity's one-time Gradle/AAR setup
   cost is worth avoiding a later costly migration.

2. **Branch: `android`**. New `android.yml` workflow triggers on push to
   `android` (+ `workflow_dispatch`). Merge to `master` only once stable so both
   mobile workflows build from one branch. iOS `.mm` sources are already gated by
   `if(IOS)` in CMake, so the two targets don't collide.

3. **Distribution: signed APK via GitHub Release.** Android sideloading is
   trivial vs iOS — a signed APK installs directly, never expires, no SideStore /
   Apple ID / 7-day cert. **Stable keystore committed to the repo**
   (`android/keystore/opengothic.jks`) so every CI build shares one signature →
   in-place updates keep app + data. (A GitHub-Secret keystore is the "cleaner"
   alternative for later; a committed key is fine for a personal sideload port —
   it protects nothing valuable.)

4. **Game data location: public `/sdcard/OpenGothic/`** via
   `MANAGE_EXTERNAL_STORAGE` (All files access). Data (`Gothic2/`), `Gothic.ini`,
   and saves live there. Copy once — survives updates AND full
   uninstall/reinstall. Cost: one "All files access" grant on first run. Chosen
   over app-specific storage (zero-permission but wiped on uninstall and buried)
   to eliminate the user's stated pain of recopying the game directory.

5. **ABI: `arm64-v8a` only.** Smaller APK, faster builds; target device is arm64.

## Architecture

### New `android/` directory (mirrors `ios/`)

```
android/
  app/
    build.gradle              # NDK + externalNativeBuild(CMake) + GameActivity AAR
    src/main/
      AndroidManifest.xml     # GameActivity, landscape lock, Vulkan feature,
                              #   MANAGE_EXTERNAL_STORAGE
      java/.../MainActivity.kt# minimal: subclass GameActivity (a few lines)
  keystore/opengothic.jks     # committed stable signing key
  patches/apply-patches.sh    # Tempest patches (android surface, etc.), like ios/
  build-android.sh            # local/CI build reference
  DEVELOPMENT.md
  TODO.md
  README-android.md
```

`app/build.gradle` points `externalNativeBuild` at the **existing top-level
`CMakeLists.txt`** — the same file desktop/iOS use — with a new
`elseif(ANDROID)` branch added. Gradle supplies the NDK toolchain, ABI, and
`ANDROID_PLATFORM`.

### New engine code (Tempest, via `apply-patches.sh` — submodule stays "dirty")

- **`system/api/androidapi.{h,cpp}`** — new `AndroidApi : SystemApi` backend on
  `game-activity/native_app_glue`: lifecycle (create / window-focus / pause /
  resume / window-destroyed), acquire `ANativeWindow*`, event pump, touch →
  `MouseEvent`/touch dispatch, **basic surface loss/recreate** on background.
- **`system/systemapi.cpp`** — add `#elif defined(__ANDROID__) static AndroidApi
  api;` in `SystemApi::inst()`.
- **`gapi/vulkan/vswapchain.cpp` + `vdevice.cpp`** — add `VK_KHR_android_surface`
  instance extension and a `vkCreateAndroidSurfaceKHR(ANativeWindow*)` path
  alongside the existing Win32/Xlib code.

### Storage / file access

- Request `MANAGE_EXTERNAL_STORAGE` on first launch (Kotlin prompt in
  `MainActivity`, or native intent).
- Point the engine's data root + working directory at `/sdcard/OpenGothic/`
  (game data under `Gothic2/`). The iOS "resolve relative paths against a data
  root" fix (RFile CWD-first) is the reference pattern; the Android root is the
  public folder instead of the app bundle.

### Build shaders

`glslang` runs as a host tool in the CMake step exactly as today; SPIR-V is
consumed unchanged by Android Vulkan.

## CI: `.github/workflows/android.yml`

Runs on `ubuntu-latest` (cheap; no macOS needed):

1. checkout with `submodules: recursive`
2. `bash android/patches/apply-patches.sh`
3. set up JDK 17 + Android SDK/NDK (`android-actions/setup-android`)
4. `./gradlew assembleRelease`
5. sign with the committed keystore (stable signature → in-place updates)
6. publish `OpenGothic.apk` to a rolling Release tagged `latest-android` +
   upload the artifact

Triggers: `push` to `android`, `workflow_dispatch`. Publishing needs
`permissions: contents: write` (as `ios.yml` already does).

## Top risks (call out honestly)

1. **Event-loop marriage** — Android's looper-driven, own-thread model vs
   Tempest's `SystemApi::exec(AppCallBack)` which wants to drive the loop
   itself. iOS solved the analogous problem with setjmp/longjmp fibers. This is
   the hardest part of M1 and where most time will go.
2. **Vulkan surface loss** on background — Android destroys the surface
   aggressively; the swapchain must be recreated. Landscape lock removes the
   rotation case; background/foreground still needs handling.
3. **4 GB RAM** — menu is fine; world load is borderline (M2 concern).
4. **Audio init** — OpenAL-soft on the Android backend (AAudio/OpenSL); guard
   init so a failure degrades gracefully instead of aborting boot.

## Workflow

Per the iOS working loop: implement a coherent cluster on Windows → update
`android/TODO.md` → commit (`feat(android): …` / `fix(android): …`) → push to
`origin android` → GitHub Actions builds the APK → user installs & tests
on-device. **Nothing compiles/runs on Windows**, so the CI build is the
verification step; changes are UNVERIFIED until CI is green.

## Success criteria for M1

- `android.yml` produces a signed `OpenGothic.apk` (CI green).
- APK installs on the Helio G99 device.
- App requests storage access, finds data in `/sdcard/OpenGothic/`, boots the
  engine, and renders the **main menu**.
- Tap navigates the menu (select / basic actions).
- Backgrounding and returning does not crash.
