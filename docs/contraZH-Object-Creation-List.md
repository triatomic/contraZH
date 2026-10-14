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
