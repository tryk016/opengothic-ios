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

## Controls (contextual Gothic scheme)

Maintainers: the input pipeline, combat semantics, ring layout and device-test matrix
are documented in [`CONTROLLER-TECHNICAL.md`](CONTROLLER-TECHNICAL.md).

[![OpenGothic controller mapping for Xbox and PlayStation pads](../assets/controller/OpenGothic_Controller_Layout.svg)](../assets/controller/OpenGothic_Controller_Layout.svg)

<details>
<summary>Text alternative: complete button mapping</summary>

| Function | Xbox | PlayStation |
|---|---|---|
| Interact / use / confirm | A | ✕ |
| Melee special / Back | B | ○ |
| Jump / climb | X | □ |
| Draw / sheathe weapon | Y | △ |
| Move / turn | Left stick | Left stick |
| Camera | Right stick | Right stick |
| Draw bow / aim; melee block | LT | L2 |
| Draw melee; attack / shoot / cast | RT | R2 |
| Walk; melee left attack | LB | L1 |
| Look back; melee right attack | RB | R1 |
| Sneak | L3 | L3 |
| Target lock | R3 | R3 |
| Items ring | D-pad ↑ | D-pad ↑ |
| Weapons / Magic ring | D-pad ↓ | D-pad ↓ |
| Status / previous combat target | D-pad ← | D-pad ← |
| Quest log / next combat target | D-pad → | D-pad → |
| Inventory (tap) / Map (hold) | View | Share / Create |
| Game menu | Menu | Options |
| Unstuck teleport | hold L3 + R3 ~2 s | hold L3 + R3 ~2 s |

</details>

Notes on feel and on-screen input:
- **There are two separate quick-rings**, and each has two concentric rows.
  D-pad ↑ opens Items (4 inner + 9 outer slots); D-pad ↓ opens
  Weapons / Magic (equipped melee and ranged weapons inside, 7 spell-book
  slots outside). D-pad ↑/↓ also switches between the panels while one is open.
  Aim by right-stick angle and distance, press A or RT to use the selected slot,
  or B to cancel. Slots show **real 3D item icons**.
- **Both panels fill automatically.** Items takes potions, food and torches
  from the live inventory (9 outer slots first, then 4 inner); a lit torch is
  added synthetically so it can be stowed again. Weapons / Magic uses the
  equipped melee/ranged weapons and active spell-book slots 4–10.
- **Target lock-on** (R3) pins the current focus via the engine's native focus
  (not a camera hack); it auto-releases when the target dies or leaves. While
  locked, **D-pad ◀ / ▶** steps to the previous / next target.
- **Contextual combat:** LT blocks in melee and aims a bow; RT attacks, shoots
  or casts. LB/RB become left/right melee attacks and otherwise provide
  walk/look-back. Outside target lock, D-pad ◀/▶ opens status/the quest log;
  while target lock is active it selects the previous/next target.
- **Inventory:** LB/RB jumps to the previous/next sorted item category; the
  sticks and D-pad retain normal grid navigation.
- **System buttons:** tap View for inventory or hold it for ~0.6 s for the map.
  Menu opens the game menu. Quick save/load remains available through the
  engine's keyboard commands, but is not assigned to the controller.
- **Left-stick response:** forward/back keeps Gothic's animation-driven motion
  but uses separate press/release thresholds; horizontal turning is scaled by
  the stick deflection. Rings, UI transitions, controller resets and app resume
  release held world actions and require a return to neutral before re-arming.
- **On-screen virtual gamepad:** with no controller, a full pad is drawn during
  play (buttons, sticks, D-pad, move + camera area). It mirrors the physical
  pad's contextual mapping and two D-pad quick-rings. It **auto-hides the moment
  a controller connects**. While a ring is open, the corner D-pad ↑/↓ glyphs
  switch panels, B cancels, and a drag/release elsewhere selects and uses a slot.
- A **lock-on reticle** brackets the pinned target (gamepad only). The former
  transient controls-hint bar is disabled; the complete mapping lives in
  Options → Controls.
- **Controller-layout screen:** Options → Controls shows a full pad diagram
  (Xelu line-art) with leader lines, button glyphs and localized labels
  (EN/DE/PL) for the whole mapping — instead of the keyboard bindings list,
  which is dead weight without a keyboard. B/Escape (or the touch Back button)
  returns to Options.
- **Haptics** fire on taking damage and on lock-on / ring-commit.
- **HUD is safe-area aware** — HP/mana/swim bars, world clock and fps counter
  avoid the notch / Dynamic Island and the rounded corners; the 3D scene stays
  full-bleed.

---
## iOS configuration

The app uses two separate files; it never overwrites the PC configuration you
copied with the game data:

1. `Documents/system/Gothic.ini` — read-only base from the PC install.
2. `Documents/Gothic.ini` — writable iOS override, with higher priority.

On the first successful launch after valid game data is installed, OpenGothic
creates the second file with this complete iOS profile when it does not exist.
An existing override — even an empty or malformed one — is not auto-populated
or replaced; values changed later through the game menu are still written there
normally. A launch that stops at the missing-data alert happens before profile
creation and will try again after the game data is copied.

```ini
[GAME]
useQuickSaveKeys=1

[INTERNAL]
vidResIndex=2

[ENGINE]
zCloudShadowScale=0
shadowResolution=512

[GAMEPAD]
deadZone=0.25
releaseZone=0.15
crossAxisGuard=0.12
triggerThreshold=0.50
lookSensitivity=0.20
invertY=0
```

When upgrading from an older build, a root override may already exist with only
the settings saved previously. Keep it and add any desired values from the
profile above, or rename/delete it once to let the app generate the complete
profile on the next launch. The copied `system/Gothic.ini` is unaffected.

`vidResIndex=2` renders only the 3D scene at half resolution and upscales it;
HUD, menus and subtitles remain native and sharp. `zCloudShadowScale=0` disables
the expensive SSAO option labelled “Cloud shadows”. Both can still be changed
in Options → Video. Keep `releaseZone < deadZone`; `crossAxisGuard` rejects the
small perpendicular component of imperfect cardinal stick motion (`0` disables
the guard).

Optional overrides can be added to the same root file:

```ini
[GAME]
showFpsCounter=1        ; on-screen FPS counter
language=2              ; force Polish when Polish data is installed

[GAMEPAD]
noStuckProtect=1        ; disable the L3+R3 unstuck warp
debugInput=1            ; transition-only controller trace in stderr.log
```

The controller does not reserve any shortcut for quick save/load. The
`useQuickSaveKeys` engine option above remains available for keyboard F5/F9.

The build runs uncapped by default for ProMotion. An optional display-rate cap
uses a different file, `Documents/system/SystemPack.ini`:

```ini
[PARAMETERS]
FPS_Limit=60            ; 0 = uncapped; do not cap below normal in-game fps
```

## Known limitations / follow-ups
- **Save-slot thumbnails** are not captured on iOS yet — slots show name, date
  and in-game time, but the preview picture is blank (the GPU screenshot readback
  aborted in the Metal driver, so a placeholder is saved instead).
- **Mesh shaders** are defaulted **off** on iOS for GPU compatibility; modern
  Apple GPUs (A17 Pro / M-series) could re-enable them.
- **No TestFlight / App Store** — requires a paid Apple Developer account and
  App Review (which would reject an engine reimplementation + copyrighted data).
  Sideloading for personal use avoids review entirely.
- `increased-memory-limit` entitlement may be stripped under free signing; large
  worlds rely on the device's default per-app limit (fine on modern devices).

> Quick save/load and native target lock-on were previously listed here as
> unfinished. Both engine features are **device-confirmed** — the earlier save
> crash-to-home is fixed and lock-on uses the engine's native focus. Quick
> save/load is intentionally not mapped to the controller. See
> [`TODO.md`](TODO.md) for the full status.

## Disabling the increased-memory-limit entitlement
If free signing rejects it, delete these two lines from `Info.plist.in`:
```xml
  <key>com.apple.developer.kernel.increased-memory-limit</key>
  <true/>
```
