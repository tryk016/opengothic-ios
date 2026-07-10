# iOS port — status & backlog

Tracked work beyond the core "build + run + control" milestone.
Bug ids (B1–B9, N1–N5) refer to the code-review report; phases refer to the
"ideal gamepad" control spec.

## ✅ Done — device-test round 1 fixes (2026-07-10)
- [x] **Save crash** — `MainWindow::saveGame` captured a GPU thumbnail
      (`screenshoot`+`submit`+`readPixels`) which aborts in the Metal driver on
      iOS. On iOS: skip that path, save a small placeholder preview + empty
      background; also guard the saving screen against an empty banner texture.
- [x] **Skip cutscenes** — output-only cutscene lines (`state==Idle`,
      `current.time>0`) ignored all input; `DialogMenu::keyDownEvent` now lets
      Esc/Return (touch Skip, pad B) call `skipPhrase` there too.
- [x] **Keep screen awake** — `application.idleTimerDisabled = YES` in
      `applicationDidBecomeActive` (via `apply-patches.sh`); the display no
      longer dims/locks mid-game when using a gamepad.
- [x] **Game Mode** — `GCSupportsGameMode` + `LSApplicationCategoryType =
      public.app-category.games` in `Info.plist` (lets iOS 18+ treat it as a game).
- [x] **FPS counter** — the existing overlay (drawn on mobile) is now toggled by
      `Gothic.ini [GAME] showFpsCounter=1` (read in `setupSettings`). NOTE: a
      literal checkbox inside the in-game options menu needs MENU.DAT (game-data)
      editing — not doable from engine code; the ini flag is the practical toggle.

## ✅ Done — critical bugfix cluster (2026-07-10)
- [x] **Dialogue voice-over** — root cause was iOS unconditionally skipping
      `Speech*.vdf` (OOM guard for iPhone 7). Now mounted on ≥4 GB devices,
      skipped only on <4 GB. ZenKit mmaps archives → low resident cost.
      (`resources.cpp`)
- [x] **B7** — split fatal exception handling: `GothicNotFoundException` shows the
      "data not found" alert + keeps a run-loop (safe, pre-window); any other
      exception logs + exits without spinning a second `Application` over a
      half-torn-down window. (`main.cpp`)
- [x] **B1** — Escape / Inventory now live on pad **and** touch: window-level
      `MainWindow::uiAction()` (shared with the keyboard path), called instead of
      the no-op `PlayerControl::onKeyPressed`. (`mainwindow.*`, `touchinput.cpp`)
- [x] **B2** — context-aware pad dispatcher (`PadCtx` + `padContext()` +
      `dispatchKey()`): pad drives menus / dialogue / inventory via synthetic
      key events, not just gameplay. (`gamepadinput.*`, `mainwindow.*`)
- [x] **B3** — gamepad quick save/load wired to `Gothic::quickSave/quickLoad`
      with the F5/F9 guards (LB+Menu / LB+View). (`gamepadinput.cpp`)
- [x] **B5** — pad disconnect mid-hold releases all world actions (no stuck keys).
- [x] **B6** — camera Y sign unified with the touch overlay; `invertY` field added.
- [x] Touch navigation for **inventory** — resolved by the same dispatcher.

## ✅ Done — iOS lifecycle & audio hardening (2026-07-10)
- [x] **B4** — `touchesCancelled` handled in Tempest `iosapi.mm` (forwards to
      `touchesEnded`); fixes stuck movement + leaked touch id on system touch
      cancel. Applied via `ios/patches/apply-patches.sh`.
- [x] **B8** — `AVAudioSession` (Playback category, activated before any
      SoundDevice); linked `-framework AVFoundation`.
      (`game/utils/audiosession.{h,mm,cpp}`, `main.cpp`, `CMakeLists.txt`)
- [x] **N2** — `implDestroyWindow` invalidates `displayLink` + nulls `owner`
      (pairs with B7). Applied via `apply-patches.sh`.
- [x] **N3** — fiber stack 1 MB → 8 MB (deep VM/vob recursion headroom).
      Applied via `apply-patches.sh`.
- [x] **N5** — `Info.plist.in`: `UIRequiresFullScreen`, `ITSAppUsesNonExempt-
      Encryption=false`, `GCSupportsControllerUserInteraction`,
      `UIApplicationSupportsIndirectInputEvents`.

## ✅ Done — input coverage + landscape (2026-07-10)
- [x] **Touch dialogue controls** — the overlay is now context-aware
      (`MainWindow::padContext`): in dialogue it shows Up/Down (pick choice),
      Select (K_Return) and **Skip** (K_ESCAPE → `skipPhrase`), so lines can be
      skipped and choices confirmed from touch. (Pad already did this via B2.)
- [x] **Touch menu value editing** — menu/inventory overlay gained a full D-pad
      incl. **◀ ▶** (K_Left/K_Right), so sliders/options can be changed, plus
      OK/Back. Routed through `dispatchKey` to the active widget.
- [x] **Landscape orientation lock** — `Info.plist` landscape-only (iPhone+iPad)
      and both `supportedInterfaceOrientations` in `iosapi.mm` restricted to
      `MaskLandscape` (via `apply-patches.sh`); still rotates Left/Right.

## ⏳ To do — deferred (needs on-device iteration)
- [ ] **B9 / N1** — pause game tick (`onTimer`) + `displayLink` while
      backgrounded. Deferred: the manual setjmp/longjmp fiber loop in `implExec`
      means naively sleeping the main context starves the UIKit run loop; needs
      careful on-device testing. (Render is already gated on `isApplicationActive`.)

## ✅ Done — ideal controls, phase 3 (2026-07-10)
- [x] **Target-lock via native focus** (spec §3) — R3 pins the current npc focus
      (`PlayerControl::toggleTargetLock`); `tickFocus` keeps it instead of
      re-finding by aim, and auto-releases when the target dies/leaves
      (`World::validateFocus` + `Npc::isDead`). D-pad ←/→ switch target
      (`focusLeft/focusRight` → `moveFocus`). Existing focus highlight shows it.
      Replaces the provisional R3→`LookBack`.

## ✅ Done — ideal controls, batch 1 (2026-07-10)
- [x] **[GAMEPAD] config** (spec §8) — `deadZone`, `triggerThreshold`,
      `lookSensitivity`, `invertY`, `saveSlots` read from `Gothic.ini [GAMEPAD]`
      in `GamepadInput::loadConfig`. Replaces the hard-coded tunables.
- [x] **Rotating quick-saves** (spec §6) — LB+Menu saves to `save_slot_1..N`
      (N=`saveSlots`), auto-named `Quick - <world>`; index persisted in
      `[GAMEPAD] padQuickSlot`. LB+View loads the last rotating slot.
- [x] **Controls-help overlay** (spec §5) — context-sensitive button hints,
      flashed for ~4 s on context change, gamepad-only. Text glyphs (no bundled
      assets). Target **lock-on reticle** (corner brackets) drawn on the pinned
      target. Touch overlay auto-hides when a gamepad is connected.

## ✅ Done — ideal controls, batch 2 (2026-07-10)
- [x] **Radial rings** (spec §4) — RB opens the weapon quick-bar, LT the item
      quick-bar; hold to aim with the right stick, release to activate
      (`Npc::useItem` equips weapons / consumes items). Content pulled live from
      inventory (weapons = NF|FF, items = POTION|FOOD), own vector segments with
      text labels + equipped/selection markers. `QuickRing` owned by
      `GamepadInput`, drawn by `MainWindow`.

## ✅ Done — ideal controls, batch 3 (2026-07-10)
- [x] **Haptics** (spec §7) — `Haptics::impact` via `UIImpactFeedbackGenerator`
      (haptics.mm/.cpp split like systemmsg). Heavy pulse when the player's HP
      drops (polled, no combat-code changes); light on lock/ring-commit; medium
      on quick-save.
- [x] **Stuck-protection** (spec §8) — hold L3+R3 ~2 s to warp to the nearest
      waypoint (`World::findWayPoint` + `Npc::setPosition`); opt out with
      `[GAMEPAD] noStuckProtect=1`.

**Ideal controls §4–§8 complete.** Everything below is polish / needs on-device
tuning (ring feel, haptic intensity, hint wording).

## ✅ Done — UI / readability
- [x] Scale up UI on iOS for high-DPI legibility (`MainWindow::uiScale`).
- [x] Dialogue subtitle window enlarged / reflowed for phone screens.
- [x] Dialogue **choice** list enlarged and made touch-friendly.
- [x] Main-menu text size verified after the uiScale change.

## ⏳ To do — Language
- [ ] Polish requires Polish game data (e.g. GOG Gold Edition or a PL install);
      then automatic, or force via `[GAME] language=2` in Documents/Gothic.ini.
