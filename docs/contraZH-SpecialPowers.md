# SpecialPower.ini

## StartCooldownOnFirstShot

* `StartCooldownOnFirstShot = No` - (Default. `Yes` delays `ReloadTime` until the unit has fired the
shots the power ordered.)

For powers whose OCL has an `Attack` nugget, the cooldown starts after the unit finishes shooting.
Until then the power is locked (cameo greyed out), through the last of `NumberOfShots`.

```
SpecialPower SpecialPowerDig
  Enum                     = SPECIAL_HELIX_NAPALM_BOMB
  ReloadTime               = 60000
  StartCooldownOnFirstShot = Yes
End
```

Notes:
* At least one shot fired: the cooldown starts even if the rest never fire (move order, empty clip,
death).
* **No** shot fired: the power comes back ready. Credits are not refunded.
* Failsafe: after `ReloadTime` of waiting the cooldown starts anyway, and the log names the power
(also catches OCLs with no `Attack` nugget).
* Ignored for `SharedSyncedTimer` powers.
