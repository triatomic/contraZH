# Object Improvements

## Upgrade Cameos
Added support for UpgradeCameo6-9 slots. These need to be defined in the controlbar .wnd files to work.

## WeaponSet
Added new parameter to an object's WeaponSet definition
* `WeaponReloadSharedAcrossSets = No` - If a unit switched between weaponset, it's reload time and ammo count (in percent) will be kept. For instance Migs will not reload in air when they get BlackNapalm. 

### AutoChooseSources
Added new types for AutoChooseSources

* `SYNC_TO_PRIMARY` - This weapon slot cannot fire on its own, but will be fired whenever PRIMARY is fired.
* `SYNC_TO_SECONDARY` - This weapon slot cannot fire on its own, but will be fired whenever SECONDARY is fired.
* `SYNC_TO_TERTIARY` - This weapon slot cannot fire on its own, but will be fired whenever TERTIARY is fired.

Syncs a weapon slot to another. The weapon slot cannot be used except when it's parent weapon is fired. This ignores all conditions and checks of the synced weapon.

Example:
```
Weapon = PRIMARY MainWeapon
Weapon = SECONDARY SyncedWeapon
AutoChooseSources = SECONDARY SYNC_TO_PRIMARY
```
In this example, the secondary weapon SyncedWeapon will not be used on its own. Whenever the unit attacks with its primary MainWeapon, the secondary weapon will be fired as well. This can be used for Aircraft to reliably fire multiple weapons on each attack.


## AmmoPips style

Added options to customize Ammo Pips, e.g. for Aircraft with a very large clip size.

Added a new parameter for object definitions:

* `AmmoPipsStyle = DEFAULT` - (Change the unit's display style for ammo pips to one of these:)
  - `DEFAULT` - default ZH style; 1 yellow pip symbol for each shot.
  - `PERCENTAGE_BAR` - display a yellow bar similar to the health bar, showing the ammo count in percent.
  - `SINGLE` - default ZH pip visuals, but only show a single symbol regardless of clip size. Ammo count is displayed via fading yellow color.

Note: This parameter might be moved to individual weapons in the future, to allow displaying ammo pips for multiple weapon slots at once.

# Object Modules

## AIUpdateInterface (And all other AIUpdate types)

New experimental parameter:
* `PreferredAttackAngle = <angle> [MIRRORED]` - (mirrored keyword is optional)
When a unit turns to attack, it will attempt to turn to this angle. If mirrored is enable it can also use the same angle +180°. Possibly useful for battleship setups with multiple turrets. Only works for locomotors that can turn in place (minTurnSpeed = 0)
Note: this is NOT needed for limited turret angles (See below). Units will follow the actual turret angles when trying to attack.

New `AutoAcquireEnemiesWhenIdle` value:
* `NOTWHILEMOVING` - the object will not shoot on the move. Intended for artillery that has to be stationary
to fire, which previously had to be faked in other ways.
Combine it with the other values as usual, e.g. `AutoAcquireEnemiesWhenIdle = Yes NOTWHILEMOVING`.

What it does, by order type:

| Order | Behaviour |
|---|---|
| Move | Drives past enemies without stopping or engaging. |
| Attack-move | Drives, spots a target, halts once in range, fires, then carries on. |
| Attack (direct order) | Closes on the target, halts, fires. |
| Stationary | Fires normally. |

This takes two separate checks, because the two halves pull against each other. Targets of opportunity - the
idle scans, including a turret scanning while the chassis under it drives - are suppressed while the object is
in motion, which is what keeps it from shooting on an ordinary move. Attack-move deliberately still acquires
on the move, because spotting a target while driving is exactly what makes the unit stop for it; the object
then keeps aiming, without firing, until it has actually come to rest. It holds its target throughout and
fires the moment it settles, so any PreAttackDelay wind-up plays where the shot is taken rather than being
spent on the approach.

### DeployStyleAIUpdate
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

### Turret
New paramters for Turret or AltTurret entries
* `MinTurretAngle = 0` - Minimum angle the turret is allowed to turn
* `MaxTurretAngle = 0` - Maximum angle the turret is allowed to turn
Notes:
- for backwards facing configurations, MaxTurretAngle can be > MinTurretAngle; Currently this is not working 100% reliably
- Remove the angle limit lines to use unlimited angle. A value of 0 will use 0 as limit.
- If the turret cannot turn to the front (e.g. side mounted gun on a helicopter), the unit will attempt to turn to the turret's firing arc
  - this feature only works for locomotors that can turn in place (minTurnSpeed = 0)

## WeaponSetUpgrade
Added new parameters for WeaponSetUpgrade
* `WeaponSetFlag = <WeaponSetFlag>` - define which Weaponset to enable (default = PLAYER_UPGRADE)
* `WeaponSetFlagsToClear = <WeaponSetFlag1> <WeaponSetFlag2> ..` - define which Weaponsets to disable
* `NeedsParkedAircraft = No` - Units with JetAIUpdate need to be parked in hangar to research this upgrade

**Notes:**
WeaponSetFlagsToClear can be used to switch between WeaponSets using multiple upgrades.
example:

```
  Behavior = WeaponSetUpgrade ModuleTag_04a
    WeaponSetFlag = PLAYER_UPGRADE
    WeaponSetFlagsToClear = PLAYER_UPGRADE2 PLAYER_UPGRADE3
    TriggeredBy = Upgrade_GLAWorkerFakeCommandSet
    RemovesUpgrades = Upgrade_GLAWorkerRealCommandSet Upgrade_GLAWorkerFakeCommandSet Test_Upgrade_DummyToggle
  End
  
  Behavior = WeaponSetUpgrade ModuleTag_04b
    WeaponSetFlag = PLAYER_UPGRADE2
    WeaponSetFlagsToClear = PLAYER_UPGRADE PLAYER_UPGRADE3
    TriggeredBy = Upgrade_GLAWorkerRealCommandSet
    RemovesUpgrades = Upgrade_GLAWorkerRealCommandSet Upgrade_GLAWorkerFakeCommandSet Test_Upgrade_DummyToggle
  End
  
  Behavior = WeaponSetUpgrade ModuleTag_04c
    WeaponSetFlag = PLAYER_UPGRADE3
    WeaponSetFlagsToClear = PLAYER_UPGRADE PLAYER_UPGRADE2
    TriggeredBy = Test_Upgrade_DummyToggle
    RemovesUpgrades = Upgrade_GLAWorkerRealCommandSet Upgrade_GLAWorkerFakeCommandSet Test_Upgrade_DummyToggle
  End
```
You can combine this with NeedsParkedAircraft to allow Jets to switch their ammo type when parked using multiple object upgrades.


## ArmorUpgrade
Added new parameters for ArmorUpgrade
* `ArmorSetFlag = <ArmorSetFlag>` - define which Armorset to enable (default = PLAYER_UPGRADE)
* `ArmorSetFlagsToClear = <ArmorSetFlag1> <ArmorSetFlag2> ..` - define which ArmorSets to disable

**Notes:**
ArmorSetFlagsToClear can be used to switch between ArmorSets using multiple upgrades (see WeaponSetUpgrade).

## LocomotorSetUpgrade
Added new parameters for LocomotorSetUpgrade
* `EnableUpgrade = No` - (if 'Yes' (default),  locomotor is switched to SET_NORMAL_UPGRADED; if 'No' locomotor is switched back to SET_NORMAL)
* `ExplicitLocomotorType = SET_SUPERSONIC` - Optional; explicitly switch to the defined locomotor set (This is not persistant, unlike EnableUpgrade)

**Notes:**
EnableUpgrade only allows for a single locomotor upgrade, but it can be toggled on/off with multiple upgrades.
With ExplicitLocomotorType, Locomotor sets can be set freely, but they will not persist if the unit changes its locomotor through other means (e.g. Aircraft taking off, or combat bike switching rider)

## MaxHealthUpgrade

Added new parameter for MaxHealthUpgrade

* `MultiplyMaxHealth = 1.0` - (multiplier on the unit's current HP; default = 1.0 -> no change.)

**Notes:**
* It is recommended to change all health upgrades to multiplicative as they will not be affected by other HP changes like veterancy, and can be freely stacked (and even reversed).
* You can still use AddMaxHealth for additive boni, and could even combine it with MultiplyMaxHealth. The formula is `NewHP = (CurrentHP * MultiplyMaxHealth) + AddMaxHealth`.

## ArmorDamageScalarUpdate (New)

This module was designed for a temporary armor buff ability. The module will apply a damage/armor scale value to every unit in the area for the given amount of time. In addition, various visual effects can be added.

example:
```
Behavior = ArmorDamageScalarUpdate ModuleTag_01
    AllowedAffectKindOf = VEHICLE STRUCTURE  ; (required; any object matching *any* of these KindOfs is a valid target)
    ForbiddenAffectKindOf = INFANTRY  ; (optional; any object matching *any* of these KindOfs is an invalid target)
    AffectsTargets = ALLIES NEUTRALS  ; (required; any combination of ALLIES, NEUTRALS, ENEMIES)
    AffectAirborne = Yes   ; (default = Yes; Should this affect units currently in the air) 
    BonusDuration = 10000     ; (required; How long effect lasts, in ms)
    BonusRange = 100     ; (required; Radius of the effect)
    ArmorDamageScalar = 0.9;   ; (default = 1.0; damage scalar to apply; 1.0 = no effect; 0.5 = takes half damage; 2.0 = takes double damage; min = 0.01)
    EffectParticleSystem = <entry from particlesystem.ini>  ; (optional; apply particlesystem to affected objects. SystemLifeTime is scaled automatically. If VolumeType = BOX, the size will be changed to the object's geometry.)
    OverrideDamageFX = <entry from damageFX.ini> ; (optional; change the object's DamageFX to this entry over the duration)
    ScaleParticleSystem = Yes  ; (default = No; If enabled, the particle system's spawn rate will be adjusted for the object's size)
    ApplyColorTint = Yes  ; (default = No; Apply a dark red color tint to the objects for the duration)
End
```
**Notes:**
- This module is designed to be put on a dummy object marker, like for EmergencyRepair or Frenzy.
- To correctly track the objects over the duration, the marker object needs to stay alive for the duration.
Lifetime is tracked in the module itself. You do not need to add a lifetimeupdate as well. If the object is killed earlier, the effect will be removed from all units.
- The effect is applied once at the beginning. Objects leaving or entering the radius later will not get the effect.
- The module can be used to apply a damage increase to enemy units (i.e. lower their armor)


## CostModiferUpgrade

New Parameters added. Example:

```
Behavior = CostModiferUpgrade ModuleTag_123
  TriggeredBy = Upgrade_CostReduction  ; entry from upgrade.ini
  EffectKindOf = VEHICLE  ; Units with this KindOf will be affected
  Percentage = -20%  ; amount that cost will be reduced
  IsOneShotUpgrade = No  ; (NEW) Is this a one time global upgrade, or should it be removed when the object dies or changes owner?
  BonusStacksWith = DIFFERENT_VALUE  ; (NEW) Stacking behavior when multiple bonus sources exist
End
```

** BonusStacksWith types **
* `DIFFERENT_VALUE` - Default behavior: Does not stack with other bonus with the same value. Does stack with bonus with different value.
* `OTHER_TYPE` - Does stack with bonus from other type of unit with the same value. Does not stack with bonus from the same type of unit. I.e. Multiple OilRefineries would not stack, but a Refinery would stack with an IndustrialPlant if it had the same value.
* `SAME_TYPE` - Bonus from multiple sources of the same type will stack. I.e. Owning multiple Refineries grants additional bonus.

**Notes:**
- Bonus stacking is additive, not multiplicative


## ProductionTimeModiferUpgrade (New)

This works just like CostModiferUpgrade (Oil Refinery) to apply a global production time reduction for the given KindOf
example:

```
Behavior = ProductionTimeModifierUpgrade ModuleTag_123
  TriggeredBy = Upgrade_CostReduction  ; entry from upgrade.ini
  EffectKindOf = VEHICLE  ; Units with this KindOf will be affected
  Percentage = -20%  ; amount that build time will be reduced
  IsOneShotUpgrade = No  ; Is this a one time global upgrade, or should it be removed when the object dies or changes owner?
  BonusStacksWith = DIFFERENT_VALUE  ; Stacking behavior when multiple bonus sources exist
End
```

## ExperienceScalarUpgrade

~~Added parameter to make module active by default (useful for negativ scalars):~~
* ~~`StartsActive = False`~~

Update: removed this, because it didn't work. You will need to grant a dummy upgrade instead.


Added new parameter to modify XP value of a unit (i.e. XP that is gained for the enemy when the unit is killed)
* `AddXPValueScalar = 0.0` - Additive modifier for XP value scalar. I.e.: XPValueNew = XPValueOld * (1.0 + AddXPValueScalar).

* `SetMaxVeterancyLevel = <VeterancyLevel>` - (Raises the unit's maximum veterancy level (e.g. to `LEVEL_FOUR` or `LEVEL_FIVE`) when this upgrade is active. Lets you unlock the extra veterancy levels via an upgrade. See [Veterancy Object Parameters](#veterancy-object-parameters).)


## Laser Improvements

### LaserUpdate
New parameters are added to allow the laser to grow/shrink or fade in/out
* `BeamFadeInDuration = 0` - (time in ms. Alpha scalar is linear interpolated from 0 to 1 over the given time at the start.)
* `BeamFadeOutDuration = 0`  - (time in ms. Alpha scalar is linear interpolated from 1 to 0 at the end of the beam's lifetime. Requires LifetimeUpdate)
* `BeamGrowDuration = 0` - (time in ms. Beam width is linear interpolated from 0 to 1 over the given time at the start.)
* `BeamShrinkDuration = 0` - (time in ms. Beam width is linear interpolated from 1 to 0 at the end of the beam's lifetime. Requires LifetimeUpdate)
* `UseMultiLaserDraw = No` - (This flag is needed, if a Laser object has multiple LaserDraw modules, to correctly update all of them)
* `UseHouseColoredParticles = No` - (Apply house colored tint to the laser's muzzle and target particle systems)

### W3DLaserDraw
Added parameters to specify grid animation. Currently only a 1xn grid is allowed (i.e. multiple columns) to work together with scrollRate.
* `TextureGridTotalColumns = 1` - (total number of columns in the texture)
* `TextureGridColumns = 1` - (actual number of columns used for the animation)
* `UseHouseColorOuter = No` - (apply house color to the laser's outer color)
* `UseHouseColorOuter = No` - (apply house color to the laser's inner color)

Note: Usually both TextureGridColumns and TextureGridTotalColumns  numbers would be the same. But having separate numbers allows to use an odd number of frames and still keep proper DDS texture sizes. E.g. you have a texture width of 512, with 8 frames of 64. But you only want 7 frames, so you can set TextureGridTotalColumns=8 and TextureGridColumns=7.

## MissileAIUpdate

Added new parameters to MissileAIUpdate module:

* `RandomPathOffset = 0.0` - distance in meters. Missile projectils will spread out randomly to fly a random arc pattern for more interesting visuals.

* `ZCorrectionFactor = 2.0` - Missiles attacking air units will have their direction shifted upwards by this factor. (2.0 is vZH default)

* `ApplyLauncherBonus = No` - Any extra weapon that is fired from the projectile (On death, fireweaponupdate, etc.) will inherit the shooter's weapon bonus values 

Notes:
- *ZCorrectionFactor* is what causes AA missiles in vZH to be never aligned to the launcher. For units with actual fire pitch (e.g. Patriot), it is recommended to set this value to 0. For units that cannot pitch (e.g. Missile Defender) a value between 1 and 2 is recommended.

## DumbProjectileBehavior

New parameters:

* `ApplyLauncherBonus = No` - Any extra weapon that is fired from the projectile (On death, fireweaponupdate, etc.) will inherit the shooter's weapon bonus values 

* `DynamicHeightMinScale = 1.0` - (Scales the projectile's arc height based on the distance to the target. At maximum weapon range the full arc height is used; the closer the target, the more the height is scaled down towards this value. `1.0` = no scaling (default). Useful so short-range shots don't arc as high, e.g. for torpedoes/arcing guns.)
* `DynamicHeightMinRange = 1.0` - (The target distance at/below which `DynamicHeightMinScale` is fully reached. The height scale interpolates between this range and the weapon's max range.)

## FreeFallProjectileBehavior
New type of projectile module that mimics the behavior of Bombs dropped by genpowers.
Projectiles will move based on physics, making it suitable for carpet bomb or strafing run aircraft weapons.
On collision, the weapon's damage and detonation effects are applied.

Example:
```
Behavior = FreeFallProjectileBehavior ModuleTag_009
   MaxLifespan = 10000  ; Maximum lifetime of the object
   TumbleRandomly = No  ; Adds random rotation, same as for DumbProjectileBehavior
   CourseCorrectionScalar = 1.0   ; guide the bomb towards the intended target. 1=no homing, 0=snapto; 0.99=smooth, 0.95=too-fast
   UseWeaponSpeed = No   ; apply additional speed from the unit's weapon
   ExitPitchRate = 0 ; Angle tilt per second to apply (respects centerOfMassOffset in the projectile's physics)
   ApplyLauncherBonus = Yes  ; Apply weapon bonus from launcher for any secondary weapon effects
   DetonateOnGround = Yes   ; Will the projectile detonate when hitting the ground or live on (to bounce or rest on ground)?
   DetonateOnCollide = Yes    ; Same as above, when hitting another object (See Weapon for collission settings)
   GarrisonHitKillRequiredKindOf = <Kindof list>  ; Same as MissileAI/DumbProjectile
   GarrisonHitKillForbiddenKindOf= <Kindof list>  ; Same as MissileAI/DumbProjectile
   GarrisonHitKillCount = 0  ; Same as MissileAI/DumbProjectile
   GarrisonHitKillFX = <FXList>  ; Same as MissileAI/DumbProjectile
End
```

Notes:
* This module's behavior depends *a lot* on the projectile's physics settings, such as mass, friction, etc. It takes some finetuning to get the exact behavior you want.
* For basic carpet bombing, you can copy the settings from existing genpowers.


## ScatterShotUpdate (New)

New module intended for weapon projectiles, to allow scattering in mid air and fire multiple shots to targets within range.

Parameters:
* `Weapon` = <entry from weapon.ini> - (The weapon to be fired at targets.)
* `NumShots` = <integer> - (default = 0; number of shots to fire)
* `TargetSearchRadius` = <decimal> - (default = 0; range around the weapon's target location or object that we look for targets)
* `TargetMinRadius` = <decimal> - (default = 0; minimum range targets need to be away from the original target)
* `MaxShotsPerTarget` = <integer> - (default = 0; How many shots to fire at *each* target. 0 = randomly attack ground only; 1 = attack each target once; >1 when each target was attacked once, start from the beginning)
* `PreferSimilarTargets` = <bool> - (default = No; Choose air or ground targets similar to the main target object. I.e. if we attack an air unit, all targets will be picked from air units first)
* `PreferNearestTargets` = <bool> - (default = No; Prefer targets closest to the original target; Otherwise choose randomly)
* `NoTargetsScatterRadius` = <decimal> - (default = 0; If not targets were found/picked, use this random scatter radius to attack the ground)
* `NoTargetsScatterMinRadius` = <decimal> - (default = 0; Minimum scatter radius when no target is found)
* `NoTargetsScatterMaxAngle` = <decimal> - (default = 0; Acceptable aim delta angle for scatter targets)
* `AttackGroundWhenNoTargets` = <bool> - (default = yes; If Yes, when we run out of targets, fire randomly at the ground; If No, do not fire.)
* `AvoidOriginalTargetObject` = <bool> - (default = no; If Yes, we never try to hit the original target of the attack)
* `AvoidPreviousTargetWhenChain` = <bool> - (default = no; When the attack was fired from another ScatterShot module, avoid that modules original target)
* `TriggerDistanceToTarget` = <real> - (default = 0; How close the projectile needs to be to the target to trigger the scatter shot.)
* `TriggerLifetime` = <integer> - (default = 0; How long after the projectile was created to trigger the scatter shot. 0 = no limit.)
* `TriggerOnImpact` = <bool> - (default = No; Trigger scatter shot on projectile impact.)
* `TriggerInstantly` = <bool> - (default = No; Trigger scatter shot instantly after the projectile was created)
* `StayAliveAfterTrigger` = <bool> - (default = No; The original weapon's projectile will stay alive after the scatter shot was triggered.)
* `TriggeredDeathType` = <DeathType> - (default = NORMAL; The death type to use for the projectile object when the scatter shot is triggered.)
* `ScatterFX` = <FXList> - (default = None; entry from FXList.ini to play when scatter shot is triggered.)

**Notes:**
- The weapon used for the scatter shot needs to have a clip size large enough to fire all shots.
- When picking targets for the scatter shots, they need to be viable targets for the weapon's damage type.
- When the original weapon targets an object, that original target will always be picked to fire a scatter weapon at.
- This module can be used on non-projectile objects as well (using lifetime or instant triggers), e.g. to create an object that fires at multiple units in range simultaneously

**Examples:**
```
  Behavior = ScatterShotUpdate ModuleTag_Scatter
    Weapon = TomahawkMissileScatterWeapon    ; simple guided rocket weapon
    NumShots = 5
    TargetSearchRadius = 120.0    ; Pick targets in 120 range
    NoTargetsScatterRadius = 60.0   ; If no targets, scatter randomly in 60 radius
    MaxShotsPerTarget = 1       ; Fire at each targets once. If we have more than 5 targets, hit the ground randomly
    TriggerDistanceToTarget = 150.0   ; Trigger at this distance to target
  End
```

## Contain Modules

Added new features for all contain modules. WeaponBonus types that are passed to passengers can now be configured.

* `PassengerWeaponBonusList = <BONUS1> <BONUS2>` - (provide a list of WeaponBonus types that will be applied to the contained objects)

Default values:
- GarrisonContain: GARRISONED
- TransportContain: CONTAINED

Setting `PassengerWeaponBonusList = None` will override the default value.

Note: HelixContain grants GARRISONED to its passengers in vanilla ZH. This is changed to CONTAINED. To restore vanilla behaviour you will need to manually set the bonus here.

### Targeting through addon turrets

A unit whose weapons live on a contained addon turret (an OverlordContain or MultiAddOnContain
rider) could not be ordered to attack anything only the turret can hit -- the cursor showed a red
cross, and the workaround was a dummy weapon on the carrier with the turret's range. This key
replaces the dummy weapon. It works on every contain module except TunnelContain and CaveContain,
where it parses but stays inert: their contained list is the player's whole shared network, so
the answers would come from units sitting at other entrances.

* `AcceptTargetsForPassengers = No` - (Yes lets the container accept attack orders on behalf of its
passengers: when the container's own weapons cannot attack a target, the passengers' weapons are
asked instead, and their best answer drives the cursor and the order. Requires
`PassengersAllowedToFire = Yes`.)

```
Behavior = OverlordContain ModuleTag_Turret
  Slots                     = 1
  AllowInsideKindOf         = PORTABLE_STRUCTURE
  PassengersAllowedToFire   = Yes
  PassengersInTurret        = Yes
  PayloadTemplateName       = AmericaThorTurretBolt
  AcceptTargetsForPassengers = Yes   ; New
End
```

Behaviour notes:
* When the order is given, the turret is told to attack (the engine already forwards attack orders
to firing passengers); the carrier's own guns do not try to aim at a target they cannot attack.
* A turret that is EMP'd, hacked, subdued or paralyzed does not answer for the carrier.
* The turret cannot move, so a target beyond its range shows the out-of-range cursor rather than a
green attack cursor -- the carrier does not automatically drive into range. Keep a dummy weapon if
you want the carrier to approach on its own.

### Add-on turret range from the carrier's center

An add-on turret is a separate object placed at a bone on its carrier -- `AddOnBoneName` for
MultiAddOnContain, the FIREPOINT bones for OverlordContain -- and with `PassengersInTurret = Yes`
that bone rides the carrier's turret. Weapon range is measured from the firing object's own
position and bounding circle, so the turret's reach swings by the bone's offset as the carrier's
turret sweeps: longer the way the barrel points, shorter the other way. The Overlord's gattling
shows it, and a turret mounted at the front of a chassis shows it plainly -- it opens fire early
forward and falls short backward, and no single `AttackRange` works in both directions.

* `AddOnWeaponRangeFromCenter = No` - (Yes measures the add-on's weapon range from the carrier's
center, using the carrier's bounding circle, instead of from its own attachment point.)

```
Behavior = OverlordContain ModuleTag_Turret
  Slots                      = 1
  AllowInsideKindOf          = PORTABLE_STRUCTURE
  PassengersAllowedToFire    = Yes
  PassengersInTurret         = Yes
  PayloadTemplateName        = AmericaThorTurretBolt
  AddOnWeaponRangeFromCenter = Yes   ; New
End
```

Behaviour notes:
* An add-on whose `AttackRange` equals the carrier's now reaches exactly as far as the carrier
does, in every direction, at every turret angle -- the range no longer has to be tuned to
compensate for where the bone sits.
* `MinimumAttackRange` moves with it, so the too-close band is measured from the carrier's hull
too, as it would be for a weapon mounted on the carrier.
* The approach distance moves with it, so a carrier ordered to attack stops where its add-on can
actually reach.
* Only the range test moves. The add-on still aims and fires from its own barrel, and its line of
sight, muzzle effects and projectiles are unchanged.
* Parses on every contain module but stays inert on TunnelContain and CaveContain, whose passengers
sit at whichever entrance they used rather than on a bone of the container.


### Filtering by object name

`AllowInsideKindOf` and `ForbidInsideKindOf` can only speak in whole KindOfs. These two name
individual objects instead, and work on every contain module -- TunnelContain, TransportContain,
GarrisonContain, CaveContain, OverlordContain, HelixContain and the rest.

* `ForbidInsideObjects = <object list>` - (These objects can never enter. Checked first, so it
beats everything else, including `AllowInsideObjects`.)
* `AllowInsideObjects = <object list>` - (If set, ONLY these objects may enter, whatever their
KindOfs say. Leave it out to allow everything the other filters permit.)

Example -- a tunnel network that refuses one specific unit:
```
Behavior = TunnelContain ModuleTag_05
  TimeForFullHeal = 5000
  ForbidInsideObjects = Aslt_GLAInfantryHijacker
End
```

Example -- a transport that carries nothing but two named units:
```
Behavior = TransportContain ModuleTag_07
  ContainMax = 4
  AllowInsideObjects = Aslt_GLAInfantryJarmenKell Aslt_GLAInfantryHijacker
End
```

Both keys take several names per line and append across repeated lines, so a long list can be
split up. Matching is on the object name and ignores case.

Note: these filters decide whether a unit may *enter*. They do not evict anyone already inside,
so changing them does not affect units that are already loaded.

## StickyBombUpdate#

Added new parameters to customize the 2D anim visuals:

* `Animation2DBase = <entry from Animation2D.ini>` - 2D anim for background bomb visuals (default = BombRemote)
* `Animation2DTimed = <entry from Animation2D.ini>` 2D anim for foreground/timer visuals; This is used when the bomb object has a LifeTimeUpdate (default = BombTimed) 
* `ShowTimer = Yes`  - Flag to disable Animation2DTimed for timed bombs - Only use base animation.

Note: Setting Animation2DBase or Timed to "None" will hide them.

## CrateCollide

Added new parameters for all CrateCollide modules:

* `AllowNeutralPlayer = No` - Crate can be picked up by the neutral player
* `AllowPickAboveTerrain = No` - Crate can be picked up when it is above ground.

## StickyBombCrateCollide

CrateCollide module to apply sticky bombs to objects. Can be used with pickup crates or projectiles.

Example:
```
Behavior = StickyBombCrateCollide ModuleTag_419
   NeedsTarget = No ; Needs an intended target object (e.g. from a projectile) cannot be picked up by regular collission
   StickyBombObject = <Object with StickyBombUpdate>  ; The object to create
   AllowMultiCollide = No ; If used as a crate, can the effect be applied to multiple objects at once?
   ShowInfiltrationEvent = No  ; If enabled, shows a radar event for the target player
   ChanceToTriggerPercent = 100%; Chance to create the StickyBombObject.
End
```

Example to use on a Missile or DumbProjectile to apply a sticksBomb to the target:
```
Behavior = StickyBombCrateCollide ModuleTag_419
  RequiredKindOf = STRUCTURE
  ForbiddenKindOf = VEHICLE INFANTRY
  BuildingPickup = Yes
  ExecuteFX = FX_SniperDroneTargetTracerPingFX
  AllowNeutralPlayer = Yes
  AllowPickAboveTerrain = Yes
  NeedsTarget = Yes
  StickyBombObject = Lazr_SniperDroneTargetTracerStickyBomb
End

; Needs separate modules for Building/Unit collissions
Behavior = StickyBombCrateCollide ModuleTag_420
  ;RequiredKindOf = VEHICLE INFANTRY
  ForbiddenKindOf = STRUCTURE
  BuildingPickup = No
  ExecuteFX = FX_SniperDroneTargetTracerPingFX
  AllowNeutralPlayer = Yes
  AllowPickAboveTerrain = Yes
  NeedsTarget = Yes
  StickyBombObject = Lazr_SniperDroneTargetTracerStickyBomb
End
```

## AdvancedCollide (New)
A collide module with more versatile parameters

* `CollideWeapon = <weapon entry>` - Weapon to fire on collide
* `OCL = <OCL entry>` - OCL to create on collide
* `FX = <FXList entry>` - FX to create on collide
* `FireOnce = No` - trigger once or for every collission
* `CollideWithGround = No` - trigger collision when hitting the ground
* `CollideWithObjects = Yes` - trigger collission when hitting objects.
* `RequiredStatus = <status entry>` - required status for this object
* `ForbiddenStatus = <status entry>` - forbidden status for this object
* `TargetRequiredStatus = <status entry>` - required status for the object we collide with
* `TargetForbiddenStatus = <status entry>` - forbidden status for the object we collide with
* `RequiredKindOf = <KindOf entry>` - required KindOf for the object we collide with
* `ForbiddenKindOf = <KindOf entry>` - forbidden KindOf for the object we collide with
* `ChanceToTriggerPercent = 100%` - Chance to trigger effects on collide
* `RollOnceForTrigger = No` - Roll for trigger chance every time we collide, or only the first time (and then keep the result)

Note: This module allows to trigger effects when objects collides with the ground, e.g. when a projectile misses it's target.

## LifeTimeUpdate

Add new parameter:
*`ShowProgressBar = No` Enables progress bar above unit health to visualize lifetime. Requires SHOW_PROGRESS_BAR KindOf.


## RadiusDecalBehavior (New)

New module to create a radius decal that follows a unit. Can be triggered via upgrade.

Example:
```
Behavior = RadiusDecalBehavior ModuleTag_decal1
    TriggeredBy = <Upgrade entry>  ; Optional
    ; < All other basic upgrade entries (ConflictsWith, RemovesUpgrades, FXListUpgrade, RequiresAllTriggers) >
    StartsActive = No  ; Set to yes, to enable initially
    Radius = 100.0 ; (radius in meters)
    RadiusDecal   ; Decal Template, Same parameters as DeliveryDecal in OCL ini
      Texture           = SCCFuelAirBomb_USA   ; Texture name
      Style             = SHADOW_ALPHA_DECAL   ; SHADOW_DECAL, SHADOW_ALPHA_DECAL or SHADOW_ADDITIVE_DECAL
      OpacityMin        = 25%    ; default = 100%, Opacity will move between min and max
      OpacityMax        = 50%    ; default = 100%
      OpacityThrobTime  = 500    ; default = 1000
      Color             = R:255 G:0 B:0 A:255 
      OnlyVisibleToOwningPlayer = Yes     ; Disable to make the decal visible to all players
      End
  End
```
Note: This module reacts to the upgrade being removed (by a different upgrade). This means you can make the decal toggle on/off.

Added parameters:
* `DecalRadius = 0.0` - (Overrides the decal radius separately from the `Radius` field; if 0, `Radius` is used.)
* `WorksWhileContained = No` - (If Yes, the decal stays visible/active while the object is contained inside a transport or structure.)

## ParkingPlaceBehavior

Added new features and parameters to ParkingPlaceBehavior (Airfield)

* `ParkedUnitsDamageScalar = 1.0` - scalar for damage taken applied to all parked aircraft (0.9 = only take 90% of damage)
* `ParkedUnitsDamageScalarUpgraded = 1.0` - same as above, after upgrade
* `DamageScalarUpgradedTriggeredBy = <upgrade name>` - upgrade to trigger the upgraded scalar value
* `RequiredKindOf = <KindOf list>` - if set, only aircraft that has these KindOfs is allowed to dock here
* `ForbiddenKindOf = <KindOf list>` - if set, aircraft that has these KindOfs is not allowed to dock here.

Notes:
* ParkedUnitsDamageScalar can be used to apply an upgrade that grants damage protection to parked aircraft
* Required/Forbidden KindOf can be used to allow only specific kinds of aircraft to land (i.e. to use different sizes, or differ between VTOL/Regular jets)

## PoisonedBehavior

Added Beta and Gamma poison tiers, so the poison-over-time effect can be strengthened once the attacker owns an upgrade. The retail parameters keep working unchanged.

* `PoisonBetaDamageInterval = 0` - tick interval while Beta poison is active. 0 uses `PoisonDamageInterval`
* `PoisonBetaDuration = 0` - how long Beta poison lasts after the last dose. 0 uses `PoisonDuration`
* `PoisonBetaDamageBonus = 1.0` - multiplier on the per-tick damage (1.2 = 20% more)
* `PoisonBetaTriggeredBy = <upgrade name>` - upgrade the attacker needs for Beta poison. Empty disables the tier
* `PoisonGammaDamageInterval = 0` - as above, for Gamma
* `PoisonGammaDuration = 0` - as above, for Gamma
* `PoisonGammaDamageBonus = 1.0` - as above, for Gamma
* `PoisonGammaTriggeredBy = <upgrade name>` - as above, for Gamma

Notes:
* The tier is picked per hit from the attacker, not the victim. The upgrade counts if the attacking player has researched it or the attacking object itself carries it, so both player and object upgrades work.
* Gamma is checked before Beta. If neither upgrade is present the hit applies normal poison.
* Only one tier is active at a time and poison never stacks. A stronger hit takes over damage, interval and duration; an equally strong hit refreshes them as retail does; a weaker hit is ignored while the stronger poison is still running.
* A hit with `DeathType = POISONED` is promoted to `POISONED_BETA` or `POISONED_GAMMA` for the active tier, so die modules and death FX can tell the tiers apart. A weapon that already names a tier death type is left alone.
* If the attacker no longer exists when the damage lands, the hit counts as normal poison.
* Healing of any kind still cures poison completely and clears the tier.

## NeutronBlastBehavior

The neutron blast that kills infantry and leaves vehicles unmanned. Two parameters were added so
the effect can be kept off things it should not touch.

```
Behavior = NeutronBlastBehavior ModuleTag_neutron
  BlastRadius = 100.0
  AffectAirborne = No
  AffectAllies = No
  AffectGarrison = Yes         ; New
  RejectEffectOnUnit = CyborgCommando AnotherUnit YetAnotherUnit   ; New
End
```

Added parameters:
* `AffectGarrison = Yes` - (No spares infantry garrisoned in a structure. Only garrisons are
affected by this: passengers of transports, tunnels and bunkers are killed either way, as before.)
* `RejectEffectOnUnit = <object list>` - (Object names that the blast skips entirely, whatever their
KindOfs. This is the only way to spare a unit the hardcoded infantry and vehicle rules, which is what
it exists for: riders such as the Cyborg Commando. A rejected object keeps its passengers too. All
names go on one line; a second line replaces the first. Matching ignores case.)

## UnitProductionBonusUpgrade (New)

This upgrade module allows to set a cost and/or build time modifier for individual types of units. This affects the whole player and not just individual factories.

Parameters:
* `<All common upgrade params; e.g. TriggeredBy>`
* `CostModifierPercentage = 0` - Percentage amount that the unit's costs are modified
* `BuildTimeModifierPercentage = 0` - Percentage amount that the unit's build time is modified
* `UnitTemplateName = <Name of an Object>` - The unit to apply this bonus to. (multiple lines are allowed)
* `IsOneShotUpgrade = No` - (Yes makes the bonus permanent: it is never removed when the granting
object dies and does not transfer when it is captured.)
* `BonusStacksWith = NO_STACKING` - (`NO_STACKING` | `OTHER_TYPE` | `SAME_TYPE`. How the bonus
stacks when several objects grant it. `NO_STACKING` is the previous behaviour, where only differing
percentages stack; `OTHER_TYPE` stacks across different source object types; `SAME_TYPE` stacks
across objects of the same type.)

**Example**
```
Behavior = UnitProductionBonusUpgrade ModuleTag_01
  TriggeredBy = Upgrade_CostReduction   ; The upgrade trigger
  CostModifierPercentage = -40%    ; Cost is reduced by 40%
  BuildTimeModifierPercentage = -80%    ; Build time is reduced by 80%
  UnitTemplateName = Tank_ChinaInfantryRedguard    ; The units this bonus applies to
  UnitTemplateName = Tank_ChinaTankBattleMaster
End
```

**Notes**
- The bonus is now removed when the granting object is destroyed and transfers with it on capture,
so it works for per-building upgrades such as Tech OilRefinery and not only as a global upgrade.
Set `IsOneShotUpgrade = Yes` to get the old permanent behaviour back.

## StealthUpgrade

Added new parameters
* `EnableStealth = Yes` - If set to No, this will disable the object's stealth instead of enabling it. Can be used for a toggleable upgrade.

* `OverrideStealthForbiddenConditions = <StealthForbiddenConditionFlags>` - Override StealthForbiddenConditions in StealthUpdate for this unit.

Note: if OverrideStealthForbiddenConditions is set, EnableStealth is ignored as there is no reason to use both functionalities at the same time.

## BattlePlanBonusBehavior (New)

New module that can be used to customize the effects of BattlePlans for individual units. This module needs to go to the unit that receives the battle plan bonus effects, not the StrategyCenter.

Parameters:
```
Behavior = BattlePlanBonusBehavior ModuleTag_BPB
  ; Generic Upgrade parameters:  
  TriggeredBy = <Upgrade entry>
  ; ---
  StartsActive = No  ; Active immediately or requires upgrade?
  OverrideGlobalBonus = No  ; Enable to skip default BattlePlan bonus effects (i.e. values from BattlePlanUpdate, or WeaponBonus effects) for this unit
  ShouldParalyzeOnPlanChange = Yes  ; Should this unit be paralyzed/disabled while switching battle plans?
  ; -- Bonus entries -- Valid <BATTLE_PLAN_NAME> are BOMBARDMENT, SEARCH_AND_DESTROY and HOLD_THE_LINE
  WeaponSet = <BATTLE_PLAN_NAME> <WEAPON_SET_FLAG>  ; Set this weaponset flag while this battleplan is active
  ArmorSet = <BATTLE_PLAN_NAME> <WEAPON_SET_FLAG>  ; Set this armorset flag while this battleplan is active
  WeaponBonus = <BATTLE_PLAN_NAME> <WEAPON_BONUS_NAME>  ; Grant this weaponbonus while this battleplan is active
  ArmorDamageScalar = <BATTLE_PLAN_NAME> 1.0   ; Set damage reduction while this battleplan is active
  SightRangeScalar = <BATTLE_PLAN_NAME> 1.0  ; Set a sight range bonus while this battleplan is active
  StatusToSet = <BATTLE_PLAN_NAME> <STATUS_TYPE>   ; Set this status when enabling this battle plan. Clear it on battle plan removal
  StatusToClear = <BATTLE_PLAN_NAME> <STATUS_TYPE>  ; Clear this status when enabling this battle plan. Set it on battle plan removal
End
```

### Examples

**Example 1 - Give extra effects to Battle plans**
```
Behavior = BattlePlanBonusBehavior ModuleTag_BP1
  TriggeredBy = Upgrade_BattlePlanBonusExample
  OverrideGlobalBonus = No
  ShouldParalyzeOnPlanChange = Yes
  WeaponSet = BOMBARDMENT PLAYER_UPGRADE
  WeaponBonus = SEARCH_AND_DESTROY FRENZY_THREE
  StatusToClear = HOLD_THE_LINE CAN_STEALTH
  ArmorDamageScalar = HOLD_THE_LINE 0.9
End
```
This unit gains the regular battlePlan bonus effects with additional effects:
- With BOMBARDMENT we switch to the upgraded WeaponSet
- With SEARCH_AND_DESTROY we gain an extra weapon bonus
- With HoldTheLine we gain additional damage reduction, but lose the ability to stealth.

**Example 2 - Allow units to gain battle plans**

```
Behavior = BattlePlanBonusBehavior ModuleTag_BP0
  StartsActive = Yes
  OverrideGlobalBonus = Yes
  ConflictsWith = Upgrade_GrantBattlePlans
  ShouldParalyzeOnPlanChange = No
End
  
Behavior = BattlePlanBonusBehavior ModuleTag_BP1
  TriggeredBy = Upgrade_GrantBattlePlans
  OverrideGlobalBonus = No
  ShouldParalyzeOnPlanChange = Yes
End
```
This adds no additional effects, but makes the unit skip default BattlePlan effects (and paralyze) initially, and applies them only after the upgrade.
Note: For this to work, the unit needs to be inlcuded in the BattlePlan's valid KindOfs, even though the intention is to not give them the battlePlan effects by default.

## WeaponBonusUpdate

Added Parameters:
* `AffectsTargets` = [ALLIES/ENEMIES/NEUTRALS]  ; Allows applying a bonus to enemies
* `AffectsAirborne` = Yes  ; allow/disallow the bonus be applied to currently airborne units
* `TintStatusType` = [TINT_STATUS type]  ; which color tint to apply
* `BonusConditionType` = [WeaponBonus type]  ; The WeaponBonus condition flag that is granted to affected units (default = the module's own bonus). Lets you pick which WeaponBonus the module applies.

## SpecialAbilityUpdate

The valid targets for the laser lock ability (`SPECIAL_MISSILE_DEFENDER_LASER_GUIDED_MISSILES`) used to be hardcoded to
`VEHICLE`, which in Zero Hour covers aircraft as well as ground vehicles, and to enemies only. These three keys make
the target filter data-driven.

* `ForbiddenTargetKindOf = <KindOf list>` - (The target may have none of these. Checked first, so it beats
`RequiredTargetKindOf`. Default = `STRUCTURE`.)
* `RequiredTargetKindOf = <KindOf list>` - (The target must have **all** of these. Default = `VEHICLE`.)
* `TargetRelationship = [ALLIES/ENEMIES/NEUTRALS]` - (Which relationships to the owner may be targeted. Several may be
listed together. Default = `ENEMIES`.)

Example - an Avenger that can only lock onto enemy ground vehicles:
```
Behavior = SpecialAbilityUpdate ModuleTag_09
  SpecialPowerTemplate  = SpecialAbilityLaserGuidedMissiles
  StartAbilityRange     = 200.0
  RequiredTargetKindOf  = VEHICLE
  ForbiddenTargetKindOf = STRUCTURE AIRCRAFT   ; New
  TargetRelationship    = ENEMIES              ; New
End
```

The defaults reproduce the original hardcoded behaviour exactly, so leaving all three keys out changes nothing.

Behaviour notes:
* `RequiredTargetKindOf` is an all of test, not an any of test. Listing two KindOfs means the target must have both.
* All three keys are honoured in two places, and always agree: the check that decides whether the cursor and the order
are valid, and the check that runs while a lock is already in progress and cancels it if the target stops qualifying.
Because the relationship is part of that test, a lock breaks off if the target changes sides mid lock - by being
hijacked or captured - unless the new relationship is also allowed.
* To let the ability target allies, **both** `TargetRelationship` here and `NEED_TARGET_ALLY_OBJECT` on the
CommandButton are needed. The button filters the player's click; this key covers the paths that never see a button and
keeps checking while the lock is held.
* Allowing a relationship other than `ENEMIES` means the ability really will shoot that target - the missile is fired as
a forced attack, so the usual "you may not attack allies" rule does not stop it. Do not allow allies unless that is what
you want.
* The AI never reads CommandButton target options, and its own scan only considers enemies, so it will not use ally or
neutral locking even when both are configured.
* These keys currently affect the laser lock ability only. Other abilities that use `SpecialAbilityUpdate` keep their
own built in target rules and ignore all three.

## DelayedUpgradeBehavior (New)

Applies or Removes Upgrade(s) after a set time. Initially triggered by an upgrade. This can be used to make temporary upgrades.

Example:
```
Behavior = DelayedUpgradeBehavior ModuleTag_DelUp
  TriggeredBy = <Upgrade entry>   ; Initial Trigger
  ; additional generic Upgrade params (ConflictsWith, RemovesUpgrades, FXListUpgrade, RequiresAllTriggers)
  UpgradesToTrigger = <Upgrade1, Upgrade2, ...>   ; Upgrades that will be triggered after the delay
  UpgradesToRemove = <Upgrade1, Upgrade2, ...>    ; Upgrades that will be removed after the delay
  TriggerAfterTime = <time in msec>    ; Time delay
End
```

## UpgradeSpecialPower (New)

Trigger an upgrade via a special power. This can be used to (e.g. in combination with DelayedUpgradeBehavior) to create a temporary upgrade with a cooldown.

Example:
```
Behavior = UpgradeSpecialPower ModuleTag_SpecUp01
  SpecialPowerTemplate = <SpecialPower Entry>
  ; Other params from SpecialPower module (UpdateModuleStartsAttack, StartsPaused, InitiateSound, ScriptedSpecialPowerOnly)
  UpgradeToGrant = <Upgrade Entry>
End
```

## OCLSpecialPower

Added new parameter:
*`SelectCreatedObject = No` The (first) created object will be automatically selected by the player

* `MinDistToSimilarRadius = 0.0` - (If > 0, the special power cannot be targeted within this radius of another object created by the same OCL special power. Useful to prevent stacking multiple deployments on top of each other. Default = 0 = no limit)

## OCLUpdate

Added parameters for directional payload delivery (e.g. for OCLs that spawn a delivery aircraft, to make it approach from a consistent direction):
* `DirectionalDelivery = No` - (If Yes, the delivered payload/aircraft is aligned to the source object's direction instead of a fixed/default direction.)
* `DirectionalDeliveryFurthestEdge = No` - (If Yes, always pick the map edge furthest along the delivery angle as the spawn/approach point.)

## TeleporterAIUpdate (New - EXPERIMENTAL)
** Warning: This module is experimental, i.e. it is not fully tested and might undergo revisions**

AIUpdate module designed to work like a Chrono Legionnaire in RA2. I.e. The unit teleports instead of moving normally, and needs to recharge depending on the distance it teleported.
** Note: Currently this module is only working properly for infantry units. It currently does not work properly for vehicles and it is not certain it ever will.**

Example:
```
Behavior = TeleporterAIUpdate ModuleTag_123
   TeleportStartFX = [FXList entry] ; FX to play at the unit's previous position when it teleports
   TeleportTargetFX = [FXList entry] ; FX to play at the unit's target position when it teleports
   TeleportRecoverEndFX = [FXList entry] ; FX to play on the unit, when it has finished recovering
   MinDistanceForTeleport = 20.0  ; distance in which the unit moves normally instead of teleporting
   DisabledDurationPerDistance = 10.0 ; How long the unit will need to recover, linear to the distance (in MS per distance). Recommended values = 5-10. 
   TeleportRecoverSoundAmbient = [AudioEvent entry] ; Ambient sound played on the unit while it recovers from a teleport.
   TeleportRecoverOpacityStart = 10%  ; Unit maximum transparency when it starts to recover.
   TeleportRecoverOpacityEnd = 80%  ; Unit minimum transparency when it has finished recovering.
   TeleportRecoverTint = TELEPORT_RECOVER  ; Color tint status to apply while recovering
   [entries from AIUpdate]
End
```
When a unit is recovering from a teleport, the conditionstate `TELEPORT_RECOVER` is set. Note: In most cases, the unit will also be MOVING after a teleport, so it's recommended to define both states.

## ChronoDeathBehavior (New)
Death Module for "Chrono" death. This makes the unit fade out and shrink.

Example:
```
Behavior = ChronoDeathBehavior ModuleTag_ChronoDeath
  DeathTypes = NONE +CHRONO  ; Death types to use
  ; Additional DieModule params (VeterancyLevels, ExemptStatus, RequiredStatus)
  StartFX = <FXList entry> ; FX to play at the start
  EndFX = <FXList entry> ; FX to play at the end
  ; Note: The following params are meant to be used (instead of StartFX/EndFX) when used in Default/Object.ini:
  StartFXInfantry = <FXList entry> ; played if the object is INFANTRY
  StartFXVehicle = <FXList entry> ; VEHICLE
  StartFXStructure = <FXList entry>  ; STRUCTURE
  EndFXInfantry = <FXList entry> ; played if the object is INFANTRY
  EndFXVehicle = <FXList entry> ; VEHICLE
  EndFXStructure = <FXList entry>  ; STRUCTURE

  OCL = <OCL entry> ; OCL to create at the start
  OCLDynamicGeometryScaleFactor = 0.0;  Used in combination with DynamicGeometryClientUpdate. If this is set, the created object will be scaled to the parent object's radius. The value here should be set to the model's default size (e.g. a value of 10.0 corresponds to a sphere with radius 10.0)

  StartScale = 1.0  ; Unit scale at the start
  EndScale = 0.1    ; Unit scale at the end
  StartOpacity = 25%  ; Opacity at the start
  EndOpacity = 0%    ; Opacity at the end
  DestructionDelay = 500   ; Duration of the sequence in ms
End
```

## DynamicGeometryClientUpdate (New)
Client module for simple animations on a drawable's opacity and scale. Can be useful for effects like EMP bubbles.

Example:
```
ClientUpdate = DynamicGeometryClientUpdate ModuleTag_02
  Opacity = INITIAL 0.0      ; value when object is created
  Opacity = MIDPOINT 1.0     ; value at midpoint duration
  Opacity = FINAL 0.0        ; value at end of duration
  Scale = INITIAL 0.0
  Scale = MIDPOINT 1.0
  Scale = FINAL 0.0
  Interpolation = SMOOTH   ; How values in between are interpolated - SMOOTH (sinus curve) or LINEAR
  TotalDuration = 666   ; in ms
  MidpointDuration = 200    ; in ms
End
```


## ResetSpecialPowerTimerWhileAliveUpdate (New)
Update Module to reset a player global (shared synch timer) superweapon while a unit is alive, this allows super unit deploys to start the cooldown when the unit is dead

Example:
```
  Behavior = ResetSpecialPowerTimerWhileAliveUpdate ModuleTag_x
    SpecialPowerTemplate = Tank_SuperweaponUniqueUnit  ; a superweapon template with shared synch timer ( like genpowers)
  End
```

## W3DModelDraw

Added parameters (To the main Draw module block, not the individual ConditionStates):

* `IgnoreAnimationSpeedScaling = No` - ignore animation scaling (e.g. for PreAttackDelay) for a draw module. Useful for having Looping animations (e.g. rotor blades or flags) along with a pre_attack anim in another draw module.

* `IgnoreRotation = No` - This model will always stay aligned to the world (i.e. Z up), ignoring rotation via movement. Do not use this on modules with Firebones or Turrets.

* `OnlyVisibleToOwningPlayer = No` - This model will only be visible to the owning player. Do not use it on modules with Firebones or Turrets.

## Energy Shields
Added a Body and Behavior module for rechargable energy shields. This adds another health bar on top and absorbs damage until it's depleted.

### ShieldBody
This Body is required for shields to work.

```
Body = ShieldBody ModuleTag_ES1
  ;<ActiveBody paramters>
  StartsActive = No  ; Enable Shields by default; Otherwise an upgrade is required (defined in EnergyShieldBehavior)
  ShieldMaxHealth = 100  ; Health points of the shield (only needed if ShieldMaxHealthPercent is not set)
  ShieldMaxHealthPercent = 30%  ; Health points relative to MaxHealth (overrides ShieldMaxHealth)
  ShieldArmorSetFlag = PLAYER_UPGRADE  ; (Optional) ArmorSetFlag to use while the shield is up
  ShieldPassThroughDamageTypes = <Damage Types> ; List of damage types that will go through the shield and damage the Health. NONE +X, ALL -Y notation.
  DefaultShieldPassThroughDamageTypes = <Damage Types> ; This has the same functionalty as above, but is predefined with all utility damage types that should always be passed through. This should not be defined in most cases, but can be overriden
End
```
NOTE: DefaultShieldPassThroughDamageTypes default values: NONE +STATUS +DEPLOY +UNRESISTABLE +HEALING +PENALTY +DISARM +HAZARD_CLEANUP +TOPPLING +SUBDUAL_UNRESISTABLE +CHRONO_UNRESISTABLE

### EnergyShieldBehavior
The second required module for shield logic

```
Behavior = EnergyShieldBehavior ModuleTag_ES2
    StartsActive = No  ; Enabled by default (no upgrade required)
    TriggeredBy = <Upgrade Name>
    ShieldRechargeDelay = 5000  ; Time (ms) of no damage taken, until shield starts recharging.
    ShieldRechargeRate = 100  ; Delay (ms) between shield recharge ticks
    ShieldRechargeAmount = 0  ; How much shield to recharge per tick (Absolute. Use only one)
    ShieldRechargeAmountPercent = 2%  ; How much shield to recharge per tick (Percentage. Use only one)
    ShieldHealthBarColor = R:128 G:255 B:255 A:255  ; RGBA color for the shield health bar
    ShieldHealthBarBackgroundColor = R:0 G:0 B:0 A:255  ; RGBA color for the shield health bar outline
    ShowHealthBarBackgroundWhenEmpty = No   ; Show empty frame when shield is depleted
    ShowHealthBarWhenUnselected = No ; Show shield health bar when the unit is not selected or mousover
    ShieldModelCondition = <ConditionStateName>  ; Optional: Conditionstate to set when shield is active
    ShieldSubObjectName = <SubObjName> ; Optional: Model subobject to show when the shield is active
    ShieldHitModelCondition = <ConditionStateName> ; Optional: Conditionstate to set when shield was hit
    ShieldHitSubObjectName = <SubObjName> ; Optional: Model subobject to show when the shield was hit
    ShieldHitConditionDuration = 500  ; Optional: How long to show conditionstate when shield was hit
    ShieldDownFX = <FXList> ; Optional: FX to play when shield is depleted
    ShieldUpFX = <FXList> ; Optional: FX to play when a depleted shield starts recharging
End
```

** Note: for the shield health bar to work, the KindOf "SHOW_PROGRESS_BAR" is required on the object** 

** Note2: When using both ShieldHitModelCondition and ShieldModelCondition parameters: When the shield is hit, both conditionstates are active at the same time. So a setup like this is required:

```
Draw = W3DModelDraw ModuleTag_ES0
  ; Inactive Shield
  DefaultConditionState
    Model = None
  End
		
  ; Active Energy Shield
  ConditionState = USER_1
    Model = ShieldActive
  End
	
  ; Energy Shield Hit
  ConditionState = USER_1 USER_2
    Model = ShieldHit
  End
End
```


## MultiAddOnContain (New)

New contain module to replicate MARV turret functionality from Kane's Wrath:
The object (Can be used on a structure or vehicle) has a container for multiple infantry units.
Each contained infantry, will spawn a portable structure on the unit, that will move and attack along with it.
Each type of infantry has it's own portable structure defined. Only units defined in the list can enter.
There is no limit on how many addOns can be defined.

Example:

```
Behavior = MultiAddOnContain ModuleTag_123
  ; Params from TransportContain:  
  Slots = 4
  EnterSound = GarrisonEnter
  ExitSound = GarrisonExit
  ImmuneToClearBuildingAttacks = Yes
  DamagePercentToUnits = 100%
  IsEnclosingContainer = No
  ; New Params
  PayloadTemplateName = <Name of Infantry object to be contained by default> ; can have multiple PayloadTemplateName entries
  AddOnBoneName = STATION  ;Name of bone to place the addOns (uses numbering from 00-99 for available slots)
  AddOnEntry = <Name of contained Infantry object 1> <Name of Portable Structure object 1>
  AddOnEntry = <Name of contained Infantry object 2> <Name of Portable Structure object 2>
  AddOnEntry = <Name of contained Infantry object 3> <Name of Portable Structure object 3>
  ...
  EmptySlotSubObjectName = COVER_EMPTY   ;Name of any subobject (numbered) that should be visible when a slot is empty
  OccupiedSlotSubObjectName = COVER_OCCUPIED   ;Name of any subobject (numbered) that should be visible when a slot is filled (hide this in art code by default)
End
```
### Container Draw Module Requirements

Like with OverlordContain, the container object needs to have one of

- `W3DOverlordTankDraw`
- `W3DOverlordTruckDraw`
- `W3DOverlordAircraftDraw`  - Note: Contary to the name, this is the default module to use (e.g for Structures) when no extra behavior is required

A new parameter was added to these modules that needs to be set when used with MultiAddOnContain:
* `HasMultiAddOns = Yes`

### AddOn Draw Module Requirements

Like with Overlord addOns, the contained portable structure object needs to have `W3DDependencyModelDraw`

`AttachToBoneInContainer` should now be set to the same numbered bone used for `AddOnBoneName` in MultiAddonContain. E.g. "STATION" if using STATION01-XX

## Drone Carrier system

Added modules to support a simplified mobile aircraft carrier.

### DoneCarrierContain

```
  Behavior = DroneCarrierContain ModuleTag_Contain
    ;<TransportContain parameters>
    Slots = 123
    HealthRegen%PerSec = 3.0
    ExitBone = STATION
    ExitDelay = 300
    HealthRegen%PerSec = 3.0
    ...
    ;
    LaunchVelocityBoost = 2.5
    LaunchBone = STATION
    NumLaunchBones = 1
    EnterPositionOffset = X:0 Y:0 Z:0
    EnterPositionOffset = X:0 Y:0 Z:0  ;Multiple entries allowed
    ...
    ContainedUnitsDeathType = TOPPLED
    KeepSlotAssignment = Yes
  End
```
### DroneCarrierAIUpdate

```
  Behavior = DroneCarrierAIUpdate ModuleTag_AI
    ;<AIUpdate parameters>
    Slots = 8
    RespawnTime = 3000
    DroneTemplateName = Lazr_AmericaVehicleCarrierDrone
    DronesEnterMainDoor = Yes
  End
```

### DroneCarrierSlavedUpdate

```
  Behavior = DroneCarrierSlavedUpdate ModuleTag_123
    <SlavedUpdate params>
    LeashRange = 400.0
  End
```

### CarrierDroneAIUpdate
```
  Behavior = CarrierDroneAIUpdate ModuleTag_312
    DockingDistance = 70.0
    DockingLocomotorType = SET_SLUGGISH
    LaunchingLocomotorType = SET_TAXIING
    LaunchTime = 750
  End
```
TODO: In-depth description

## BuffUpdate

Works similar to WeaponBonusUpdate (Frenzy module) to apply buffs to objects in an area

- `RequiredAffectKindOf = <List of KindOfs>`
- `RequiresAllKindOfs = Yes/No   ; Require all listed KindOfs, or just one of them`
- `ForbiddenAffectKindOf = <List of KindOfs>`
- `AffectsTargets = <combination of ALLIES | ENEMIES | NEUTRALS>`
- `AffectAirborne = Yes/No`
- `BuffDuration = duration in msec  ; how long to apply the buff`
- `BuffDelay = duration in msec  ; how often to apply the buff`
- `BuffRange = 0.0  ; area radius`
- `BuffTemplateName = <BuffTemplate.ini entry>  ; the template to apply`


# Naval / Water Modules

## PhysicsBehavior

Added parameters to support objects floating/impacting on water (used together with the ship death behavior and water effects).

* `DoWaterPhysics = No` - (If Yes, the object reacts to the water surface, e.g. floating and slowing down in water instead of falling through. Default = No)
* `WaterExtraFriction = 0.0` - (Additional friction applied while the object is in/on water. Default = 0.0)
* `WaterImpactFX = <FXList>` - (FXList played once when the object first impacts the water surface. Default = None)

## ShipSlowDeathBehavior (New)

A death behavior for ships and other large naval objects. It plays a multi-stage sinking sequence: an initial delay, an optional toppling motion, and finally sinking below the water surface, each with their own FX/OCL and model condition states.

The module has a lot of parameters, grouped by stage below. All durations are in milliseconds, all angles in degrees.

```
Behavior = ShipSlowDeathBehavior ModuleTag_ShipDeath
  ; --- common DieModule params (DeathTypes, VeterancyLevels, ExemptStatus, ...) ---

  ; --- Initial delay before the death sequence starts ---
  InitialDelay = 0
  InitialDelayVariance = 0
  InitialConditionFlag = <ModelCondition>   ; condition flag set when the death starts (default = RUBBLE)

  ; --- Wobble (gentle rocking while sinking) ---
  WobbleMaxAnglePitch = 0
  WobbleMaxAngleYaw = 0
  WobbleMaxAngleRoll = 0
  WobbleInterval = 0

  ; --- Topple stage (ship tips over) ---
  ToppleStyle = TOPPLE_FRONT   ; TOPPLE_FRONT, TOPPLE_BACK, TOPPLE_SIDE_LEFT, TOPPLE_SIDE_RIGHT (random if omitted)
  ToppleFrontMinPitch = 0
  ToppleFrontMaxPitch = 0
  ToppleBackMinPitch = 0
  ToppleBackMaxPitch = 0
  ToppleSideMinRoll = 0
  ToppleSideMaxRoll = 0
  ToppleMinHeightOffset = 0
  ToppleMaxHeightOffset = 0
  ToppleDuration = 0
  ToppleDurationVariance = 0
  ToppleMinPushForce = 0
  ToppleMaxPushForce = 0
  ToppleMinPushForceSide = 0
  ToppleMaxPushForceSide = 0
  ToppleAngleCorrectionRate = 0   ; angular velocity used to settle the topple angle
  ToppleConditionFlag = <ModelCondition>   ; condition flag set when toppling starts (default = RUBBLE)
  FXStartTopple = <FXList>
  OCLStartTopple = <OCL>

  ; --- Sink stage (ship goes below water) ---
  SinkHowFast = 100%       ; sink speed as a percentage
  SinkConditionFlag = <ModelCondition>   ; condition flag set when sinking starts (default = RUBBLE)
  FXStartSink = <FXList>
  OCLStartSink = <OCL>
  SinkAttachParticle = <ParticleSystem>   ; particle system attached while sinking (e.g. bubbles)
  SinkAttachParticleBones = <Bone1> <Bone2> ...   ; bones to attach the particle to
  SoundSinkLoop = <AudioEvent>            ; looping sound while sinking

  ; --- Pilot / ground impact ---
  OCLEjectPilot = <OCL>     ; OCL created to eject a pilot/crew
  FXHitGround = <FXList>    ; FX when the wreck reaches the sea/ground floor
  OCLHitGround = <OCL>      ; OCL when the wreck reaches the sea/ground floor
End
```

Notes:
* The three condition flags (`InitialConditionFlag`, `ToppleConditionFlag`, `SinkConditionFlag`) all default to `RUBBLE`. Set them to distinct states if you want different models/animations per stage.
* `ToppleStyle` accepts the topple direction names; if omitted, a random direction is chosen.

## HeightDieUpdate

Added parameter:
* `TargetHeightIncludesWater = No` - (If Yes, the target height check used to trigger the death also takes the water surface height into account, so the object dies relative to the water level rather than the terrain below it. Useful for projectiles/objects over water. Default = No)

## Die Modules - Water Depth Conditions

All Die modules (the shared `DieMux` conditions) now support restricting the death to a water depth range. This lets you trigger different death effects depending on how deep the water is (e.g. a normal death on land/shallow water, a sinking death in deep water).

* `MinWaterDepth = 0.0` - (Minimum water depth required for this death module to trigger. Default = 0)
* `MaxWaterDepth = 0.0` - (Maximum water depth allowed for this death module to trigger; 0 = no upper limit. Default = 0)

## BunkerBusterBehavior

Added parameter:
* `WorksOverWater = Yes` - (If Yes (default), the bunker buster triggers its behavior even when it hits the water surface. Set to No to make it ignore targets over water.)

# Bridges

## Bridge.ini / TerrainRoad (Bridge type)

* `BridgeHoleAreaPercentage = 100%` - (Percentage of the bridge span that is removed/becomes impassable when the bridge is destroyed. Lets you tune how much of the bridge collapses.)

## BridgeBehavior

Added parameters to push objects on a bridge when it is repaired:
* `RepairPushDuration = 0` - (Duration in ms over which objects standing on the bridge are pushed up when the bridge is repaired/restored.)
* `RepairPushForce = 0` - (Acceleration applied to objects on the bridge while it is being repaired.)

## DrawBridgeUpdate (New) / DrawBridgeTowerUpdate (New)

New modules implementing an openable "draw bridge" that can be raised and lowered, triggered via a special power.

`DrawBridgeUpdate` (on the bridge object):
```
Behavior = DrawBridgeUpdate ModuleTag_DrawBridge
  OpeningDuration = 0       ; time in ms to open (raise) the bridge
  ClosingDuration = 0       ; time in ms to close (lower) the bridge
  OpeningPushForce = 0      ; acceleration applied to objects when the bridge opens (pushes them off)
  ClosingDamageTime = 0     ; time in ms during closing when objects underneath take damage
  BridgeOpeningFX = <FXList>    ; played when the bridge starts opening
  BridgeOpenFX = <FXList>       ; played once fully open
  BridgeClosingFX = <FXList>    ; played when the bridge starts closing
  BridgeClosedFX = <FXList>     ; played once fully closed
  BridgeOpeningAudio = <AudioEvent>   ; looping audio while opening
  BridgeClosingAudio = <AudioEvent>   ; looping audio while closing
End
```

`DrawBridgeTowerUpdate` (on the bridge tower/control object):
```
Behavior = DrawBridgeTowerUpdate ModuleTag_DrawBridgeTower
  SpecialPowerTemplate = <SpecialPower entry>   ; special power that triggers opening/closing
End
```

# Jumpjet / Special Ability Modules

## SpecialAbilityUpdate

Added parameters to control turning before an ability is used:
* `RequiresMoveToTurn = No` - (If Yes, the unit must use its movement/locomotor to turn towards the target before the ability can fire, instead of snapping its facing.)
* `FacingAngleTolerance = 0` - (Allowed facing angle error (in degrees) towards the target before the ability is considered "facing" the target.)

## JumpjetContain (New)

A contain module for a "jumpjet"-style carrier that launches and lands riders which fly using jumpjet missiles. Handles the flying/landing animation states and landing safety checks.

```
Behavior = JumpjetContain ModuleTag_Jumpjet
  ;<TransportContain parameters>
  FlyingConditionFlag = <ModelCondition>    ; condition flag set while flying
  LandingConditionFlag = <ModelCondition>   ; condition flag set while landing
  LandingDistance = 0.0                     ; distance from the ground at which landing begins
  KillWhenLandingInWater = No               ; kill the unit if it tries to land in water
  KillWhenLandingInWaterSlop = 0.0          ; extra water-depth tolerance before the water kill triggers
  KillWhenLandingOnCliff = No               ; kill the unit if it tries to land on an (impassable) cliff
  KillWhenLandingOnImpassable = No          ; kill the unit if it tries to land on impassable terrain
End
```

## JumpjetMissileAIUpdate (New)

AI update for the projectiles/riders launched by a jumpjet carrier.

```
Behavior = JumpjetMissileAIUpdate ModuleTag_JumpjetMissile
  ScatterRadius = 0.0        ; random scatter radius around the target (default = 0)
  MaxSearchRadius = 100.0    ; radius to search for targets (default = 100)
  InitialPitchAngle = 0.0    ; initial launch pitch angle in degrees (default = 0)
End
```

# Animation Blending

A set of features to smoothly blend between animations and trigger frame-accurate effects. (PRs #85, #86, #91, #93)

## W3DModelDraw (module level)

* `KeepRecoilAcrossStates = No` - (If Yes, weapon recoil offset is kept when the model switches condition states, instead of being reset. Avoids the gun "snapping back" when the firing state changes. Default = No)

## ConditionState parameters

* `AnimationBlendTime = 0` - (Per ConditionState; number of animation frames to blend from the previous state's animation into this state's animation, for a smooth transition. Default = 0 = no blending)

* `AutoConditionState = <ConditionFlag1> <ConditionFlag2> ...` - (Shorthand that auto-generates a combined condition state from the **previously defined** ConditionState by OR-ing in the listed condition flags. It copies the previous state's model/animation/overrides and creates a new state for each of the previous state's condition sets combined with the auto flags. It has no body.)

  Requirements/notes:
  - Must come after a regular ConditionState (it refers to the last defined one).
  - Requires at least one condition flag.
  - It is an asset error if it would produce a condition state that already exists.
  - Useful for animation blending, e.g. to duplicate a state for an additional flag without retyping the whole block.

* `FXEvent = <frame> <boneName> <FXList>` - (Per ConditionState; plays the given FXList at the given bone when the animation playback crosses the given frame number. Can be added multiple times. Lets you trigger effects at exact animation frames, e.g. footstep dust or muzzle smoke.)

## AnimationSteeringUpdate (New)

A client update that drives steering/lean animations based on the unit's turning.

```
ClientUpdate = AnimationSteeringUpdate ModuleTag_AnimSteer
  MinAngle = 0           ; minimum turn angle (degrees) before a steering animation is used
  SkipCenteringAnims = No   ; if Yes, do not play the "return to center" animations
End
```

## W3DDecalDraw (New)

A draw module that renders a single ground decal for its object. Mainly used together with the [Decal FX nugget](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/FXList-&-ParticleSystems#decal-entries) to create temporary decals (e.g. scorch marks, footprints) via FXLists.

```
Draw = W3DDecalDraw ModuleTag_Decal
  Texture = <texture name>
  Style = SHADOW_ALPHA_DECAL   ; SHADOW_DECAL, SHADOW_ALPHA_DECAL or SHADOW_ADDITIVE_DECAL
  Color = R:255 G:255 B:255 A:255
  Opacity = 100%
  SizeX = 1.0       ; world-space size in X
  SizeY = 1.0       ; world-space size in Y
  FadeInTime = 0    ; ms to fade in
  FadeOutTime = 0   ; ms to fade out (use with LifetimeUpdate)
  RenderAboveWater = No   ; Yes draws the decal on the water surface instead of the terrain below
End
```
Note: `SHADOW_DECAL` is no longer a valid Style for this module. Use `SHADOW_ALPHA_DECAL` or
`SHADOW_ADDITIVE_DECAL`.

# Target Designator

## SpecialPowerDesignatorUpdate (New)

Update module that lets a unit "designate" an area, enabling special powers that require a designator (see [`NeedsTargetDesignator`](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/SpecialPowers#new-parser-fields) on the SpecialPower). While active, it can show a radius decal and optionally apply a temporary status to objects in the area.

```
Behavior = SpecialPowerDesignatorUpdate ModuleTag_Designator
  SpecialPowerTemplate = <SpecialPower entry>   ; the power that this designator enables
  DesignatorRadius = 0.0       ; radius of the designated area
  AlwaysShowDecal = No         ; always show the radius decal, even when the power is not ready
  TriggerStatusType = <ObjectStatus>   ; status applied to objects in the designated area
  TriggerStatusTime = 0        ; how long (ms) the status stays on after leaving the area
  TriggerFX = <FXList>         ; FX played when the designator triggers
End
```

# Crate / Salvage Upgrades

## AutoHealBehavior

Added parameters so that auto-healing can also grant salvage upgrades / promotions (e.g. a repair bay that "salvages" or veterans up units while healing them):
* `GrantSalvageUpgrade = No` - (If Yes, apply a salvage crate upgrade to the unit while it is being healed.)
* `GrantPromotion = No` - (If Yes, give the unit a veterancy level-up while it is being healed.)

## CrateApplyUpgrade (New)

An upgrade module that applies "crate"-style effects (salvage weapon/armor upgrades or a veterancy level-up) when triggered, reusing the salvage-crate logic.
* `SalvageCrate = No` - (If Yes, apply a salvage weapon/armor upgrade. Only affects units that are `KINDOF_WEAPON_SALVAGER` / `KINDOF_ARMOR_SALVAGER` and not already at the highest salvage tier.)
* `LevelUp = No` - (If Yes, grant a veterancy level-up, up to HEROIC, if the unit is trainable.)

## Crate.ini (CrateData)

* `AllowWater = No` - (If Yes, this salvage/pickup crate may be created on water. By default crates are only created on land. Default = No)

# Multiple Portable Structures (Overlord contain)

`OverlordContain` (and Overlord-style contain redirection) now allows holding multiple add-ons at once, as long as they are `KINDOF_PORTABLE_STRUCTURE`. Objects with this KindOf are always added directly to the Overlord's own contain list (rather than being redirected to a single passenger), so several portable structures can be mounted on the same carrier. See also [MultiAddOnContain](#multiaddoncontain-new).

# Water Genpower Support

Improvements to make existing generals-power superweapons work correctly over water. (PR #97)

## SpectreGunshipUpdate

* `HitWaterSurface = No` - (If Yes, the gunship's strafing shots detonate at the water surface instead of passing through / hitting the sea floor.)
* `GattlingStrafeFXParticleSystemWater = <ParticleSystem>` - (Particle system used for strafe impacts that land on water, instead of the regular land FX.)

## ParticleUplinkCannonUpdate

* `HitWaterSurface = No` - (If Yes, the uplink cannon beam detonates at the water surface over water.)

See also [OrbitalBeamUpdate `HitWaterSurface`](#orbitalbeamupdate-new) and the [DeliverPayload `StrafingWeaponTargetsWater`](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Object-Creation-List#deliverpayload) flag.

See also [ParticleUplinkCannonUpdate `TornadoObjectName`](#particleuplinkcannonupdate-tornado).

# Scorch Mark Selection

## ParticleUplinkCannonUpdate

The particle beam used to burn a random scorch decal (`SCORCH_1` to `SCORCH_4`) every time it marked the ground, which
a mod cannot change. Mods that replace the scorch art with crater-like decals end up with craters punched along the
beam path. This key picks which decals the beam is allowed to use, or turns them off entirely.

* `ScorchType = SCORCH_1 SCORCH_2 SCORCH_3 SCORCH_4` - (The scorch types the beam may burn. One name always burns that
type, several pick randomly among just those, and `NONE` burns no scorches at all. Accepts `SCORCH_1`, `SCORCH_2`,
`SCORCH_3`, `SCORCH_4`, `SHADOW_SCORCH` and `NONE`.)

Example - a beam that only leaves the fourth scorch decal:

```
Behavior = ParticleUplinkCannonUpdate ModuleTag_12
  TotalScorchMarks = 20
  ScorchMarkScalar = 1.0
  ScorchType       = SCORCH_4   ; New
End
```

Omitting `ScorchType` reproduces the original random `SCORCH_1` to `SCORCH_4` behaviour exactly, so data that does not
mention it is unaffected.

Behaviour notes:
* `NONE` only suppresses the decal. The beam still counts its scorch marks, still fires its `GroundHitFX` on the same
rhythm and still reveals the shroud, so the timing of everything else is untouched.
* `SHADOW_SCORCH` was never reachable before, because the old random pick stopped at `SCORCH_4`. It is only used if you
name it.

# Veterancy

## Veterancy Object Parameters

With the two extra veterancy levels (`LEVEL_FOUR`, `LEVEL_FIVE`; see [New Enum Definitions](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/New-Enum-Definitions#veterancy-levels)), the per-object veterancy parameters now support up to 6 levels:

* `MaxVeterancyLevel = HEROIC` - (The highest veterancy level this object may ever reach. Default = `HEROIC` (vanilla behavior). Set to `LEVEL_FOUR` / `LEVEL_FIVE` to allow the extra levels.)
* `ExperienceRequired = <lvl0> <lvl1> ... <lvl5>` - (XP required to reach each level. Accepts up to 6 values now.)
* `ExperienceValue = <lvl0> <lvl1> ... <lvl5>` - (XP granted to the killer when this object is destroyed, per level. Up to 6 values.)
* `SkillPointValue = <lvl0> <lvl1> ... <lvl5>` - (Skill points granted per level. Up to 6 values.)

Notes:
* Health bonuses for the new levels are set in GameData via `HealthBonus_Four` / `HealthBonus_Five`.
* The maximum level can also be raised at runtime via the [`SetMaxVeterancyLevel`](#experiencescalarupgrade) upgrade.

## RiderChangeContain

Extended from 8 to 16 rider slots. Additional slots `Rider9` through `Rider16` can now be defined, using the same syntax as `Rider1`-`Rider8`. Corresponding `RIDER9`-`RIDER16` enums were added for ArmorSet/WeaponSet/ModelCondition/ObjectStatus (see [New Enum Definitions](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/New-Enum-Definitions#rider-slots-combat-bike)).

# On-Kill Modules

Two new behavior modules that trigger an effect when the object **kills** another object. Both share a common set of "kill condition" parameters (`KillMux`) that filter which kills count.

## Shared Kill Conditions (KillMux)

These parameters are available on both On-Kill modules below:

* `KilledKindOf = <KindOf list>` - (Only trigger when the killed victim matches these KindOfs.)
* `ForbiddenKilledKindOf = <KindOf list>` - (Do not trigger if the victim matches any of these KindOfs.)
* `RequiresAllKindOfs = No` - (If Yes, the victim must match all `KilledKindOf` entries instead of any.)
* `RequiredKilledStatus = <ObjectStatus list>` - (Victim must have these status flags.)
* `ForbiddenKilledStatus = <ObjectStatus list>` - (Victim must not have these status flags.)
* `KilledRelationship = <ALLIES | ENEMIES | NEUTRALS>` - (Only count kills of victims with this relationship to the killer.)
* `DamageTypes = <DamageType flags>` - (Only trigger if the killing blow used one of these damage types. NONE/ALL +X -Y notation.)
* `DeathTypes = <DeathType flags>` - (Only trigger for these death types.)
* `TriggerChance = 100%` - (Chance to trigger on a qualifying kill.)
* `CooldownTime = 0` - (Minimum time (ms) between triggers.)

## CreateObjectOnKillBehavior (New)

Spawns an OCL when the object kills a qualifying victim.
```
Behavior = CreateObjectOnKillBehavior ModuleTag_CreateOnKill
  StartsActive = Yes
  ; --- shared KillMux conditions (see above) ---
  KilledKindOf = INFANTRY
  ; --- module params ---
  CreationList = <OCL entry>          ; OCL spawned at the victim's location
  CreateAtKillerLocation = No         ; if Yes, spawn at the killer's position instead of the victim's
  CreateObjectForVictim = No          ; if Yes, the created object is owned by the victim's player instead of the killer's
End
```

## FireWeaponOnKillBehavior (New)

Fires a weapon at the victim when the object kills a qualifying target.
```
Behavior = FireWeaponOnKillBehavior ModuleTag_FireOnKill
  StartsActive = Yes
  ; --- shared KillMux conditions (see above) ---
  KilledKindOf = VEHICLE
  ; --- module params ---
  KillWeapon = <Weapon entry>         ; weapon fired at the victim on kill
End
```

# Superweapon / Special Power Update Modules

## OrbitalBeamUpdate (New)

A special power update module for an "orbital beam" style superweapon that charges up several beams and then fires a final beam/weapon. (PR #99)

```
Behavior = OrbitalBeamUpdate ModuleTag_OrbitalBeam
  SpecialPowerTemplate = <SpecialPower entry>   ; (via SpecialPowerModule params)
  InitialDelay = 0             ; ms before the sequence starts
  InitialSound = <AudioEvent>
  InitialFX = <FXList>         ; played when the sequence starts
  InitialOCL = <OCL entry>     ; created when the sequence starts

  ; --- Charge beams (multiple beams that converge before the final shot) ---
  NumChargeBeams = 1
  ChargeBeamName = <LaserName>            ; laser template used for charge beams
  ChargeBeamRadius = 0.0                  ; radius the charge beams spread over
  ChargeBeamHeight = 0.0
  ChargeBeamRotation = 0                  ; rotation angle (degrees)
  ChargeBeamsStartCenter = No             ; start beams from the center
  ChargeBeamInterpolation = <InterpolationType>   ; how the beams move in
  RandomizeChargeBeamOrder = No
  ChargeBeamStartFX = <FXList>
  ChargeStartFX = <FXList>                ; played once as charging begins
  ChargeStartOCL = <OCL entry>            ; created once as charging begins
  ChargeSound = <AudioEvent>
  ChargeWeapon = <Weapon entry>           ; weapon(s) fired per charge beam (can repeat)
  ChargeBeamParticleSystem = <ParticleSystem> ...       ; particle systems for the charge beam
  ChargeBeamParticleSystemLand  = <ParticleSystem> ...  ; used over land
  ChargeBeamParticleSystemWater = <ParticleSystem> ...  ; used over water
  DelayBetweenChargeBeams = 0
  DelayChargeToAnim = 0
  BeamAnimationDuration = 0
  DelayAnimationToFinal = 0

  ; --- Final beam / weapon ---
  FinalBeamName = <LaserName>
  FinalBeamDuration = 0
  FinalWeapon = <Weapon entry>
  FinalFX = <FXList>
  FinalOCL = <OCL entry>        ; created when the final beam fires

  AreaDecalRadius = 0.0         ; radius of the area decal drawn at the target
  HitWaterSurface = No          ; if Yes, beam/effects detonate at the water surface over water
End
```
Note: many fields have a matching `...Frames` internal name; all `Delay`/`Duration` values are in milliseconds. Refer to the field list for the full set.

## ChronoSphereUpdate (New)

Special power update module implementing a Chronosphere-style teleport superweapon: teleports units from a source area to a target area after a delay. (PR #100)

```
Behavior = ChronoSphereUpdate ModuleTag_Chrono
  SpecialPowerTemplate = <SpecialPower entry>
  Radius = 0.0                 ; radius of the source/target area
  RequiredKindOf = <KindOf list>     ; only these units are teleported
  ForbiddenKindOf = <KindOf list>    ; these units are never teleported
  TeleportDelay = 0            ; ms between activation and the teleport happening
  SourceFX = <FXList>          ; FX at the source area
  SourceOCL = <OCL>            ; OCL at the source area
  TargetFX = <FXList>          ; FX at the target area
  TargetOCL = <OCL>            ; OCL at the target area
  UnitSourceFX = <FXList>      ; FX on each teleported unit at the source
  UnitTargetFX = <FXList>      ; FX on each teleported unit at the target
End
```

## TeleportSelfSpecialPower (New)

Special power update module that teleports its own object to a clicked ground position. This is the activated-ability counterpart to [TeleporterAIUpdate](#teleporteraiupdate-new---experimental), which instead replaces all normal movement with teleporting; a unit with this module walks normally the rest of the time.

Pair it with a `SpecialPowerModule` (or `SpecialAbility`) that sets `UpdateModuleStartsAttack = Yes`, otherwise the power triggers itself and a rejected click still consumes the recharge. The `SpecialPower` entry needs `BehaviorEnum = SPECIAL_JUMPJET`, which supplies the position-only validation (no water, no cliffs).

```
Behavior = TeleportSelfSpecialPower ModuleTag_Teleport
  SpecialPowerTemplate = <SpecialPower entry>
  MaxTeleportRange = 0.0       ; furthest the object may teleport, 0 = unlimited
  TeleportDelay = 0            ; ms between activation and the teleport happening
  TeleportStartFX = <FXList>   ; FX at the position we left
  TeleportTargetFX = <FXList>  ; FX at the position we arrived at

  ; Recovery is off entirely unless RecoverDuration is set
  RecoverDuration = 0          ; ms the unit is immobilized after landing
  TeleportRecoverEndFX = <FXList>             ; FX when the recovery finishes
  TeleportRecoverSoundAmbient = <AudioEvent>  ; looped while recovering
  TeleportRecoverTint = <TintStatus>          ; color tint applied while recovering
  TeleportRecoverCondition = <ModelCondition> ; e.g. TELEPORT_RECOVER
  TeleportRecoverOpacityStart = 100%          ; opacity when the recovery starts
  TeleportRecoverOpacityEnd = 100%            ; opacity when the recovery ends
End
```

Notes:
* The destination is validated when the order is given and again when it fires, so a spot that becomes blocked during `TeleportDelay` cancels the teleport rather than stranding the unit.
* An unaffordable `Cost`, a dead/garrisoned/disabled caster, or an invalid destination all refuse the order without consuming the recharge.
* `RecoverDuration` uses `DISABLED_TELEPORT_RECOVER`, which genuinely immobilizes the unit. Because any disable pauses special power countdowns, the effective cooldown becomes `ReloadTime` + `RecoverDuration`.
* Do **not** set `KINDOF_TELEPORTER` on a unit using this module - that KindOf excludes units from group speed, leader selection and column formations, which would break normal squad movement.

## MultiLocationSpecialPowerUpdate (New)

Special power update that can create objects/OCLs at multiple resolved map locations (e.g. edges relative to the source/target). Used together with the [multi-target CommandButton](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/SpecialPowers#multi-target-special-powers) fields.

```
Behavior = MultiLocationSpecialPowerUpdate ModuleTag_MultiLoc
  SpecialPowerTemplate = <SpecialPower entry>
  CreateLocation = CREATE_AT_EDGE_NEAR_SOURCE   ; where the OCL/object is created
  ; valid values:
  ;   CREATE_AT_EDGE_NEAR_SOURCE (default)
  ;   CREATE_AT_EDGE_NEAR_TARGET
  ;   CREATE_AT_EDGE_FARTHEST_FROM_TARGET
  ;   CREATE_AT_LOCATION
  InitialDelay = 0        ; ms before the first creation
  Delay = 0               ; ms between creations at each location
  OCL = <OCL entry>       ; default OCL created at each location
  UpgradeOCL = <Upgrade> <OCL>   ; optional per-upgrade OCL override (can repeat)
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

## PropagateUpgradeToContainedUpgrade (New)

Upgrade module that grants upgrades to everything currently contained inside the object when it
triggers. Useful for a transport passing a bought upgrade down to its riders.

```
Behavior = PropagateUpgradeToContainedUpgrade ModuleTag_Propagate
  TriggeredBy = <Upgrade entry>
  UpgradeToPropagate = <Upgrade entry>   ; granted to each contained object (can repeat)
End
```

## GlobalLightingModifierUpdate (New)

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

## TornadoUpdate (New)

Applies a tornado effect around the object: nearby units are dragged toward it, lifted, spun, and
damaged, and they fall when the effect fades. The strength follows a weak - strong - weak envelope,
so the tornado builds up, holds, and dies away like the particle uplink cannon beam does.

The module never moves its own object, so a tornado that wanders needs a `Locomotor` and an AI
module on the object like any other unit. A stationary one can be spawned by a weapon through
`ProjectileDetonationOCL` or `FireOCL` with a `CreateObject` nugget.

```
Behavior = TornadoUpdate ModuleTag_Tornado
  Radius = 120                    ; (required; how far out units are grabbed)
  RingRadius = 12                 ; (distance from the axis victims orbit at; 0 = a tenth of Radius)
  PullForce = 0.6                 ; (inward speed toward the centre, in distance per frame)
  LiftForce = 1.5                 ; (climb speed toward MaxLiftHeight, in height per frame)
  SpinForce = 6                   ; (orbit speed around the centre; a negative value orbits the other way)
  YawRate = 360                   ; (how fast a victim spins about its own axis, in degrees per second)
  MaxLiftHeight = 40              ; (above this height over the tornado ground, lift stops, so victims hover)
  MaxVictimSpeed = 12             ; (speed cap on victims; 0 = uncapped, which lets them spiral away)
  MassReference = 100             ; (victims heavier than this spin proportionally slower; 0 = no scaling)
  ReleaseSpeed = 0                ; (horizontal speed kept when released; 0 drops them straight down)
  RequiredKindOf = VEHICLE INFANTRY   ; (optional; a victim must be at least one of these)
  ForbiddenKindOf = AIRCRAFT      ; (optional; a victim must be none of these)
  AffectsTargets = ENEMIES NEUTRALS   ; (default = ALLIES ENEMIES NEUTRALS)
  AffectAirborne = No             ; (default = No)
  IgnoreVictimGeometry = No       ; (default = No; Yes stops held victims shoving each other apart)
  RampUpTime = 2000               ; (ms to reach full strength)
  FullStrengthTime = 6000         ; (ms at full strength; 0 = until the object dies or a controller ends it)
  RampDownTime = 2000             ; (ms to fade to nothing; victims fall once it reaches zero)
  DamagePerSecond = 20            ; (damage rate at full strength)
  DamageRadius = 0                ; (default = 0, which uses Radius)
  DamagePulseDelay = 500          ; (ms between damage pulses; 0 disables damage)
  DamageType = EXPLOSION          ; (default = EXPLOSION)
  DeathType = EXPLODED            ; (default = EXPLODED)
  KillObjectWhenDone = No         ; (default = No; Yes destroys the object once the ramp down finishes)
End
```
**Notes:**
- A victim needs a `PhysicsBehavior`. Lift, pull and spin all ignore `Mass`, so every held unit
  rides at the same height and speed. The one per unit resistance is `ShockResistance`, which
  scales the whole effect down and doubles as an immunity dial.
- Structures, immobile objects and projectiles take damage but are never pulled. Units inside a
  transport are untouched; only the transport itself is grabbed.
- Airborne targets are skipped unless `AffectAirborne = Yes`. Aircraft with a fixed flight height
  overwrite their own height every frame, so lifting them does not work well.
- Falling damage on release is the victim's own, from `MinFallHeightForDamage` and
  `FallHeightDamageFactor` on its `PhysicsBehavior`. It only counts a steep descent, which is why
  `ReleaseSpeed = 0` is the reliable way to splat units.
- `IgnoreVictimGeometry` suppresses collision push-apart between held victims. Because they are
  all steered onto the same ring they overlap constantly, and wide units can be shoved around
  faster than the orbit settles, which reads as juddering. It is restored on release.
- The module does not end the object. Pair it with a `LifetimeUpdate`, or the OCL `MinLifetime` and
  `MaxLifetime` fields, or set `KillObjectWhenDone`. A tornado with `FullStrengthTime = 0` and none
  of these, and no controller such as the cannon, fades out by itself after 30 seconds.

## ParticleUplinkCannonUpdate (Tornado)

* `TornadoObjectName = <object>` - (Creates this object at the beam ground point when the orbital
  beam appears, drags it along with the beam, ramps it down when the beam starts decaying, and
  destroys it when the beam dies or the cannon is sold or destroyed.)

The named object is expected to carry a [TornadoUpdate](#tornadoupdate-new). Give it
`FullStrengthTime = 0` and a `RampDownTime` matching the cannon `WidthGrowTime`, so that the tornado
follows the beam for as long as it fires and then fades out with it.

# Misc Improvements

## ObjectExtend
Allows to use inheritence in the parser. `ObjectExtend` is similar to `ObjectReskin` but inherit all modules and allow modifications. The parent object must be defined before the ObjectExtend.  

Syntax: `ObjectExtend NewObject ObjectToInheritFrom`  

Use `RemoveModule ModuleTag_xx` to remove a module inherited from the parent.  
WeaponSet and ArmorSet are inherited, but if you define another one in the child, the inherited sets are removed.  
If the parent has multiple WeaponSets all will be removed from the child if the child defines one. 

Examples:  
```
ObjectExtend ZExtendedTank Chem_GLAVehicleRadarVan  
  ; *** ART Parameters ***  
  SelectPortrait = SNHacker2_L ;define other icons  
  ButtonImage = SNHacker2  
    
  RemoveModule ModuleTag_01 ; Remove the parent's draw  
  
  Draw = W3DTankDraw ModuleTag_1337 ; add a new draw  
    OkToChangeModelColor = Yes  
    DefaultConditionState  
      Model = NVInferno  
      Turret = Turret  
      TurretPitch = TurretEL01  
      WeaponFireFXBone = PRIMARY Muzzle  
      WeaponRecoilBone = PRIMARY Barrel  
      WeaponLaunchBone = PRIMARY Muzzle  
      HideSubObject = PARTSUP TurretEl02  
      ShowSubObject = TurretEL01 TURRETPARTS  
    End  
    ConditionState = RUBBLE REALLYDAMAGED  
      Model = NVInferno_D  
    End  
    TrackMarks = EXTnkTrack.tga  
    TreadAnimationRate = 2.0  
  End  
  Draw = W3DModelDraw ModuleTag_0123 ; add a second draw (tree on top)  
    DefaultConditionState  
      Model = PTOak01  
    End  
  End  
  Prerequisites ; change the prerequisites  
    Object = Tank_ChinaCommandCenter TEST_MODE  
  End  
  WeaponSet ; define new weaponset, all parent inherited weapons are cleared  
    Conditions = None  
    Weapon = PRIMARY Tank_InfernoCannonGun  
  End  
  RemoveModule ModuleTag_Stealth ; Remove the stealth update inherited from the parent  
End  
ObjectExtend ZExtendedTank2 ZExtendedTank  ; inherit again
  RemoveModule ModuleTag_0123  ; remove the tree draw
  WeaponSet  ; replaces all weaponset
    Conditions = None  
    Weapon = PRIMARY MarauderTankGun  
  End  
  ArmorSet  ; replaces all armor sets
    Conditions = None  
    Armor = TruckArmorExtended  
    DamageFX = TankDamageFX  
  End  
End  
```
