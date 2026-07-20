# Android port — architecture and maintainer notes

This document describes the current Android implementation. End-user
installation instructions are in [`README-android.md`](README-android.md).
Historical designs, plans and measured investigations are under
[`docs/superpowers/`](../docs/superpowers/).

## Repository and worktree rules

The Android port shares one repository and remote with the iOS/upstream
branch, but development is isolated in a separate worktree:

| Path | Purpose |
|---|---|
| `E:\claude\opengothic-android` | Working tree for branch `android`; all Android edits happen here |
| `E:\claude\opengothic\OpenGothic` | Main checkout on `master`; do not edit during Android work |
| `E:\claude\opengothic\OpenGothic\lib\Tempest` | Tempest source used for reading and patch dry-runs |

Tempest is a submodule and may be intentionally dirty in the main checkout.
Never commit it. Android-specific Tempest changes belong in
[`android/patches/apply-patches.sh`](patches/apply-patches.sh), which CI
applies to a clean submodule checkout.

Push Android work only to `origin/android`. Do not push Android commits to
`master`.

The Android branch is an independent product line. Compatibility with the iOS
branch is not a release requirement: Android-specific implementations are
allowed when they are the safer or faster route. Reusing platform-neutral code
is welcome, but must not block an Android fix or force changes into `master`.

## Current status

Code baseline audited on 2026-07-20 after the Helio performance pass:

- Android code HEAD: `0e1f0a6b`;
- `master`: `97f7db4e`, an ancestor of `android` with no master-only commits;
- Android contains `master` plus its Android-only history;
- `latest-android` remains a WIP prerelease;
- full production shaders are restored;
- there are no uncommitted `TEMP_BISECT_*` modules or Tempest submodule
  changes in this worktree.

Final safety build [CI run 29746062340](https://github.com/tryk016/opengothic-ios/actions/runs/29746062340)
completed successfully, validated all 84 generated shader modules and moved
`latest-android` to `0e1f0a6b`. The published APK has SHA-256
`CBC5DBAF822467C66855682CC5EC2B3453AB529AFDC278C5C016BDC54D7D4365`.
That exact artifact was installed on the SM-X115 and verified through menu,
new-game world entry and Home/resume with the same process ID.

### Hardware matrix

| Device | GPU | Verified result |
|---|---|---|
| Samsung Galaxy Tab A9 / SM-X115, Android 15, ~3.5 GB usable RAM | Mali-G57 MC2 | Full Khorinis load, landscape presentation, touch, dialogue, background/resume and ASTC cache; about 15–16 FPS in the measured Xardas-room scene |
| Samsung Galaxy A23 5G / SM-A236B, Android 14, ~3.4 GB usable RAM | Adreno 619, Samsung driver 512.548.0 | Deterministic SIGSEGV in Qualcomm `libllvm-glnext.so` while compiling the first textured 3D graphics pipeline |
| Pixel Tablet AVD / API 35 | host Vulkan, x86_64 | Boot, menu, intro, touch and lifecycle smoke tests |

The Adreno failure is a confirmed shader-compiler crash. The scalar slot path,
VSM capability gate, stripped Release SPIR-V and several minimal shader
variants were tested without reaching the first 3D frame. The final
depth-only variant passed HiZ and moved the crash to the next material
pipeline, so this is not a single-HiZ-shader failure. Full shaders are
restored and the A23 investigation is deferred. See
[`2026-07-17-adreno-compiler-crash-investigation.md`](../docs/superpowers/reports/2026-07-17-adreno-compiler-crash-investigation.md).

## Build architecture

`android/` is a Gradle + NDK application that drives the repository's root
`CMakeLists.txt` through `externalNativeBuild`.

- Android package: `opengothic.gothic2`
- Native library: `Gothic2Notr`
- Minimum Android version: API 26
- Compile/target SDK: 34
- NDK: `26.3.11579264`
- ABIs: `arm64-v8a`, `x86_64`
- Java/Kotlin target: 17
- Game framework: `androidx.games:games-activity:3.0.5`
- Renderer: Tempest Vulkan with `VK_KHR_android_surface`

`GameActivity` requires Gradle Prefab support so CMake can resolve
`game-activity`'s native package. The static GameActivity library is linked
under `--whole-archive`; otherwise Release linker garbage collection removes
JNI entry points that are referenced only by the JVM.

The Android entry point is
[`game/main_android.cpp`](../game/main_android.cpp). It:

1. calls `Tempest::AndroidApi::setAndroidApp()` before any window can be
   created;
2. changes the working directory to `/sdcard/OpenGothic`;
3. synthesizes `-g /sdcard/OpenGothic/Gothic2`;
4. runs the same engine bootstrap used by the other platforms.

## Tempest patching

[`android/patches/apply-patches.sh`](patches/apply-patches.sh) is
grep-guarded, idempotent and fails when an expected upstream anchor changes.
It currently provides:

- the `AndroidApi` window, lifecycle and input backend;
- Android Vulkan instance/surface/present support;
- landscape `preTransform` handling;
- Android acquire/present handling for the deliberate transform mismatch;
- GameActivity and Android library linkage;
- ASTC 4×4 plumbing in `TextureFormat`, `Pixmap` and Vulkan;
- a raw compressed `Pixmap` constructor used by the ASTC loader;
- descriptor-indexing feature discovery promoted into Vulkan 1.2;
- lifecycle protection against `ANativeWindow*` address reuse.

Run the script against a clean Tempest checkout when testing patch drift.
Running it twice should leave the second invocation as a no-op. Never stage
the patched submodule.

## Window, lifecycle and landscape

`AndroidApi` waits for `APP_CMD_INIT_WINDOW` before returning the first
window. Each engine frame drains native app commands and the GameActivity
input buffers.

Surface teardown is synchronous:

- `APP_CMD_TERM_WINDOW` calls `MainWindow::onSurfaceDestroyed()` and waits
  for the Vulkan device to become idle before Android releases the native
  window;
- `APP_CMD_INIT_WINDOW` causes a complete `Swapchain` replacement, including
  a new `VkSurfaceKHR`;
- `g_wasTerminated` forces the complete path even if Android reuses the same
  `ANativeWindow*` address, avoiding an ABA-style stale-surface bug.

The manifest fixes the Activity to landscape. Portrait-native panels report
a rotated surface transform even though the logical window extent is already
landscape. The Android swapchain therefore requests
`VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR` and lets SurfaceFlinger rotate the
frame. This deliberately produces persistent `VK_SUBOPTIMAL_KHR` on the
tested device; Android maps that known result to success while true
out-of-date and lifecycle events still rebuild the surface.

This broad Android-wide `SUBOPTIMAL` handling is a known risk: it should
eventually be limited to the deliberate transform-mismatch case.

Background/resume is verified. Full Activity destroy/recreate in the same
process is not hardened: global AndroidApi state and callbacks are not fully
reset for a second native Activity lifetime.

## Input

Touch events are translated to Tempest mouse events and are sufficient for
menu selection and basic gameplay. Current limitations:

- only `pointers[0]` is processed;
- `ACTION_POINTER_DOWN/UP` is not implemented;
- hardware key events, including Back, are drained but not mapped;
- controllers and Android haptics are not implemented.

Consequently, the current overlay is not yet a complete multitouch virtual
gamepad.

## Storage and writable profile

Required game data:

```text
/sdcard/OpenGothic/Gothic2/
├── Data/
├── _work/
└── system/
```

The port currently requests `MANAGE_EXTERNAL_STORAGE`. The
`requestLegacyExternalStorage` manifest flag is not sufficient on modern
Android; the all-files permission is the actual access mechanism.

There is a first-run race: `GameActivity` may start the native thread before
the settings screen grants permission. After granting access, the user may
need to close and relaunch the app. Missing data currently produces a log
message and native-thread exit, not a visible Android dialog.

The engine uses `/sdcard/OpenGothic` as its writable working directory. On a
fresh successful launch, the shared mobile profile in
[`game/gothic.cpp`](../game/gothic.cpp) creates `Gothic.ini` with:

- `vidResIndex=2` (half-resolution render);
- `zCloudShadowScale=0` (SSAO disabled);
- 512 px shadow maps on Android;
- mobile gamepad defaults.

The copied `system/Gothic.ini` is not modified. A pre-existing writable
`Gothic.ini` is preserved and does not necessarily receive every newer
Android default.

`zMaxFpsMode=1` is active on Android and targets 30 FPS. Android uses an exact
monotonic `sleep_until` deadline and does not use Tempest's desktop
busy-spin tail. The limit is only a cap: the measured Xardas-room scene
remains GPU-bound at about 15–16 FPS, while the menu holds about 29.7 FPS.
See
[`2026-07-20-helio-g99-performance.md`](../docs/superpowers/reports/2026-07-20-helio-g99-performance.md).

## Texture memory and ASTC

Mali-G57 and the tested Adreno 619 report no BC/DXT sampling but do support
ASTC 4×4. Without transcoding, Tempest decompresses Gothic's DXT textures to
RGBA8, causing a large memory expansion.

The Android build links `astcenc` and lazily converts DXT texture mip chains
to ASTC 4×4. Results on Mali-G57:

- 867 textures;
- about 158 MiB ASTC versus 633 MiB RGBA8;
- about 61.5 seconds of encoding during the first world load;
- about 160 MB of disk cache;
- subsequent world load around 8 seconds;
- PSS reduced from roughly 1.94 GB to 1.28–1.35 GB.

The cache lives under `/sdcard/OpenGothic/astc/` and survives APK updates.
It is not yet production-hardened:

- the custom header and payload dimensions are not fully cross-validated;
- invalidation uses source size and a manual cache version, not a content
  hash;
- sanitized resource names can collide;
- the public directory must be treated as untrusted input;
- the custom `.astc` files are not the standard 16-byte-header ASTC
  container.

`[INTERNAL] androidTexCap` remains a diagnostic/fallback setting. A positive
value takes precedence over ASTC; the normal default is `0` (disabled).

## Adreno investigation

The tested A23 crashes in Qualcomm's compiler during
`vkCreateGraphicsPipelines`. ASTC was excluded as the trigger by reproducing
the same crash with `androidTexCap=512`.

The application-side shader ladder was executed and removed. The repository
now retains the generally correct parts: a scalar slot path, capability-gated
VSM, Release shaders without debug information and CI validation that rejects
runtime descriptor arrays/non-uniform capabilities in every generated slot
module. None fixed Samsung driver 512.548.0.

If the investigation resumes, start with zero relevant Vulkan Validation
errors and a standalone reproducer for the first pipeline that still crashes
after the depth-only variant. Do not repeat the removed `TEMP_BISECT_*`
sequence. A separate Android backend is not justified; the preferred
architecture is a measured mobile pass profile in the existing Vulkan
renderer.

## CI and distribution

[`.github/workflows/android.yml`](../.github/workflows/android.yml) runs on
code-affecting pushes to `android` and on manual dispatch. Markdown and
`docs/**` changes are intentionally ignored.

The job:

1. checks out submodules;
2. installs JDK 17, Android SDK and host `glslang-tools`;
3. applies Tempest patches;
4. builds a signed Release APK;
5. validates 84 generated slot/PFX SPIR-V modules for both ABIs;
6. moves `latest-android` to the exact Android commit and verifies its target;
7. replaces `OpenGothic.apk` and uploads the same artifact.

Green CI proves compilation and linkage, not runtime correctness. Runtime
verification on the real devices remains mandatory.

Known CI/release issues:

- `ubuntu-latest`, `glslang-tools` and SPIRV-Tools are not version-pinned, so
  identical source can still produce different SPIR-V after runner updates;
- tool versions are printed and wrapper generation is fail-loud, but Gradle
  Wrapper is still generated on the runner instead of committed;
- the current action versions emit a Node.js 20 deprecation warning on the
  hosted runner.

### Signing model

The committed PKCS12 keystore provides stable in-place updates. Its key and
passwords are public, so anyone can sign another APK with the same update
identity. This is an accepted convenience trade-off for the personal
sideload port, but users must not treat the signature as proof of provenance.

## Building locally

Requirements:

- JDK 17;
- Android SDK 34;
- NDK `26.3.11579264`;
- CMake 3.22.1;
- Gradle 8.7;
- `glslangValidator`;
- Bash for the patch/build scripts.

From the Android worktree:

```bash
bash android/patches/apply-patches.sh
bash android/build-android.sh [buildNum]
```

The output is a signed `*-release.apk` under `android/`.

## Current priorities

1. Profile individual GPU passes on Mali with AGI/Perfetto or timestamp
   queries before attempting another renderer shortcut.
2. Design a correctness-preserving mobile HiZ/depth and direct-light path;
   the measured 30 FPS target is not yet met.
3. Harden ASTC cache validation and cache keys.
4. Reach zero relevant Vulkan Validation errors before resuming Adreno.
5. Pin the shader toolchain and commit a known Gradle Wrapper.
6. Gate native startup on storage permission and show visible Android errors.
7. Harden full Activity recreation and narrow `SUBOPTIMAL` suppression.
8. Add multitouch, Android controller support and haptics.
9. Add a reliable Android native crash unwinder.

Historical M1 task detail remains in the
[`M1 implementation plan`](../docs/superpowers/plans/2026-07-14-android-port-m1.md).
The active backlog is [`TODO.md`](TODO.md), and measured M2 results are in
[`2026-07-16-android-m2-report.md`](../docs/superpowers/reports/2026-07-16-android-m2-report.md).
