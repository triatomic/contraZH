# CommandButton.ini

This page documents new `CommandButton` command types and parameters.

## REVERSE_MOVE (New Command)

A new context command that orders the selected unit(s) to move to a clicked ground position **driving in reverse**, instead of turning around first.

* `Command = REVERSE_MOVE`

It works like `ATTACK_MOVE`/`MOVE`: the player clicks the button, then clicks a target position on the terrain. The unit drives backwards along the whole path (no three-point turn), forcing reverse movement regardless of the [locomotor reverse heuristics](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Locomotor#reverse-backwards-movement).

Example:
```
CommandButton Command_ReverseMove
  Command       = REVERSE_MOVE
  Options       = NEED_TARGET_POS
  TextLabel     = CONTROLBAR:ReverseMove
  ButtonImage   = SCReverseMove
  CursorName    = Move
  InvalidCursorName = GenericInvalid
End
```

Add the button to the unit's `CommandSet` like any other command button.

**Notes:**
* Requires a locomotor that can rotate in place / reverse (e.g. `TREADS`, `HOVER`). The reverse speed and behavior are tuned via the [Locomotor backwards-movement parameters](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Locomotor#reverse-backwards-movement).
* Plays the normal move voice response.
* A manual `REVERSE_MOVE` order forces reversing for the entire path. The GameData flag [`ReverseMoveIgnoreAngleThreshold`](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/GameData#reverse-movement) controls whether a manual order reverses regardless of heading.

## Multi-Target Special Powers

CommandButton parameters were added to aim a special power at multiple target points in one activation (`NumberOfTargets`, anchor/target radii, markers, ...). See [Multi-Target Special Powers](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/SpecialPowers#multi-target-special-powers).
