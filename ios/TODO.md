# iOS port — status & backlog

Controller architecture and its device verification matrix are maintained in
[`CONTROLLER-TECHNICAL.md`](CONTROLLER-TECHNICAL.md).

## ⏳ Backlog — next round (updated 2026-07-13, device round 5)
- [ ] **Right-stick vertical camera — implementation complete, device
      confirmation pending.** The iOS backend already supplied `ry`, but the
      dispatcher sent it only to `PlayerControl`; normal gameplay discards
      `Npc::setDirectionY`, so only horizontal look appeared to work. The pad
      now drives `Camera::onRotateMouse` with per-axis dead-zones and a 50 ms
      local frame-time cap. Verify up/down, `invertY=1`, diagonal look and idle
      drift on hardware.
- [ ] **Contextual zGamePad-style controls + two quick-rings — implementation
      complete, device confirmation pending.** LT blocks/aims, RT attacks,
      shoots or casts, LB/RB provide walk/look-back or melee side attacks, L3
      sneaks and X jumps. D-pad ↑ opens Items (automatic 4+9 slots), while
      D-pad ↓ opens the separate Weapons / Magic panel (2 equipped weapons + 8
      spell-book slots); both use concentric radial selection. D-pad ←/→ opens
      status/log or switches combat focus. View tap/hold opens inventory/map;
      Menu opens the game menu. In normal inventory, R3 opens Items assignment,
      the right stick picks a sector, RT assigns, LT clears and B closes; the
      unique 13-slot layout is stored per save and old saves retain automatic
      mode. Controller quick save/load and FPP are not assigned. Verify every
      context, both rings, assignment save/load, automatic fallback and touch.

## ✅ Done — controller, landing, inventory and shadows (2026-07-12, rounds 2–5)
> **DEVICE-CONFIRMED (round 3):** mobsi levitation gone (player + NPCs), and the
> former ring/icon input foundation worked. **Round 4:** torch
> stow works and shadow maps at 512 are accepted on device. **Round 5:** the
> external-controller movement stalls and post-jump hover are gone.
- [x] **External-controller movement stability — device-confirmed.** The
      event queue (`df4d7f7e`) preserves short press/release transitions, and
      the activation-only cross-axis guard (`bb65d567`) rejects cardinal-axis
      noise without blocking intentional diagonals.
- [x] **Post-jump hover — device-confirmed.** Landing transitions
      `T_JUMP_2_*` are excluded from fresh Run→Jump entry, and a successful
      ground snap is accepted only when its residual distance is within the
      collision epsilon. Temporary `[jump]` / `[jumpup]` logs were removed
      after the clean device round.
- [x] **Torch stow/refund — device-confirmed.** Stowing refunds one
      `ItLsTorch`; light→stow is lossless even for the last lit torch.
- [x] **Legacy Remake-style D-pad retired.** Direct weapon draw and the two
      global `quickSlotL/R` bindings were replaced by the two D-pad radial
      panels and contextual status/log/focus actions described above.
- [x] **Legacy shoulder rings retired.** The former RB magic and LT item rings
      were replaced by the separate, concentric-row Items and Weapons / Magic
      D-pad panels.
- [x] **Target switching moved from a right-stick flick to D-pad ←/→** while
      locked; native focus validation and the lock-on reticle remain.
- [x] **shadowResolution** — no longer hard-coded 2048: `[ENGINE]
      shadowResolution` in Gothic.ini, default **512 on iOS** (1024 showed no
      measurable device gain; 512 is now device-confirmed). Rebuilds live via
      setupSettings.
- [x] **Mobsi levitation — ROOT CAUSE FOUND & FIXED** (device log round 1:
      every attach lands ~1 m up, `nodeDy≈97 fixMoved=0 groundIsMobsi=0`).
      ZS_POS is a skeleton-root point but `Interactive::attach` fed it to
      `Npc::setPosition`, which expects the FEET — upstream regression
      `ac2316d4` ("remove Npc::translateY") dropped the `y-npc.translateY()`
      conversion, and `a2318ba2` (fixNpcPosition early-out when no collision)
      removed the down-ray that used to mask it — a character floating 1 m up
      collides with nothing, so `fixMoved=0`. Not iOS-specific: the May-17
      Windows release (v0.92) has ac2316d4 but predates a2318ba2, hence "works
      on Windows". Fix: convert root→feet via `mv-(centerPosition()-position())`
      before `setPos` (pure-Y, preserves beds/ladders). Diagnostics were
      removed after device confirmation.

Tracked work beyond the core "build + run + control" milestone.
Bug ids (B1–B9, N1–N5) refer to the code-review report; phases refer to the
"ideal gamepad" control spec.

## ✅ Done — fps unlock, HUD safe-area, ring icons, mobsi levitation (2026-07-12)
- [x] **ProMotion unlock** — `preferredFrameRateRange(30,120,120)` on the
      CADisplayLink (apply-patches.sh) + `CADisableMinimumFrameDurationOnPhone`
      in Info.plist. DEVICE-CONFIRMED: menu 60, in-game 40–45 (was hard 30).
- [x] **Triple buffering** — Tempest pins `maximumDrawableCount=2` on iOS
      (2 GB-iPhone memory guard); with `allowsNextDrawableTimeout=NO` the
      present path blocks in `nextDrawable()` up to a full vsync every frame.
      Patched 2→3 via apply-patches.sh (~15 MB extra); lifts the fixed
      per-frame stall on all displays, incl. non-ProMotion 60 Hz.
- [x] **HUD safe-area** — `utils/safearea.{h,cpp,mm}` reads
      `UIWindow.safeAreaInsets`×`contentScaleFactor`; cached per painted frame
      in `MainWindow::paintEvent` (ctor can run pre-layout, and a same-size
      relayout never reaches `resizeEvent` — Widget::resize early-outs).
      HP/mana/swim bars, fps counter and world clock no longer fall under the
      rounded corners / Dynamic Island. 3D stays full-bleed.
- [x] **Quick-ring 3D item icons** (spec §4.5) — `QuickRing::paint` collects
      live icons into `InventoryMenu`'s `InventoryRenderer` (resolved by cls
      via one inventory-iterator pass; vanished items = empty tile). Renderer
      flush gate widened: items also draw when the ring collected them while
      the menu is closed (`Renderer::draw`), leftover icons cleared on ring
      close.
- [x] **Stick/pad X = turn** — physical left-stick X now turns proportionally
      while retaining `RotateL/RotateR` edges for classic combat, lockpicking
      and rotate+jump side-steps; the touch move-pad remains digital classic
      turning (`playercontrol.cpp:565`).
- [x] **Runes & scrolls in the Weapons / Magic panel** — all eight outer
      spell-book slots expose the magic assigned to keys 3–10. Slot 3 is
      included because automatic inventory assignment puts the first rune
      there; omitting it made the first equipped rune invisible on the ring.
- [x] **Persistent Items-ring assignment mode** — R3 on a highlighted item in
      the normal player inventory opens the ring above the inventory; RT moves
      the class to the selected sector, LT clears, and B closes. The first
      mutation freezes the automatic layout, unique class bindings persist in
      optional per-save `game/quickitems`, and missing items return to their
      sector when reacquired. Device confirmation remains in the next-round
      checklist above.
- [x] **Mobsi levitation — round-2 diagnostics closed/superseded.** The added
      `[mobsi] attach:`/`[mobsi] quit:` data (`nodeDy≈97`, `fixMoved=0`) led to
      the root-vs-feet `Interactive::attach` fix documented above; the logs
      were removed after the clean device round.
- [x] **Mobsi levitation — candidate #1 closed (not the observed cause)** —
      a real bug in `GameScript::fixNpcPosition`: when the npc overlaps another NPC at the
      `ZS_POS` node, the ring-search down-ray (`+100cm → −1000cm`) can land on
      TOP of the interactive's collider (`DynamicWorld::ray` hits `C_Object`
      too) and the spot passes the npc-vs-npc-only `hasCollision()` check —
      the user then plays the use animation standing on the barrel/cupboard.
      Fixed: candidates whose ray hit an interactive (`ray.vob!=nullptr`) are
      rejected. Note: dt-spike hypothesis refuted — game logic dt is already
      clamped to 50 ms at the source (`MainWindow::tick`).

## ✅ Done — device-test round 1 fixes (2026-07-10)
- [x] **Save crash** — `MainWindow::saveGame` captured a GPU thumbnail
      (`screenshoot`+`submit`+`readPixels`) which aborts in the Metal driver on
      iOS. On iOS: skip that path, save a small placeholder preview + empty
      background; also guard the saving screen against an empty banner texture.
      This was the safe interim fix and was later superseded by the fence-safe
      preview path documented below.
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
- [x] **B3 retired from the pad** — F5/F9 quick save/load remains an engine
      keyboard feature, but LB+Menu / LB+View and the rotating controller slots
      were deliberately removed for the contextual control scheme.
- [x] **B5** — controller disconnect, lifecycle reset, ring and UI transition
      release only world actions actually held by the pad, then require neutral
      re-arm. A generation epoch covers resets occurring between rendered frames
      and avoids release-side effects from inactive Heal/Potion actions.
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

## 🔥 In progress — save crash-to-home (round 3)
- Round 1 (`c0f30e50`, `d4de87ec`): removed GPU-thumbnail readback + fixed the
  `applicationSupportDirectory` MRC over-release. Save persists, crash remained.
- Round 2 (`56684300`): `catch(std::exception)` guards in `render()` +
  `implProcessEvents`. Crash remained, **no guard logged** → not a std::exception.
- Round 3 (`189145ae` + this): `catch(...)` guards + **exception identity
  logging** (`utils/exceptiondump.{h,mm,cpp}` — rethrows and names NSExceptions
  incl. throw-site backtrace); the loader-thread `noexcept` lambda now also has
  `catch(...)` (a foreign exception there aborted with no log at all);
  `implProcessEvents` guard additionally catches `NSException*` with
  name/reason; `terminateHandler` writes the NSException identity to crash.log.
- **Round 3 RESULT (device log, 2026-07-10): exception hypothesis REFUTED.**
  crash.log tag is `SIGABRT` (signal handler), not `std::terminate` → no
  exception ever unwound; none of the four guards logged. Disassembly of the
  shipped binary: `implProcessEvents+368` is the **return address of the final
  `swapContext()` call** (+48=objc_autoreleasePoolPush, +360=objc_autoreleasePoolPop,
  +364=bl swapContext) → the game fiber finished its dispatch cleanly (pool
  popped) and the abort happens **inside libobjc on the UIKit/apple-fiber side**;
  the frames below libobjc are stale fiber-stack remnants (manual setjmp/longjmp
  breaks the FP walk).
- Round 4 (`640f5d08`): recover the **real abort reason** — `CrashLog::dumpStack`
  dumps `__DATA,__crash_info` annotations; `[save]` breadcrumbs; stderr →
  `Documents/stderr.log`.
- **Round 4 RESULT (device, iOS 26.6 beta): SMOKING GUN.**
  `objc[2004]: Invalid or prematurely-freed autorelease pool 0x…` — and the
  `.ips` shows `AutoreleasePoolPage::badPop` called from
  `implProcessEvents` (main thread). Breadcrumbs: **three quick-saves in a row
  succeeded**, then a save-slot enumeration ran (7× "Unable to open file" =
  save/load menu opened), and the **4th save died mid-saving-screen** between
  "startSave dispatched" and worker completion. So: the ObjC autorelease-pool
  STACK (per-thread TLS, shared by both setjmp/longjmp fibers) gets desynced —
  the game-loop pool's page is invalidated by the time implProcessEvents pops it.
  Not an exception, not MRC in our .mm files — a fiber/pool architecture defect.
- Round 5 (this): **(a) mitigation** — new `apply-patches.sh` step
  `no-objc-pool`: implProcessEvents no longer pushes an ObjC pool on the game
  fiber (plain scope; game-dispatch autoreleases drain in UIKit's own
  per-runloop-cycle pools — bounded); **(b) probe** — `utils/poolprobe.{h,mm,cpp}`
  (`_objc_autoreleasePoolPrint`, weak import) dumps the pool stack to
  stderr.log at save-begin / saving frames / finalize, so if a desync survives
  the mitigation we see exactly whose pool breaks. If the crash stops but pool
  dumps look wrong, the deeper fix is rethinking the fiber loop (e.g. drain
  pools only on the UIKit side).

## ✅ Save crash — FIXED & DEVICE-CONFIRMED (2026-07-10, round 5)
- `no-objc-pool` patch confirmed on device: the crash scenario (multiple
  quick-saves + menu between them + menu save) no longer crashes.
- **Follow-up found via the same logs:** `RFile` on iOS resolved relative
  paths against the app **bundle**, while `WFile` writes to CWD (Documents) —
  so loading a save failed ("Unable to open file"), slots had no
  header/date/thumbnail, and Gothic.ini could not be read back. Fixed with the
  `cwd-first` patch in apply-patches.sh (try CWD, then bundle).
- **DEVICE-CONFIRMED (round 6):** slots show name/date/game-time, loading works
  from both the main menu and in-game menu, and Gothic.ini settings persist.
- Follow-ups done: saves now **auto-named** "&lt;world&gt; - day D, H:MM" (menu; no
  system keyboard on iOS); the former rotating pad shortcut was later retired;
  temporary `[save]` breadcrumbs + PoolProbe call-sites removed (probe util kept
  dormant in `utils/poolprobe.*`); README "saving is broken" note replaced with
  a thumbnail-only limitation.
- **DEVICE-CONFIRMED (2026-07-13):** real save thumbnails are restored without
  reusing the old synchronous callback path. The saving banner appears
  immediately; an 800 px attachment is submitted with the regular frame and read
  only after its fence, with the placeholder retained as fallback. Four tested
  saves completed preview readback in 78/83/86/83 ms, with working save/load and
  no fallback, Metal error or crash. Merged into the production iOS baseline.

## ⏳ To do — deferred (needs on-device iteration)
- [ ] **B9 / N1** — pause game tick (`onTimer`) + `displayLink` while
      backgrounded. Deferred: the manual setjmp/longjmp fiber loop in `implExec`
      means naively sleeping the main context starves the UIKit run loop; needs
      careful on-device testing. (Render is already gated on `isApplicationActive`.)

## ✅ Done — ideal controls, phase 3 (2026-07-10)
- [x] **Target-lock via native focus** (spec §3) — R3 pins the current npc focus
      (`PlayerControl::toggleTargetLock`); `tickFocus` keeps it instead of
      re-finding by aim, and auto-releases when the target dies/leaves
      (`World::validateFocus` + `Npc::isDead`). D-pad ←/→ switches target
      (`focusLeft/focusRight` → `moveFocus`). Existing focus highlight shows it.
      Replaces the provisional R3→`LookBack`.

## ✅ Done — ideal controls, batch 1 (2026-07-10)
- [x] **[GAMEPAD] config** (spec §8) — `deadZone`, `releaseZone`,
      `crossAxisGuard`, `triggerThreshold`, `lookSensitivity` and `invertY` are
      read from `Gothic.ini [GAMEPAD]` in `GamepadInput::loadConfig`. The
      temporary transition-only `debugInput` trace was retired after device
      validation.
- [x] **First-run iOS profile** — on the first successful launch with valid
      game data, when `Documents/Gothic.ini` is absent, create and flush a
      focused override with performance, shadow, keyboard quick-save and stable
      controller defaults; never copy or overwrite `system/Gothic.ini` and
      never auto-populate an existing root override (even an empty one).
- [x] **Controller quick save/load retired.** The engine's guarded F5/F9 path
      remains, but no controller button is reserved for it and the controller
      `saveSlots` / `padQuickSlot` configuration is no longer documented.
- [x] **Controller help + lock-on reticle** (spec §5) — the full mapping is
      available in Options → Controls; the former transient context-hint bar
      was retired because it duplicated that screen and obscured gameplay.
      The target reticle still brackets the pinned target, and the touch
      overlay auto-hides when a gamepad is connected.

## ✅ Done — ideal controls, batch 2 (2026-07-10)
- [x] **Two radial panels** (spec §4) — D-pad ↑ opens Items with 4 inner + 9
      outer slots, initially filled automatically from potions, food and
      torches and later held as a manually assignable per-save layout; D-pad ↓
      opens Weapons / Magic with equipped melee
      and ranged weapons inside + 8 spell-book slots outside. Right-stick
      angle selects a segment and distance selects a row; A/RT activates and B
      cancels. Live inventory objects render as 3D item icons.

## ✅ Done — ideal controls, batch 3 (2026-07-10)
- [x] **Haptics** (spec §7) — `Haptics::impact` via `UIImpactFeedbackGenerator`
      (haptics.mm/.cpp split like systemmsg). Heavy pulse when the player's HP
      drops (polled, no combat-code changes); light on lock/ring-commit.
- [x] **Stuck-protection** (spec §8) — hold L3+R3 ~2 s to warp to the nearest
      waypoint (`World::findWayPoint` + `Npc::setPosition`); opt out with
      `[GAMEPAD] noStuckProtect=1`.

## ✅ Done — controller glyphs (2026-07-10)
- [x] **Xelu CC0 glyphs** (Xbox Series set) bundled in `assets/controller/`,
      loaded from the app bundle (`padglyphtex.mm`, `Resources::loadTexturePm`)
      and drawn as real button art; falls back to the drawn glyphs
      (`padglyph.cpp`: A/B/X/Y discs, LB/RB/LT/RT pills, sticks, D-pad, Menu/View)
      if a texture is missing. Used by the touch overlay.

## ✅ Done — full on-screen virtual gamepad (2026-07-10)
- [x] World touch overlay is now a **full virtual pad** (16 glyph buttons):
      A/B/X/Y, contextual shoulders/triggers, L3(sneak)/R3(lock), D-pad (two
      rings + status/log/focus), View tap/hold (inventory/map), Menu, plus move
      pad + camera.
      Each wired to its action via `ctrl`/`uiAction`/new `MainWindow::pad*`
      bridges. Touch uses the same two modal radial panels and slot selection as
      the physical controller. Uses Xelu glyphs.
      NOTE: button placement/sizes are v1 — tune on device.

## ⏳ Spec gaps / simplifications (control spec §1–§8) — deliberate, not blocking
- [ ] Concentric rings and radial centre-easing are implemented; on-device
      tuning of row thresholds, sector highlight and touch selection remains.
- [ ] Target-lock has no **camera auto-pull** toward the pinned target (spec §3).
- [ ] No dedicated **Dead**-screen / **Container**-vs-Inventory / **MapDoc**
      pad contexts (folded into Menu/Inventory) (spec §1.1).
- [x] Inventory LB/RB jumps between the previous/next sorted item category.
- [ ] Inventory right-stick 3D item preview remains pending (spec §2.4).
- [ ] Overlay **daylight tint** (spec §5.5, optional).
- [ ] Deferred B9/N1 background pause (needs on-device fiber testing).

Otherwise §1–§8 implemented; remaining is polish / on-device tuning (ring feel,
haptic intensity and glyph sizing).

## ✅ Done — UI / readability
- [x] Scale up UI on iOS for high-DPI legibility (`MainWindow::uiScale`).
- [x] Dialogue subtitle window enlarged / reflowed for phone screens.
- [x] Dialogue **choice** list enlarged and made touch-friendly.
- [x] Main-menu text size verified after the uiScale change.
- [x] Controller-layout labels wrap to at most two lines, use height-aware
      callout spacing, reserve the build-version line, and infer EN/DE/PL from
      the active `MENU.DAT` strings (including Polish data with `GAME.language=-1`).
- [x] README controller tables replaced by a clickable, HiDPI-safe mapping SVG;
      the complete text mapping remains available in a collapsed accessible
      fallback. Leader lines render above the pad body; Left stick/L3 and the
      four D-pad actions use one bracket/leader each, and trigger/bumper depth
      matches a physical controller.
