# OpenGothic on iOS — build & install guide

Play OpenGothic (Gothic II: Night of the Raven) on iPhone/iPad with a Bluetooth
controller or the complete on-screen virtual gamepad. Two routes are documented:
**no Mac** (recommended for you) and **with a Mac**.

> You must legally own Gothic II: NotR. The game ships **no** assets; you supply
> them from your own installation.

---

## Route A — No Mac, no build (download + sideload)

### 1. Choose and get the unsigned .ipa

You do **not** need to fork or build anything. Two maintained variants use the
same game code, controls, saves and iOS performance profile:

| Variant | When to use it | SideStore source | Direct IPA |
|---|---|---|---|
| **MetalFX Temporal — recommended** | First choice on supported iPhones/iPads; best reconstruction quality available in this port | `https://github.com/tryk016/opengothic-ios/releases/download/metalfx-temporal/apps.json` | [OpenGothic-MetalFX-Temporal.ipa](https://github.com/tryk016/opengothic-ios/releases/download/metalfx-temporal/OpenGothic-MetalFX-Temporal.ipa) |
| **Lanczos compatibility** | Use if the Temporal build crashes or shows a device-specific graphics problem | `https://github.com/tryk016/opengothic-ios/releases/download/latest/apps.json` | [OpenGothic-unsigned.ipa](https://github.com/tryk016/opengothic-ios/releases/download/latest/OpenGothic-unsigned.ipa) |

In SideStore open **Sources → +**, paste the selected source URL, then install
OpenGothic. New builds from that source appear as updates.

The recommended build uses Apple's MetalFX Temporal upscaler, which combines
the current image with depth, motion information and frame history. At the same
50% or 75% internal render scale it should provide the best image reconstruction
of the available variants. If Temporal is unavailable at runtime, the build
automatically tries MetalFX Spatial and then the existing Lanczos path. This is
an image-quality recommendation, not a guarantee of higher FPS on every device.
The path has been device-tested on Apple A17 Pro at half resolution with water
reflections, sky effects and additional shadows enabled, without observed
artifacts.

The compatibility build always uses the established single-frame Lanczos
upscaler for reduced-resolution rendering and has no MetalFX dependency.

An unsigned IPA cannot be launched by tapping it in Files; SideStore, AltStore
or Sideloadly must sign and install it first.

> Maintainers only: `.github/workflows/ios-metalfx-temporal.yml` publishes the
> recommended MetalFX build, while `.github/workflows/ios.yml` publishes the
> Lanczos compatibility build to the `latest` release. Both use a macOS runner,
> `cmake` + `glslang`, `iphoneos` arm64 and disabled code signing.

### 2. Sign & install with your own free Apple ID
- **SideStore (recommended, refreshes on-device over Wi‑Fi):** after adding the
  selected source above, tap install. SideStore re-signs with your free Apple ID **and
  auto-refreshes the 7-day certificate itself over any Wi‑Fi — no PC needed**
  after the one-time pairing setup. See https://sidestore.io.
- **AltStore / Sideloadly (alternative):** install **AltServer for Windows**
  (needs iTunes + iCloud from Apple's site — **not** the Microsoft Store
  versions) and **AltStore** on the iPhone; sign in with a **free Apple ID**;
  choose **Install .ipa** and select the downloaded Temporal or Lanczos IPA. AltServer re-signs and
  installs over USB/Wi‑Fi, but auto-refresh needs the PC running on the same
  Wi‑Fi. See https://altstore.io.
- On the phone: Settings → General → VPN & Device Management → **trust** your
  developer certificate.
- The free cert **expires after 7 days**; SideStore/AltStore auto-refresh it.
  Free accounts allow **3 sideloaded apps** at a time.

### Updating an existing installation

When the selected SideStore source shows a new version, tap **Update**. Both
variants use the same bundle identifier, so installing one over the other
preserves the app's `Documents` container, including copied game data, saves and
`Gothic.ini`; you do not copy them again.

The Temporal version line is intentionally higher than the Lanczos line.
SideStore may therefore not offer Lanczos as an automatic update after Temporal.
If you need the compatibility build, download its IPA and sign/install it over
the existing app with SideStore, AltStore or Sideloadly. **Do not uninstall the
app first** unless the Documents folder is backed up: uninstalling removes the
game data and saves.

### 3. Copy game data (from Windows)
The app enables File Sharing (`Info.plist`: `UIFileSharingEnabled`,
`LSSupportsOpeningDocumentsInPlace`). Copy the **contents** of your Gothic II
folder into the app's Documents on the first installation:
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
4. Run on the device; trust the certificate as in Route A. Copy game data via
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
| Edit Items ring (inventory) | R3 | R3 |
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
  Weapons / Magic (equipped melee and ranged weapons inside, 8 spell-book
  slots outside). D-pad ↑/↓ also switches between the panels while one is open.
  Aim by right-stick angle and distance, press A or RT to use the selected slot,
  or B to cancel. Slots show **real 3D item icons**.
- **Items starts automatic and can be assigned manually.** Before its first
  edit it takes potions, food and torches from the live inventory (9 outer
  slots first, then 4 inner). In the normal player inventory, highlight any
  non-gold item and press R3; aim at a sector with the right stick, press RT to
  assign, LT to clear, or B to close without another change. The stable layout
  is stored per save.
  Missing/consumed items leave empty bindings and return to the same sector
  when acquired again. A lit torch is added synthetically so it can be stowed.
  Weapons / Magic remains automatic and uses equipped melee/ranged weapons and
  all active spell-book slots 3–10.
- **Target lock-on** (R3) pins the current focus via the engine's native focus
  (not a camera hack); it auto-releases when the target dies or leaves. While
  locked, **D-pad ◀ / ▶** steps to the previous / next target.
- **Contextual combat:** LT blocks in melee and aims a bow; RT attacks, shoots
  or casts. LB/RB become left/right melee attacks and otherwise provide
  walk/look-back. Outside target lock, D-pad ◀/▶ opens status/the quest log;
  while target lock is active it selects the previous/next target.
- **Inventory:** LB/RB jumps to the previous/next sorted item category; R3
  opens Items-ring assignment for the highlighted player item; the sticks and
  D-pad retain normal grid navigation.
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
Existing explicit menu choices are preserved. Profile version 2 performs one
targeted migration: the old generated 512 px shadow setting becomes 1024 px and
a missing FPS choice becomes the new 30 FPS default. Other existing values are
not auto-populated or replaced. A launch that stops at the missing-data alert
happens before profile creation and will try again after the game data is copied.

```ini
[GAME]
useQuickSaveKeys=1

[INTERNAL]
vidResIndex=2
iosProfileVersion=2

[ENGINE]
zCloudShadowScale=0
shadowResolution=1024
zMaxFpsMode=1

[GAMEPAD]
deadZone=0.25
releaseZone=0.15
crossAxisGuard=0.12
triggerThreshold=0.50
lookSensitivity=0.20
invertY=0
```

When upgrading from an older build, a root override may already exist with only
the settings saved previously. Keep it: the targeted version-2 migration above
runs once, while all unrelated values and explicit FPS selections stay intact.
You can still rename/delete the override once to regenerate the complete profile.
The copied `system/Gothic.ini` is unaffected.

`vidResIndex=2` renders only the 3D scene at half resolution and upscales it;
HUD, menus and subtitles remain native and sharp. `zCloudShadowScale=0` disables
the expensive SSAO option labelled “Cloud shadows”. Both can still be changed
in Options → Video. Keep `releaseZone < deadZone`; `crossAxisGuard` rejects the
small perpendicular component of imperfect cardinal stick motion (`0` disables
the guard).

Options → Video → **Drawing distance** now changes the world far plane immediately
and persists through the existing `PERFORMANCE/sightValue` setting. 100% is
approximately 1 km; 80%, 60% and 40% are approximately 800, 600 and 400 m. This
controls the world view distance and is separate from object-specific fading.

Optional overrides can be added to the same root file:

```ini
[GAME]
showFpsCounter=1        ; on-screen FPS counter
language=2              ; force Polish when Polish data is installed

[GAMEPAD]
noStuckProtect=1        ; disable the L3+R3 unstuck warp
```

The controller does not reserve any shortcut for quick save/load. The
`useQuickSaveKeys` engine option above remains available for keyboard F5/F9.

The build defaults to 30 FPS to reduce sustained iPhone load. On iOS,
Options → Game repurposes the existing “Gothic 1 controls” choice as
“FPS limit”, with Off, 30 and 60 FPS values. The selection is applied
immediately and persisted in the root `Documents/Gothic.ini` as
`ENGINE/zMaxFpsMode` (`0`, `1` or `2`). The original `GAME/useGothic1Controls`
value remains unchanged, so the selected combat-control scheme is preserved.
The iOS limiter changes the native `CADisplayLink` cadence; it does not sleep
the render/UI thread. Off requests the adaptive 30–120 Hz ProMotion range, while
30 and 60 request fixed display-link rates.

The production iOS profile also uses three frames in flight, renders directly
to the Metal drawable, allocates SSAO targets only when SSAO is enabled, and
avoids unnecessary full skeletal-pose work for distant/offscreen NPCs while
preserving their animation events. These are build-level optimizations and do
not require additional `Gothic.ini` entries.

Saving now shows its banner immediately. The slot preview is captured through a
small render attachment and read back only after the frame fence, avoiding the
old synchronous Metal-driver abort. If preview capture fails, saving still
continues with a placeholder. This path has been confirmed on a physical device.

## Increased memory limit and free signing

An `Info.plist` key cannot grant an entitlement. Entitlements come from the
provisioning profile and the app's final code signature. SideStore re-signs the
downloaded IPA with the capabilities available to your Apple ID; with a free
account, `com.apple.developer.kernel.increased-memory-limit` is normally absent.
The game detects that condition and runs against the normal iOS memory ceiling.
No file needs to be removed from the downloaded IPA or from `Info.plist.in` to
install the regular release.
