## OpenGothic for iOS

An **unofficial iOS port** of [OpenGothic](https://github.com/Try/OpenGothic) — the open-source
re-implementation of *Gothic II: Night of the Raven*. This fork adds the plumbing to build, sideload,
and play OpenGothic on iPhone/iPad with a Bluetooth controller **or** a full on-screen virtual gamepad.

> ### ⚠️ Work in progress
> This fork is under **active development**. The core loop — gameplay, the controller mapping, the
> on-screen virtual gamepad, save/load and haptics — has been **tested and confirmed on a device**,
> and the hard 30 fps cap is lifted (ProMotion). It is still rough in places and being tuned, so
> expect bugs. See [`ios/TODO.md`](ios/TODO.md) for the current, device-verified status and the
> remaining gaps.

> ### Credit
> **The entire engine is the work of [Try](https://github.com/Try) and the OpenGothic contributors.**
> OpenGothic and its rendering engine [Tempest](https://github.com/Try/Tempest) are what make this
> possible — this fork only finishes and wires up the iOS path. Please support the upstream project:
> ⭐ [Try/OpenGothic](https://github.com/Try/OpenGothic) · 💬 [Discord](https://discord.gg/G9XvcFQnn6).
> Not affiliated with or endorsed by the original authors; distributed under the same [license](LICENSE).
>
> Controller glyphs are **[Xelu's Free Controller & Keyboard Prompts](https://thoseawesomeguys.com/prompts)**
> by Nicolae "Xelu" Berbece (CC0).

![Screenshot](scr0.png)

---

### Prerequisites

*Gothic II: Night of the Raven* is required — OpenGothic ships **no** game assets or scripts. You must
legally own the game and supply its data yourself (see *Language & voices* below for edition notes).

Target: iPhone/iPad on **iOS 15+**, arm64. Best on modern GPUs (A-series / M-series). Locked to landscape.

### Install — download & play (no Mac, no build)

No fork, no compiling — a prebuilt **unsigned `.ipa`** is published on every update. Detailed guide:
**[ios/README-ios.md](ios/README-ios.md)**.

1. **Install with SideStore** (recommended). In SideStore: **Sources → +**, paste
   `https://github.com/tryk016/opengothic-ios/releases/download/latest/apps.json`, then install
   OpenGothic. SideStore signs it with your **free Apple ID** and **auto-refreshes the 7-day certificate
   over Wi-Fi** — no manual reinstalling. *(AltStore or Sideloadly also work, using the `.ipa` from the
   [Releases page](https://github.com/tryk016/opengothic-ios/releases/latest).)*
2. **Add your game data.** Copy the `Data/`, `_work/`, and `system/` folders from your own Gothic II
   install into the app's **Documents** folder (Files app on iOS). Launch and play.

<sub>Building it yourself (maintainers only): trigger the [`iOS build`](.github/workflows/ios.yml) Action, or use [`ios/build-ios.sh`](ios/build-ios.sh) + Xcode on a Mac — see the guide.</sub>

### Controls

Two input modes; the on-screen overlay hides automatically when a controller is connected.

**Bluetooth controller (Gothic Classic scheme — Xbox / PS5 buttons):**

| Function | Xbox / PS5 | | Function | Xbox / PS5 |
|---|---|---|---|---|
| Interact / attack | A / ✕ | | Move (stick ◀▶ = turn) | Left stick |
| Jump / climb | B / ○ | | Look | Right stick |
| Sneak / crouch | X / □ | | Block / parry | RT / R2 |
| Draw weapon | Y / △ | | Magic quick-ring (runes, scrolls) | RB / R1 (hold) |
| Walk/run toggle | L3 | | Item quick-ring (potions, food) | LT / L2 (hold) |
| Target lock-on | R3 | | Draw melee / bow | D-pad ↑ / ↓ |
| Switch target (locked) | flick Right stick ◀▶ | | Quick slots (assignable) | D-pad ← / → |
| Pause / menu | Menu / Options | | Inventory | View / Touchpad |
| Quick save / load | LB + Menu / LB + View | | Unstuck warp | hold L3 + R3 ~2 s |

- **Quick-rings:** hold RB (magic) or LT (items), aim with the right stick, release to equip/use —
  tiles show real 3D item icons.
- **Quick slots (D-pad ← / →):** bind any inventory item — potion, food, rune, scroll, torch — by
  highlighting it in the inventory and **holding D-pad ← or → for ~0.6 s**. Unassigned slots drink the
  best healing (←) / mana (→) potion.
- Config lives in `Documents/Gothic.ini` under `[GAMEPAD]` — `deadZone`, `lookSensitivity`, `invertY`,
  `triggerThreshold`, `saveSlots`, `noStuckProtect` (quick-slot bindings are stored there too).

**On-screen virtual gamepad (no controller):** a full pad is drawn during play — move pad + camera area,
A/B/X/Y, shoulders/triggers, sticks, D-pad, View/Menu — using the Xelu glyphs. Tap a quick-ring button,
drag to aim, release to activate. Menus and dialogues get on-screen D-pad + OK/Back/Skip.

### Language & voices

Language — and whether dialogue has voice-over — comes entirely from **your game data**, not the app.
The Steam release is usually English. For Polish (text + voices) you need Polish game data (e.g. the GOG
*Gold Edition*, which is multi-language, or a Polish install/localization). Once Polish data is in place
it is used automatically; you can also force it with `[GAME] language=2` in a `Gothic.ini` placed in the
app's Documents folder.

Dialogue voice-over lives in `Speech.vdf` / `Speech_Addon.vdf` — these are mounted on devices with ≥4 GB
RAM (skipped on iPhone 7/8 to avoid running out of memory, leaving subtitles only).

### Optional settings (`Documents/Gothic.ini`)

```ini
[GAME]
showFpsCounter=1        ; on-screen FPS counter

[INTERNAL]
vidResIndex=2           ; 3D render scale: 0=full, 1=75%, 2=half (big fps win)

[ENGINE]
zCloudShadowScale=0     ; SSAO off ("Cloud shadows" in the video menu)
shadowResolution=512    ; shadow-map size (iOS default 512, PC 2048)

[GAMEPAD]
deadZone=0.25
lookSensitivity=0.20
invertY=0
saveSlots=5             ; rotating quick-save slots
; noStuckProtect=1      ; disable the L3+R3 unstuck warp
```

See the **Recommended settings** section of [ios/README-ios.md](ios/README-ios.md) for the full
performance guide (upscale + SSAO give the biggest wins).

### Known limitations

- **Still a work in progress** — the core game loop is device-tested, but expect rough edges and
  ongoing tuning (see the notice above and `ios/TODO.md`).
- Save-slot preview thumbnails are not captured on iOS yet (slots show name, date and in-game time, but a
  blank picture).
- Sideload certificate expires weekly (auto-refresh via AltStore/SideStore).
- Mesh shaders are disabled on iOS for GPU compatibility.
- On-screen virtual-pad button layout is a first pass and still needs on-device tuning.
- Radial rings are a single-ring first version; some smaller items in `ios/TODO.md` are not done yet.

### What this fork adds on top of upstream

- **Build/distribution:** cloud build of an unsigned `.ipa` (`.github/workflows/ios.yml`); `ios/` build
  script, sideload/data guide, and submodule patches (`ios/patches/apply-patches.sh`).
- **Controller:** GameController support (`game/utils/gamepad.*`), a context-aware dispatcher that also
  drives menus/dialogues (`game/ui/gamepadinput.*`), native target lock-on, radial magic/item rings with
  3D item icons (`game/ui/quickring.*`), Remake-style D-pad with two player-assignable quick slots,
  rotating quick-saves, haptics (`game/utils/haptics.*`), stuck-protection, and a `[GAMEPAD]` config
  section.
- **On-screen input:** a full virtual gamepad + menu/dialogue/inventory controls with controller glyphs
  (`game/ui/touchinput.*`, `game/ui/padglyph.*`, `assets/controller/`), a controls-help hint bar and a
  lock-on reticle.
- **iOS lifecycle/robustness:** graceful "data not found" message instead of a crash
  (`game/utils/systemmsg.*`), audio-session setup (`game/utils/audiosession.*`), landscape lock, keep
  the screen awake, Game Mode keys, a save-crash fix, and dialogue voice-over on ≥4 GB devices.
- **Performance & display:** ProMotion unlock + triple-buffered Metal swapchain (lifts the hard 30 fps
  cap), safe-area-aware HUD (nothing hides under the notch / Dynamic Island), configurable shadow
  resolution, and the upscale-based render-scale guide.
- **Engine fixes (upstream-ready):** interaction users no longer float ~1 m above the ground
  (root-vs-feet regression in `Interactive::attach`), menus get their geometry on first open
  (`MenuRoot::setMenu`), and NPC de-overlap no longer parks characters on top of furniture colliders
  (`GameScript::fixNpcPosition`).

---

*For the engine itself — Windows/Linux/macOS builds, features, mods, command-line arguments, graphics
options, and the contribution guide — see the upstream project:*
**[Try/OpenGothic](https://github.com/Try/OpenGothic)**.
