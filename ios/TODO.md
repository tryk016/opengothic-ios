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

## ⏳ To do — next: iOS lifecycle & audio hardening
- [ ] **B4** — handle `touchesCancelled` in Tempest `iosapi.mm` (Control Center /
      incoming call → stuck movement + leaked touch id). Via `ios/patches/` + PR upstream.
- [ ] **B8** — configure `AVAudioSession` (playback category, interruption obs.);
      link `-framework AVFoundation`. (`game/utils/audiosession.mm`, `CMakeLists.txt`)
- [ ] **B9 / N1** — pause game tick + `displayLink` in background (sim + battery
      drain while backgrounded). (Tempest `iosapi.mm`)
- [ ] **N2** — `implDestroyWindow` empty → invalidate `displayLink`, null `owner`.
- [ ] **N3** — 1 MB fiber stack for the whole engine → enlarge to 8–16 MB / guard page.
- [ ] **N5** — `Info.plist.in`: `UIRequiresFullScreen`, explicit landscape
      orientations, `ITSAppUsesNonExemptEncryption=false`, GameController keys.

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
