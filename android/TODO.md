# Android port — current status and backlog

Last audited: 2026-07-20.

This file is the active backlog. The original M1 task-by-task plan is
preserved in
[`2026-07-14-android-port-m1.md`](../docs/superpowers/plans/2026-07-14-android-port-m1.md);
measured M2 results are in
[`2026-07-16-android-m2-report.md`](../docs/superpowers/reports/2026-07-16-android-m2-report.md).

## Repository state at the audit baseline

- Branch: `android`
- Android code HEAD after the performance pass: `0e1f0a6b`
- `master`: `97f7db4e`, already contained in `android` with no master-only
  commits
- History: `android` contains `master` plus Android-only commits
- Release: `latest-android`, WIP prerelease
- Full shaders restored; all `TEMP_BISECT_*` modules removed
- Tempest remains patch-script-managed and is not committed as a dirty
  submodule

## Completed

### M1 — boot-to-menu foundation

- [x] Gradle + NDK + GameActivity project
- [x] `arm64-v8a` and `x86_64` builds
- [x] Android Vulkan surface and Tempest `AndroidApi`
- [x] Stable signed APK updates
- [x] Public data root at `/sdcard/OpenGothic/Gothic2`
- [x] Required `Data/`, `_work/` and `system/` layout
- [x] Main menu and intro
- [x] Primary-pointer touch mapped into the engine
- [x] Surface destruction/recreation on background/resume
- [x] Protection against reused `ANativeWindow*` addresses
- [x] Fixed landscape and correct presentation on portrait-native panels
- [x] Keep-screen-on during loading/cutscenes
- [x] WIP GitHub Release distribution

### M2 — target Mali-G57 hardware

Verified on Samsung Galaxy Tab A9 / Helio G99 / Mali-G57 MC2 / approximately
3.5 GB usable RAM:

- [x] Real arm64 boot and Vulkan initialization
- [x] Full Khorinis world load
- [x] Correct landscape frame and touch coordinates
- [x] Dialogue selection and basic gameplay
- [x] Sustained in-world sessions without the earlier SIGSEGV
- [x] Background/resume on real hardware
- [x] Null descriptor padding fixed at the bindless binding layer
- [x] Missing-texture fallback semantics restored
- [x] Mobile `Gothic.ini` profile created and flushed
- [x] DXT → ASTC 4×4 transcoding with persistent disk cache

Measured ASTC result on Mali:

- 867 textures;
- approximately 158 MiB ASTC instead of 633 MiB RGBA8;
- 61.5 seconds of encoding on the first world load;
- approximately 160 MB disk cache;
- approximately 8 seconds for a cached world load;
- process PSS reduced from about 1.94 GB to 1.28–1.35 GB.

The earlier conclusions that “3.5 GB is over the edge”, that the Mali crash
was caused by generic memory pressure, and that `Gothic.ini` was not loaded
are obsolete. The crash was caused by invalid null descriptors in padded
bindless arrays and was fixed; ASTC then reduced memory pressure further.

## Deferred — Adreno 619 compatibility

The Galaxy A23 / Adreno 619 / Samsung driver 512.548.0 deterministically
crashes inside Qualcomm's `libllvm-glnext.so` while creating the first
textured 3D graphics pipeline. This is a real driver compiler crash, but an
application-side workaround has not been exhausted.

Reference:
[`2026-07-17-adreno-compiler-crash-investigation.md`](../docs/superpowers/reports/2026-07-17-adreno-compiler-crash-investigation.md).

- [x] Build and run the shader-bisect ladder on the A23.
- [x] Build Android Release shaders without debug information.
- [x] Implement a true scalar `!BINDLESS` path without IBO/VBO/morph runtime
      descriptor arrays.
- [x] Gate VSM on the exact descriptor capabilities it requires.
- [x] Validate generated slot/PFX SPIR-V in CI and reject runtime descriptor
      arrays/non-uniform capabilities.
- [x] Test depth-only pipelines without a fragment stage and with a minimal
      vertex module. This passed HiZ and moved the crash to the next material
      pipeline; it did not reach a 3D frame.
- [x] Remove every `TEMP_BISECT_*` module and restore production shaders.
- [ ] Eliminate all remaining relevant Vulkan Validation errors.
- [ ] Log/export Tempest's final reflected layout and every Vulkan descriptor
      binding for a standalone crashing pipeline.
- [ ] Test `VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT`.
- [x] Log `glslangValidator`/SPIRV-Tools versions used by CI.
- [ ] Pin the shader toolchain versions.
- [ ] If the simplified, validation-clean path still crashes, produce a
      minimal standalone reproducer before declaring this exact driver
      combination known-broken.

Do not write a separate Android backend. The scalar Vulkan slot path now
exists; further Adreno work needs a validation-clean standalone reproducer.

## P1 — ASTC cache hardening

The transcoder is functionally and performance-validated, but its public
cache is not yet safe to treat as trusted or production-ready.

- [ ] Validate cached width/height against the current source texture.
- [ ] Validate mip count against dimensions.
- [ ] Recompute and validate exact ASTC payload size for every mip.
- [ ] Require exact file size: header plus payload, with no truncation or
      trailing data.
- [ ] Add checked arithmetic and upper bounds before allocations.
- [ ] Include a content hash in cache invalidation; source byte size alone is
      insufficient.
- [ ] Include cache format, ASTC profile/block size/preset and encoder
      version in the key/header.
- [ ] Add a hash of the full resource name to avoid collisions after filename
      sanitization.
- [ ] Treat `/sdcard/OpenGothic/astc/` as untrusted input and rebuild invalid
      entries.
- [ ] Test truncated, corrupted, oversized and interrupted-write cache files.
- [ ] Consider an ownership-taking `Pixmap` API to avoid copying the entire
      encoded payload.
- [ ] Add a cache size/cleanup policy for mods and replaced game data.

The current cache uses a custom 36-byte `CacheHeader` followed by concatenated
ASTC mip payloads. Its `.astc` extension does not mean it is the standard
16-byte-header ASTC container.

## P1 — CI and release correctness

- [x] Create/update `latest-android` with target `android`, so the tag and
      source archive correspond to the APK rather than `master`.
- [ ] Commit a known Gradle Wrapper instead of generating it on the runner
      (generation is now fail-loud).
- [ ] Pin or containerize the shader toolchain.
- [x] Print `glslangValidator --version` and SPIRV-Tools version in every CI
      run.
- [x] Validate every generated slot/PFX module for both Android ABIs.
- [ ] Keep the release prerelease/WIP warning, but describe the actual
      compatibility matrix: Mali tested, Adreno 619 blocked.
- [x] Repair local `origin/android` tracking configuration; the true remote
      tip must not be inferred from the stale local remote-tracking ref.

## P1 — Android platform hardening

- [ ] Gate native startup until `MANAGE_EXTERNAL_STORAGE` is granted.
- [ ] Show a visible Android error UI for missing data and fatal startup
      errors.
- [ ] Reset AndroidApi global state and callbacks for full Activity
      destroy/recreate in the same process.
- [ ] Restrict `VK_SUBOPTIMAL_KHR` suppression to the deliberate
      identity/current-transform mismatch.
- [ ] Add a reliable native crash unwinder for API 26+.
- [ ] Test repeated Home/resume and destroy/recreate during:
  - first boot;
  - ASTC encoding;
  - world load;
  - active gameplay.
- [ ] Test cache and save behavior when storage is full or removed.

## P2 — controls and usability

- [ ] Multi-touch pointer tracking, including pointer down/up transitions.
- [ ] Android-tuned virtual gamepad layout.
- [ ] Hardware controller mapping.
- [ ] Back key mapping.
- [ ] Android haptics.
- [ ] Save-name text input.
- [ ] Radial/quick-select controls and target lock-on.
- [ ] Verify persistence of every menu-controlled setting across relaunches.

## P2 — performance follow-up

The measured Xardas room is not fragment-fill-rate-bound: lowering
`vidResIndex` did not improve FPS there. Queue telemetry narrows the wait to
GPU present/acquire, while CPU tick/animation/render encoding remain below
the 33.3 ms budget. Do not generalize the indoor result to outdoor Khorinis.

- [ ] Measure a fixed outdoor scene at full, 75% and half resolution.
- [x] Measure `sightValue` 2 (60 km) versus 0 (20 km) in the Xardas room:
      no material improvement.
- [ ] Measure `modelDetail` in an outdoor scene where it is proven active.
- [ ] Record thermals and throttling with every comparison.
- [ ] Measure peak PSS/RSS during the largest first-time ASTC encodes.
- [x] Add a repeatable SurfaceFlinger BLAST-layer latency procedure and
      render/submit/present telemetry.
- [ ] Profile individual passes using GPU timestamps or AGI/Perfetto before
      changing the render graph again.

## Optional iOS reuse — not an Android requirement

Android's ASTC resource logic is a possible base for older Apple GPUs without
BC support, but it is not currently implemented on iOS. Future iOS work
would still require:

- building/linking `astcenc`;
- defining `HAS_ASTCENC`;
- Tempest Metal mapping for ASTC 4×4;
- a suitable private cache directory and cache migration policy;
- real device capability and memory measurements.

Keep this work deferred until the Android port's Adreno path and ASTC cache
are stable. Android fixes do not need to preserve source-level compatibility
with the iOS branch, and iOS reuse must not block Android delivery.
