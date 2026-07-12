# OpenGothic on iOS — build & install guide

Play OpenGothic (Gothic II: Night of the Raven) on iPhone/iPad with a Bluetooth
controller. Two routes are documented: **no Mac** (recommended for you) and
**with a Mac**.

> You must legally own Gothic II: NotR. The game ships **no** assets; you supply
> them from your own installation.

---

## Route A — No Mac, no build (download + sideload)

### 1. Get the unsigned .ipa
You do **not** need to fork or build anything. Two ways, easiest first:

- **SideStore source (recommended — one tap + auto-updates):** in SideStore →
  **Sources → +**, paste
  `https://github.com/tryk016/opengothic-ios/releases/download/latest/apps.json`,
  then install **OpenGothic** from that source. New builds appear as updates.
- **Direct download:** open the
  **[Releases page](https://github.com/tryk016/opengothic-ios/releases/latest)** and
  download the `OpenGothic-unsigned.ipa` asset.

> Maintainers only: the workflow `.github/workflows/ios.yml` (macos runner,
> `cmake`+`glslang`, `iphoneos` arm64, signing disabled) builds the `.ipa` and
> **publishes it to the `latest` Release automatically** on every push, plus
> uploads it as an artifact. Trigger via push or Actions → "iOS build" → Run
> workflow. Regular users never need this.

### 2. Sign & install with your own free Apple ID
- **SideStore (recommended, refreshes on-device over Wi‑Fi):** after adding the
  source above, tap install. SideStore re-signs with your free Apple ID **and
  auto-refreshes the 7-day certificate itself over any Wi‑Fi — no PC needed**
  after the one-time pairing setup. See https://sidestore.io.
- **AltStore / Sideloadly (alternative):** install **AltServer for Windows**
  (needs iTunes + iCloud from Apple's site — **not** the Microsoft Store
  versions) and **AltStore** on the iPhone; sign in with a **free Apple ID**;
  choose **Install .ipa** → `OpenGothic-unsigned.ipa`. AltServer re-signs and
  installs over USB/Wi‑Fi, but auto-refresh needs the PC running on the same
  Wi‑Fi. See https://altstore.io.
- On the phone: Settings → General → VPN & Device Management → **trust** your
  developer certificate.
- The free cert **expires after 7 days**; SideStore/AltStore auto-refresh it.
  Free accounts allow **3 sideloaded apps** at a time.

### 3. Copy game data (from Windows)
The app enables File Sharing (`Info.plist`: `UIFileSharingEnabled`,
`LSSupportsOpeningDocumentsInPlace`). Copy the **contents** of your Gothic II
folder into the app's Documents:
- Source: `C:\Program Files (x86)\Steam\steamapps\common\Gothic II`
- Copy the `Data/`, `_work/`, and `system/` folders in.
- Options: **iTunes for Windows** → device → File Sharing → OpenGothic → Add;
  or the **Files** app on the device (e.g. from a USB drive / iCloud Drive).

The game auto-detects data in Documents (`commandline.cpp`) and validates
`Data`, `_work/Data`, and compiled scripts. If data is missing you get an
on-screen alert instead of a crash.

---

## Route B — With a Mac (Xcode, free Apple ID)

1. `brew install cmake glslang`; install Xcode + `xcode-select --install`.
2. `./ios/build-ios.sh` then `open build-ios/Gothic2Notr.xcodeproj`.
3. Xcode → Settings → Accounts → add your **free** Apple ID (a "Personal Team"
   appears). Target → Signing & Capabilities → **Automatically manage signing**,
   pick the personal team, set a unique bundle id if needed.
4. If signing fails on `com.apple.developer.kernel.increased-memory-limit`,
   remove that key from `Info.plist.in` (see below) and rebuild.
5. Run on the device; trust the certificate as in Route A. Copy game data via
   Finder → device → Files. Same 7-day expiry (rebuild weekly, or use AltStore).

---

## Recommended settings (do this first)

The engine defaults to full native resolution with SSAO on — on a phone that
means ~17–25 fps, sluggish menus and slow saves. Set this once after the first
launch; the change applies live, no restart needed:

**Path A — in-game menu (no file editing):**
1. Options → Video → *resolution*: **upscale(half)** (safe everywhere) or
   **upscale(75%)** (newer devices, e.g. A17 Pro / A18).
2. Options → Video → untick **Cloud shadows** — in OpenGothic this switch is
   actually SSAO; disabling it is the single biggest fps win.

Only the 3D scene is rendered at the reduced resolution and upscaled (Lanczos);
HUD, menus and subtitles always stay sharp at native resolution.

**Path B — the same via config files:** in `Documents/Gothic.ini` set
`vidResIndex` and `zCloudShadowScale` as shown in the config reference below.

**Optional frame cap** — steadier pacing and less thermal throttling on long
sessions. Create `Documents/system/SystemPack.ini` (if it does not exist):

```ini
[PARAMETERS]
FPS_Limit=30       ; 0 = uncapped
```

---

## Controls (Gothic Classic scheme)

| Function | Xbox | PS5 |
|---|---|---|
| Primary interact / attack | A | ✕ |
| Secondary / draw weapon | Y | △ |
| Jump / climb / swim up | B | ○ |
| Crouch / dive / sneak | X | □ |
| Move | Left stick | Left stick |
| Look | Right stick | Right stick |
| Toggle sprint | L3 | L3 |
| Lock-on target | R3 | R3 |
| Block / parry | RT | R2 |
| Quick-access items | D-pad | D-pad |
| Inventory | View | Touchpad |
| Pause menu | Menu | Options |
| Quick save | LB + Menu | L1 + Options |
| Quick load | LB + View | L1 + Touchpad |

---
## Config reference (Documents/Gothic.ini)

`Documents/Gothic.ini` is created by the app on first run. Its values override
the `system/Gothic.ini` you copied from the PC, so this is the right place for
device-specific tweaks. Add only the entries you want to change:

```ini
[GAME]
showFpsCounter=1        ; on-screen FPS counter (mobile)
language=2              ; force Polish if you have Polish data
useQuickSaveKeys=1      ; enable quick save/load (may default off)

[INTERNAL]
vidResIndex=2           ; 3D render scale: 0=full, 1=upscale(75%), 2=upscale(half)

[ENGINE]
zCloudShadowScale=0     ; 0 = SSAO off ("Cloud shadows" in the video menu)

[GAMEPAD]
deadZone=0.25           ; stick dead zone (0..1)
triggerThreshold=0.50   ; how far RT/R2 must be pressed to register (0..1)
lookSensitivity=0.20    ; right-stick look speed
invertY=0               ; 1 = invert vertical look
saveSlots=5             ; rotating quick-save slot count
;noStuckProtect=1       ; disable the L3+R3 unstuck warp
;padQuickSlot=<n>       ; managed automatically (rotating save index)
```

A connected controller works out of the box — every `[GAMEPAD]` value above is
the built-in default. Add a line only when you want a different value.

## Known limitations / follow-ups
- **Quick save/load** combos are detected but not yet bound to an engine action
  (`KeyCodec::Action` has no save/load entry) — currently logged. Needs a hook.
- **Lock-on** is mapped provisionally to `LookBack`; revisit against Gothic's
  targeting.
- **Mesh shaders** are defaulted **off** on iOS for GPU compatibility; modern
  Apple GPUs (A17 Pro / M-series) could re-enable them.
- **No TestFlight / App Store** — requires a paid Apple Developer account and
  App Review (which would reject an engine reimplementation + copyrighted data).
  Sideloading for personal use avoids review entirely.
- `increased-memory-limit` entitlement may be stripped under free signing; large
  worlds rely on the device's default per-app limit (fine on modern devices).

## Disabling the increased-memory-limit entitlement
If free signing rejects it, delete these two lines from `Info.plist.in`:
```xml
  <key>com.apple.developer.kernel.increased-memory-limit</key>
  <true/>
```
