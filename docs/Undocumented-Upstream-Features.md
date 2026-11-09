# Undocumented Upstream Features

Written by contraZH. These GeneralsGameCode_Modding features are in upstream's code but not in
upstream's wiki, so this page covers them until upstream does. The other Upstream reference pages
are copied from upstream's wiki unchanged.

## InGameUI.ini

### Generic RadiusCursor definitions

Radius cursors used to need one hardcoded keyword each (`AttackDamageAreaRadiusCursor`,
`AttackScatterAreaRadiusCursor`, `AttackContinueAreaRadiusCursor` and so on), so a new cursor could
not be added from INI alone. A generic form is now accepted, where the cursor type is named on the
header line:

```
RadiusCursor GUARD_AREA
  Texture = SCCommandCenterRadiusCursor
  Style = SHADOW_ALPHA_DECAL
  OpacityMin = 20%
  OpacityMax = 40%
End
```

The old per-cursor keywords still work. The cursor type must be a name from the radius cursor list;
see [RadiusCursorType](New-Enum-Definitions.md#radiuscursortype).

## GlobalLightingModifierUpdate

Update module that tints the global map lighting while the object lives. Intended for effects like
a superweapon darkening the sky.

```
Behavior = GlobalLightingModifierUpdate ModuleTag_Lighting
  TargetColor = R:0 G:0 B:0   ; colour blended towards
  BlendMode = DARKEN          ; DARKEN | COLORIZE | BRIGHTEN
  Intensity = 1.0             ; 0.0 - 1.0, strength of the blend
  InitialDelay = 0            ; ms before the effect starts
  Duration = 0                ; ms the effect is held at full intensity; 0 = while the object lives
  FadeInTime = 0              ; ms to reach full intensity
  FadeOutTime = 0             ; ms to fade back out
  RequiredUpgrade = <Upgrade> ; optional; the modifier only applies once this upgrade is present
End
```

## DrawBridgeUpdate

FX and audio keys beside the ones on the [DrawBridgeUpdate](Objects-&-Modules.md) entry:

```
Behavior = DrawBridgeUpdate ModuleTag_DrawBridge
  BridgeOpeningFX = <FXList>    ; played when the bridge starts opening
  BridgeOpenFX = <FXList>       ; played once fully open
  BridgeClosingFX = <FXList>    ; played when the bridge starts closing
  BridgeClosedFX = <FXList>     ; played once fully closed
  BridgeOpeningAudio = <AudioEvent>   ; looping audio while opening
  BridgeClosingAudio = <AudioEvent>   ; looping audio while closing
End
```

## MultiLocationSpecialPowerUpdate

```
Behavior = MultiLocationSpecialPowerUpdate ModuleTag_MultiLoc
  WaitForAttackComplete = No     ; advance only once the caster has stopped attacking
  MaxWaitPerTarget = 0           ; ms; safety cap when WaitForAttackComplete is used
End
```

### Multi-click artillery

If the OCL contains an Attack nugget, the caster attacks each point with one of its own weapon
slots. `aiAttackPosition` replaces the previous order rather than queueing behind it, so with a
fixed `Delay` the next point cuts the previous barrage short.

* `WaitForAttackComplete = Yes` - advance to the next point only once the caster has actually
stopped attacking, instead of after a fixed delay.
* `MaxWaitPerTarget = 5000` - safety cap in ms. Without it, a point the caster cannot attack
would stall the whole sequence.

Note: `Delay` is still used as the poll interval, so leave it at a small value when
`WaitForAttackComplete` is enabled.

## W3DDecalDraw

`Style` must be `SHADOW_ALPHA_DECAL` or `SHADOW_ADDITIVE_DECAL`. Any other style, `SHADOW_DECAL`
included, draws as `SHADOW_ALPHA_DECAL`, and a debug build asserts on it.

## Turret arcs

Notes on `MinTurretAngle` / `MaxTurretAngle` from the [Turret](Objects-&-Modules.md) entry:

* Angles are relative to the hull. Positive turns the turret to its left, negative to its right, so `MinTurretAngle` is always the rightmost edge of the arc and `MaxTurretAngle` the leftmost.
* To mirror an arc onto the other side, negate and swap the values: a left sponson with `MinTurretAngle = 0` / `MaxTurretAngle = 50` becomes `MinTurretAngle = -50` / `MaxTurretAngle = 0` on the right.
* For backwards facing configurations, set MaxTurretAngle < MinTurretAngle; the arc then wraps through the rear (e.g. `150` / `-150` covers the rear 60 degrees). Currently this is not working 100% reliably.

## Corrections to upstream pages

### ChatCommands

A command is typed with a slash prefix, for example `/money`. The first word of the message, with
the slash removed, is matched against the defined command names. A message that does not start
with a slash is always sent as ordinary chat.

The name is matched case-insensitively, so `/MONEY` works as well as `/money`. The slash is only how
the message is recognised as a command - it is not part of the name in `ChatCommands.ini`.

### FXList decal Scale

The Decal nugget's `Scale` reads two plain numbers, low then high, plus an optional distribution
name. `Scale = 1.0 1.0` gives a fixed size and `Scale = 0.8 1.2` a random one. A single number or
the `Min:0.8 Max:1.2` form does not parse.
