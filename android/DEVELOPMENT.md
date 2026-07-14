# Android port — architecture & maintainer notes

This is the maintainer reference for the Android port — the analogue of
[`ios/CONTROLLER-TECHNICAL.md`](../ios/CONTROLLER-TECHNICAL.md) on the iOS side.
End-user install instructions live in [`README-android.md`](README-android.md);
task-by-task implementation status and the full M2 backlog live in
[`TODO.md`](TODO.md). The original design and plan documents are
[`docs/superpowers/specs/2026-07-14-android-port-design.md`](../docs/superpowers/specs/2026-07-14-android-port-design.md)
and [`docs/superpowers/plans/2026-07-14-android-port-m1.md`](../docs/superpowers/plans/2026-07-14-android-port-m1.md).

## Status (M1, complete)

On the `android` branch, OpenGothic **boots and runs Gothic II: Night of the
Raven**. Confirmed on an **x86_64 desktop emulator** (AVD `emulator-5554`,
Pixel Tablet profile, API 35), driven directly via `adb` — not yet on real
arm64 hardware:

- The engine boots, loads VDFs from device storage, and renders the full main menu.
- Touch works: tapping the on-screen overlay reaches the engine as mouse clicks.
- "Nowa gra" (New Game) loads a world and plays the in-engine intro cutscene.
- Backgrounding (Home) and returning to the app no longer crashes; rendering
  resumes correctly.

This is well past the original M1 goal ("boot to menu"). What is *not* yet
done — most importantly, validation on real hardware under real memory
pressure — is tracked in [`TODO.md`](TODO.md)'s M2 section and summarized at
the bottom of this document.

## Architecture

`android/` is a Gradle + NDK project that drives the existing repo-root
`CMakeLists.txt` via Gradle's `externalNativeBuild` (`android/app/build.gradle`
points `cmake.path` at `../../CMakeLists.txt`) rather than maintaining a
separate build definition — the same engine source tree builds for Windows,
Linux, macOS, iOS, and now Android.

- **Framework:** [GameActivity](https://developer.android.com/games/agdk/game-activity)
  (`androidx.games:games-activity:3.0.5`), Google's native-activity successor
  built on `android_native_app_glue`. Requires `buildFeatures { prefab true }`
  in `app/build.gradle` so CMake's `find_package(game-activity REQUIRED
  CONFIG)` can see the AAR's exported native headers/libs (gotcha #1 below).
- **IDs:** package/applicationId `opengothic.gothic2`; native library
  `Gothic2Notr` (root `CMakeLists.txt`'s `project(Gothic2Notr ...)`, matched by
  `android:value="Gothic2Notr"` in the manifest's `android.app.lib_name`
  meta-data and `System.loadLibrary("Gothic2Notr")` in `MainActivity.kt`).
- **ABIs:** `arm64-v8a` (real devices — `app/build.gradle`'s `abiFilters`
  comment names the Helio G99, a budget MediaTek chipset, as an example
  target) and `x86_64` (added in `f31974ed` purely so the port could be
  iterated on a fast desktop emulator instead of real hardware).
- **CMake:** root `CMakeLists.txt` gains `if(ANDROID) add_library(SHARED)…`
  (overriding the desktop build's `add_executable`), an `elseif(ANDROID)`
  branch calling `find_package(game-activity REQUIRED CONFIG)` and adding
  `lib/Tempest/Engine` (not just its public `include/`) to the include path,
  and a link step that pulls in `game-activity::game-activity_static` under
  `-Wl,--whole-archive`, plus `android` and `log`. Every pre-existing
  `if(UNIX)`/`elseif(UNIX)` branch in the file was audited and guarded, since
  CMake sets `UNIX` true for Android too.
- **Tempest backend:** a new `AndroidApi` (`SystemApi` implementation)
  providing window/lifecycle/input on `game-activity`'s native_app_glue, plus
  a `VK_KHR_android_surface` swapchain path in
  `vulkanapi.cpp`/`vswapchain.cpp`. All of this is authored **inside
  [`android/patches/apply-patches.sh`](patches/apply-patches.sh)**, never
  committed directly to the `lib/Tempest` submodule (see "Submodule patching"
  below).
- **Entry point:** [`game/main_android.cpp`](../game/main_android.cpp)'s
  `android_main(android_app*)`, the Android analogue of `game/main.cpp`'s
  `main()`. It sets the `AndroidApi` app handle, `chdir("/sdcard/OpenGothic")`,
  synthesizes `CommandLine{"opengothic","-g","/sdcard/OpenGothic/Gothic2"}`,
  then runs the same bootstrap as `main.cpp`'s mobile path (log setup,
  `CrashLog::setup()`, zenkit/dmusic logger callbacks, `VulkanApi` → `Device`
  → `Resources` → `Gothic` → `GameMusic` → `MainWindow` →
  `Tempest::Application::exec()`), wrapped in the same
  `catch(GothicNotFoundException&)` / `catch(std::exception&)` pair
  `main.cpp` uses.

### Submodule patching

`lib/Tempest` is a third-party submodule and is never modified in git
directly. `android/patches/apply-patches.sh` (mirroring
`ios/patches/apply-patches.sh`) applies grep-guarded, idempotent perl edits to
`systemapi.cpp` (dispatch to `AndroidApi` under `__ANDROID__`),
`vulkanapi.cpp` (`VK_KHR_ANDROID_SURFACE_EXTENSION_NAME`), `vswapchain.cpp`
(`VK_USE_PLATFORM_ANDROID_KHR` surface creation/present-support), and
Tempest's own `CMakeLists.txt` (Android Vulkan lib + `game-activity` link),
then heredoc-writes the wholly-new `Engine/system/api/androidapi.h`/`.cpp`
files. CI runs it fresh on every build (`.github/workflows/android.yml`'s
"Apply submodule patches" step); running it twice locally is a no-op the
second time — verified during development by diffing the submodule tree
before/after a repeat run.

## Event loop and lifecycle design

This is the part with no iOS precedent — iOS's `UIApplication` main loop and
`CADisplayLink` don't map onto `android_native_app_glue`'s command-queue
model — so it's worth walking through in detail.

**Boot ordering invariant.** `android_main` isn't handed a window directly —
`ANativeWindow*` only becomes valid once `game-activity` delivers
`APP_CMD_INIT_WINDOW` — but the engine's normal boot sequence constructs a
`Window` (and its `Swapchain`) synchronously and expects a valid handle
immediately. `AndroidApi`'s constructor resolves this by **blocking**,
pumping `ALooper_pollAll` until `app->window != nullptr`, before returning.
Because `AndroidApi` is a lazily-constructed singleton (`SystemApi::inst()`),
this means `Tempest::AndroidApi::setAndroidApp(app)` **must** be called before
the first `Tempest::Window` is constructed anywhere — `main_android.cpp`
calls it as the very first line of `android_main`, with a comment warning
against reordering it after `MainWindow`.

**Per-frame pump (`AndroidApi::implProcessEvents`).** Each frame:
1. Drains `game-activity`'s command queue (`android_app::cmdPollSource` /
   `onAppCmd`).
2. Drains the per-frame motion-event buffer
   (`android_app_swap_input_buffers` → `GameActivityMotionEvent`), mapping
   `AMOTION_EVENT_ACTION_DOWN/UP/MOVE` (and `CANCEL`, treated as `UP`) for the
   **primary pointer only** to `Tempest::MouseEvent`s dispatched via
   `dispatchMouseDown/Up/Move`. No multi-touch, no coordinate scaling (unlike
   iOS's `contentScaleFactor` multiply — `GameActivityPointerAxes_getX/Y`
   already reports surface pixels). The event shape and ordering were copied
   from `iosapi.mm`'s `touchesBegan/Moved/Ended` for consistency across
   platforms.
3. Compares `g_app->window` against a remembered `g_lastWindow` to decide
   between the two recovery paths below.

**Surface loss on background/resume — the hardest bug of M1.** Backgrounding
(Home) survived from Task 1 onward, but returning to the foreground reliably
SIGSEGV'd until Task 5. Root cause: `APP_CMD_TERM_WINDOW` destroys the
`ANativeWindow`, but `VSwapchain` (built from that window's `VkSurfaceKHR`)
was never torn down or rebuilt; `APP_CMD_INIT_WINDOW` on resume hands back a
**different** `ANativeWindow*`, and the next frame rendered against the
stale, freed surface. Confirmed by reading `vswapchain.cpp`:
`VSwapchain::reset()` only calls `cleanupSwapchain()` + `createSwapchain()` —
it rebuilds swapchain images against the *existing* `surface` member, which
is private and set once, in the constructor. It cannot recover a changed
window handle.

The fix (commit `e71fd931`) reuses machinery that already existed for the
*initial* boot swapchain — `Tempest::Device::swapchain(SystemApi::Window*)` —
to do a **full** `Swapchain` reconstruction (new `VkSurfaceKHR` and all),
rather than teaching `vswapchain.cpp` to self-heal. This needed **zero**
changes to `vswapchain.cpp`/`vulkanapi.cpp`/`device.cpp`. Two new hooks:

- `AndroidApi::setSurfaceCallbacks(onSurfaceDestroyed, onSurfaceCreated)`,
  registered once from `main_android.cpp` right after `MainWindow wx(device)`
  is constructed.
- `MainWindow::onSurfaceDestroyed()` — `device.waitIdle()`, draining
  in-flight GPU work before the window dies.
- `MainWindow::onSurfaceCreated(SystemApi::Window* w)` — `device.waitIdle();
  swapchain = device.swapchain(w); renderer.resetSwapchain();` plus a camera
  viewport update — the same recovery shape the pre-existing
  `catch(SwapchainSuboptimal&)` path in `MainWindow::tick()` already used,
  just with a full `Swapchain` replacement instead of `.reset()`.

The `onSurfaceDestroyed` call happens **synchronously inside the
`APP_CMD_TERM_WINDOW` handler**, not deferred to the next
`implProcessEvents` tick like the ordinary resize path. This matters:
`android_native_app_glue`'s `TERM_WINDOW` post-exec (which nulls
`android_app::window` and signals a condvar) runs only *after* the app's
`onAppCmd` callback returns, and the platform's `onNativeWindowDestroyed`
(UI thread) blocks on that condvar before actually releasing the
`ANativeWindow`. Deferring the GPU-drain would race the OS's real teardown of
the window's buffer queue — exactly what the logcat line `Surface:
freeAllBuffers: N buffers were freed while being dequeued!` describes. A
genuinely **different** window (vs. a same-window resize, e.g. rotation) is
detected by comparing `g_app->window` to `g_lastWindow` in
`implProcessEvents`, so ordinary resizes keep using the original, cheap
`dispatchResize → swapchain.reset()` path, unchanged.

Both callbacks are wrapped in `try/catch(...)`, logged via `Tempest::Log::e`
(tag `app` in logcat), so a C++ exception can never unwind across the glue's
C callback boundary (`handleAppCmd`), which would otherwise be undefined
behavior.

Deliberately **not** hardened: `Tempest::SoundDevice` construction (the
"audio-init try/catch guard"). It's a member nested inside `Resources`'s
member-initializer list — not something `main_android.cpp` constructs
directly — and five other call sites construct their own `SoundDevice` too.
Guarding it properly means changing shared, cross-platform submodule code;
skipped as out of scope since audio already works on-device (the emulator's
audio HAL chokes harmlessly; the engine's PCM output is fine). Any exception
on this path today is still caught by the outer bootstrap
`catch(std::exception&)` in `main_android.cpp` — not a silent crash, just not
finely diagnosed.

## Data root and storage permission

Game data lives at **`/sdcard/OpenGothic/Gothic2/`** (`Data/`, `_work/`,
`system/`), with saves and `Gothic.ini` one level up at
`/sdcard/OpenGothic/`. This needs `MANAGE_EXTERNAL_STORAGE` ("All files
access"), requested via the manifest permission plus
`android:requestLegacyExternalStorage="true"`. `MainActivity.onCreate()`
calls `ensureAllFilesAccess()`, which redirects to
`Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION` (falling back to the
generic `ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION` if that intent isn't
resolvable) when the permission isn't already granted. M1 assumes the grant
happens before `android_main` runs; there is no in-native polling/gating for
a mid-session grant, only the Settings redirect-then-relaunch flow.

`main_android.cpp` does `::chdir("/sdcard/OpenGothic")` early (before the
log-file and engine bootstrap) so relative paths — `log.txt`, `Gothic.ini`,
and saves — resolve under a writable, known directory instead of whatever CWD
the native-glue thread starts with. Game data itself is addressed by the
absolute `-g /sdcard/OpenGothic/Gothic2` argument, independent of the
`chdir`.

**`Gothic.ini`:** unlike iOS (`game/gothic.cpp`'s `#if defined(__IOS__)`
branch, which auto-writes a starter profile with iOS-tuned defaults on first
run), Android takes the plain desktop/Linux code path — it looks for
`Gothic.ini` in the current directory and falls back to built-in defaults if
absent, exactly like a fresh Windows/Linux install. M1 observed "no
Gothic.ini - using default settings" at the main menu, which is expected and
non-blocking (no gameplay-blocking issue was seen). Whether changed options
actually flush to, and reload correctly from, `/sdcard/OpenGothic/Gothic.ini`
across a restart has not been verified on-device — tracked in `TODO.md`.

## Gotchas (each a real CI or runtime failure, now fixed)

1. **`find_package(game-activity)` needs `buildFeatures { prefab true }`.**
   Without it, CMake can't see the GameActivity AAR's exported CMake package.
   Fixed in `973ba151`.
2. **`crashlog.cpp`'s `backtrace()` guard.** Android is Linux (`__LINUX__` is
   defined there too), but Bionic only declares `backtrace()` /
   `backtrace_symbols()` starting at API 33 — this project's `minSdk` is 26.
   Guarded with `(defined(__LINUX__) || defined(__APPLE__)) &&
   !defined(__ANDROID__)`. Fixed in `5de98570`.
3. **GameActivity's JNI entry points get stripped in Release builds.**
   `--gc-sections` removes `Java_..._initializeNativeCode` and friends
   because nothing in native code calls them directly (only the JVM does, via
   JNI) — this compiles and links fine, then throws `UnsatisfiedLinkError` at
   startup, on-device only. Green CI does **not** mean it runs. Fixed by
   linking `game-activity::game-activity_static` under
   `-Wl,--whole-archive ... -Wl,--no-whole-archive` in `5baf4940`.
4. **`Swapchain::reset()` doesn't rebuild the `VkSurfaceKHR`.** See "Surface
   loss on background/resume" above; fixed in `e71fd931`.

## CI and distribution

[`.github/workflows/android.yml`](../.github/workflows/android.yml) runs on
`ubuntu-latest` on every push to `android` (skipping `**.md` / `docs/**` via
`paths-ignore`, so a doc-only commit like this one does not trigger a build):
checkout with submodules → JDK 17 → Android SDK → `glslang-tools` (host
shader compiler) → `bash android/patches/apply-patches.sh` →
`./gradlew assembleRelease -PbuildNum=<run number>` → locate the signed APK →
publish it to the `latest-android` GitHub Release → upload it as a build
artifact.

Signing uses a **committed** PKCS12 keystore,
`android/keystore/opengothic.jks` (added in `0aef8bbd` via `openssl`, since no
local JDK/`keytool` was available when it was generated) — a stable signature
across builds without needing CI secrets.

**The `latest-android` release is deliberately marked work-in-progress.**
`dc4d41db` changed the release-publish step to a GitHub prerelease titled
*"OpenGothic Android — WIP test build (do not install)"*, with notes stating
the build is "incomplete and not ready for players" and asking people not to
file issues against it yet. Keep this notice intact until the port is
further along — `README-android.md` mirrors it.

## Verification model

No NDK/Gradle/CMake toolchain is available on the Windows machine this port
was written on, so every task was implemented against the existing iOS
backend and Tempest's public interfaces, then verified by pushing to
`android` and either reading CI logs or driving an actual running instance.
Two real bugs (the GameActivity JNI stripping and the resume SIGSEGV) only
showed up on a **running emulator**, not in the CI build log — green CI means
"it compiled and linked", not "it runs". All on-device confirmation so far is
on an **x86_64 emulator** (`emulator-5554`, Pixel Tablet AVD profile, API 35)
driven directly via `adb`; no physical device and no arm64 target have been
exercised yet.

## Commit timeline

Chronological, oldest first (`git log --oneline` on `android`):

| Commit | Subject |
|---|---|
| `8e85f570` | docs(android): M1 boot-to-menu port design |
| `aaf6dc6b` | docs(android): M1 boot-to-menu implementation plan |
| `7cf1574b` | feat(android): first-light scaffolding — Gradle+GameActivity, AndroidApi backend, VK android surface, CI, clear-screen APK |
| `0aef8bbd` | build(android): add PKCS12 signing keystore (openssl, no local JDK) |
| `973ba151` | fix(android): enable prefab so find_package(game-activity) resolves |
| `5de98570` | fix(android): exclude Android from execinfo backtrace path |
| `f31974ed` | build(android): also build x86_64 for desktop emulator testing |
| `5baf4940` | fix(android): keep GameActivity JNI entry points (--whole-archive) |
| `4cdaba69` | feat(android): boot the real OpenGothic engine on the glue thread |
| `9932e342` | feat(android): point engine at /sdcard/OpenGothic/Gothic2 via -g + chdir |
| `e3ab6dab` | feat(android): touch input — tap dispatched as mouse click for menu nav |
| `e71fd931` | fix(android): recreate Vulkan surface on background/resume to stop SIGSEGV |
| `dc4d41db` | chore(android): mark latest-android release as WIP / do-not-install |

`7cf1574b` / `0aef8bbd` / `973ba151` land after a rebase of `android` onto an
updated `master` (this machine's local `master` had drifted 63 commits behind
the fork's `origin/master`); the `lib/Tempest` submodule pointer was
identical before and after, so the patch script's anchors stayed valid.

## Building locally

There is no local Android toolchain on the machine this port was developed
on, so this has only been exercised through CI. For a maintainer with a real
Android SDK/NDK + JDK 17 + `glslangValidator` set up,
[`build-android.sh`](build-android.sh) mirrors the CI job exactly:

```bash
bash android/patches/apply-patches.sh   # patch lib/Tempest (idempotent)
bash android/build-android.sh [buildNum]
```

This produces a signed `*-release.apk` under `android/`, using the committed
keystore.

## Remaining / deferred work

Tracked in detail in [`TODO.md`](TODO.md)'s M2 section. Headline items:

- **Validate on real arm64 hardware under real memory pressure** (~4 GB RAM
  class devices) — the actual point of this port, and the least-tested part
  of it: only the menu and intro cutscene have been reached so far, on a
  desktop emulator.
- A tuned, Android-specific on-screen virtual gamepad (today's overlay is the
  shared iOS `mobileUi` widget — functional for menu taps, not tuned for
  Android).
- Real controllers + haptics via the Android Game Controller Library.
- Radial quick-select rings and target lock-on adapted for Android.
- `GameTextInput` (or equivalent) for save-name entry.
- `Gothic.ini` persistence verification (see "Data root" above).
- The audio-init try/catch guard (see "Event loop" above).

---

*For the engine itself, see the [root README](../README.md) and the upstream
[Try/OpenGothic](https://github.com/Try/OpenGothic) project.*
