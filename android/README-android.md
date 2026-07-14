# OpenGothic on Android — install guide

> ### ⚠️ Work in progress — not ready for players yet
> The Android port boots *Gothic II: Night of the Raven*, reaches the main
> menu, starts a new game and plays the intro cutscene, and survives
> backgrounding/resuming — but so far only on a **desktop emulator**. It has
> **not** been validated on real phones, nor under the low-RAM conditions
> (roughly 4 GB) that are the actual target hardware. Expect crashes, no
> settings persistence, and untuned touch controls. The published build is
> explicitly marked **"do not install"** on its GitHub Release. Only install
> it if you want to help test an early, unfinished build — not to play the
> game yet.

You must legally own *Gothic II: Night of the Raven*. OpenGothic ships **no**
game assets or scripts; you supply them yourself from your own installation.

---

## 1. Get the APK

Download the latest test build from the
**[`latest-android` release](https://github.com/tryk016/opengothic-ios/releases/tag/latest-android)**
(asset `OpenGothic.apk`). It rebuilds and republishes automatically on every
push to the `android` branch — see
[`DEVELOPMENT.md`](DEVELOPMENT.md#ci-and-distribution) for the CI pipeline.
Every build is signed with the same committed keystore, so installing a new
one over an older one behaves like a normal app update (no uninstall needed).

## 2. Allow installing from this source

Android blocks installing APKs from outside the Play Store by default. When
you open the downloaded file (or tap "Install" from your browser's or file
manager's download notification), Android prompts to allow that specific app
to install unknown apps — accept it, then continue the install. (Manually:
Settings → Apps → Special app access → Install unknown apps.)

## 3. Install and grant storage access

Install the APK, then launch it. On first launch it needs full storage
access to read your game files from `/sdcard/`:

- The app automatically opens **Settings → Allow access to manage all
  files**. Toggle it **on** for OpenGothic, then go back to the app (or
  relaunch it if it doesn't resume on its own).
- This is Android's `MANAGE_EXTERNAL_STORAGE` ("All files access")
  permission — required because Gothic's data folder layout doesn't fit the
  scoped-storage model regular apps use.

## 4. Copy your game data

Copy the **contents** of your Gothic II: NotR installation onto the device,
under:

```
/sdcard/OpenGothic/Gothic2/
├── Data/
├── _work/
└── system/
```

That is: copy the `Data/`, `_work/`, and `system/` folders from something like
`C:\Program Files (x86)\Steam\steamapps\common\Gothic II` into
`/sdcard/OpenGothic/Gothic2/` on the device — over a USB cable (MTP file
transfer) or with any file manager on the device itself. Saves and the
writable `Gothic.ini` override are created one level up, directly under
`/sdcard/OpenGothic/`.

If the data isn't there yet, or storage access wasn't granted, you'll see an
on-screen "Gothic II data not found" message instead of a crash — copy the
data (or grant the permission), then relaunch.

## 5. Play

Launch **OpenGothic**. Menus are navigated by touch: tap the buttons on the
on-screen control overlay (the same overlay used by the iOS port) rather than
tapping menu text directly — e.g. tap the confirm/"A" button to select "Nowa
gra" (New Game). There's no Android-tuned virtual gamepad yet, no
Bluetooth/USB controller support, and settings persistence across restarts is
unconfirmed — see [`DEVELOPMENT.md`](DEVELOPMENT.md#remaining--deferred-work)
for the full list of what's still missing.

---

### Requirements

- Android 8.0 (API 26) or newer.
- A Vulkan-capable GPU (the manifest requires the
  `android.hardware.vulkan.version` feature).
- An `arm64-v8a` device for real phones/tablets. An `x86_64` build is also
  produced, but that's for desktop-emulator testing, not real hardware.

### Known limitations (M1)

- Not yet tested on real hardware, or under the real (~4 GB) memory pressure
  that is the actual target — only a desktop emulator has run this build so
  far.
- On-screen controls are the shared iOS overlay, untuned for Android; no
  physical controller or haptics support yet.
- `Gothic.ini` settings persistence across restarts hasn't been verified.
- Expect rough edges generally — this is a first boot-to-menu milestone, not
  a finished port.

---

*For the engine itself — Windows/Linux/macOS builds, features, mods,
command-line arguments, graphics options, and the contribution guide — see
the upstream project:* **[Try/OpenGothic](https://github.com/Try/OpenGothic)**.
*Maintainer/architecture notes for this port are in [`DEVELOPMENT.md`](DEVELOPMENT.md).*
