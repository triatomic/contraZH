# Objects & Modules

## Object.ini

### CaptureTimePercent

* `CaptureTimePercent = 100%` - (Default. Scales how long it takes to capture this object.)

Multiplies the capturer's `PreparationTime` when the target is this object. Applies to infantry
capture (`SPECIAL_INFANTRY_CAPTURE_BUILDING`) and Black Lotus capture
(`SPECIAL_BLACKLOTUS_CAPTURE_BUILDING`).

```
Object AmericaCommandCenter
  CaptureTimePercent = 200%
End
```

With `PreparationTime = 17000`, capturing this Command Center takes 34 seconds.

Notes:
* The capture flash and the raising-flag animation follow the scaled time.
* Changes the simulation; replays must use the same INI.

### QuickGamePrerequisites

* `QuickGamePrerequisites` - (Default: none. Replaces `Prerequisites` when
[Quick Game](contraZH-Maps-and-Setup.md#quick-game) is on.)

Takes the same `Object` and `Science` lines as `Prerequisites`. An object that leaves it out uses
its `Prerequisites` in a Quick Game too.

```
Object AmericaParticleCannonUplink
  Prerequisites
    Object  = AmericaStrategyCenter
    Science = SCIENCE_Rank5
  End
  QuickGamePrerequisites
    Object  = AmericaStrategyCenter
  End
End
```

In a Quick Game the uplink needs only the Strategy Center; otherwise it also needs rank 5.

Notes:
* An empty `QuickGamePrerequisites` block lets the object be built with no prerequisites in a
Quick Game.
* The build tooltip lists the requirements of the current game.
* A map.ini override replaces the list, as it does for `Prerequisites`.

## Turret modules

### MaxPhysicalPitch

* `MaxPhysicalPitch = 90` - (Default. The highest pitch a turret with `AllowsPitch = Yes` will aim at,
in degrees.)

Counterpart of `MinPhysicalPitch`. Stops a vehicle's cannon pointing skyward at out-of-range
targets.

## RiderChangeContain

Two opt-in fields on the combat bike contain module, both default `No`.

### SurviveScuttle

* `SurviveScuttle = No` - (Default. `Yes` keeps the transport alive after its rider dismounts.)

The abandoned bike topples into its `ScuttleStatus` pose and stays. A new valid rider stands it back
up.

Notes:
* Not box-selectable, but can be right-clicked, so a rider can be ordered onto it.
* Enemies can still destroy it; only its owner can mount it.
* `ScuttleDelay` is unused.

### SilentScuttle

* `SilentScuttle = No` - (Default. `Yes` scuttles the bike without announcing a loss.)

A dismount scuttle no longer triggers "Unit Lost", a radar ping or an "under attack" warning. A bike
killed by an enemy still announces. Kill credit and score are unchanged. Not needed with
`SurviveScuttle = Yes`.

## FireWeaponWhenDamagedBehavior

### NoHealthLoss

* `NoHealthLoss = No` - (Default. `Yes` refunds the health taken by any hit that passes the
`DamageTypes` and `DamageAmount` gate.)

For using the module as a trigger (e.g. a 1 point `MELEE` hit) without the trigger hit costing
health.

Notes:
* Every qualifying hit is refunded, even when no weapon fires.
* Qualifying hits can no longer kill the object or change its damage state.
* The hit still plays damage effects and raises "under attack".
* Other damage types and smaller hits drain health normally.

## FireWeaponUpdate

### Upgrade parameters

`FireWeaponUpdate` and `FireWeaponAdvancedUpdate` now take the standard upgrade keys, so a
self-firing weapon can be switched on by an upgrade instead of needing a separate object variant.

* `TriggeredBy` - upgrades that switch the module on. Any one of them is enough unless
`RequiresAllTriggers = Yes`.
* `ConflictsWith` - upgrades that switch it off. A conflict wins over a trigger.
* `RequiresAllTriggers = No` - (Default. `Yes` demands every upgrade in `TriggeredBy`.)
* `RemovesUpgrades` - upgrades stripped from the object when the trigger first completes.
* `FXListUpgrade` - played once when the trigger first completes.

With no `TriggeredBy` listed the module fires as it always has, so existing objects need no change.

Notes:
* The gate is re-read every frame rather than latched, so an upgrade named in `ConflictsWith` stops
the firing again once it completes. Most upgrade modules only ever turn on; this one can turn back
off.
* `RemovesUpgrades` and `FXListUpgrade` still run only once, on the frame the trigger completes.
* Both the object's own upgrades and its owning player's upgrades count toward the gate.

## MinefieldBehavior

### KindOf filters

Which objects can set the mine off, checked on top of `DetonatedBy` and `WorkersDetonate`:

```ini
  Behavior = MinefieldBehavior ModuleTag_Mine
    DetonationWeapon = AntiTankMineWeapon
    DetonatedBy      = ENEMIES NEUTRAL
    RequiredKindOf   = VEHICLE
    ForbiddenKindOf  = HUGE_VEHICLE
  End
```

* `RequiredKindOf` - (A detonator needs at least one of these. Default = everything.)
* `ForbiddenKindOf` - (A detonator may have none of these. Default = none.)

An object that fails the filter walks over the mine without triggering it. Mine clearing is
unaffected.

## GenerateMinefieldBehavior

### Placement blockers

`SkipIfThisMuchUnderStructure` used to skip a mine only under a `STRUCTURE`. Two keys now pick the
blocking objects:

* `BlockedByKindOf = STRUCTURE` - (Default. A mine is skipped when an object with any of these
overlaps it. `NONE` places mines under anything; `+VEHICLE` adds to the default.)
* `NotBlockedByKindOf` - (An object with any of these never blocks a mine. Default = none.)

## StealthUpdate

### New StealthForbiddenConditions

New values for `StealthForbiddenConditions` (and `OverrideStealthForbiddenConditions` on
`StealthUpgrade`):

* `RIDERS_FIRING_PRIMARY`, `RIDERS_FIRING_SECONDARY`, `RIDERS_FIRING_TERTIARY` - no stealth while a
passenger fired that slot this frame or last. Only for `PassengersAllowedToFire = Yes`. Unlike
`RIDERS_ATTACKING`, they reveal only on shots, so the container can re-cloak between them.
* `UNIT_CREATED` - no stealth in the frame it finishes producing a unit (production queue, spawner
or drone release, dozer finishing a structure).

Each breaks stealth for one frame; `StealthDelay` decides when it can cloak again.

## Model conditions

### UNMANNED

Set while an object is `DISABLED_UNMANNED` (pilot sniped, drone whose carrier died, or the
`NAMED_SET_UNMANNED_STATUS` script action) and cleared when it is manned again. A
`ConditionState = UNMANNED` block on a draw module can hide crew art, such as the gunner on a
technical, while the vehicle sits empty.

```ini
  Draw = W3DModelDraw ModuleTag_Gunner
    DefaultConditionState
      Model = GunnerMesh
    End
    ConditionState = UNMANNED
      Model = NONE
    End
  End
```

## New Behavior Modules

### TimeOfDayOverrideUpdate

Switches the map's time of day, piggybacking on an existing special power. Two modes:

* On the structure firing the power: switches at launch, for `Duration`.
* On an object the power creates, with `ActivateOnCreate`: switches when it is created and reverts
when it dies.

```
Behavior = TimeOfDayOverrideUpdate ModuleTag_TODOverride01
  SpecialPowerTemplate = SuperweaponScudStorm  ; optional, see below
  TimeOfDay            = NIGHT
  FallbackTimeOfDay    = AFTERNOON
  Duration             = 60000
End
```

Or on an object the power creates, where the effect itself decides how long the night lasts:

```
Behavior = TimeOfDayOverrideUpdate ModuleTag_TODOverride01
  TimeOfDay        = NIGHT
  ActivateOnCreate = Yes
End
```

* `SpecialPowerTemplate` - (Only react to this power. Leave it out and the module reacts to every
special power the object fires. Not used when `ActivateOnCreate` is set.)
* `TimeOfDay = NIGHT` - (`MORNING` | `AFTERNOON` | `EVENING` | `NIGHT`. What the world switches to.)
* `FallbackTimeOfDay = AFTERNOON` - (Revert target when the map already sits at `TimeOfDay`. Never
switched to.)
* `Duration = 0` - (In milliseconds. `0` makes the switch permanent, and firing again toggles it
back. Any other value reverts after that long. Ignored with `ActivateOnCreate`.)
* `ActivateOnCreate = No` - (Yes switches on creation and reverts on death, instead of on a special
power.)

A real time of day change: the map's night lighting, night model variants, ambient sounds, sky,
water and player indicator colours all come with it.

Notes:
* Without `ActivateOnCreate` the switch lands at **launch**, not impact.
* A map already at `TimeOfDay` is left alone.
* Global: every player and observer sees it.
* Overlapping holders share the change; it reverts when the last one lets go.
* Multiplayer and replay safe.
* A timed switch reverts if the firing structure dies.
* Runtime switches now relight trees, bibs, bridges and roads immediately; the debug time of day
hotkey benefits too, and refreshes player indicator colours.

### GrantTemporaryStealthBehavior

Grants stealth for a set time. It runs beside permanent stealth (`GrantStealthBehavior`, upgrades,
innate) and never removes it when it runs out. Two modes:

* On the object firing a special power: grants when the power fires.
* On an object an OCL or weapon creates, with `ActivateOnCreate`: grants once, on creation.

```
Behavior = GrantTemporaryStealthBehavior ModuleTag_TempStealth01
  SpecialPowerTemplate = SpecialAbilityCloak  ; optional, see below
  Duration             = 10000
  Radius               = 0
  KindOf               = INFANTRY VEHICLE
  ForbiddenKindOf      = AIRCRAFT
End
```

* `SpecialPowerTemplate` - (Only react to this power. Leave it out and the module reacts to every
special power the object fires. Not used when `ActivateOnCreate` is set.)
* `Duration = 0` - (In milliseconds. How long the stealth lasts. Required.)
* `Radius = 0` - (`0` grants to a single object. Any other value grants to every allied object
within that distance.)
* `KindOf` - (A receiver needs at least one of these. Default = everything.)
* `ForbiddenKindOf` - (A receiver may have none of these. Default = none.)
* `ActivateOnCreate = No` - (Yes grants on creation instead of on a special power.)

Who receives the stealth:

| Trigger | `Radius = 0` | `Radius > 0` |
| --- | --- | --- |
| Power without a target | the firing object | allies around the firing object |
| Power on an object | the target | allies around the target |
| Power on a location | nobody | allies around the location |
| `ActivateOnCreate` | the created object | allies around the created object |

A unit ability needs a special power, a button and a `SpecialAbility` module beside this one:

```
SpecialPower SpecialAbilityCloak
  Enum        = SPECIAL_CIA_INTELLIGENCE
  ReloadTime  = 30000
  PublicTimer = No
End

CommandButton Command_Cloak
  Command      = SPECIAL_POWER
  SpecialPower = SpecialAbilityCloak
  TextLabel    = CONTROLBAR:Cloak
  ButtonImage  = SUGPS02
End

; on the unit
Behavior = SpecialAbility ModuleTag_CloakPower
  SpecialPowerTemplate = SpecialAbilityCloak
End
Behavior = GrantTemporaryStealthBehavior ModuleTag_Cloak
  SpecialPowerTemplate = SpecialAbilityCloak
  Duration             = 10000
End
```

The engine decides by the power's `Enum` which kind of target it accepts. These work without an
update module of their own:

| Target | `Enum` | Button `Options` |
| --- | --- | --- |
| None | `SPECIAL_CIA_INTELLIGENCE`, `SPECIAL_COMMUNICATIONS_DOWNLOAD` | - |
| Allied object | `SPECIAL_TANKHUNTER_TNT_ATTACK` | `CONTEXTMODE_COMMAND NEED_TARGET_ALLY_OBJECT` |
| Location | `SPECIAL_RADAR_VAN_SCAN` | `CONTEXTMODE_COMMAND NEED_TARGET_POS` |

`SPECIAL_TANKHUNTER_TNT_ATTACK` accepts structures and ground vehicles only. A power on an object
has no range limit and lands the moment the player clicks.

A field dropped by an OCL, for an area effect that needs no special power on the caster:

```
Object TempStealthField
  KindOf = IMMOBILE UNATTACKABLE INERT
  Behavior = GrantTemporaryStealthBehavior ModuleTag_01
    ActivateOnCreate = Yes
    Duration         = 15000
    Radius           = 150
  End
  Behavior = LifetimeUpdate ModuleTag_02
    MinLifetime = 100
    MaxLifetime = 100
  End
End
```

Notes:
* A receiver needs a `StealthUpdate`, as with the GPS Scrambler. Its `StealthForbiddenConditions`
and `StealthDelay` still apply while the grant runs.
* Only allies of the granting object receive it, its own units included.
* A second grant never shortens a running one. The later end time wins.
* When the time is up, a unit that can stealth by other means stays stealthed.
* A portable structure or drone that joins a carrier running on temporary stealth alone gets the
time the carrier has left, not permanent stealth.
* A player order does not end it, unlike the supply center's `GrantTemporaryStealth`.
* Disguisers such as the Bomb Truck are skipped.
* List the module before the power's own update module (e.g. `SpecialAbilityUpdate`), which stops
later modules from seeing the power fire.
