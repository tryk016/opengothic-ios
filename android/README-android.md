# OpenGothic on Android — install and compatibility guide

> ### ⚠️ Pre-alpha test build
>
> The Android port is playable on the tested Samsung Galaxy Tab A9
> (Helio G99 / Mali-G57 MC2): it loads Khorinis, renders in landscape,
> accepts touch input, survives background/resume, and has completed sustained
> gameplay tests. The tested Galaxy A23 (Adreno 619, Samsung driver
> 512.548.0) currently crashes inside Qualcomm's Vulkan shader compiler before
> the first 3D gameplay frame. Compatibility with other GPUs is unknown.
>
> The published APK remains a work-in-progress prerelease. Install it for
> testing, not as a finished mobile edition.

You must legally own *Gothic II: Night of the Raven*. OpenGothic ships no game
assets or scripts; you supply them from your own installation.

## 1. Get the APK

Download `OpenGothic.apk` from the
[`latest-android` release](https://github.com/tryk016/opengothic-ios/releases/tag/latest-android).
CI rebuilds it after code-affecting pushes to the `android` branch;
documentation-only pushes are intentionally ignored.

Every published build uses the same committed signing key, so a new APK can
be installed over the previous version without uninstalling the app or
recopying game data.

> **Signing warning:** the keystore and its passwords are public in this
> repository. This is convenient for personal sideload updates, but it means
> the APK signature alone does not prove that a build came from the
> maintainer. Download builds only from this repository's official
> `latest-android` release.

## 2. Allow installation from this source

Android blocks APK installation from outside the Play Store by default. Open
the downloaded APK and allow your browser or file manager to install unknown
apps when Android asks. The setting is also available under:

`Settings → Apps → Special app access → Install unknown apps`

## 3. Grant storage access

On first launch, OpenGothic opens the Android settings page for
**Allow access to manage all files**. Enable it for OpenGothic.

The current port needs `MANAGE_EXTERNAL_STORAGE` because it reads game data
from the fixed shared-storage path `/sdcard/OpenGothic/Gothic2/`. A future
app-private or Storage Access Framework implementation could avoid this
permission.

GameActivity can start the native thread before permission is granted. After
enabling access, fully close and relaunch OpenGothic if the first launch has
already exited.

## 4. Copy game data once

Copy exactly these three folders from your Gothic II: NotR installation:

```text
/sdcard/OpenGothic/Gothic2/
├── Data/
├── _work/
└── system/
```

For a Steam installation, the source is usually:

```text
C:\Program Files (x86)\Steam\steamapps\common\Gothic II
```

`_work/` is mandatory. It contains compiled scripts, music, and video data
that are not replaced by the VDF archives.

Writable files are kept one level above the game data:

- `/sdcard/OpenGothic/Gothic.ini`
- `/sdcard/OpenGothic/log.txt`
- `/sdcard/OpenGothic/astc/`
- save data created by the engine

If game data or permission is missing, the native thread currently writes
`Gothic II data not found` to logcat/`log.txt` and exits. A visible Android
error dialog has not been implemented yet.

## 5. First launch and the ASTC cache

On the first successful launch, the port creates
`/sdcard/OpenGothic/Gothic.ini` with the mobile profile:

- half-resolution rendering (`vidResIndex=2`);
- cloud shadows/SSAO disabled;
- 512 px shadow maps on Android;
- mobile gamepad defaults.

The copied `system/Gothic.ini` remains untouched. The writable override is
loaded across relaunches; persistence of every individual option changed
through the in-game menu still needs dedicated coverage.

The profile currently also writes `zMaxFpsMode=1`, but that selector is read
only by the iOS path. Android's effective cap still comes from
`SystemPack.ini` `PARAMETERS/FPS_Limit`, then `ENGINE/zMaxFps`. Therefore the
port does **not** currently guarantee a 30 FPS default.

On GPUs without BC/S3TC but with ASTC, the first world load builds a compressed
texture cache under `/sdcard/OpenGothic/astc/`. On the tested Mali-G57:

- encoding added about 61.5 seconds to the first world load;
- the cache occupied about 160 MB;
- a subsequent world load took about 8 seconds;
- process memory fell from about 1.94 GB PSS to 1.28–1.35 GB.

Do not delete this directory unless you want the cache rebuilt. The cache
format is still experimental and its validation needs hardening, so do not
copy cache files from untrusted sources.

## 6. Controls

Menus and basic gameplay accept touch through the shared mobile control
overlay. Tap the on-screen confirm/"A" control to select a menu item; menu
text itself is not a native Android button.

Current limitations:

- touch handling tracks only the primary pointer, so simultaneous movement
  and action input is not complete;
- there is no Android-tuned virtual gamepad;
- hardware controller input and haptics are not implemented;
- the Android Back key is drained but not mapped to a game action.

## Requirements and tested devices

- Android 8.0 / API 26 or newer;
- Vulkan-capable GPU;
- `arm64-v8a` for real phones/tablets;
- approximately 4 GB RAM or more is recommended;
- `x86_64` is included for desktop emulator testing.

| Device | GPU | Result |
|---|---|---|
| Samsung Galaxy Tab A9 / SM-X115 | Mali-G57 MC2 | Khorinis loads and runs; landscape, touch, lifecycle and ASTC verified |
| Samsung Galaxy A23 5G / SM-A236B | Adreno 619 | Deterministic Qualcomm shader-compiler crash before the first 3D gameplay frame; workaround investigation remains open |
| Pixel Tablet AVD | host Vulkan / x86_64 | Boot, menu, intro, touch and lifecycle smoke-tested |

## Known limitations

- This is still a pre-alpha build; compatibility beyond the devices above is
  unknown.
- Adreno 619 with the tested Samsung driver is currently blocked.
- The ASTC cache is public and not yet fully validated against corrupted or
  malicious files.
- First-run storage permission is not gated before native startup.
- Missing data and fatal errors do not yet produce a native Android dialog.
- Android crash logs do not currently provide a reliable native stack unwind.
- Full Activity destroy/recreate in the same process has not been hardened.

For architecture, build, CI and current engineering priorities, see
[`DEVELOPMENT.md`](DEVELOPMENT.md). Historical implementation details and
measurements live under [`docs/superpowers/`](../docs/superpowers/).

For the upstream engine, desktop builds, mods and contribution guide, see
[Try/OpenGothic](https://github.com/Try/OpenGothic).
