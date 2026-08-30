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
* `GroundHugMaxSlope = 35` - (Optional. The steepest ground the object will follow. Terrain ahead
*rising* more steeply than this stops the following, so cliffs are collided with instead of
climbed; ground *falling* more steeply is followed at this angle instead, so the object glides
off a drop rather than diving down it. Left at 0 it uses `MaxThrustAngle`, which is the steepest
the object could fly onto anyway. Default = 0, meaning `MaxThrustAngle`)
* `GroundHugLookAhead = 15.0` - (Optional. How far ahead of the object the terrain is sampled.
Larger values start the climb earlier. The distance is sampled in steps and the object reacts to
the steepest step in it, so changing this changes when the object reacts, not how steep it judges
the ground to be. Left at 0 it is worked out from `Speed`, far enough to give a few frames of
warning and never less than one pathfinder cell. Default = 0, meaning automatic)
* `PreferredHeightDamping = 0.3` - (Existing key, but set it: it damps the hug as well. The default
of 1.0 means the height is matched in a single frame, which reintroduces the bump-grinding this
feature exists to avoid. Lower is smoother. Default = 1.0)

Notes:
* Only has an effect on `Appearance = THRUST`, and only while `PreferredHeight` is 0. A locomotor
with a `PreferredHeight` keeps using that and ignores these keys.
* `GroundHugHeight` is the only one that has to be set. The other two work themselves out from
`MaxThrustAngle` and `Speed`; set them only to override what that gives you.
* Unlike `StickToGround`, the height change is a thrust adjustment damped by
`PreferredHeightDamping` and clamped by `MaxThrustAngle`, not a hard snap onto the ground. That is
what lets it ride over bumps rather than grind into them.
* `MaxThrustAngle` sets the ceiling on how sharply the object can pitch onto a slope. A very small
value (say 1 degree) cannot bank onto a ramp at all, so terrain following will appear not to work
however these keys are set.
* A hugging locomotor should always set `GroundHugMaxSlope`. With no limit it will try to climb
anything, including vertical faces.
* The flight follows the *slope* of the ground, extrapolated from the sampled gradient, with a
correction back toward `GroundHugHeight`. That is what lets it conform to ramps and rolling
ground of any length, and what makes crossing an edge continuous: the object leaves a drop on a
bounded glide and settles back down as the ground comes up to meet it, instead of nose-diving
at the low ground the moment it clears the lip.

## Ships
### TODO

# LocomtorExtend

Locomotor definitions can be extended via a LocomtorExtend entry:

- `LocomotorExtend <NewLocomotor> <ParentLocomotor>

All parameters from Locomotor can be used to override values from the parent Locomotors . It is possible to extend an extended Locomotor.
Note: The extended locomotor needs to be defined below the parent locomotor.