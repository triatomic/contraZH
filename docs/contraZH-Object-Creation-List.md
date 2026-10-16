# ObjectCreationList.ini

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

## ObjectCreationListExtend

* `ObjectCreationListExtend <NewList> <ParentList>` - (Runs every nugget of the parent, then the
nuggets in its own block.)

```
ObjectCreationListExtend OCL_ChinaTankDeathWithCrew OCL_ChinaTankDeath
  CreateObject
    ObjectNames = ChinaInfantryTankHunter
    Count       = 2
  End
End
```

Notes:
* The parent must be defined above the extending block. A missing parent asserts in debug builds and
the block loads without it.
* Nuggets only add. A parent nugget cannot be changed or removed.
* The copy is taken at load. Redefining the parent later does not change the extended list.
* Naming the list itself as parent appends nuggets to it.
