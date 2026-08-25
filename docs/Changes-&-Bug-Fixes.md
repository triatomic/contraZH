# Introduction

This page describes changes made to the default behavior of the code. This will be mostly bug fixes.
We try to keep changes to a minimum. This list contains all changes that will affect the default behavior. 

# Limits
## Increased Upgrade Limit to 1024

# Code Changes

## Minor Laser visual improvements
### Module: LaserDraw
In vanilla ZH, a laser beam always moves along with the shooter's firebone, but the Muzzle particle system will stay in place.
This has been changed. The muzzle particle system will now always move with the shooter's firebone along with the laser beam.

## Fixed HelixContain passenger fireports not working

### Module: HelixContainUpdate

Transports with HelixContain and fireports, now have their passenger correctly fire from the Firepoint bones

Note: This requires the transport to have the FIREPOINTXX bones. It is not enough to put them on a portable Bunker object.

## Changed default behavior for Airborne collissions in PhysicsUpdate 

To avoid helicopters randomly dying via a chain reaction, we changed the conditions when "VehicleCrashesIntoNonBuildingWeaponTemplate" is fired. This wepaon should be triggered when an object (i.e. a dying helicopter) crashes into a ground vehicle, but it often triggers already mid air if the object collides with airborne projectiles, lasers or dummy objects. We changed the default behavior to ignore airborne collissions.

### Module: PhysicsUpdate

Changed conditions for _VehicleCrashesIntoNonBuildingWeaponTemplate_ to be fired on non-airborne collissions only. Added new parameter to restore default behavior

- `VehicleCrashWeaponAllowAirborne = No` - (restores default behavior to also trigger VehicleCrashesIntoNonBuildingWeaponTemplate on airborne collissions. Not Recommended)

## VTOL Improvements
(Module: JetAIUpdate)

VTOL Aircraft = Aircraft which needs parking space in airfields but with NeedsRunway = No in JetAIUpdate.

Various fixes and improvements:

* Add support for Jets (WINGS locomotor) to land/take off without runways. Previously only aircraft with HOVER locomotor could park/land. This requires a HOVER locomotor in locomotor set SET_VTOL.
* Slightly changed landing and takeoff behavior of all VTOL aircraft. Vertical take off now takes slightly longer.
* Fixed wrong parking orientation for VTOL aircraft. VTOL aircraft were turning the wrong way when reloading ammo in their parking spot. This is fixed.

## Fixed Spectre Gunship getting selected for every player

Fixed issue in SpectreGunshipDeploymentUpdate causing the gunship to get selected for every player.
The gunship is now selected for the controlling player only.

## Fixed Bunker Buster clearing out Tunnels while under construction

When a bunker buster bomb hits a garrisoned structure that is under construction (this can only really happen for tunnels), the contained units will no longer be thrown out.

## Fixed USA Chemsuit Upgrade crashing when a unit starts as veteran

Units with Chemsuit upgrade now correctly show the decal and when starting with veterancy. 

## Fixed Decal Shadows being applied multiple times

Fixed issue for units with texture based shadows (SHADOW_DECAL, SHADOW_ALPHA_DECAL or SHADOW_ADDITIVE_DECAL) where the shadow decal is applied multiple times if the unit has multiple draw modules. Now only 1 instance of the shadow is applied.

## Fixed VeterancyCrateCollide module not working for regular pick up crates

VeterancyCrateCollide only worked for pilots. Now it can be used for regular crates.

## Fixed PoisonedBehavior not using POISON damage typer
In order to prevent an infinite self-poisoning loop, the damage being dealt was set to DAMAGE_UNRESISTABLE.
This is now fixed and POISON damage is applied.

## Fixed Transitionstates anims not working reliably on Night/Snow maps
Fixed an issue where TransitionStates are incorrectly initialized of Night or Snow states are not explicitly defined for a Draw module. This caused Turrets or firebones sometimes not working during Transition animations. 

## Fixed FXLists being wrongly aligned
By default, FXList without "OrientToObject = Yes" will not have their offset aligned to the parent object. This prevents the issue of large effects like Nuke mushroom clouds to be "drifting away". The original behavior can be restored by using "OrientOffset = Yes".

## Fixed shared weapon reload
Fixed an issue with `WeaponReloadSharedAcrossSets` (see [Objects & Modules](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Objects-&-Modules#weaponset)) where reload time / ammo count was not shared correctly when switching between weapon sets.

## Fixed looping firing sound issue
### Module: FiringTracker
Fixed an issue where a weapon's looping firing sound could keep playing after the unit stopped firing.

## Fixed LocomotorWorksWhenDisabled
### Module: AIUpdate
Fixed and added safeguards for `LocomotorWorksWhenDisabled`, so disabled units with this flag move correctly across the various AIUpdate types.

## Veterancy weapon FX for scatter/secondary projectiles
Projectiles (`DumbProjectileBehavior`, `FreeFallProjectileBehavior`, `MissileAIUpdate`, `NeutronMissileUpdate`) now snapshot the **launcher's** veterancy level and use it to pick the per-veterancy weapon FX (`VeterancyProjectileExhaust`, `VeterancyFireFX`, `VeterancyProjectileDetonationFX`, `VeterancyFireOCL`, ...). This makes veterancy-based exhaust/FX work correctly for ScatterShot and other secondary weapon effects.
