# Locomotor.ini

## Reverse (Backwards) Movement

Locomotors that can rotate in place (e.g. `TREADS`, `HOVER`) can now reverse towards a goal that is behind them instead of always turning around first. The behavior is now configurable per locomotor.

* `BackwardsMoveAngleThreshold = 90` - (The goal must be at least this many degrees off the current heading before the unit reverses instead of turning around. Default = 90°)
* `BackwardsMoveDistanceFactorThreshold = 5.0` - (Maximum reverse distance, as a factor of the object's MajorRadius. Beyond this the unit turns around instead. Default = 5.0)
* `BackwardsMoveSpeedFactor = 1.0` - (Speed multiplier applied while moving backwards. E.g. 0.5 = reverse at half speed. Default = 1.0)

Notes:
* Defaults match the values the hardcoded backwards-movement logic used before these were configurable.
* A global override exists in GameData: [`ReverseMoveIgnoreAngleThreshold`](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/GameData#reverse-movement).

## Ground Hugging

`THRUST` locomotors can follow the terrain without a `PreferredHeight`, riding over small bumps
while still colliding with cliffs. Intended for sonic-wave style projectiles that must stay above
the ground without chasing a target.

* `GroundHugHeight = 4.0` - (Height above the surface to hold, even when `PreferredHeight` is 0.
Terrain following is off while this is 0. Default = 0)
* `GroundHugMaxSlope = 35` - (If the terrain ahead rises more steeply than this many degrees, stop
following it, so cliffs are collided with instead of climbed. No slope limit when 0. Default = 0)
* `GroundHugLookAhead = 15.0` - (How far ahead of the object the terrain is sampled. Larger values
start the climb earlier. 0 means one pathfinder cell, i.e. 10. Default = 0)

Notes:
* Only has an effect on `Appearance = THRUST`, and only while `PreferredHeight` is 0. A locomotor
with a `PreferredHeight` keeps using that and ignores these keys.
* Unlike `StickToGround`, the height change is a thrust adjustment damped by
`PreferredHeightDamping` and clamped by `MaxThrustAngle`, not a hard snap onto the ground. That is
what lets it ride over bumps rather than grind into them.
* `MaxThrustAngle` sets the ceiling on how sharply the object can pitch onto a slope. A very small
value (say 1 degree) cannot bank onto a ramp at all, so terrain following will appear not to work
however these keys are set.
* A hugging locomotor should always set `GroundHugMaxSlope`. With no limit it will try to climb
anything, including vertical faces.

## Ships
### TODO

# LocomtorExtend

Locomotor definitions can be extended via a LocomtorExtend entry:

- `LocomotorExtend <NewLocomotor> <ParentLocomotor>

All parameters from Locomotor can be used to override values from the parent Locomotors . It is possible to extend an extended Locomotor.
Note: The extended locomotor needs to be defined below the parent locomotor.