# Locomotors

## Ground-following projectiles

`GroundCollisionAngle` lets a projectile follow terrain while colliding with steep uphill slopes.
Use it with `StickToGround = Yes` on the projectile's locomotor.

| Setting | Default | Effect |
| --- | --- | --- |
| `GroundCollisionAngle` | `0` | Maximum uphill slope in degrees above horizontal. `0` disables this mode. Enabled values must be greater than `0` and less than `90`. |

The projectile stays at ground height on both uphill and downhill paths. Small bumps and ramps
below the angle limit do not trigger ground impacts. A steeper uphill slope triggers the normal
projectile collision at the foot of the slope. Downhill slopes do not block it.
The engine checks the path at intervals of at most one world unit, including when a projectile
crosses a whole cliff face in one frame. The angle is measured along the direction of travel.

For the sonic wave in [issue #45](https://github.com/triatomic/contraZH/issues/45), add these lines
to `SonicWaveLocomotor`:

```ini
  StickToGround = Yes
  GroundCollisionAngle = 45
```

`45` is a starting value. Choose an angle above the map's ramp slopes and below its cliff slopes.
To keep the wave from homing on a moving target, use `TryToFollowTarget = No` in its
`MissileAIUpdate` module.

The mode applies only to projectiles with a current locomotor. Falling and freefall behavior
take precedence. Object collisions and configured water collisions still apply.
The slope check also applies during the missile's final direct move toward its target.
Entering water triggers a configured water collision even while the projectile follows the ground.
Omitting `GroundCollisionAngle` preserves existing movement and collision behavior.

Enabling the mode changes the simulation. All players and replays need the same INI definitions
and an executable that supports this setting.
