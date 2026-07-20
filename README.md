# OpenGothic for iOS

An **unofficial iOS port** of [OpenGothic](https://github.com/Try/OpenGothic) — the open-source
re-implementation of *Gothic II: Night of the Raven*. This fork adds the plumbing to build, sideload,
and play OpenGothic on iPhone/iPad with a Bluetooth controller **or** a full on-screen virtual gamepad.

An experimental **Android port** is also in development on the
[`android` branch](https://github.com/tryk016/opengothic-ios/tree/android).
It has been validated on real arm64 hardware and loads/runs Khorinis on the
target Mali-G57 device. The tested Adreno 619 currently crashes inside
Qualcomm's Vulkan shader compiler before the first 3D gameplay frame, so the
port remains pre-alpha. See the current compatibility matrix in
[`android/README-android.md`](android/README-android.md) to try it anyway, or
[`android/DEVELOPMENT.md`](android/DEVELOPMENT.md) for the architecture notes.

> ### ⚠️ Work in progress
> This fork is under **active development**. The core loop — gameplay, the on-screen virtual gamepad,
> save/load with slot previews, haptics and the stable iOS performance profile — has been **tested and
> confirmed on a device**. The hard 30 fps cap is lifted (ProMotion), with optional Off/30/60 FPS
> pacing. The physical-controller movement response and jump landing are also device-confirmed. It is
> still rough in places and being tuned, so expect bugs.

> ### Credit
> **The entire engine is the work of [Try](https://github.com/Try) and the OpenGothic contributors.**
> OpenGothic and its rendering engine [Tempest](https://github.com/Try/Tempest) are what make this
> possible — this fork only finishes and wires up the iOS path. Please support the upstream project:
> ⭐ [Try/OpenGothic](https://github.com/Try/OpenGothic) · 💬 [Discord](https://discord.gg/G9XvcFQnn6).
> Not affiliated with or endorsed by the original authors; distributed under the same [license](LICENSE).
>
> Controller glyphs are **[Xelu's Free Controller & Keyboard Prompts](https://thoseawesomeguys.com/prompts)**
> by Nicolae "Xelu" Berbece (CC0).

![OpenGothic running on iPhone with the on-screen virtual controller](scr0.png)

---

### Prerequisites

*Gothic II: Night of the Raven* is required — OpenGothic ships **no** game assets or scripts. You must
legally own the game and supply its data yourself.

Target: iPhone/iPad on **iOS 15+**, arm64. Best on modern GPUs (A-series / M-series). Locked to landscape.

### Install — download & play (no Mac, no build)

No fork, no compiling — two prebuilt **unsigned `.ipa`** variants are maintained. Detailed guide:
**[ios/README-ios.md](ios/README-ios.md)**.

1. **Install MetalFX Temporal (recommended).** In SideStore: **Sources → +**, paste
   `https://github.com/tryk016/opengothic-ios/releases/download/metalfx-temporal/apps.json`, then
   install OpenGothic MetalFX Temporal. It uses Apple's temporal upscaler for the best reconstruction
   quality available in this port and automatically falls back to MetalFX Spatial and then Lanczos if
   required.
2. **Use the Lanczos compatibility build only if Temporal causes a crash or graphics problem.** Add
   `https://github.com/tryk016/opengothic-ios/releases/download/latest/apps.json`, or download the IPA
   from the [Lanczos compatibility release](https://github.com/tryk016/opengothic-ios/releases/latest).
3. **Add your game data.** Copy the `Data/`, `_work/`, and `system/` folders from your own Gothic II
   install into the app's **Documents** folder (Files app on iOS). This is needed only for the first
   install; normal SideStore updates preserve Documents, saves and settings. Launch and play.

Both variants use the same bundle identifier. Updates and installs over the existing app preserve its
Documents container, but **do not uninstall the app when switching variants**: uninstalling removes
the copied game data and saves unless they are backed up. See the detailed guide if SideStore does not
offer the lower-version Lanczos build as an automatic update.

<sub>Maintainers: trigger the [`MetalFX Temporal`](.github/workflows/ios-metalfx-temporal.yml)
or [`Lanczos compatibility`](.github/workflows/ios.yml) workflow, or use
[`ios/build-ios.sh`](ios/build-ios.sh) + Xcode on a Mac — see the guide.</sub>

### Controls

Two input modes; the on-screen overlay hides automatically when a controller is connected.

**Bluetooth controller (contextual Gothic scheme — Xbox / PlayStation buttons):**

[![OpenGothic controller mapping for Xbox and PlayStation pads](assets/controller/OpenGothic_Controller_Layout.svg)](assets/controller/OpenGothic_Controller_Layout.svg)

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
| Walk; melee left attack; previous Journal/Statistics page | LB | L1 |
| Look back; melee right attack; next Journal/Statistics page | RB | R1 |
| Sneak | L3 | L3 |
| Target lock | R3 | R3 |
| Edit Items ring (inventory) | R3 | R3 |
| Items ring | D-pad ↑ | D-pad ↑ |
| Weapons / Magic ring | D-pad ↓ | D-pad ↓ |
| Quest log / previous combat target | D-pad ← | D-pad ← |
| Map / next combat target | D-pad → | D-pad → |
| Inventory | View | Share / Create |
| Game menu | Menu | Options |
| Unstuck teleport | hold L3 + R3 ~2 s | hold L3 + R3 ~2 s |

</details>

- **Two separate quick-rings:** D-pad ↑ opens the Items ring (4 inner + 9 outer slots);
  D-pad ↓ opens the Weapons / Magic ring (equipped melee and ranged weapons inside, 8 spell-book
  slots outside). These are two panels, not one combined wheel; D-pad ↑/↓ also switches between them
  while open. Aim by the right-stick angle and distance, press A or RT to use the selected slot, or B
  to cancel. Tiles show real 3D item icons.
- **Automatic or assigned Items ring:** until its first edit, the Items ring fills its 9 outer slots first,
  then its 4 inner slots, using potions, food and torches from the live inventory. To customize it, highlight
  any non-gold item in the normal player inventory, press R3, point at a sector with the right stick, then
  press RT to assign it or LT to clear that sector; B closes the editor without another change. The manual layout is stored per save. A consumed
  or missing item leaves its binding empty and reappears there when acquired again. A lit torch is included
  synthetically so it can still be stowed. The Weapons / Magic ring remains automatic and uses equipped gear
  plus all active spell-book slots 3–10.
- **Contextual combat:** LT blocks in melee and aims a bow; RT attacks, shoots or casts. LB/RB become
  left/right melee attacks and otherwise provide walk/look-back. Outside target lock, D-pad ← opens
  the quest journal and D-pad → opens the map; while target lock is active they select the previous/next target.
- **Journal and Statistics:** D-pad ← opens the Journal. On its category screen, D-pad selects a
  category, A enters it and B closes the page. In a quest list, D-pad ↑/↓ selects a quest, A opens its
  text and B returns one level. LB/RB switches directly between the Journal and Statistics pages.
- **Inventory:** LB/RB jumps to the previous/next sorted item category; R3 opens Items-ring assignment
  for the highlighted player item; the sticks and D-pad retain normal grid navigation.
- **System buttons:** View opens the inventory and Menu opens the game menu. Quick save/load remains
  available to the engine through its keyboard commands, but is not
  assigned to the controller.
- **Left-stick response:** the vertical axis keeps Gothic's animation-driven movement with
  press/release hysteresis; the horizontal axis turns proportionally to the deflection. A sloped axial
  guard rejects accidental movement while the
  stick is held mostly sideways (and accidental turning while held mostly forward/back). Returning to
  neutral, opening a ring/UI, disconnecting or resuming the app releases controller-owned actions before
  input can re-arm.
- Config lives in `Documents/Gothic.ini` under `[GAMEPAD]` — `deadZone`, `releaseZone`,
  `crossAxisGuard`, `lookSensitivity`, `invertY`, `triggerThreshold` and `noStuckProtect`.

**On-screen virtual gamepad (no controller):** a full pad is drawn during play — move pad + camera area,
A/B/X/Y, shoulders/triggers, sticks, D-pad, View/Menu — using the Xelu glyphs. It mirrors the physical
pad's contextual mapping and two D-pad quick-rings. Menus and dialogues get on-screen D-pad +
OK/Back/Skip; the Journal and Statistics pages additionally show LB/RB page controls. While a ring is
open, only corner controls remain: D-pad ↑/↓ switches the two panels and
B cancels; drag anywhere else and release to use the selected sector.

### iOS configuration

The copied `Documents/system/Gothic.ini` is never overwritten. On the first
successful launch after valid game data is installed, OpenGothic creates a
separate `Documents/Gothic.ini` override if it is absent, with the complete iOS
profile: half-resolution 3D rendering, SSAO off, 1024 px shadow maps, a 30 FPS
default, quick-save support and all stable `[GAMEPAD]` defaults (including
`crossAxisGuard=0.12`). Existing explicit FPS choices remain unchanged; the
legacy generated 512 px shadow setting is upgraded once to 1024 px.

The generated profile, upgrade note, override priority, optional FPS cap and
diagnostic settings are documented in the
[iOS configuration reference](ios/README-ios.md#ios-configuration).

Options → Video → **Drawing distance** is live on iOS: 100% corresponds to an
approximately 1 km world far plane, while 80%/60%/40% correspond to roughly
800/600/400 m. Options → Game → **FPS limit** provides Off, 30 and 60 FPS;
30 FPS is the iOS default.

### Known limitations

- **Still a work in progress** — the core game loop is device-tested, but expect rough edges and
  ongoing tuning.
- Mesh shaders are disabled on iOS for GPU compatibility.
- On-screen virtual-pad button layout is a first pass and still needs on-device tuning.

### What this fork adds on top of upstream

- **Build/distribution:** cloud build of an unsigned `.ipa` (`.github/workflows/ios.yml`); `ios/` build
  script, sideload/data guide, and submodule patches (`ios/patches/apply-patches.sh`).
- **Controller:** event-driven GameController snapshots (`game/utils/gamepad.*`), a release-safe,
  context-aware dispatcher with left-stick hysteresis and proportional turning that also drives
  menus/dialogues (`game/ui/gamepadinput.*`), native target lock-on, two concentric-row radial panels
  with 3D item icons (`game/ui/quickring.*`), contextual zGamePad-inspired combat controls, haptics
  (`game/utils/haptics.*`), stuck-protection, and a `[GAMEPAD]` config.
- **On-screen input:** a full virtual gamepad + menu/dialogue/inventory controls with controller glyphs
  (`game/ui/touchinput.*`, `game/ui/padglyph.*`, `assets/controller/`), a complete controller-layout
  screen and a lock-on reticle.
- **iOS lifecycle/robustness:** graceful "data not found" message instead of a crash
  (`game/utils/systemmsg.*`), audio-session setup (`game/utils/audiosession.*`), landscape lock, keep
  the screen awake, Game Mode keys, fence-safe save-slot previews with immediate save feedback, and
  dialogue voice-over on ≥4 GB devices.
- **Performance & display:** ProMotion with native Off/30/60 display-link pacing, triple buffering,
  direct Metal drawable rendering, on-demand SSAO buffers, reduced offscreen/distant NPC pose work,
  live menu-controlled drawing distance, safe-area-aware HUD, configurable shadow resolution, and
  the upscale-based render-scale guide. The recommended build adds Apple MetalFX Temporal upscaling
  with automatic MetalFX Spatial and Lanczos fallbacks; a separate Lanczos-only compatibility build
  remains available.
---

*For the engine itself — Windows/Linux/macOS builds, features, mods, command-line arguments, graphics
options, and the contribution guide — see the upstream project:*
**[Try/OpenGothic](https://github.com/Try/OpenGothic)**.
