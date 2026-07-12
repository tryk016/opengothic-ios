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

## Controls (Gothic Classic scheme)

[![OpenGothic controller mapping for Xbox and PlayStation pads](../assets/controller/OpenGothic_Controller_Layout.svg)](../assets/controller/OpenGothic_Controller_Layout.svg)

<details>
<summary>Text alternative: complete button mapping</summary>

| Function | Xbox | PlayStation |
|---|---|---|
| Interact / attack | A | ✕ |
| Draw / sheathe weapon | Y | △ |
| Jump / climb / swim up | B | ○ |
| Sneak / dive | X | □ |
| Move / turn | Left stick | Left stick |
| Camera | Right stick | Right stick |
| Magic quick-ring | RB (hold) | R1 (hold) |
| Item quick-ring | LT (hold) | L2 (hold) |
| Walk / run | L3 | L3 |
| Target lock | R3 | R3 |
| Switch locked target | flick Right stick ← / → | flick Right stick ← / → |
| Parry | RT | R2 |
| Melee / ranged weapon | D-pad ↑ / ↓ | D-pad ↑ / ↓ |
| Quick slots 1 / 2 | D-pad ← / → | D-pad ← / → |
| Inventory | View | Share / Create |
| Game menu | Menu | Options |
| Quick save | LB + Menu | L1 + Options |
| Quick load | LB + View | L1 + Share / Create |
| Unstuck teleport | hold L3 + R3 ~2 s | hold L3 + R3 ~2 s |

</details>

Notes on feel and on-screen input:
- **Quick-rings** are *hold-to-aim*: hold the ring button, steer the highlight
  with the right stick, release to equip magic / use an item. Contents are
  pulled live from your inventory and shown as **real 3D item icons**.
- **Target lock-on** (R3) pins the current focus via the engine's native focus
  (not a camera hack); it auto-releases when the target dies or leaves. While
  locked, a hard horizontal **flick of the right stick** steps to the next /
  previous target.
- **Quick slots** (D-pad ◀ / ▶) fire any item you bind to them — potion, food,
  rune, scroll, torch. To bind: open the inventory, highlight the item and
  **hold D-pad ◀ or ▶ for ~0.6 s** (a short press still navigates). Slots
  persist in `Gothic.ini`; unassigned slots drink the best healing (◀) / mana
  (▶) potion. A slot bound to the torch toggles it: first press lights the
  torch, second press stows it back into the inventory (bind the torch while
  one is still in the inventory — a lit torch has no row to highlight).
- **Left-stick response:** forward/back keeps Gothic's animation-driven motion
  but uses separate press/release thresholds; horizontal turning is scaled by
  the stick deflection. Rings, UI transitions, controller resets and app resume
  release held world actions and require a return to neutral before re-arming.
- **On-screen virtual gamepad:** with no controller, a full pad is drawn during
  play (buttons, sticks, D-pad, move + camera area). Tap a ring button, drag to
  aim, release to activate. It **auto-hides the moment a controller connects**.
- A **lock-on reticle** brackets the pinned target (gamepad only). The former
  transient controls-hint bar is disabled; the complete mapping lives in
  Options → Controls.
- **Controller-layout screen:** Options → Controls shows a full pad diagram
  (Xelu line-art) with leader lines, button glyphs and localized labels
  (EN/DE/PL) for the whole mapping — instead of the keyboard bindings list,
  which is dead weight without a keyboard. B/Escape (or the touch Back button)
  returns to Options.
- **Haptics** fire on taking damage, on lock-on / ring-commit, and on quick-save.
- **HUD is safe-area aware** — HP/mana/swim bars, world clock and fps counter
  avoid the notch / Dynamic Island and the rounded corners; the 3D scene stays
  full-bleed.

---
## iOS configuration

The app uses two separate files; it never overwrites the PC configuration you
copied with the game data:

1. `Documents/system/Gothic.ini` — read-only base from the PC install.
2. `Documents/Gothic.ini` — writable iOS override, with higher priority.

On a fresh install, when the second file does not exist, OpenGothic creates it
immediately with this complete iOS profile. An existing override is not
auto-populated or replaced; values changed later through the game menu are still
written there normally.

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
saveSlots=5
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

Quick-slot ids and the rotating-save index are managed by the game and written
to this override automatically.

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
> unfinished. Both are now implemented and **device-confirmed** — quick save/load
> (incl. the earlier save crash-to-home) works from menus and gamepad, and lock-on
> uses the engine's native focus. See [`TODO.md`](TODO.md) for the full status.

## Disabling the increased-memory-limit entitlement
If free signing rejects it, delete these two lines from `Info.plist.in`:
```xml
  <key>com.apple.developer.kernel.increased-memory-limit</key>
  <true/>
```
