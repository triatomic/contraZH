# CommandButtons & Selection

## New CommandButton Commands

### HOLD_FIRE

Suppresses **automatic target acquisition only**; explicit attack orders still fire. Covers the
unit, its addons and sub-turrets, and contained infantry.

```
CommandButton Command_HoldFire
  Command       = HOLD_FIRE
  Options       = CHECK_LIKE     ; required, or the button never renders as toggled on
  TextLabel     = CONTROLBAR:HoldFire
  ButtonImage   = SNHoldFire
  DescriptLabel = CONTROLBAR:TooltipHoldFire
End
```

Per-object parameter:
* `HoldFireAllowsRetaliation = Yes` - (Default. `No` stops the unit returning fire even when attacked.)

Note: garrisoned buildings cannot hold fire (most have no AI module). Infantry inside a *unit* are
covered.

### TOGGLE_FIRE_WEAPON

Like `FIRE_WEAPON`, but a second click stops it.

```
CommandButton Slth_Command_JammerStationActivate_HumanPlayer
  Command          = TOGGLE_FIRE_WEAPON
  Options          = CHECK_LIKE     ; required, or the button never renders as toggled on
  WeaponSlot       = SECONDARY
  MaxShotsToFire   = 60
  TextLabel        = CONTROLBAR:GLAJamm
  ButtonImage      = SUJPulse
  ButtonBorderType = ACTION
  DescriptLabel    = CONTROLBAR:ToolTipGLAFireJamm
End
```

Notes:
* The button reflects the unit's real state, switching off once `MaxShotsToFire` is spent.
* Stopping ends only the firing; a move order survives.
* Stays clickable while reloading between shots.
* In a mixed selection, if any unit is firing, the click stops all.

### AUTO_FILL

Selects nearby infantry and orders them into the selected container.

```
CommandButton Command_AutoFill
  Command       = AUTO_FILL
  Options       = OK_FOR_MULTI_SELECT
  TextLabel     = CONTROLBAR:AutoFill
  ButtonImage   = SNAutoFill
  DescriptLabel = CONTROLBAR:TooltipAutoFill
End
```

Takes uncontained infantry not already in the group, nearest first. They break off whatever they are
doing.

### EVACUATE_TO_WORK

Evacuates like `EVACUATE`, then sends the supply gatherers among the passengers back to gathering.

```
CommandButton Command_BackToWork
  Command       = EVACUATE_TO_WORK
  Options       = OK_FOR_MULTI_SELECT
  TextLabel     = CONTROLBAR:BackToWork
  ButtonImage   = SNBackToWork
  DescriptLabel = CONTROLBAR:TooltipBackToWork
End
```

Notes:
* Applies to passengers with a `WorkerAIUpdate` or `SupplyTruckAIUpdate`; other passengers exit and idle.
* A gatherer returns to the dock the player last assigned it, else the nearest warehouse.
* A gatherer carrying boxes delivers them to a supply center first.
* Greyed out under the same conditions as `EVACUATE`.

### TOGGLE_DEPLOY
New command for deploying artillery, replacing the old trick of pointing a `FIRE_WEAPON` button at a dummy
weapon slot. That worked, but it went through the weapon lock and brought its bugs with it.

```
CommandButton Command_Deploy
  Command           = TOGGLE_DEPLOY
  Options           = OK_FOR_MULTI_SELECT
  ButtonImage       = SNNukeCannonDeploy
  ButtonBorderType  = ACTION
  TextLabel         = CONTROLBAR:ToolTipDeploy
  UnitSpecificSound = NukeCannonVoiceDeploy
End
```

`TOGGLE_DEPLOY` needs no `WeaponSlot`. It flips the object between deployed and packed, and the button shows
as toggled on while deployed. It works on any object with a `DeployStyleAIUpdate`; on anything else the
button is unavailable.

A deploy asked for this way latches, so the unit holds the stance instead of packing up again the moment
nothing is in range - which is the point, since it lets a unit deploy before the enemy arrives. The latch is
dropped when the player gives any order other than an attack, so a move order packs the unit up as usual.
Automatic deploying when a target comes into range is unchanged.

Selecting a mixed group and pressing the button moves the whole group to one stance rather than flipping each
unit separately, so a second press does not undo half of them.

### Queue reorder

Off by default; enable with `QueueReorder = Yes` in the `GameData` block of GameData.ini.

* Ctrl+click a queue cameo moves it one place earlier. The displaced entry restarts its build time;
finished units of a batch stay finished.
* No effect on the first entry or a finished unit waiting to exit.
* Plain click still cancels; Shift+click still cancels all of that type.

## REVERSE_MOVE hotkey

Caps Lock arms upstream's `REVERSE_MOVE` order for any selection, without the button. See
[Reverse move hotkey](contraZH-Options.md#reverse-move-hotkey).

## Structure Multi-Select

Structures of one type can be selected together.

* The select-matching key (E by default) picks same-type structures on screen, or map-wide with Alt.
* Shift+click and Shift+double click add a same-type structure.
* Shift plus a team number (0 to 9) adds that team if it holds only that structure type.
* Mixing with units or other structure types still replaces the selection.

The bar shows shared commands. Build and upgrade commands every producer has are enabled even without
`OK_FOR_MULTI_SELECT`. The queue shows the nine entries closest to finishing.

* Click a unit or upgrade: queue it on the producer that finishes first.
* Shift+click a unit: queue five, spread across producers, stopping at money or unit limits.
* Shift+click an upgrade: queue it on the five fastest producers.
* Shift+click a queue entry: cancel that type everywhere.
