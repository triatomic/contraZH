# Extend Blocks

An Extend block defines a new entry that starts as a copy of an existing one. Only the
differences go in the block.

```
<Type>Extend <NewName> <ParentName>
  ; keys or nuggets that differ from the parent
End
```

## Rules for every Extend

* The parent must be loaded first: above the Extend in the same file, or in a file the game
loads earlier.
* The copy is taken at load. Editing the parent's block changes every entry that extends it.
* An extended entry can itself be a parent, so chains work.
* An Extend with an empty body is an exact copy under a new name.

| Block | File | What the child changes |
|---|---|---|
| `ArmorExtend` | Armor.ini | Damage type values, set or multiplied |
| `LocomotorExtend` | Locomotor.ini | Any Locomotor key |
| `WeaponExtend` | Weapon.ini | Any Weapon key |
| `ObjectExtend` | Object INIs | Keys, modules, WeaponSet, ArmorSet |
| `ObjectCreationListExtend` | ObjectCreationList.ini | Adds nuggets after the parent's |
| `FXListExtend` | FXList.ini | Adds nuggets after the parent's |
| `ParticleSystemExtend` | ParticleSystem.ini | Any ParticleSystem key |

## Armor.ini

* `ArmorExtend <NewArmor> <ParentArmor>` - (Copies every damage coefficient of the parent.)
* `Armor = <DamageType> <Percent>` - (Sets one damage type.)
* `ArmorMult = <DamageType> <Percent>` - (Multiplies the inherited value, e.g. `50%` halves it.)

```
ArmorExtend TruckArmorNukeUpgraded TruckArmor
  Armor = RADIATION 0%
  Armor = POISON    37%
End
```

Notes:
* `Armor = DEFAULT` resets every damage type, wiping the inherited values. List it first if the
child needs a new default, then the types that differ from it.
* A missing parent is an INI error.
* Contra's Armor.ini names the parent in the header instead of the old `;Base armor is` comment.

## Locomotor.ini

* `LocomotorExtend <NewLocomotor> <ParentLocomotor>` - (Copies every Locomotor key.)

```
LocomotorExtend CrusaderFastLocomotor CrusaderLocomotor
  Speed        = 40
  SpeedDamaged = 30
End
```

Notes:
* A missing parent asserts in debug builds. Release builds skip the block, so the new locomotor
does not exist.

## Weapon.ini

* `WeaponExtend <NewWeapon> <ParentWeapon>` - (Copies every Weapon key.)

```
WeaponExtend Chem_HindRocketPodWeapon HindRocketPodWeapon
  ProjectileDetonationOCL = Chem_OCL_PoisonFieldSmall
End
```

Notes:
* A missing parent asserts in debug builds. Release builds skip the block, so the new weapon does
not exist.

## Object INIs

* `ObjectExtend <NewObject> <ParentObject>` - (Copies keys and modules.)
* `RemoveModule <ModuleTag>` - (Drops one inherited module.)
* A `WeaponSet` or `ArmorSet` in the child replaces all inherited sets of that kind.

```
ObjectExtend ZExtendedTank Chem_GLAVehicleRadarVan
  ButtonImage = SNHacker2
  RemoveModule ModuleTag_Stealth
  WeaponSet
    Conditions = None
    Weapon     = PRIMARY Tank_InfernoCannonGun
  End
End
```

Notes:
* Full reference with module replacement: [Objects & Modules](Objects-&-Modules.md#objectextend).

## ObjectCreationList.ini

* `ObjectCreationListExtend <NewList> <ParentList>` - (Runs every nugget of the parent, then the
nuggets in its own block.)

```
ObjectCreationListExtend OCL_CHINAInfPowerPlantExplode OCL_ChPt1_Base
  CreateDebris
    ModelNames  = ChPt88
    Offset      = X:23 Y:8 Z:70
    Mass        = 10.0
    Count       = 1
    Disposition = SEND_IT_FLYING
  End
End
```

Notes:
* Nuggets only add. An inherited nugget cannot be changed or removed; to drop one, write the list
as a plain `ObjectCreationList`.
* Naming the list itself as parent appends nuggets to it.
* A missing parent asserts in debug builds, and the block loads without it.
* Inherited nuggets create their objects first. The first object created is the one modules like
`CreateObjectDie` and `OCLSpecialPower` act on, so keep a list's key object in its own block or
at the start of its parent.

## FXList.ini

* `FXListExtend <NewList> <ParentList>` - (Plays every nugget of the parent, then the nuggets in
its own block.)

```
FXListExtend FX_AmericaWarFactoryDeathFinal FX_StructureExplosionSmoke_Base
  ViewShake
    Type = STRONG
  End
  Sound
    Name = BuildingDie
  End
End
```

Notes:
* Nuggets only add. An inherited nugget cannot be changed or removed.
* Naming the list itself as parent appends nuggets to it.
* A missing parent asserts in debug builds, and the block loads without it.
* Nuggets stay in memory until the game exits, as `ObjectCreationList` nuggets always have.
Redefining an `FXList`, for example in a map.ini, keeps the old nuggets, so each map load that
redefines lists adds a little memory.

## ParticleSystem.ini

* `ParticleSystemExtend <NewSystem> <ParentSystem>` - (Copies every key of the parent. Keys in the
block override the copied values.)

```
ParticleSystemExtend TEXplosive23PurpleBig TEXplosive23RedBig
  Color1 = R:160 G:60 B:220 0
  Color2 = R:90 G:30 B:140 25
End
```

Notes:
* A key left out takes the parent's value, not the default. Set it explicitly to get the default
back.
* Keep `Type`, `VelocityType` and `VolumeType` the same as the parent's, or restate every key of
the new type. The velocity and volume keys of different types share storage.
* A missing parent asserts in debug builds, and the block loads without it.
* Saving from the in-game particle editor writes every system back in full and drops the Extend
form.

## Base lists in Contra's INIs

Lists that only exist to be extended end in `_Base`, e.g. `OCL_GXMammoth_D01_Base` or
`FX_StructureExplosionSmoke_Base`. Each holds a nugget set that several lists share, such as a
building's debris pieces.

* Editing a `_Base` list changes every list built on it.
* To give one list a different version of a shared nugget, move that list off the base and write
its nuggets out in full.
