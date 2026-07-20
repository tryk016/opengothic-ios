# ASTC Transcoder — Phase 1 (Decision Gate) Implementation Plan

> **Status: ARCHIVED / EXECUTED.** This is a historical execution plan, not
> the active backlog. Unchecked boxes below do not mean the work is pending.
> Current work is tracked in
> [`android/TODO.md`](../../../android/TODO.md).
>
> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prove or kill the on-device DXT→ASTC transcoder in ~2 CI cycles by validating four independent kill-risks, before any cache or loader logic exists.

> ## ⚠️ EXECUTED 2026-07-16 — read this before following the steps below
>
> This plan was executed. Tasks 1 and 2 are **done**; the steps below are kept as the historical
> record. Six things were wrong or missing in the plan as written — corrected in the real commits
> (`8c07a4bd`, `c1597c00`, `cc417ca2`) but **not** rewritten into the step text, so that the gap
> between plan and reality stays visible:
>
> 1. **`astcenc_context_alloc` takes 4 args in 5.6.0, not 3.** The plan's benchmark used
>    `astcenc_context_alloc(&config, 1, &ctx)`; the real signature ([astcenc.h:761](../../../lib/astcenc/Source/astcenc.h))
>    has a 4th `const astcenc_context* parent_context` (pass `nullptr`). **This cost one CI cycle.**
>    `astcenc_compress_image`'s 6-arg signature was fine.
> 2. **The CMake snippet only enabled NEON — it would have broken the x86_64 build.**
>    `abiFilters` is `'arm64-v8a', 'x86_64'` (android/app/build.gradle:24) and NEON does not exist on
>    x86_64. The ISA must be selected per `ANDROID_ABI`: `astcenc-neon-static` / `astcenc-sse4.1-static`.
> 3. **`ASTCENC_WERROR` defaults to ON** — astcenc turns its own warnings into errors, which under the
>    NDK's clang can fail our build. Must be forced OFF. The plan did not list it.
> 4. **`Tempest::Log` has no `bool` overload** (explicit ones exist for int8..uint64/float/double).
>    `bool→int` is a promotion so it would compile, but the caps log now uses explicit `int()` casts
>    rather than resting on overload-resolution ranking.
> 5. **Pinned tag is 5.6.0, not 5.3.0** (latest stable at execution time).
> 6. **The plan's premise that Adreno "has BC" was WRONG — and this is the most consequential of the
>    six.** Step 7 predicted `A23: DXT1=1 DXT5=1`; the measurement was **`DXT1=0 DXT5=0 ASTC4x4=1`**,
>    identical to Mali. Both tested mobile GPUs rely on ETC2/ASTC and expose no BC/S3TC through
>    Vulkan; this is not evidence that every mobile Vulkan implementation behaves identically.
>    The CMake comment shipped in Task 2 ("desktop and Adreno sample DXT natively and never need it")
>    is wrong for the same reason and should be corrected in `CMakeLists.txt`. Consequence is
>    favourable: ASTC helps every tested device through one path.
>
> **Verified as written:** the target name guess `astcenc-neon-static` was correct
> (`astc${CODEC}-${ISA}-static`, Source/cmake_core.cmake:18), the header is at `Source/astcenc.h`,
> all seven perl patches applied exactly once, and every option name matched.
>
> **Results:** see §"Faza 1 — wynik" in
> [the design doc](../specs/2026-07-16-astc-transcoder-design.md) and the
> [work report](../reports/2026-07-16-android-m2-report.md).

**Architecture:** Add `ASTC4x4` to Tempest's shared `TextureFormat` enum via idempotent perl patches, link astcenc for arm64, and log two facts at startup: whether the GPU samples ASTC4x4, and how fast astcenc encodes. Nothing in the resource-loading path changes, so a failure is a clean revert.

**Tech Stack:** C++17, Tempest (Vulkan), astcenc (ARM, Apache-2.0), Android NDK/CMake/Gradle, GitHub Actions, adb.

Spec: [`docs/superpowers/specs/2026-07-16-astc-transcoder-design.md`](../specs/2026-07-16-astc-transcoder-design.md) §6 "Faza 1".

## Global Constraints

- **Branch: `android` only.** Never touch `master` or iOS. Repo `E:\claude\opengothic-android` (git worktree).
- **`lib/Tempest` is a submodule that must NEVER be `git add`ed.** Every engine edit goes in `android/patches/apply-patches.sh` as an idempotent perl patch: `if grep -q MARKER; then echo skip; else perl -0777 -pi -e '...'; if grep -q MARKER; then echo patched; else echo ERROR; exit 1; fi; fi`.
- **Nothing builds on Windows.** Verification = push → GitHub Actions → APK → adb. One cycle ≈ 8 min. **Always dry-run perl patches against a copy of the real file before pushing.**
- **`gh` always needs `--repo tryk016/opengothic-ios`** (bare `gh` targets upstream `Try/OpenGothic` and fails with 401).
- **`ASTC4x4` goes at the END of the enum** (after `RGBA16F`, before `Last`). `pixmap.cpp:68` does `kfrm[uint8_t(frm)-uint8_t(DXT1)]` positional arithmetic; inserting mid-enum corrupts it.
- **Do not touch** the `[INTERNAL] androidTexCap` escape hatch (default 0/off) or the resource-loading path.
- Real-file reference copy for dry-runs: `E:\claude\opengothic\OpenGothic\lib\Tempest\Engine\...` (main clone, same submodule commit).
- Scratch dir: `C:\Users\pbaran\AppData\Local\Temp\claude\E--claude-opengothic\d1bba110-13cd-455a-95fd-fc1cf3fa0a78\scratchpad`.
- Devices: Tab A9 / Mali-G57 = `R83Y81NE23H` (target). Galaxy A23 / Adreno 619 = `R5CT92SB0YL` (control — **the plan assumed it "has BC"; Phase 1 measured `DXT1=0`, so it does NOT**. See the EXECUTED banner, item 6.)
- adb: `C:\Users\pbaran\AppData\Local\Android\Sdk\platform-tools\adb.exe`.

### Verified facts this plan depends on

| Fact | Evidence |
|---|---|
| Enum tail is `RGBA16F,` → `Last` → `};` | `abstractgraphicsapi.h:130-132` |
| Vulkan caps probing is **generic** — loops `i<TextureFormat::Last`, calls `nativeFormat(i)` + `vkGetPhysicalDeviceFormatProperties`, and for `isCompressedFormat(i)` sets `smpFormat |= (1ull<<i)` | `vdevice.cpp:678-698` |
| ⇒ `hasSamplerFormat(ASTC4x4)` works **for free** once enum + `nativeFormat` + `isCompressedFormat` exist. No caps patch. | same |
| `smpFormat` is a `uint64_t` bitmask indexed by enum value; `Last` = 26 → 27 after. Safe (<64). | `vdevice.cpp:677` |
| **Tempest does NOT use `-Werror`** ⇒ a missed switch case is a warning, not a build failure | `lib/Tempest/Engine/CMakeLists.txt` has only `-Wno-*` for third-party deps |
| `-Werror` applies only to the game target (`target_compile_options(${PROJECT_NAME} PRIVATE ... -Werror)`) | `CMakeLists.txt:29` |
| **`game/` contains no switch on `TextureFormat`** ⇒ `-Werror` is not a risk here | grep: no matches |
| CI checks out `submodules: recursive` ⇒ a new `lib/astcenc` submodule just works | `android.yml:21` |
| CI ignores `docs/**` and `**.md` ⇒ doc commits do NOT trigger builds | `android.yml:6-8` |
| `Tempest::Device` is created at `main_android.cpp:145` | read |

## File Structure

| File | Responsibility | Action |
|---|---|---|
| `android/patches/apply-patches.sh` | All Tempest edits. New section `(f) ASTC4x4 format support` | Modify |
| `.gitmodules` + `lib/astcenc` | astcenc as submodule | Create |
| `CMakeLists.txt` | Build astcenc (NEON, static) and link it into the game on Android | Modify |
| `game/main_android.cpp` | TEMP Phase-1 diagnostics: caps log + encode benchmark | Modify |

Phase 1 adds **no new source files** — it is deliberately a thin, revertible probe.

---

### Task 1: Tempest ASTC4x4 format plumbing + capability log

Adds `ASTC4x4` to the shared enum and logs whether the GPU can sample it. Validates kill-risks **#1 (enum surgery)** and **#2 (Mali samples ASTC)**.

**Files:**
- Modify: `android/patches/apply-patches.sh` (append new section before `(d) Tempest CMakeLists.txt`)
- Modify: `game/main_android.cpp:146` (add log after `CrashLog::setGpu(device.properties().name);`)

**Interfaces:**
- Consumes: nothing.
- Produces: `Tempest::TextureFormat::ASTC4x4` (enum value, appended before `Last`); `Tempest::Detail::nativeFormat(ASTC4x4) == VK_FORMAT_ASTC_4x4_UNORM_BLOCK`; `isCompressedFormat(ASTC4x4) == true`; `Pixmap::blockSizeForFormat(ASTC4x4) == 16`; `Pixmap::componentCount(ASTC4x4) == 4`. Task 2 and Phase 2 rely on these exact names.

- [ ] **Step 1: Dry-run all seven Tempest patches against real copies (BEFORE editing the script)**

This is the test. A wrong regex costs an 8-minute CI cycle, so prove every pattern matches first.

```bash
cd "/c/Users/pbaran/AppData/Local/Temp/claude/E--claude-opengothic/d1bba110-13cd-455a-95fd-fc1cf3fa0a78/scratchpad"
T="/e/claude/opengothic/OpenGothic/lib/Tempest/Engine"
cp "$T/gapi/abstractgraphicsapi.h" ./aga.h
cp "$T/formats/pixmap.cpp"         ./pm.cpp
cp "$T/gapi/vulkan/vdevice.h"      ./vd.h

# (1) enum: append ASTC4x4 before Last
perl -0777 -pi -e 's/(    RGBA16F,\r?\n)(    Last)/${1}    ASTC4x4,\n${2}/' ./aga.h
# (2) formatName
perl -0777 -pi -e 's/(      case RGBA16F:     return "RGBA16F";\r?\n)(      case Last:)/${1}      case ASTC4x4:     return "ASTC4x4";\n${2}/' ./aga.h
# (3) isCompressedFormat
perl -0777 -pi -e 's/(    return f==TextureFormat::DXT1 \|\| f==TextureFormat::DXT3 \|\| f==TextureFormat::DXT5;)/    return f==TextureFormat::DXT1 || f==TextureFormat::DXT3 || f==TextureFormat::DXT5 || f==TextureFormat::ASTC4x4;/' ./aga.h
# (4) pixmap isCompressed
perl -0777 -pi -e 's/(    return frm==TextureFormat::DXT1 \|\|\r?\n           frm==TextureFormat::DXT3 \|\|\r?\n           frm==TextureFormat::DXT5;)/    return frm==TextureFormat::DXT1 ||\n           frm==TextureFormat::DXT3 ||\n           frm==TextureFormat::DXT5 ||\n           frm==TextureFormat::ASTC4x4;/' ./pm.cpp
# (5) blockSizeForFormat  (DXT5->16 is unique to this function)
perl -0777 -pi -e 's/(    case TextureFormat::DXT5:        return 16;\r?\n)/${1}    case TextureFormat::ASTC4x4:     return 16;\n/' ./pm.cpp
# (6) componentCount      (DXT5->4 is unique to this function)
perl -0777 -pi -e 's/(    case TextureFormat::DXT5:        return 4;\r?\n)/${1}    case TextureFormat::ASTC4x4:     return 4;\n/' ./pm.cpp
# (7) vulkan nativeFormat
perl -0777 -pi -e 's/(    case TextureFormat::RGBA16F:\r?\n      return VK_FORMAT_R16G16B16A16_SFLOAT;\r?\n)(    \})/${1}    case TextureFormat::ASTC4x4:\n      return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;\n${2}/' ./vd.h

echo "== each must print exactly 1 =="
grep -c 'ASTC4x4,'                          ./aga.h
grep -c 'case ASTC4x4:     return "ASTC4x4"' ./aga.h
grep -c 'f==TextureFormat::ASTC4x4'          ./aga.h
grep -c 'frm==TextureFormat::ASTC4x4'        ./pm.cpp
grep -c 'ASTC4x4:     return 16;'            ./pm.cpp
grep -c 'ASTC4x4:     return 4;'             ./pm.cpp
grep -c 'VK_FORMAT_ASTC_4x4_UNORM_BLOCK'     ./vd.h
echo "== enum tail must read RGBA16F, ASTC4x4, Last =="
grep -n -A3 'RGBA16F,' ./aga.h | head -5
```

- [ ] **Step 2: Run it and confirm every count is 1**

Expected: seven `1`s, and the enum tail printed as:
```
    RGBA16F,
    ASTC4x4,
    Last
```
If any count is `0`, the anchor text drifted — re-read the real file and fix the regex **before** touching the script. Then `rm ./aga.h ./pm.cpp ./vd.h`.

- [ ] **Step 3: Add the patch section to apply-patches.sh**

Insert immediately **before** the `# (d) Tempest CMakeLists.txt` banner. Note `PM` and `AGA` are new file vars — add them next to the existing `SYS`/`VK`/`SW`/`CM` declarations at the top, and to the existence-check `for f in ...` loop.

At the top, extend the vars and the check loop:
```bash
AGA="$ROOT/lib/Tempest/Engine/gapi/abstractgraphicsapi.h"
PM="$ROOT/lib/Tempest/Engine/formats/pixmap.cpp"
VD="$ROOT/lib/Tempest/Engine/gapi/vulkan/vdevice.h"
```
```bash
for f in "$SYS" "$VK" "$SW" "$CM" "$AGA" "$PM" "$VD"; do
```

Then the new section:
```bash
# --------------------------------------------------------------------------
# (f) ASTC4x4 texture format support.
#
# Historical draft: Mali and older Apple GPUs without BC make device.cpp:199
# decompress DXT to RGBA8. Newer Apple hardware may expose BC. The later
# measured texture payload was 633MiB RGBA8 versus 158MiB ASTC.
#
# ASTC 4x4 is chosen because it is 16 bytes per 4x4 block -- exactly the shape
# pixmap.cpp/mttexture.cpp already hardcode for DXT3/DXT5 -- so NO block-math
# generalization is needed. 6x6 would require surgery in Vulkan AND Metal.
#
# The enum entry MUST go last (after RGBA16F, before Last): pixmap.cpp does
# kfrm[uint8_t(frm)-uint8_t(DXT1)] positional arithmetic, which inserting a
# value between DXT1..DXT5 would corrupt.
#
# Nothing else is needed for capability detection: vdevice.cpp:678 probes
# formats generically (for i<Last: nativeFormat(i) -> vkGetPhysicalDeviceFormatProperties),
# so hasSamplerFormat(ASTC4x4) starts working as soon as these patches land.
# --------------------------------------------------------------------------

if grep -q 'ASTC4x4' "$AGA"; then
  echo "skip: abstractgraphicsapi.h ASTC4x4 (already patched)"
else
  perl -0777 -pi -e 's/(    RGBA16F,\r?\n)(    Last)/${1}    ASTC4x4,\n${2}/' "$AGA"
  perl -0777 -pi -e 's/(      case RGBA16F:     return "RGBA16F";\r?\n)(      case Last:)/${1}      case ASTC4x4:     return "ASTC4x4";\n${2}/' "$AGA"
  perl -0777 -pi -e 's/(    return f==TextureFormat::DXT1 \|\| f==TextureFormat::DXT3 \|\| f==TextureFormat::DXT5;)/    return f==TextureFormat::DXT1 || f==TextureFormat::DXT3 || f==TextureFormat::DXT5 || f==TextureFormat::ASTC4x4;/' "$AGA"
  if [ "$(grep -c 'ASTC4x4' "$AGA")" = "3" ]; then
    echo "patched: abstractgraphicsapi.h ASTC4x4 (enum + formatName + isCompressedFormat)"
  else
    echo "ERROR: failed to patch abstractgraphicsapi.h ASTC4x4 (expected 3 hits, got $(grep -c 'ASTC4x4' "$AGA"))" >&2
    exit 1
  fi
fi

if grep -q 'ASTC4x4' "$PM"; then
  echo "skip: pixmap.cpp ASTC4x4 (already patched)"
else
  perl -0777 -pi -e 's/(    return frm==TextureFormat::DXT1 \|\|\r?\n           frm==TextureFormat::DXT3 \|\|\r?\n           frm==TextureFormat::DXT5;)/    return frm==TextureFormat::DXT1 ||\n           frm==TextureFormat::DXT3 ||\n           frm==TextureFormat::DXT5 ||\n           frm==TextureFormat::ASTC4x4;/' "$PM"
  perl -0777 -pi -e 's/(    case TextureFormat::DXT5:        return 16;\r?\n)/${1}    case TextureFormat::ASTC4x4:     return 16;\n/' "$PM"
  perl -0777 -pi -e 's/(    case TextureFormat::DXT5:        return 4;\r?\n)/${1}    case TextureFormat::ASTC4x4:     return 4;\n/' "$PM"
  if [ "$(grep -c 'ASTC4x4' "$PM")" = "3" ]; then
    echo "patched: pixmap.cpp ASTC4x4 (isCompressed + blockSize=16 + componentCount=4)"
  else
    echo "ERROR: failed to patch pixmap.cpp ASTC4x4 (expected 3 hits, got $(grep -c 'ASTC4x4' "$PM"))" >&2
    exit 1
  fi
fi

if grep -q 'VK_FORMAT_ASTC_4x4_UNORM_BLOCK' "$VD"; then
  echo "skip: vdevice.h ASTC4x4 (already patched)"
else
  perl -0777 -pi -e 's/(    case TextureFormat::RGBA16F:\r?\n      return VK_FORMAT_R16G16B16A16_SFLOAT;\r?\n)(    \})/${1}    case TextureFormat::ASTC4x4:\n      return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;\n${2}/' "$VD"
  if grep -q 'VK_FORMAT_ASTC_4x4_UNORM_BLOCK' "$VD"; then
    echo "patched: vdevice.h ASTC4x4 -> VK_FORMAT_ASTC_4x4_UNORM_BLOCK"
  else
    echo "ERROR: failed to patch vdevice.h ASTC4x4 (pattern not found)" >&2
    exit 1
  fi
fi
```

- [ ] **Step 4: Add the capability log**

In `game/main_android.cpp`, after line 146 (`CrashLog::setGpu(device.properties().name);`):

```cpp
    // TEMP Phase-1 decision gate: does this GPU sample ASTC4x4, and does it
    // really lack BC? Drives the DXT->ASTC transcoder go/no-go (see
    // docs/superpowers/specs/2026-07-16-astc-transcoder-design.md §6). Revert
    // once Phase 1 concludes.
    Tempest::Log::i("[astcdiag] caps: DXT1=",   device.properties().hasSamplerFormat(Tempest::TextureFormat::DXT1),
                    " DXT5=",                   device.properties().hasSamplerFormat(Tempest::TextureFormat::DXT5),
                    " ASTC4x4=",                device.properties().hasSamplerFormat(Tempest::TextureFormat::ASTC4x4));
```

- [ ] **Step 5: Commit and push**

```bash
cd /e/claude/opengothic-android
git add android/patches/apply-patches.sh game/main_android.cpp
git commit -m "android: add ASTC4x4 texture format to Tempest + capability log (Phase 1)"
git push origin android
```

- [ ] **Step 6: Wait for CI and confirm it is green**

```bash
cd /e/claude/opengothic-android
gh run list --repo tryk016/opengothic-ios --branch android --limit 1 --json databaseId,headSha,status,conclusion
```
Expected: `conclusion: success`. The CI log must contain `patched: abstractgraphicsapi.h ASTC4x4 (enum + formatName + isCompressedFormat)` (and the pixmap/vdevice lines). If the build fails on a `-Wswitch` error, note which file — Tempest has no `-Werror`, so this would be unexpected and means a switch lives in a target that does.

- [ ] **Step 7: Install and read the capability log on BOTH devices**

```bash
ADB="C:/Users/pbaran/AppData/Local/Android/Sdk/platform-tools/adb.exe"
SCRATCH="C:/Users/pbaran/AppData/Local/Temp/claude/E--claude-opengothic/d1bba110-13cd-455a-95fd-fc1cf3fa0a78/scratchpad"
cd /e/claude/opengothic-android
gh release download latest-android --repo tryk016/opengothic-ios --pattern OpenGothic.apk --dir "$SCRATCH" --clobber

for DEV in R83Y81NE23H R5CT92SB0YL; do
  "$ADB" -s $DEV shell input keyevent KEYCODE_WAKEUP
  "$ADB" -s $DEV install -r "$SCRATCH/OpenGothic.apk"
  "$ADB" -s $DEV shell appops set opengothic.gothic2 MANAGE_EXTERNAL_STORAGE allow
  "$ADB" -s $DEV logcat -c
  "$ADB" -s $DEV shell am start -n opengothic.gothic2/.MainActivity
  sleep 20
  echo "=== $DEV ==="
  "$ADB" -s $DEV logcat -d | grep astcdiag
done
```

Expected:
- **Tab A9 (`R83Y81NE23H`, Mali-G57): `[astcdiag] caps: DXT1=0 DXT5=0 ASTC4x4=1`** ← **the kill-risk #2 answer**
- **Historical expectation for A23:** `DXT1=1 DXT5=1`. **Actual measured
  result:** `DXT1=0 DXT5=0 ASTC4x4=1`; the expectation was wrong.

**If Tab A9 reports `ASTC4x4=0`, STOP.** The whole transcoder is dead; record it in the plan and report. No further Phase-1 work.

- [ ] **Step 8: Confirm zero regression**

The game must still boot to menu and load Khorinis exactly as before (nothing in the loading path changed).
```bash
"$ADB" -s R83Y81NE23H shell input tap 660 311; sleep 1; "$ADB" -s R83Y81NE23H shell input tap 1168 723
sleep 100
"$ADB" -s R83Y81NE23H shell pidof opengothic.gothic2   # must be non-empty
"$ADB" -s R83Y81NE23H logcat -d | grep -E "signal 11|Fatal signal"   # must be empty
```
Expected: a live pid, no signal 11.

---

### Task 2: astcenc for arm64 + encode benchmark

Links ARM's astcenc into the Android build and measures real encode throughput. Validates kill-risks **#3 (astcenc builds on arm64)** and **#4 (encode speed)**.

**Files:**
- Create: `lib/astcenc` (git submodule) + `.gitmodules` entry
- Modify: `CMakeLists.txt` (astcenc subdirectory + link, Android only)
- Modify: `game/main_android.cpp` (benchmark next to the Task 1 log)

**Interfaces:**
- Consumes: nothing from Task 1 (independent; only shares the `[astcdiag]` log tag).
- Produces: a linkable `astcenc-neon-static` target and `#include <astcenc.h>` availability on Android. Phase 2 reuses both.

- [ ] **Step 1: Add astcenc as a submodule pinned to a release tag**

```bash
cd /e/claude/opengothic-android
git submodule add https://github.com/ARM-software/astc-encoder lib/astcenc
cd lib/astcenc && git checkout 5.3.0 && cd ../..
git add .gitmodules lib/astcenc
```
Pin to a tag — never track a moving branch. CI checks out `submodules: recursive`, so nothing else is needed there.

- [ ] **Step 2: Build astcenc in CMakeLists.txt (Android only)**

Append after the ZenKit block (`target_link_libraries(${PROJECT_NAME} zenkit)`), so astcenc sits with the other `lib/` dependencies:

```cmake
# astcenc — DXT->ASTC transcoding on mobile GPUs that lack BC/S3TC.
# Historical draft (wrong for the tested Adreno): mobile GPUs without BC need ASTC.
if(ANDROID)
  set(ASTCENC_ISA_NEON     ON  CACHE BOOL "" FORCE)   # arm64
  set(ASTCENC_ISA_NATIVE   OFF CACHE BOOL "" FORCE)
  set(ASTCENC_CLI          OFF CACHE BOOL "" FORCE)   # library only, no command-line tool
  set(ASTCENC_SHAREDLIB    OFF CACHE BOOL "" FORCE)
  set(ASTCENC_UNITTEST     OFF CACHE BOOL "" FORCE)
  set(ASTCENC_DECOMPRESSOR OFF CACHE BOOL "" FORCE)
  set(ASTCENC_DIAGNOSTICS  OFF CACHE BOOL "" FORCE)
  add_subdirectory(lib/astcenc)
  target_include_directories(${PROJECT_NAME} PRIVATE lib/astcenc/Source)
  target_link_libraries(${PROJECT_NAME} astcenc-neon-static)
endif()
```

- [ ] **Step 3: Add the benchmark**

In `game/main_android.cpp`, add the include near the top with the other includes:
```cpp
#include <astcenc.h>
```

Add this free function above `android_main`:
```cpp
// TEMP Phase-1 decision gate: measure real astcenc throughput on this CPU.
// The job size is known from measurement: 1.38GB of RGBA8 / 4 bytes-per-pixel
// = ~345 Mpixels for all of Khorinis including mips. This turns the design's
// "~1-3 min" estimate into a fact. See
// docs/superpowers/specs/2026-07-16-astc-transcoder-design.md §5.3/§6.
// Revert once Phase 1 concludes.
static void astcBenchmark() {
  const unsigned int dimX = 512, dimY = 512;

  astcenc_config config = {};
  astcenc_error  st = astcenc_config_init(ASTCENC_PRF_LDR, 4, 4, 1, ASTCENC_PRE_FAST, 0, &config);
  if(st!=ASTCENC_SUCCESS) {
    Tempest::Log::e("[astcdiag] config_init failed: ", astcenc_get_error_string(st));
    return;
    }

  astcenc_context* ctx = nullptr;
  st = astcenc_context_alloc(&config, 1, &ctx);
  if(st!=ASTCENC_SUCCESS) {
    Tempest::Log::e("[astcdiag] context_alloc failed: ", astcenc_get_error_string(st));
    return;
    }

  // Structured, non-trivial content: flat images would encode unrealistically
  // fast, pure noise unrealistically slow.
  std::vector<uint8_t> src(size_t(dimX)*size_t(dimY)*4);
  for(size_t i=0; i<src.size(); i+=4) {
    const uint32_t p = uint32_t(i/4);
    const uint32_t x = p % dimX;
    const uint32_t y = p / dimX;
    src[i+0] = uint8_t(x ^ y);
    src[i+1] = uint8_t(x + y);
    src[i+2] = uint8_t(x * 3 + y * 5);
    src[i+3] = 255;
    }

  uint8_t*      slice = src.data();
  astcenc_image image = {};
  image.dim_x     = dimX;
  image.dim_y     = dimY;
  image.dim_z     = 1;
  image.data_type = ASTCENC_TYPE_U8;
  image.data      = reinterpret_cast<void**>(&slice);

  const astcenc_swizzle swz{ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};
  const size_t blocks = size_t((dimX+3)/4) * size_t((dimY+3)/4);
  std::vector<uint8_t> dst(blocks*16);   // ASTC 4x4 = 16 bytes per block

  const auto t0 = std::chrono::steady_clock::now();
  st = astcenc_compress_image(ctx, &image, &swz, dst.data(), dst.size(), 0);
  const double ms = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
  astcenc_context_free(ctx);

  if(st!=ASTCENC_SUCCESS) {
    Tempest::Log::e("[astcdiag] compress failed: ", astcenc_get_error_string(st));
    return;
    }

  const double mpx       = double(dimX)*double(dimY)/1e6;
  const double mpxPerSec = mpx/(ms/1000.0);
  Tempest::Log::i("[astcdiag] astcenc 512x512 PRE_FAST 1-thread: ", ms, " ms => ", mpxPerSec, " Mpx/s"
                  " | Khorinis 345 Mpx => ", 345.0/mpxPerSec, " s single-threaded"
                  " (~", (345.0/mpxPerSec)/6.0, " s if it scales over 6 cores)"
                  " | out=", dst.size()/1024, " KiB vs rgba8=", src.size()/1024, " KiB");
  }
```

Ensure `<chrono>` and `<vector>` are included (add if absent).

Call it right after the Task 1 caps log:
```cpp
    astcBenchmark(); // TEMP Phase-1 decision gate
```

- [ ] **Step 4: Commit and push**

```bash
cd /e/claude/opengothic-android
git add .gitmodules lib/astcenc CMakeLists.txt game/main_android.cpp
git commit -m "android: link astcenc (arm64/NEON) + encode benchmark (Phase 1)"
git push origin android
```

Note: `git add lib/astcenc` records a **submodule pointer**, which is correct and expected. This is not the `lib/Tempest` prohibition — that rule exists because Tempest is patched in-place at build time; astcenc is used unmodified.

- [ ] **Step 5: Confirm CI links astcenc**

```bash
gh run list --repo tryk016/opengothic-ios --branch android --limit 1 --json headSha,status,conclusion
```
Expected: `success`. **A link error here is the kill-risk #3 answer** — capture the exact error; the fallback is the offline-PC variant in spec §9.

- [ ] **Step 6: Read the benchmark on the Tab A9**

```bash
ADB="C:/Users/pbaran/AppData/Local/Android/Sdk/platform-tools/adb.exe"
SCRATCH="C:/Users/pbaran/AppData/Local/Temp/claude/E--claude-opengothic/d1bba110-13cd-455a-95fd-fc1cf3fa0a78/scratchpad"
cd /e/claude/opengothic-android
gh release download latest-android --repo tryk016/opengothic-ios --pattern OpenGothic.apk --dir "$SCRATCH" --clobber
"$ADB" -s R83Y81NE23H install -r "$SCRATCH/OpenGothic.apk"
"$ADB" -s R83Y81NE23H logcat -c
"$ADB" -s R83Y81NE23H shell am start -n opengothic.gothic2/.MainActivity
sleep 25
"$ADB" -s R83Y81NE23H logcat -d | grep astcdiag
```
Expected: a line with `ms => N Mpx/s | Khorinis 345 Mpx => T s single-threaded`. Record `T`.

---

### Task 3: Decision gate — record results and decide

No code. This task exists so the go/no-go is written down rather than assumed.

**Files:**
- Modify: `docs/superpowers/specs/2026-07-16-astc-transcoder-design.md` (append a "Wynik Fazy 1" section)

- [ ] **Step 1: Fill in the four answers**

| # | Kill-risk | Criterion | Result |
|---|---|---|---|
| 1 | Enum surgery | CI green, patch lines present, no regression | |
| 2 | Mali samples ASTC4x4 | Tab A9: `ASTC4x4=1`, `DXT1=0` | |
| 3 | astcenc on arm64 | APK links | |
| 4 | Encode speed | Khorinis extrapolation `T` | |

- [ ] **Step 2: Apply the decision rule**

- **All four pass, and `T`/6 is roughly ≤3 min** → proceed to Phase 2 (spec §5.2/§5.3/§5.4/§5.5).
- **#2 fails (`ASTC4x4=0`)** → **transcoder is dead.** Revert both commits. Memory stays at 1.38 GB; the only remaining lever is `androidTexCap` and its measured quality trade-off.
- **#3 fails** → fall back to the offline-PC variant (spec §9). Task 1's work is kept — §5.1 is shared.
- **#4 is far worse than estimated** (e.g. `T`/6 ≫ 10 min) → fall back to the offline-PC variant. Task 1's work is kept.

- [ ] **Step 3: Record the outcome in the spec and commit**

```bash
cd /e/claude/opengothic-android
git add docs/superpowers/specs/2026-07-16-astc-transcoder-design.md
git commit -m "docs: record ASTC Phase 1 decision-gate results"
git push origin android
```
(Doc-only commits do not trigger CI — `android.yml` has `paths-ignore: docs/**`.)

- [ ] **Step 4: Revert the temporary diagnostics if Phase 2 does not start immediately**

The benchmark and caps log are startup-cost and log noise. If Phase 2 is deferred, revert the `main_android.cpp` diagnostics but **keep** the `apply-patches.sh` ASTC4x4 section — it is the reusable foundation for both the on-device and offline variants.

---

## Self-Review

**Spec coverage (§6 "Faza 1"):**

| Spec requirement | Task |
|---|---|
| Patche Tempesta z §5.1 (enum, formatName, isCompressedFormat×2, blockSizeForFormat, componentCount, mapa Vulkana) | Task 1, Steps 1–3 |
| astcenc podpięty do builda (arm64 + NEON), linkuje się | Task 2, Steps 1–2, 5 |
| Log `hasSamplerFormat(DXT1)` i `hasSamplerFormat(ASTC4x4)` | Task 1, Steps 4, 7 |
| Mikro-benchmark 512×512 → ms | Task 2, Steps 3, 6 |
| Kryterium: `DXT1=false, ASTC4x4=true` | Task 1, Step 7 |
| Kryterium: APK buduje się i linkuje | Task 2, Step 5 |
| Kryterium: Mpx/s → ekstrapolacja na 345 Mpx | Task 2, Steps 3, 6 |
| Kryterium: zero regresji | Task 1, Step 8 |
| Brama decyzyjna + fallback do §9 | Task 3 |
| Bez cache'u, bez zmian ścieżki zasobów | Enforced by File Structure — only 4 files, none in the loading path |

**Note on cycle count:** the spec says "one CI cycle". This plan uses **two** (Task 1, Task 2) because the enum surgery and the astcenc link have unrelated failure modes; separating them means a red build names its own cause. Cost: ~8 extra minutes. They can be pushed together if preferred — Task 2 does not depend on Task 1.

**Placeholder scan:** no TBD/TODO; every code step carries complete code; every command has expected output.

**Type consistency:** `ASTC4x4` is spelled identically in all seven patches, the caps log, and the benchmark comment. `astcenc-neon-static` matches astcenc's CMake target naming for `ASTCENC_ISA_NEON=ON`. `astcenc_*` API calls match astcenc 5.x (`astcenc_config_init`, `astcenc_context_alloc`, `astcenc_compress_image`, `astcenc_context_free`, `astcenc_get_error_string`).

**Known unknown flagged for the implementer:** the astcenc CMake target name and option spellings are from astcenc's documented conventions but are **not verified against the pinned tag** (the submodule is not checked out locally). If `astcenc-neon-static` does not exist, run `grep -rn "add_library" lib/astcenc/Source/CMakeLists.txt` after `git submodule update --init lib/astcenc` and use the real name. This is a build-time error, caught in Task 2 Step 5.
