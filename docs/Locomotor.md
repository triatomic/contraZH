# Locomotor.ini

## Reverse (Backwards) Movement

Locomotors that can rotate in place (e.g. `TREADS`, `HOVER`) can now reverse towards a goal that is behind them instead of always turning around first. The behavior is now configurable per locomotor.

* `BackwardsMoveAngleThreshold = 90` - (The goal must be at least this many degrees off the current heading before the unit reverses instead of turning around. Default = 90°)
* `BackwardsMoveDistanceFactorThreshold = 5.0` - (Maximum reverse distance, as a factor of the object's MajorRadius. Beyond this the unit turns around instead. Default = 5.0)
* `BackwardsMoveSpeedFactor = 1.0` - (Speed multiplier applied while moving backwards. E.g. 0.5 = reverse at half speed. Default = 1.0)

Notes:
* Defaults match the values the hardcoded backwards-movement logic used before these were configurable.
* A global override exists in GameData: [`ReverseMoveIgnoreAngleThreshold`](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/GameData#reverse-movement).

## Ships
### TODO

# LocomtorExtend

Locomotor definitions can be extended via a LocomtorExtend entry:

- `LocomotorExtend <NewLocomotor> <ParentLocomotor>

All parameters from Locomotor can be used to override values from the parent Locomotors . It is possible to extend an extended Locomotor.
Note: The extended locomotor needs to be defined below the parent locomotor.