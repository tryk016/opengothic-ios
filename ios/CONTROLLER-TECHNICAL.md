# Controller and quick-ring architecture

This is the maintainer reference for the iOS controller implementation. User-facing
controls remain in [`README-ios.md`](README-ios.md); current verification work remains
in [`TODO.md`](TODO.md).

The current implementation was introduced by commit `60ce08a2` and compiled,
packaged and published successfully by GitHub Actions run `29211433774`.

## Input pipeline

1. `game/utils/gamepad.mm` reads `GCExtendedGamepad` on its private handler queue.
   It publishes the newest analog snapshot and queues lossless digital button edges.
2. `Gamepad::consume()` returns one `GamepadInputFrame`: the newest state, ordered
   digital transitions, controller generation and overflow count.
3. `game/ui/gamepadinput.cpp` selects exactly one `PadCtx` (`World`, `Dialog`,
   `Menu`, `Inventory` or `Loading`) and routes the frame only to that context.
4. World actions enter `PlayerControl`; UI contexts receive complete synthetic key
   taps through `MainWindow::dispatchKey`.

LT and RT are intentionally analog-only. They are evaluated against
`[GAMEPAD] triggerThreshold`; Apple `isPressed` callbacks are not queued because
their system-defined threshold could disagree with the configured value. The tradeoff
is that an exceptionally fast complete trigger press/release between two simulation
ticks can be missed. Normal digital buttons retain FIFO edges, including A/B in menus
and dialogue.

Controller generation changes, `PlayerControl::inputGeneration()`, UI transitions,
ring opening and disconnects release controller-owned state. Continuous inputs must
return to neutral before they can re-arm. This prevents an input held in a menu or
before app resume from leaking into gameplay.

## Final mapping

| Control | World action |
|---|---|
| A / Cross | Interact/use while unarmed; confirm in UI/rings |
| B / Circle | Melee special; Back in UI; cancel a ring |
| X / Square | Jump/climb |
| Y / Triangle | Draw/sheathe the last weapon |
| LT / L2 | Unarmed: draw bow; melee: block; bow/crossbow: aim |
| RT / R2 | Unarmed: draw melee; armed: attack/shoot/cast |
| LB / L1 | Melee: left attack; otherwise temporary walk |
| RB / R1 | Melee: right attack; otherwise look back |
| L3 | Toggle sneak |
| R3 | Toggle native target lock |
| D-pad Up | Open/switch to the Items ring |
| D-pad Down | Open/switch to the Weapons/Magic ring |
| D-pad Left | Status, or previous target while locked |
| D-pad Right | Quest log, or next target while locked |
| View | Tap: Inventory; hold 600 ms: scripted Map |
| Menu | Game menu |
| L3 + R3 | Hold about 2 seconds: nearest-waypoint unstuck teleport |

Controller quick-save/load shortcuts and the first-person binding are deliberately
not assigned. Engine keyboard F5/F9 remains available when enabled.

## Combat semantics

The mobile dispatcher does not synthesize the Gothic 1 or Gothic 2 keyboard presets.
It sends internal actions declared in `game/utils/keycodec.h`:

- `PadAttack` -> `ActForward`;
- `PadAim` -> `ActGeneric` for bow/crossbow;
- `PadAttackLeft` / `PadAttackRight` -> directional melee attacks;
- `Parade` -> `ActBack` for melee block;
- `PadSpecial` -> `ActMove` for the engine's moving-forward melee attack path.

`PlayerControl::rebuildPadCombatAction()` rebuilds `actrl` from all semantic buttons
still held. Priority is attack, special, left, right, block, aim. Releasing RT while LT
is still held therefore returns to aim instead of clearing combat input.

Contextual controls must never change meaning during one physical hold. If
`WeaponState` changes while LT/RT/LB/RB remains down, the dispatcher releases the old
semantic action and suppresses the new one until a real release. LT also latches an
explicit `Idle` meaning in Mage state, preventing a held no-op LT from drawing a bow
after magic is sheathed.

LB walk is not the existing toggle action. `PlayerControl::setGamepadWalk(bool)` adds
`WM_Walk` only for the duration of the hold, remembers whether the mode already
existed, and removes only the bit it owns.

## Two separate quick-rings

The rings are separate modal `QuickRing` instances, never one combined wheel.

### Items

- 13 fixed sectors: 9 outer, then 4 inner overflow sectors.
- Filled automatically from potions, food and torches in the live inventory.
- A burning hand torch is absent from the inventory iterator, so a display-only
  synthetic `ItLsTorch` cell is added. Committing it calls the normal inventory-use
  path and stows the real torch without losing an item.

### Weapons / Magic

- 9 fixed sectors.
- 2 inner sectors: currently equipped melee and ranged weapon.
- 7 outer sectors: active spell-book slots 4 through 10, including runes/scrolls.
- Committing returns `WeaponMele`, `WeaponBow` or `WeaponMage4..10` to
  `PlayerControl`, preserving normal draw/sheathe animation queuing.

The right-stick angle selects a sector. Stick distance selects inner/outer row with a
dead zone and hysteresis (`0.28`, inner transition `0.62`, outer transition `0.72`).
A or RT commits; B cancels; D-pad Up/Down switches panels. Empty sectors remain
visible but commit nothing.

Touch uses the same two modal panels. While open, the full virtual-pad overlay is
hidden and only corner D-pad Up/Down and B controls remain. A drag elsewhere selects;
release commits. A touch ring is cancelled when the app leaves `PadCtx::World`, so it
cannot retain a synthetic item from an unloaded world.

Rendering is procedural in `game/ui/quickring.cpp`: subdivided triangle sectors,
dark translucent fill, amber border and gold selection. Live 3D icons are collected in
the inventory renderer and flushed after the Painter layer.

## Inventory category navigation

The original `InventoryMenu` had no controller category command. The fork adds
`InventoryMenu::moveCategory(int direction)`:

- LB selects the first item of the previous category;
- RB selects the first item of the next category;
- selection wraps at both ends;
- categories come from the already sorted page and each item's `mainFlag`;
- existing selection, scrolling and `INV_CHANGE` sound paths are reused.

This does not create new tabs and does not change item sorting. It works on the active
inventory/container page. The touch inventory exposes matching LB/RB glyphs.

## Configuration

The stable `[GAMEPAD]` settings are:

```ini
[GAMEPAD]
deadZone=0.25
releaseZone=0.15
crossAxisGuard=0.12
triggerThreshold=0.50
lookSensitivity=0.20
invertY=0
```

Optional diagnostics and recovery settings:

```ini
[GAMEPAD]
debugInput=1
noStuckProtect=1
```

Keep `releaseZone < deadZone`. `crossAxisGuard` raises the activation threshold of
the perpendicular left-stick axis and prevents imperfect cardinal motion from
starting an unintended turn or step.

## Main implementation files

- `game/utils/gamepad.h`, `game/utils/gamepad.mm` - backend state and event FIFO.
- `game/ui/gamepadinput.h`, `game/ui/gamepadinput.cpp` - context routing and mapping.
- `game/game/playercontrol.h`, `game/game/playercontrol.cpp` - semantic combat,
  temporary walk and target control.
- `game/ui/quickring.h`, `game/ui/quickring.cpp` - contents, radial selection and draw.
- `game/ui/touchinput.h`, `game/ui/touchinput.cpp` - virtual-pad parity.
- `game/ui/inventorymenu.h`, `game/ui/inventorymenu.cpp` - category jumps.
- `game/ui/padsystemgesture.h` - View tap/hold and Menu reducer with constexpr tests.
- `game/ui/paddiagram.cpp` - localized EN/DE/PL in-game diagram.
- `assets/controller/OpenGothic_Controller_Layout.svg` - README diagram.

## Device verification checklist

- Test both Gothic 1 and Gothic 2 control presets with melee, bow/crossbow and magic.
- Hold each contextual shoulder/trigger while drawing and sheathing weapons; no held
  input may silently become a different action.
- Confirm short A/B presses in dialogue, pause menu and inventory.
- Verify both ring sizes, inner/outer hysteresis, empty sectors, spells 4-10 and the
  last burning-torch stow case.
- Verify touch panel switching/cancel, and opening a menu/loading transition with a
  touch ring active.
- Test LB/RB category wrap on player inventory, chest and trade pages.
- Test View tap versus 600 ms hold, right-stick vertical look, `invertY=1`, left-stick
  cross-axis guard, disconnect/reconnect and app background/resume.

Update this file whenever controller semantics, ring geometry or input ownership
changes; update both user-facing diagrams in the same commit.
