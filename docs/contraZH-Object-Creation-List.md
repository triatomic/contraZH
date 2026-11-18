# ObjectCreationList.ini

## CreateObject UseTransportOccupants

`UseTransportOccupants = Yes` transfers every passenger from the source object's contain module
into its transport's configured airborne exit container. The default is `No`.

* `TransportContain` supplies `ExitInAirTemplate` and `ExitInAirVehicleTemplate` for each passenger.
* Each passenger keeps its object identity, owner, health, veterancy, upgrades, and experience.
* `ObjectNames`, `Count`, and `ObjectCount` do not select or limit passengers in this mode.
* Missing airborne exit templates and incompatible containers leave passengers aboard.
* `PutInContainer` applies to newly created objects. It is ignored in this passenger transfer mode.
* The container receives the usual OCL position, offset, disposition, force, and lifetime settings.
* `IgnorePrimaryObstacle` and `InvulnerableTime` also apply to each passenger.
* `RequiresLivePlayer` checks the source player's state before releasing passengers.
* Use `RequiresTransportOccupants = Yes` on the firing weapon to disable it while empty.

```
ObjectCreationList OCL_DropTransportOccupants
  CreateObject
    UseTransportOccupants = Yes
    IgnorePrimaryObstacle = Yes
    Disposition = RANDOM_FORCE
    MinForceMagnitude = 10
    MaxForceMagnitude = 12
    MinForcePitch = 50
    MaxForcePitch = 60
    InvulnerableTime = 2000
    RequiresLivePlayer = Yes
  End
End
```

A `FireOCL` releases passengers at the aircraft's position when it fires. The weapon's range
controls how close the aircraft must get to the ordered location.

Changes simulation when enabled. [Issue #126](https://github.com/triatomic/contraZH/issues/126).

## Attack nugget: FireRegardlessOfOrders

* `FireRegardlessOfOrders = No` - (Default. `Yes` fires every `NumberOfShots` no matter what the
unit is ordered to do meanwhile.)

Normally new orders cancel the `Attack` nugget's barrage. With `Yes` the shots are queued on the
unit and fired from the launch bone whenever `WeaponSlot` is ready, while the unit stays fully
controllable. The delivery decal stays until the last shot.

```
ObjectCreationList SUPERWEAPON_TomahawkStrike
  Attack
    WeaponSlot             = TERTIARY
    NumberOfShots          = 6
    FireRegardlessOfOrders = Yes
  End
End
```

Notes:
* No turning or aiming, so do not use a turret weapon. `PreAttackDelay` is ignored.
* Range is not checked.
* Entering a container that blocks passenger fire drops the rest.
* Timing follows the weapon (`DelayBetweenShots`, clip reloads). Running dry with
`AutoReloadsClip = No` drops the rest.
* Firing again replaces the queue. Death drops it. Save/load keeps it.

## Attack nugget: placed shots

When a `SPECIAL_POWER_CONSTRUCT` button fires the `Attack` nugget, the weapon's `FireOCL` spawns
at the spot and rotation the player placed, instead of at the shooter with the shooter's heading.
The placement cursor's legality check therefore applies to where the object really lands. Powers
fired by the AI, by scripts or by a plain `SPECIAL_POWER` button are unchanged.

```
ObjectCreationList OCL_SpecialPowerDig
  Attack
    WeaponSlot    = SECONDARY
    NumberOfShots = 1
  End
End

Weapon PlaceTunnelWeapon
  FireOCL        = OCL_CreateSneakAttackTunnelStart
  PreAttackDelay = 3795
  ...
End
```

Notes:
* Only `FireOCL` moves. `ProjectileDetonationOCL` is unchanged.
* Each of `NumberOfShots` spawns at the placement. A new order drops the placement with the
attack, unless `FireRegardlessOfOrders = Yes`. Save/load keeps it.

## ObjectCreationListExtend

See [Extend Blocks](contraZH-Extend.md#objectcreationlistini).
