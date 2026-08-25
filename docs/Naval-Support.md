# Overview

A number of changes were added for improved naval / water-based unit support. The relevant parameters live across several modules and INI files; this page collects the water-related features and links to the detailed documentation.

## Ships and Naval Death

* **[ShipSlowDeathBehavior](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Objects-&-Modules#shipslowdeathbehavior-new)** - multi-stage sinking death (delay → topple → sink) for ships, with FX/OCL and condition states per stage.
* **[PhysicsBehavior water parameters](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Objects-&-Modules#physicsbehavior)** - `DoWaterPhysics`, `WaterExtraFriction`, `WaterImpactFX` for objects reacting to the water surface.
* **[Die water depth conditions](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Objects-&-Modules#die-modules---water-depth-conditions)** - `MinWaterDepth` / `MaxWaterDepth` to trigger different death effects depending on water depth.
* **[HeightDieUpdate `TargetHeightIncludesWater`](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Objects-&-Modules#heightdieupdate)** - die relative to the water surface.
* **[Water Impact OCL nugget](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Object-Creation-List#water-impact-createobject)** - `WaterImpactFX` / `WaterImpactSound` for debris hitting water.

## Water Effects

* **[FXList height & surface restrictions](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/FXList-&-ParticleSystems#height-and-surface-restrictions)** - `AllowedSurface = WATER/LAND/ALL` and `Min/MaxAllowedHeight` for ParticleSystem and Sound nuggets, so effects can be restricted to water or land.
* **[Water depth terrain lighting](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/GameData#water-depth-terrain-lighting)** - darken/tint terrain based on depth below the water surface.
* **[`HideScorchmarksAboveGround`](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/GameData#scorchmarks-on-water)** - avoid scorch decals floating on the water surface.

## Naval Weapons

* **[DumbProjectileBehavior `DynamicHeightMinScale`/`DynamicHeightMinRange`](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Objects-&-Modules#dumbprojectilebehavior)** - scale projectile arc height by distance, useful for torpedoes and arcing naval guns.
* Torpedo / `MissileAIUpdate` behavior was improved internally (no new parameters) to travel correctly towards naval targets.

## Salvage / Crates on Water

* **[Crate `AllowWater`](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Objects-&-Modules#crateini-cratedata)** - allow salvage/pickup crates to be created on water.

## Carriers

* **[Drone Carrier system](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Objects-&-Modules#drone-carrier-system)** - modules for a simplified mobile aircraft carrier (`DroneCarrierContain`, `DroneCarrierAIUpdate`, `CarrierDroneAIUpdate`, ...).
