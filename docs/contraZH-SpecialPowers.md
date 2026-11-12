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
* **No** shot fired: the power comes back ready. Upfront costs are not refunded;
deferred costs are not charged.
* Failsafe: after `ReloadTime` of waiting the cooldown starts anyway, and the log names the power
(also catches OCLs with no `Attack` nugget).
* Ignored for `SharedSyncedTimer` powers.

## DeductCostOnFirstShot

`DeductCostOnFirstShot = No` by default. Set it to `Yes` to deduct `Cost` once, when the
caster fires its first shot, instead of when the power triggers.

```
SpecialPower SpecialPowerDig
  Enum                     = SPECIAL_HELIX_NAPALM_BOMB
  ReloadTime               = 60000
  Cost                     = 500
  DeductCostOnFirstShot     = Yes
  StartCooldownOnFirstShot  = Yes
End
```

* The player must have enough money when the power triggers. Money is not reserved.
* If the player spends that money before firing, the first shot takes up to the remaining balance.
* Cancelling before the first shot costs nothing. Later shots in the burst cost nothing extra.
* Cooldown timing still follows `StartCooldownOnFirstShot`. With that setting off, the cooldown
starts when the power triggers, even if the attack is later cancelled.
* The caster must fire the shot itself, as with an OCL `Attack` nugget. Shots from spawned units
do not count. A caster that cannot fire pays when the power triggers.
* Any shot from the caster during the wait counts, including one at a target ordered afterwards.
* The shot wait ends after `ReloadTime`. A caster still attacking at that point pays then.
* A script or crate that resets the timer during the wait also collects the cost.
* Saving and loading preserves any unpaid cost.

## Eva events for Contra superweapons

Events for the remaining Contra superweapons. These are not tied to a special power enum, so any
template can use them, including several templates sharing one set.

* SUPERWEAPONDETECTED_OWN_NUCLEAR_STORM
* SUPERWEAPONDETECTED_ALLY_NUCLEAR_STORM
* SUPERWEAPONDETECTED_ENEMY_NUCLEAR_STORM
* SUPERWEAPONLAUNCHED_OWN_NUCLEAR_STORM
* SUPERWEAPONLAUNCHED_ALLY_NUCLEAR_STORM
* SUPERWEAPONLAUNCHED_ENEMY_NUCLEAR_STORM
* SUPERWEAPONREADY_OWN_NUCLEAR_STORM
* SUPERWEAPONREADY_ALLY_NUCLEAR_STORM
* SUPERWEAPONREADY_ENEMY_NUCLEAR_STORM

* SUPERWEAPONDETECTED_OWN_WEATHER_CONTROL
* SUPERWEAPONDETECTED_ALLY_WEATHER_CONTROL
* SUPERWEAPONDETECTED_ENEMY_WEATHER_CONTROL
* SUPERWEAPONLAUNCHED_OWN_WEATHER_CONTROL
* SUPERWEAPONLAUNCHED_ALLY_WEATHER_CONTROL
* SUPERWEAPONLAUNCHED_ENEMY_WEATHER_CONTROL
* SUPERWEAPONREADY_OWN_WEATHER_CONTROL
* SUPERWEAPONREADY_ALLY_WEATHER_CONTROL
* SUPERWEAPONREADY_ENEMY_WEATHER_CONTROL

* SUPERWEAPONDETECTED_OWN_HATF_MISSILE
* SUPERWEAPONDETECTED_ALLY_HATF_MISSILE
* SUPERWEAPONDETECTED_ENEMY_HATF_MISSILE
* SUPERWEAPONLAUNCHED_OWN_HATF_MISSILE
* SUPERWEAPONLAUNCHED_ALLY_HATF_MISSILE
* SUPERWEAPONLAUNCHED_ENEMY_HATF_MISSILE
* SUPERWEAPONREADY_OWN_HATF_MISSILE
* SUPERWEAPONREADY_ALLY_HATF_MISSILE
* SUPERWEAPONREADY_ENEMY_HATF_MISSILE

* SUPERWEAPONDETECTED_OWN_COMMAND_UPLINK
* SUPERWEAPONDETECTED_ALLY_COMMAND_UPLINK
* SUPERWEAPONDETECTED_ENEMY_COMMAND_UPLINK
* SUPERWEAPONLAUNCHED_OWN_COMMAND_UPLINK
* SUPERWEAPONLAUNCHED_ALLY_COMMAND_UPLINK
* SUPERWEAPONLAUNCHED_ENEMY_COMMAND_UPLINK
* SUPERWEAPONREADY_OWN_COMMAND_UPLINK
* SUPERWEAPONREADY_ALLY_COMMAND_UPLINK
* SUPERWEAPONREADY_ENEMY_COMMAND_UPLINK

* SUPERWEAPONDETECTED_OWN_ATMOSPHERIC_LENS
* SUPERWEAPONDETECTED_ALLY_ATMOSPHERIC_LENS
* SUPERWEAPONDETECTED_ENEMY_ATMOSPHERIC_LENS
* SUPERWEAPONLAUNCHED_OWN_ATMOSPHERIC_LENS
* SUPERWEAPONLAUNCHED_ALLY_ATMOSPHERIC_LENS
* SUPERWEAPONLAUNCHED_ENEMY_ATMOSPHERIC_LENS
* SUPERWEAPONREADY_OWN_ATMOSPHERIC_LENS
* SUPERWEAPONREADY_ALLY_ATMOSPHERIC_LENS
* SUPERWEAPONREADY_ENEMY_ATMOSPHERIC_LENS

* SUPERWEAPONDETECTED_OWN_ICBM
* SUPERWEAPONDETECTED_ALLY_ICBM
* SUPERWEAPONDETECTED_ENEMY_ICBM
* SUPERWEAPONLAUNCHED_OWN_ICBM
* SUPERWEAPONLAUNCHED_ALLY_ICBM
* SUPERWEAPONLAUNCHED_ENEMY_ICBM
* SUPERWEAPONREADY_OWN_ICBM
* SUPERWEAPONREADY_ALLY_ICBM
* SUPERWEAPONREADY_ENEMY_ICBM

* SUPERWEAPONDETECTED_OWN_TOMAHAWK_STORM
* SUPERWEAPONDETECTED_ALLY_TOMAHAWK_STORM
* SUPERWEAPONDETECTED_ENEMY_TOMAHAWK_STORM
* SUPERWEAPONLAUNCHED_OWN_TOMAHAWK_STORM
* SUPERWEAPONLAUNCHED_ALLY_TOMAHAWK_STORM
* SUPERWEAPONLAUNCHED_ENEMY_TOMAHAWK_STORM
* SUPERWEAPONREADY_OWN_TOMAHAWK_STORM
* SUPERWEAPONREADY_ALLY_TOMAHAWK_STORM
* SUPERWEAPONREADY_ENEMY_TOMAHAWK_STORM

### Which template uses which event set

The Contra superweapon templates these were added for. Several templates share one `Enum`, so the
event set is chosen per template, not per enum.

| Template | Event set |
| --- | --- |
| SuperweaponNuclearStorm | NUCLEAR_STORM |
| SuperweaponEMPStorm (weather control device) | WEATHER_CONTROL |
| SuperweaponHatfMissile, SuperweaponHatfVMissile | HATF_MISSILE |
| SuperweaponStrategicBombing (strategic command uplink) | COMMAND_UPLINK |
| SuperweaponAtmoLenzWeapon | ATMOSPHERIC_LENS |
| SuperweaponICBMMissile | ICBM |
| SuperweaponTomahawkStorm | TOMAHAWK_STORM |

`SuperweaponParticleUplinkCannon`, `SuperweaponParticleUplinkCannonSupW`, `SuperweaponNeutronMissile`,
`Nuke_SuperweaponNeutronMissile` and `SuperweaponScudStorm` keep the vanilla hardcoded events, so they
got no new set.
