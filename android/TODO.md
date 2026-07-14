# Android port — TODO / status

Tracks M1 (boot-to-menu) task-by-task status. See
`docs/superpowers/plans/2026-07-14-android-port-m1.md` for the full plan and
`.superpowers/sdd/progress.md` for the cross-task ledger.

**M1 status: COMPLETE**, and the port reached further than its "boot to
menu" goal. On an x86_64 emulator (`emulator-5554`, Pixel Tablet AVD profile,
API 35), Gothic II: NotR boots, renders the main menu, takes touch input,
starts a new game via "Nowa gra", plays the in-engine intro cutscene, and
survives backgrounding/resuming without crashing. See
[`DEVELOPMENT.md`](DEVELOPMENT.md) for the architecture, event-loop/lifecycle
design, the CI gotchas, and the full commit timeline; see
[`README-android.md`](README-android.md) for the (currently work-in-progress)
end-user install guide.

**Not yet done:** none of this has been confirmed on real arm64 hardware, or
under the ~4 GB RAM memory pressure that is the actual target device class —
see the M2 section at the bottom.

## Task 1: First light — scaffolding, CI, clear-screen APK

Status: **COMPLETE — device-confirmed.** Implemented, then verified via CI
green and, later, on an x86_64 emulator: the dark-red `#3a0000` clear screen
rendered through the new Vulkan/AndroidApi swapchain path. Fully superseded
by Tasks 2-5 below, which replaced the clear-screen stub with the real
engine.

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

Status: **COMPLETE — device-confirmed.** On the emulator, logcat showed
"OpenGothic v1.0 dev" boot, followed by a controlled `GothicNotFoundException`
("Gothic path is not provided... -g <path>") — no crash. Commit `4cdaba69`.

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

Status: **COMPLETE — device-confirmed.** On the emulator, the full Gothic
II: NotR main menu renders (VDFs mounted from `/sdcard/OpenGothic/Gothic2`,
Vulkan device on the host GPU, shaders compiled). Log showed "no Gothic.ini
- using default settings" — non-blocking; the RFile CWD-first patch was not
needed to reach the menu. Commit `9932e342`.

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

Status: **COMPLETE — device-confirmed, and further than planned.** Taps
reach the engine via `dispatchMouse*`; tapping the shared iOS `mobileUi`
overlay's confirm zone activated "Nowa gra", loaded a world, and played the
in-engine intro cutscene. Audio outputs correctly (the emulator's audio HAL
chokes harmlessly; the engine's PCM output is fine). Commit `e3ab6dab`.

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

Status: **COMPLETE — device-confirmed.** Backgrounding (Home) and resuming
the emulator no longer SIGSEGVs, in both the main menu and in-game; the
render loop stays alive and animating after resume (the intro title kept
rendering). Commit `e71fd931`. Also: `dc4d41db` marked the `latest-android`
GitHub Release WIP / do-not-install. With this task done, M1 is complete and
only Task 6 (docs) remained.

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

## Task 6: Documentation

Status: **COMPLETE.** `DEVELOPMENT.md`, `README-android.md` written/finalized,
this file's M1 statuses updated to reflect device confirmation, and the root
`README.md` now points at the Android build alongside the existing iOS entry.

## Post-M1: final whole-branch review — hardening fixes

Status: **COMPLETE.** The branch's final whole-branch review verdicted "Ready
to merge" with four Minor-hardening findings (no functional defects). All
four addressed; no behavior change expected on any currently-tested path.

- [x] **crashlog.cpp Android guard asymmetry.** The `std::cout` traceback
      dispatch in `CrashLog::dumpStack` was missing the `&& !defined(__ANDROID__)`
      guard its `fout`/crash.log twin already had. `tracebackLinux`'s own body
      is Android-guarded already, so this was harmless today — a symmetry fix
      against a latent foot-gun. Both dispatch sites now read
      `#elif (defined(__LINUX__) || defined(__APPLE__)) && !defined(__ANDROID__)`.
- [x] **Key-event buffer never cleared.** `implProcessEvents` drained
      `android_input_buffer` motion events and called
      `android_app_clear_motion_events(ib)`, but never cleared key events — so
      hardware key events (incl. Back) would accumulate in game-activity's
      fixed-size key-event buffer and get dropped once full. Added
      `android_app_clear_key_events(ib)` right after the existing
      motion-events clear (symbol verified against the real, upstream
      `android_native_app_glue.h`: `void android_app_clear_key_events(struct
      android_input_buffer* inputBuffer);` — same shape as
      `android_app_clear_motion_events`, matches exactly). Still not
      *processing* key events (out of scope, same as Task 4's touch-only
      scope) — just draining the buffer so it can't overflow.
- [x] **ABA hazard in resume/surface-change detection.** The window-changed
      check only compared `g_app->window != g_lastWindow` by pointer value —
      if the OS ever reused the same address for a new `ANativeWindow` after
      an `APP_CMD_TERM_WINDOW`, the comparison would read "unchanged", the
      cheap `dispatchResize`/`Swapchain::reset()` path would run against the
      dead surface, and Task 5's SIGSEGV fix would be reintroduced. Added
      `g_wasTerminated` (`std::atomic_bool`, same declaration pattern as
      `g_running`/`g_windowChanged`): set in `handleAppCmd`'s
      `APP_CMD_TERM_WINDOW` case right after `onSurfaceDestroyed()` runs;
      consumed via `g_wasTerminated.exchange(false)` into a local **before**
      the `g_app->window!=g_lastWindow` comparison in `implProcessEvents`
      (deliberately not inlined into the `||`, since short-circuit evaluation
      would otherwise skip the exchange whenever the pointer already
      differed, leaking the flag set into a later, genuinely-same-window
      resize tick). A window-changed tick now takes the full
      `onSurfaceCreated` recreate path if the pointer differs **or**
      `g_wasTerminated` was set, closing the ABA hole while a genuine
      same-window resize (no preceding `TERM_WINDOW`) still takes the cheap
      path, unchanged. `g_lastWindow`'s own update logic is untouched.
- [x] **vulkanapi.cpp patch-step guard granularity.** The two perl edits (the
      `VK_KHR_ANDROID_SURFACE_EXTENSION_NAME` macro define, and the
      `SURFACE_EXTENSION_NAME` `#elif defined(__ANDROID__)` dispatch branch)
      sat under one shared skip-guard/post-check that only re-verified the
      first macro — a partial apply (e.g. the second edit's context drifting
      upstream) could go undetected on every future run. Split into two
      independent guard+post-check blocks, one per edit, mirroring the
      per-sub-edit pattern `vswapchain.cpp`'s three steps (and
      `systemapi.cpp`'s two steps) already use elsewhere in this same script.

Verified: `bash android/patches/apply-patches.sh` run twice back-to-back —
first run patches all steps (including both halves of the now-split
vulkanapi.cpp guard), second run reports "skip: ... (already patched)" for
every step, confirming idempotency held. `lib/Tempest` reverted afterward to
its pre-existing dirty state (only `Engine/io/rfile.mm` and
`Engine/system/api/iosapi.mm` modified — both unrelated pre-existing local
changes, untouched by and unrelated to this pass).

## M2 — real-device test IN PROGRESS (2026-07-14)

First run on **real arm64 hardware**: Samsung Galaxy Tab A9 (SM-X115) —
Helio G99 / Mali-G57 MC2 / 3.5 GB usable / Android 15 (the exact target
class). **The port runs on the real device:** arm64 native code executes
(the emulator was x86_64), Vulkan initialises on the real Mali-G57
(`I/app: GPU = Mali-G57 MC2`), and the full "NOC KRUKA" main menu renders.
Menu footprint ≈ 470–490 MB PSS with ~1 GB free; under memory pressure the
low-memory-killer evicted *background* apps but spared the foreground game.

**Data layout confirmed mandatory:** `validateGothicPath()` needs `Data/`
**and** `_work/Data/` **and** `_work/Data/Scripts/_compiled/` — copying only
`Data/` yields `E/app: Invalid gothic path` and the engine exits (the process
then lingers as a ~28 MB husk on the home screen, so it looks like nothing
happened). The `_work/` tree (446 MB; the GOTHIC.DAT/MENU.DAT compiled scripts
plus Music + Video) is **not** optional and is **not** in the VDFs.
README-android.md already lists it — the one-time copy must include it.

Fixed on-device (committed):
- [x] **Screen doze mid-play** (`a0b08b7f`) — no wake-lock meant the 30 s
      screen-off timeout dozed the display during the input-less intro/load,
      destroying the Vulkan surface and stalling the engine on a black frame.
      `MainActivity` now holds `FLAG_KEEP_SCREEN_ON`.
- [x] **180° orientation flip** (`b28b9dcf`) — `sensorLandscape` let the
      surface transform flip (ROTATE_90↔ROTATE_270) after a doze/wake, which
      desynced touch input from the rendered frame (the mobileUi A/B buttons
      jumped to their mirror position and taps stopped landing). Manifest
      locked to fixed `landscape`.

Open — needs attention before touch is truly playable:
- [ ] **Content rendered rotated 90°** on portrait-native panels.
      `vswapchain.cpp:312` sets `preTransform = currentTransform` but the
      engine never pre-rotates, so on a portrait-native panel the game draws
      sideways inside the (correctly landscape) window — in every orientation
      (the emulator hid this: its transform is identity). Proper fix is Android
      pre-rotation: `preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR`
      **plus** swapping `imageExtent` when the transform is 90/270 — note
      `findSwapExtent` returns `capabilities.currentExtent` (portrait
      800×1340), so a naive one-line `preTransform` change risks an infinite
      recreate/SUBOPTIMAL loop (hang) — **plus** transforming input
      coordinates to match. This is a shared-Tempest change (via
      apply-patches) that needs real CI + on-device iteration; it is the top
      blocker for real touch playability and was deliberately **not** attempted
      autonomously.
- [ ] **World-load-under-3.5 GB memory test still pending** — only the menu
      (~490 MB, ~1 GB free) has been measured; a full new-game world load
      under real memory pressure was blocked by the orientation/input bug above
      and is the next thing to finish once these fixes are on-device.

## M2 (deferred, not part of M1 — captured for later milestones)

M1 proved the engine boots, renders, takes touch input, and survives
backgrounding — on an **x86_64 desktop emulator**. None of the items below
were in scope for "boot to menu, survive resume", and none have been
started:

- **Real low-RAM/arm64 hardware validation — the actual point of the port.**
  M1 was only run on a desktop-class x86_64 emulator. The production target
  is budget arm64 phones around 4 GB RAM (see the `Helio G99` comment in
  `android/app/build.gradle`'s `abiFilters`); world-load memory behavior
  under real memory pressure is completely unvalidated — only the main menu
  and the intro cutscene have been reached so far, not sustained gameplay.
- Full on-screen virtual gamepad, tuned for Android (the current shared iOS
  `mobileUi` overlay works for menu confirm/cancel taps but was neither
  designed nor tuned for Android touch/screen sizes).
- Real controllers + haptics via the Android Game Controller Library (the
  iOS port's `GCExtendedGamepad`-based pipeline in `game/utils/gamepad.mm`
  has no Android equivalent yet).
- Radial/quick-select rings (`game/ui/quickring.*`) and target lock-on
  adapted for Android touch/controller input.
- `GameTextInput` (or equivalent) for on-screen save-name entry.
- Verify `Gothic.ini` persistence on Android: the engine takes the same code
  path as desktop/Linux (not iOS's auto-populated profile — see
  `game/gothic.cpp`'s `#if defined(__IOS__)` branch), reading/writing
  `Gothic.ini` from the `chdir`-ed `/sdcard/OpenGothic/` working directory.
  M1 only observed "no Gothic.ini - using default settings" at the menu;
  whether in-game option changes actually flush and reload correctly across
  a restart has not been tested.
- Audio-init try/catch guard around `Tempest::SoundDevice` construction
  (skipped in Task 5 as scope-creep — audio already works; this would only
  harden a currently-unobserved failure mode).
- Storage-permission gating/polling before bootstrap (noted since Task 3):
  M1 assumes `MANAGE_EXTERNAL_STORAGE` is already granted at first launch via
  `MainActivity.ensureAllFilesAccess()`'s Settings redirect, with no in-native
  polling for a mid-session grant.
