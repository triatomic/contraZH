# SpecialPower Enums

Every `Enum` value a `SpecialPower` can carry, grouped by the logic the engine hardcodes to it, plus
the Eva event system that announces superweapons. Line numbers refer to
`GeneralsMD/Code/GameEngine` unless a path says `Core/`.

# How Enum is used

* `Enum = <value>` - The power's type. Objects carry a bit per type, so `hasSpecialPower`,
the shortcut bar's source lookup (`Player::findMostReadyShortcutSpecialPowerOfType`) and
`SpecialPowerMaskType` all key on it. Two powers with the same `Enum` are the same power to that
code.
* `BehaviorEnum = <retail value>` - (Optional. Only read for values from `SPECIAL_ION_CANNON`
onward.) The retail type whose targeting rules this power borrows.

`ActionManager` decides where a power may be used (`canDoSpecialPowerAtLocation`,
`canDoSpecialPowerAtObject`, `canDoSpecialPower`, `Source/Common/RTS/ActionManager.cpp:1631`,
`:1893`, `:2294`). Every switch there runs on a **behavior type**: for a retail value that is the
value itself; for anything from `SPECIAL_ION_CANNON` onward it is `BehaviorEnum`, and when that is
unset, the entry in `ActionManager::getFallbackBehaviorType` (`:2142`). A value with no fallback
case behaves as `SPECIAL_NEUTRON_MISSILE`.

Everything outside `ActionManager` (Eva chains, `SpecialAbilityUpdate`, AI, cursor hints) keys on
the real `Enum`, not on `BehaviorEnum`.

# Retail values

The 67 values shipped with Zero Hour (`Include/Common/SpecialPowerType.h:40-128`). Each appears in
one or more of the groups below.

## Placement checks (location powers)

`canDoSpecialPowerAtLocation`, `ActionManager.cpp:1715-1841`. These powers target a spot on the
map.

* **Rejected on water** (`:1717-1723`): `SPECIAL_PARADROP_AMERICA`, `INFA_SPECIAL_PARADROP_AMERICA`,
`SPECIAL_CRATE_DROP`, `SPECIAL_TANK_PARADROP`. The switch falls through, so these also get the
cliff test below.
* **Rejected on water or a cliff cell** (`:1725`): `SPECIAL_JUMPJET` (and `SPECIAL_TELEPORT_SELF`
through its fallback).
* **Rejected on shrouded ground** (`:1735-1783`): `SPECIAL_DAISY_CUTTER`, `AIRF_SPECIAL_DAISY_CUTTER`,
`SPECIAL_PARADROP_AMERICA`, `SPECIAL_TANK_PARADROP`, `INFA_SPECIAL_PARADROP_AMERICA`,
`SPECIAL_CARPET_BOMB`, `SPECIAL_CHINA_CARPET_BOMB`, `SPECIAL_LEAFLET_DROP`,
`EARLY_SPECIAL_LEAFLET_DROP`, `EARLY_SPECIAL_CHINA_CARPET_BOMB`, `AIRF_SPECIAL_CARPET_BOMB`,
`SUPR_SPECIAL_CRUISE_MISSILE`, `SPECIAL_CLUSTER_MINES`, `NUKE_SPECIAL_CLUSTER_MINES`,
`SPECIAL_EMP_PULSE`, `SPECIAL_CRATE_DROP`, `SPECIAL_NAPALM_STRIKE`, `SPECIAL_BLACK_MARKET_NUKE`,
`SPECIAL_ANTHRAX_BOMB`, `SPECIAL_TERROR_CELL`, `SPECIAL_AMBUSH`, `SPECIAL_NEUTRON_MISSILE`,
`NUKE_SPECIAL_NEUTRON_MISSILE`, `SUPW_SPECIAL_NEUTRON_MISSILE`, `SPECIAL_SCUD_STORM`,
`SPECIAL_DEMORALIZE` (only with `ALLOW_DEMORALIZE`), `SPECIAL_A10_THUNDERBOLT_STRIKE`,
`AIRF_SPECIAL_A10_THUNDERBOLT_STRIKE`, `SPECIAL_SPECTRE_GUNSHIP`, `AIRF_SPECIAL_SPECTRE_GUNSHIP`,
`SPECIAL_REPAIR_VEHICLES`, `EARLY_SPECIAL_REPAIR_VEHICLES`, `SPECIAL_GPS_SCRAMBLER`,
`SLTH_SPECIAL_GPS_SCRAMBLER`, `SPECIAL_ARTILLERY_BARRAGE`, `SPECIAL_FRENZY`, `EARLY_SPECIAL_FRENZY`,
`SPECIAL_PARTICLE_UPLINK_CANNON`, `SUPW_SPECIAL_PARTICLE_UPLINK_CANNON`,
`LAZR_SPECIAL_PARTICLE_UPLINK_CANNON`, `SPECIAL_CLEANUP_AREA`, `SPECIAL_BATTLESHIP_BOMBARDMENT`,
`SPECIAL_JUMPJET`.
* **Anywhere on the map, shroud or not** (`:1785-1791`): `SPECIAL_SPY_SATELLITE`,
`SPECIAL_RADAR_VAN_SCAN`, `SPECIAL_SPY_DRONE`, `SPECIAL_HELIX_NAPALM_BOMB`.
* **Always allowed** (`:1792`): `SPECIAL_LAUNCH_BAIKONUR_ROCKET`.
* **Sneak attack** (`:1818-1841`, keyed on the real `Enum`): `SPECIAL_SNEAK_ATTACK` runs the
building placement legality test (`TheBuildAssistant->isLocationLegalToBuild`).

## Object-target powers

These reject a bare location (`ActionManager.cpp:1796-1813`) and apply a per-value rule in
`canDoSpecialPowerAtObject` (`:1904-2137`):

* `SPECIAL_CASH_BOUNTY` - never valid on an object (`:1906`).
* `SPECIAL_BATTLESHIP_BOMBARDMENT` - non-allies only (`:1909`).
* `SPECIAL_TANKHUNTER_TNT_ATTACK` - structure, or a vehicle that is not an aircraft (`:1916`).
* `SPECIAL_BOOBY_TRAP` - allied or neutral structures (`:1923`).
* `SPECIAL_MISSILE_DEFENDER_LASER_GUIDED_MISSILES` - `SpecialAbilityUpdate::isValidLaserLockTarget`
(`:1933`).
* `SPECIAL_HACKER_DISABLE_BUILDING` - enemy capturable structure that is not a rebuild hole
(`:1951`).
* `SPECIAL_INFANTRY_CAPTURE_BUILDING`, `SPECIAL_BLACKLOTUS_CAPTURE_BUILDING` - `canCaptureBuilding`
(`:1964`, `:1139`).
* `SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK` - `canDisableVehicleViaHacking` (`:1968`, `:1247`).
* `SPECIAL_BLACKLOTUS_STEAL_CASH_HACK` - `canStealCashViaHacking` (`:1971`, `:1372`).
* `SPECIAL_CASH_HACK` - enemy capturable `CASH_GENERATOR` not under construction (`:1974`).
* `SPECIAL_DISGUISE_AS_VEHICLE` - a vehicle that is not an aircraft, boat or cliff jumper and has no
`RailroadBehavior` (`:1997`).
* `SPECIAL_DEFECTOR` - `canMakeObjectDefector`, enemies, not a structure (`:2018`).
* `SPECIAL_REMOTE_CHARGES`, `SPECIAL_TIMED_CHARGES`, `SPECIAL_HELIX_NAPALM_BOMB` - charge count
limit, one charge per target, remote and timed charges exclude each other (`:2087-2136`).
* `SPECIAL_DETONATE_DIRTY_NUKE`, `SPECIAL_CHANGE_BATTLE_PLANS`, `SPECIAL_TOGGLE_DRAWBRIDGE` -
listed as object powers but validated by their own modules.

## Source-only powers

`canDoSpecialPower` (`ActionManager.cpp:2354-2362`) allows these without a target; every other
power needs one: `SPECIAL_REMOTE_CHARGES`, `SPECIAL_CIA_INTELLIGENCE`,
`SPECIAL_COMMUNICATIONS_DOWNLOAD`, `SPECIAL_DETONATE_DIRTY_NUKE`, `SPECIAL_CHANGE_BATTLE_PLANS`,
`SPECIAL_LAUNCH_BAIKONUR_ROCKET`, `SPECIAL_TOGGLE_DRAWBRIDGE`.

## SpecialAbilityUpdate state machine

`Source/GameLogic/Object/Update/SpecialAbilityUpdate.cpp` switches on the real `Enum` to run the
unit ability. Only these values do anything there; every other power routes through its
`SpecialPower` module unchanged.

* `SPECIAL_INFANTRY_CAPTURE_BUILDING`, `SPECIAL_BLACKLOTUS_CAPTURE_BUILDING` - abort when the unit
moves or the target changes hands (`:273`, `:308`), raise the flag and radar infiltration ping,
fire `EVA_BuildingBeingStolen` (`:1062-1092`), capture and academy stat (`:1500`, `:1543`).
* `SPECIAL_HACKER_DISABLE_BUILDING`, `SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK` - disable target
(`:1441`), completion voice (`:825`).
* `SPECIAL_BLACKLOTUS_STEAL_CASH_HACK` - abort on re-stealth (`:319`), steal cash with floating text
(`:1552`), completion voice (`:828`).
* `SPECIAL_MISSILE_DEFENDER_LASER_GUIDED_MISSILES` - laser object and weapon lock (`:1047`,
`:1362`, `:1827`).
* `SPECIAL_TANKHUNTER_TNT_ATTACK`, `SPECIAL_TIMED_CHARGES`, `SPECIAL_BOOBY_TRAP` - place a sticky
object, reject an already trapped target (`:1390`, `:1411`).
* `SPECIAL_REMOTE_CHARGES` - booby trap check (`:1608`), command check (`:623`).
* `SPECIAL_HELIX_NAPALM_BOMB` - spawn the bomb (`:1384`).
* `SPECIAL_DISGUISE_AS_VEHICLE` - `StealthUpdate::disguiseAsObject` (`:1674`).
* `SPECIAL_JUMPJET` - finish facing before unpack (`:463`), create and enter the flight container
(`:1690`).
* Special-object retention on completion (`:2126-2143`): kept for TNT, timed charges, booby trap,
remote charges, disguise, helix bomb, jumpjet; killed for the laser and the four hack and capture
powers.

## Client cursor and control bar (Core)

* `Core/GameEngine/Source/GameClient/MessageStream/CommandXlat.cpp:2310-2325` -
`SPECIAL_BLACKLOTUS_CAPTURE_BUILDING` shows the hack hint cursor,
`SPECIAL_INFANTRY_CAPTURE_BUILDING` the capture hint, and both auto-issue the capture command.
* `CommandXlat.cpp:2678-2686` - the hack commands are issued with the literal values
`SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK`, `SPECIAL_BLACKLOTUS_STEAL_CASH_HACK`,
`SPECIAL_HACKER_DISABLE_BUILDING`.
* `Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarCommand.cpp:1525` -
`SPECIAL_CHANGE_BATTLE_PLANS` reads the active state from `BattlePlanUpdate`; `:1534` -
`SPECIAL_TOGGLE_DRAWBRIDGE` from `DrawBridgeTowerUpdate`.

## AI and scripts

* `Source/GameLogic/AI/AISkirmishPlayer.cpp:1209` - `SPECIAL_CLUSTER_MINES`,
`NUKE_SPECIAL_CLUSTER_MINES` mine the base entrances (flank, backdoor, center waypoint paths)
instead of the generic superweapon grid scan.
* `Source/GameLogic/AI/AIPlayer.cpp:1213` - `SPECIAL_SNEAK_ATTACK` aims at rich undefended areas
rather than military units.
* `Source/GameLogic/AI/AIGroup.cpp:3065` - `SPECIAL_JUMPJET` gives each group member a
formation-relative destination.
* `Source/GameLogic/Object/Update/CommandButtonHuntUpdate.cpp:286-293` - hunt flags for
`SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK`, `SPECIAL_INFANTRY_CAPTURE_BUILDING`,
`SPECIAL_TIMED_CHARGES`, `SPECIAL_TANKHUNTER_TNT_ATTACK`.
* `Source/GameLogic/Object/Collide/SquishCollide.cpp:84` - an active
`SPECIAL_TANKHUNTER_TNT_ATTACK` prevents the unit being crushed.
* `Source/GameLogic/ScriptEngine/ScriptActions.cpp:4290` - the AI superweapon script action re-solves
the target for `SPECIAL_SNEAK_ATTACK` because it places a building. Every other script condition
and action is template-name driven.

## Hardcoded Eva chains

Retail superweapons announce through if-chains on the real `Enum`. They only run for values below
`SPECIAL_ION_CANNON` whose Eva fields are unset (see [Eva events](#eva-events)).

* Detected on construction (`Source/Common/RTS/Player.cpp:1720-1854`): particle cannon
(`SPECIAL_PARTICLE_UPLINK_CANNON`, `SUPW_`, `LAZR_`), nuke (`SPECIAL_NEUTRON_MISSILE`, `NUKE_`,
`SUPW_`), `SPECIAL_SCUD_STORM`.
* Launched (`Source/GameLogic/Object/SpecialPower/SpecialPowerModule.cpp:913-979`): the same three
groups plus GPS scrambler (`SPECIAL_GPS_SCRAMBLER`, `SLTH_`) and `SPECIAL_SNEAK_ATTACK`.
* Ready (`Source/GameClient/InGameUI.cpp:4544-4591`): particle cannon, nuke, scud storm.

Known issue, deferred: in `Player::onStructureConstructionComplete` the particle cannon check at
`:1720` sits before the INI branch and the nuke and scud checks at `:1746` and `:1765` sit inside
it, so a retail superweapon can queue its detected line twice. Harmless in play because the flag is
a boolean, but the structure differs from the other two sites.

## Values with no logic outside the fallback map

Everything the OFS fork added as a faction-prefixed alias. Each maps to a retail behavior in
`getFallbackBehaviorType` (`ActionManager.cpp:2142-2257`) and has no other mention in code.

* -> `SPECIAL_PARADROP_AMERICA`: `AIRF_SPECIAL_PARADROP_AMERICA`, `SOCOM_SPECIAL_SUPPLY_DROP`,
`SOCOM_SPECIAL_TANK_PARADROP`, `TANK_SPECIAL_TANK_PARADROP`, `TANK_SPECIAL_PARADROP`,
`SUPW_SPECIAL_PARADROP_AMERICA`, `SUPW_SPECIAL_TANK_PARADROP`.
* -> `SPECIAL_CIA_INTELLIGENCE`: `SECW_SPECIAL_HUNTER_SEEKER`.
* -> `SPECIAL_JUMPJET`: `SPECIAL_TELEPORT_SELF`.
* -> `SPECIAL_SPY_SATELLITE`: `CHINA_SPECIAL_SPY_SATELLITE`, `SECW_SPECIAL_SPY_SATELLITE`,
`LAZR_SPECIAL_SPY_SATELLITE`.
* -> `SPECIAL_CARPET_BOMB`: `AIRF_SPECIAL_SUPERSONIC_AIRSTRIKE`, `AIRF_SPECIAL_HEAVY_AIRSTRIKE`,
`SOCOM_SPECIAL_COASTAL_BOMBARDEMENT`, `TANK_SPECIAL_NAPALM_BOMB`, `TANK_SPECIAL_CHINA_CARPET_BOMB`,
`NUKE_SPECIAL_NUCLEAR_AIRSTRIKE`, `NUKE_SPECIAL_CHINA_CARPET_BOMB`, `NUKE_SPECIAL_BALLISTIC_MISSILE`,
`SECW_SPECIAL_SYSTEM_HACK`, `DEMO_SPECIAL_SUICIDE_PLANE`, `DEMO_SPECIAL_CARPET_BOMB`,
`CHEM_SPECIAL_CARPET_BOMB`, `CHEM_SPECIAL_AIRSTRIKE`, `FORT_SPECIAL_AIRSTRIKE`,
`FORT_SPECIAL_CARPET_BOMB`, `LAZR_SPECIAL_DAISY_CUTTER`, `LAZR_SPECIAL_AIRSTRIKE`,
`SUPW_SPECIAL_AIRSTRIKE`.
* -> `SPECIAL_AMBUSH`: `AIRF_SPECIAL_HELICOPTER_AMBUSH`, `DEMO_SPECIAL_AMBUSH`, `CHEM_SPECIAL_AMBUSH`,
`LAZR_SPECIAL_AMBUSH`.
* -> `SPECIAL_FRENZY`: `AIRF_SPECIAL_HOLO_PLANES`, `TANK_SPECIAL_FRENZY`, `NUKE_SPECIAL_FRENZY`,
`DEMO_SPECIAL_FRENZY`, `CHEM_SPECIAL_FRENZY`, `FORT_SPECIAL_FRENZY`.
* -> `SPECIAL_CLUSTER_MINES`: `TANK_SPECIAL_CLUSTER_MINES`.
* -> `SPECIAL_REPAIR_VEHICLES`: `TANK_`, `NUKE_`, `DEMO_`, `CHEM_`, `FORT_SPECIAL_REPAIR_VEHICLES`,
`LAZR_SPECIAL_NANO_SWARM`, `SUPW_SPECIAL_FORCEFIELD`.
* -> `SPECIAL_EMP_PULSE`: `TANK_SPECIAL_EMP_PULSE`, `NUKE_SPECIAL_NEUTRON_BOMB`,
`SECW_SPECIAL_EMP_HACK`.
* -> `SPECIAL_ARTILLERY_BARRAGE`: `TANK_`, `NUKE_`, `DEMO_`, `FORT_SPECIAL_ARTILLERY_BARRAGE`,
`LAZR_SPECIAL_ORBITAL_STRIKE`, `SUPW_SPECIAL_ORBITAL_STRIKE`.
* -> `SPECIAL_CASH_HACK`: `NUKE_SPECIAL_CASH_HACK`.
* -> `SPECIAL_SPECTRE_GUNSHIP`: `SECW_SPECIAL_DRONE_GUNSHIP`, `LAZR_SPECIAL_SPECTRE_GUNSHIP`,
`SUPW_SPECIAL_SPECTRE_GUNSHIP`.
* -> `SPECIAL_SNEAK_ATTACK`: `DEMO_SPECIAL_SNEAK_ATTACK`, `CHEM_SPECIAL_SNEAK_ATTACK`. Note the
placement legality test and AI targeting above key on the real `Enum`, so these aliases only get
the shroud check.
* -> `SPECIAL_GPS_SCRAMBLER`: `DEMO_`, `CHEM_`, `FORT_SPECIAL_GPS_SCRAMBLER`.
* -> `SPECIAL_ANTHRAX_BOMB`: `DEMO_SPECIAL_ANTHRAX_BOMB`, `CHEM_SPECIAL_ANTHRAX_BOMB`.
* -> `SPECIAL_LEAFLET_DROP`: `CHEM_SPECIAL_VIRUS`, `SUPW_SPECIAL_CRYOBOMB`.
* -> `SPECIAL_TOGGLE_DRAWBRIDGE`: itself.

## Values with no logic at all

These fall to the map's default, `SPECIAL_NEUTRON_MISSILE` (shroud-checked location power), unless
`BehaviorEnum` says otherwise: `SPECIAL_ION_CANNON`, `SPECIAL_CLUSTER_MISSILE`,
`SPECIAL_SUNSTORM_MISSILE`, `SPECIAL_METEOR_STRIKE`, `SPECIAL_PUNISHER_CANNON`,
`SPECIAL_CHEMICAL_MISSILE`, `SPECIAL_CHRONOSPHERE`, `SOCOM_SPECIAL_AIR_DEPLOY_MARKER`,
`SPECIAL_DEMORALIZE_OBSOLETE`.

# Contra values

Added by this fork so each Contra superweapon has its own type instead of sharing a retail one.
Each gets its retail behavior from the fallback map; none has a hardcoded Eva line, so give them
Eva fields (below).

| Enum | Behaves as |
|---|---|
| `SPECIAL_STRATEGIC_BOMBING` | `SPECIAL_PARTICLE_UPLINK_CANNON` |
| `SPECIAL_TOMAHAWK_STORM` | `SPECIAL_PARTICLE_UPLINK_CANNON` |
| `SPECIAL_EMP_STORM` | `SPECIAL_PARTICLE_UPLINK_CANNON` |
| `SPECIAL_ICBM_MISSILE` | `SPECIAL_NEUTRON_MISSILE` |
| `SPECIAL_ATMO_LENS` | `SPECIAL_NEUTRON_MISSILE` |
| `SPECIAL_HATF_MISSILE` | `SPECIAL_SCUD_STORM` |
| `SPECIAL_HATF_V_MISSILE` | `SPECIAL_SCUD_STORM` |
| `SPECIAL_NUCLEAR_STORM` | `SPECIAL_NEUTRON_MISSILE` |
| `SPECIAL_MISSILE_SILO` | `SPECIAL_CHINA_CARPET_BOMB` |

`SPECIAL_CUSTOM_01` to `SPECIAL_CUSTOM_16` are a reserve for data. They carry no behavior, so a
power using one must set `BehaviorEnum`; a debug build asserts when it is missing.

Moving a Contra power from a retail value to one of these drops the retail Eva lines it used to get
for free. Add the nine `Eva*` fields, or the superweapon goes silent.

# Eva events

Eva announcements are read from data. Any name works; it does not have to exist in the engine.

* `EvaEvent <Name>` - A block in `Data\INI\Eva.ini` (or any file under `Data\INI\Eva\`) defines a
message. A name the engine does not know is registered when the block is read.
* `EvaDetectedOwn`, `EvaDetectedAlly`, `EvaDetectedEnemy`, `EvaLaunchedOwn`, `EvaLaunchedAlly`,
`EvaLaunchedEnemy`, `EvaReadyOwn`, `EvaReadyAlly`, `EvaReadyEnemy` - (SpecialPower. Default: unset.)
The message to play when the structure carrying the power is built, when the power fires, and when
it comes off cooldown, from the point of view of the local player.
* `EnemyDetectionEvaEvent`, `OwnDetectionEvaEvent` - (`StealthUpdate` module. Default: unset.) The
message when the object is revealed.

Rules:
* Names are case-insensitive. Any name is accepted on the fields above, in any INI order:
`SpecialPower.ini` and object INIs load before `Eva.ini` and may name events it has not defined
yet.
* `None` disables the field. On a retail-value power that silences the hardcoded chain; leaving the
field unset keeps it. Values from `SPECIAL_ION_CANNON` onward have no chain, so unset and `None`
are the same there.
* A name that no `EvaEvent` block defines never plays and never errors. Debug builds log such names
at startup.
* A second `EvaEvent` block with the same name replaces the first one completely.
* The built-in `LOWPOWER` message cannot be assigned to a power.

```
EvaEvent SuperweaponReady_Own_TomahawkStorm
  Priority            = 10
  TimeBetweenChecksMS = 30000
  ExpirationTimeMS    = 5000
  SideSounds
    Side   = AmericaSuperWeaponGeneral
    Sounds = EvaUSA_SuperweaponReady_TomahawkStorm
  End
End
```

```
SpecialPower SuperweaponTomahawkStorm
  Enum             = SPECIAL_TOMAHAWK_STORM
  ReloadTime       = 180000
  PublicTimer      = Yes
  EvaDetectedOwn   = None
  EvaDetectedAlly  = SuperweaponDetected_Ally_TomahawkStorm
  EvaDetectedEnemy = SuperweaponDetected_Enemy_TomahawkStorm
  EvaLaunchedOwn   = SuperweaponLaunched_Own_TomahawkStorm
  EvaLaunchedAlly  = SuperweaponLaunched_Ally_TomahawkStorm
  EvaLaunchedEnemy = SuperweaponLaunched_Enemy_TomahawkStorm
  EvaReadyOwn      = SuperweaponReady_Own_TomahawkStorm
  EvaReadyAlly     = SuperweaponReady_Ally_TomahawkStorm
  EvaReadyEnemy    = SuperweaponReady_Enemy_TomahawkStorm
End
```

Only a `SideSounds` block whose `Side` matches the observing player's side plays, so list every
side that can hear the line.

# Adding a new power

1. Pick an `Enum`. Reuse a retail value when the power should share its logic and voice lines;
otherwise use a Contra value, a `SPECIAL_CUSTOM_xx` with `BehaviorEnum`, or add a value to
`SpecialPowerType.h` and the matching string to `SpecialPowerMaskType::s_bitNameList` in
`SpecialPower.cpp`.
2. Give a new engine value a case in `getFallbackBehaviorType`, or rely on `BehaviorEnum` in data.
3. Add the nine `Eva*` fields and matching `EvaEvent` blocks if the power should announce.
4. Test placement (water, cliff, shroud), the shortcut bar, and each Eva line as own, ally and
enemy.
