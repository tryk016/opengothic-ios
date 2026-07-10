# iOS port — status & backlog

Tracked work beyond the core "build + run + control" milestone.
Bug ids (B1–B9, N1–N5) refer to the code-review report; phases refer to the
"ideal gamepad" control spec.

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

## ⏳ To do — deferred (needs on-device iteration)
- [ ] **B9 / N1** — pause game tick (`onTimer`) + `displayLink` while
      backgrounded. Deferred: the manual setjmp/longjmp fiber loop in `implExec`
      means naively sleeping the main context starves the UIKit run loop; needs
      careful on-device testing. (Render is already gated on `isApplicationActive`.)
- [ ] Landscape orientation lock — behaviour change; wants coordinated
      `Info.plist` + `ViewController::supportedInterfaceOrientations` update and
      a call on whether portrait should be allowed.

## ⏳ To do — "ideal controls" (bigger, self-contained; control spec §3–§8)
- [ ] Target-lock via native focus (`moveFocus`/`setTarget`) — replaces provisional
      R3→`LookBack`. (spec §3)
- [ ] Radial rings: weapons (RB) + items (LB) quick-bars. (spec §4)
- [ ] Controls-help overlay + button glyphs (Xelu CC0) + target reticle. (spec §5)
- [ ] Rotating quick-saves with auto-names. (spec §6)
- [ ] Haptics via `GCController.haptics` / Core Haptics. (spec §7)
- [ ] Stuck-protection + `[GAMEPAD]` config in `Gothic.ini` (dead-zones,
      sensitivity, invert-Y, save slots). (spec §8) — wires the `invertY` field.

## UI / readability (pre-existing)
- [x] Scale up UI on iOS for high-DPI legibility (`MainWindow::uiScale`).
- [ ] Dialogue subtitle window is small — enlarge / reflow for phone screens.
- [ ] Dialogue **choice** list is small — enlarge and make touch-friendly.
- [ ] Verify main-menu text size after the uiScale change; tune if needed.

## Language
- [ ] Polish requires Polish game data (e.g. GOG Gold Edition or a PL install);
      then automatic, or force via `[GAME] language=2` in Documents/Gothic.ini.
