# New Enum Values

## Weapon Slots
New weapon slots were added for 8 slots in total
In addition to PRIMARY, SECONDARY, TERTIARY, you can use
* `WEAPON_FOUR`
* `WEAPON_FIVE`
* `WEAPON_SIX`
* `WEAPON_SEVEN`
* `WEAPON_EIGHT`

For conditionstates (e.g. FIRING_A) you can use the letters D-H for the new types.

For Stealth forbidden conditions, you can use "FIRING_WEAPON_FOUR", etc.

For synced weapons (AutoChooseSources) you can use "SYNC_TO_WEAPON_FOUR", etc.

## DamageTypes

### Generic DamageTypes

(no special logic attached, can be used for any weapon)

* `SONIC`
* `ACID`
* `JET_BOMB`
* `ANTI_TANK_GUN`
* `ANTI_TANK_MISSILE`
* `ANTI_AIR_GUN`
* `ANTI_AIR_MISSILE`
* `ARTILLERY`
* `SEISMIC`
* `RAD_BEAM`
* `TESLA`

### Functional DamageTypes

* `CHRONO_GUN` - Disables units and removes them after a health treshold is reached. Uses parameters from GameData. TODO: detailed tutorial

## DeathTypes

* `CHRONO`

## KindOfs

### Functional KindOfs
These kindofs have some hardcoded functions attached to them and should not be used for general classification.

* `CAN_RETALIATE` - required for DRONE units to bypass hardcoded cannot_retaliate behavior
* `ENABLE_INFANTRY_LIGHTING` - Enables infantry ambient lighting for non-infantry
* `DISABLE_INFANTRY_LIGHTING` - Disables infantry ambient lighting for infantry
* `TELEPORTER` - Used with TeleportAIUpdate module. Required for pathfinding tweaks.
* `SHOW_PROGRESS_BAR` - Allows the object to show progress bar above healthbar (used for EnergyShields)

### Generic KindOfs

New generic kindofs were added. These do not serve any internal purposes beyond classification. You can use them in various combinations for Required or Forbidden kindofs in various modules:

* `NO_BATTLE_PLAN` (note: this still needs to be added as invalidKindof)
* `VTOL`
* `LARGE_AIRCRAFT`
* `MEDIUM_AIRCRAFT`
* `SMALL_AIRCRAFT`
* `ARTILLERY`
* `HEAVY_ARTILLERY`
* `ANTI_AIR`
* `SCOUT`
* `COMMANDO`
* `HEAVY_INFANTRY`
* `SUPERHEAVY_VEHICLE`
* `EXTRA1`
* `EXTRA2`
* `EXTRA3`
* `EXTRA4`
* `EXTRA5`
* `EXTRA6`
* `EXTRA7`
* `EXTRA8`
* `EXTRA9`
* `EXTRA10`
* `EXTRA11`
* `EXTRA12`
* `EXTRA13`
* `EXTRA14`
* `EXTRA15`
* `EXTRA16`

## Locomotor Sets

* `LOCOMOTORSET_VTOL` - This is used for VTOL aircraft for takeoff and landing. This should be a HOVER (=helicopter) locomotor, so a jet with WINGS locomotor can properly land and take off.

## ArmorSetFlags
Added new ArmorSetFlags:

* `PLAYER_UPGRADE2`
* `PLAYER_UPGRADE3`
* `PLAYER_UPGRADE4`

## WeaponSetFlags
Added new WeaponSetFlags that can be manually used for upgrades:

* `PLAYER_UPGRADE2`
* `PLAYER_UPGRADE3`
* `PLAYER_UPGRADE4`
(Note: There will be more in the future)

Added new WeaponSetFlags for specific conditions:

* `GARRISONED` - If a unit has a weaponset with this flag, it will be chosen when the unit is garrisoned inside a structure
* `CONTAINED` - If a unit has a weaponset with this flag, it will be chosen when the unit is contained inside a transport

## ConditionStates
For each new WeaponSetFlag there is a corresponding ConditionState

* `WEAPONSET_PLAYER_UPGRADE2`
* `WEAPONSET_PLAYER_UPGRADE3`
* `WEAPONSET_PLAYER_UPGRADE4`

Conditionstates that are used for VTOL aircraft (i.e. JetAIUpdate with NeedsRunway = No)

* `TAKEOFF`
* `LANDING`

Conditionstate used for TeleporterAIUpdate
* `TELEPORT_RECOVER`

## WeaponBonus types

Added new WeaponBonus types for the following conditions:

* `CONTAINED` - this bonus is applied to passengers in TransportContain (similar to GARRISONED)
* `BATTLEPLAN_BOMBARDMENT_TWO`, `BATTLEPLAN_HOLDTHELINE_TWO`, `BATTLEPLAN_SEARCHANDDESTROY_TWO` -
a second set of battle plan bonus flags. These are not applied by the battle plans themselves; they
are set manually, so a second tier of bonus can be granted on top of the existing
`BATTLEPLAN_*` conditions.

Bonus values can be defined in GameData.ini (no bonus by default). Example:

`WeaponBonus = CONTAINED RANGE 125%`

## Radius Cursor types

The cursor type named on a generic `RadiusCursor <TYPE>` block in GameData.ini. The full list:

`NONE`, `ATTACK_DAMAGE_AREA`, `ATTACK_SCATTER_AREA`, `ATTACK_CONTINUE_AREA`, `GUARD_AREA`

`EMERGENCY_REPAIR`, `FRIENDLY_SPECIALPOWER`, `OFFENSIVE_SPECIALPOWER`

`SUPERWEAPON_SCATTER_AREA`, `PARTICLECANNON`, `A10STRIKE`, `CARPETBOMB`, `DAISYCUTTER`

`PARADROP`, `SPYSATELLITE`, `SPECTREGUNSHIP`, `HELIX_NAPALM_BOMB`, `NUCLEARMISSILE`, `EMPPULSE`

`ARTILLERYBARRAGE`, `NAPALMSTRIKE`, `CLUSTERMINES`, `SCUDSTORM`, `ANTHRAXBOMB`, `AMBUSH`

`RADAR`, `SPYDRONE`, `FRENZY`, `CLEARMINES`, `AMBULANCE`, `IONCANNON`, `CLUSTERMISSILE`

`SUNSTORM`, `METEORSTRIKE`, `PUNISHER`, `CHEMICALMISSILE`, `CARPETBOMB_USA`, `MOAB`

`SUPERSONICSTRIKE`, `HELISUPPORT`, `INTERCEPTORS`, `HOLOGRAMS`, `PARADROP_AIRF`

`COASTALBARRAGE`, `PARADROP_COMMANDOS`, `IONSTRIKE`, `CRYOBOMB`, `FORCEFIELD`, `SPACESHIP`

`ORBITALSTRIKE`, `DROPPODS`, `DROPPODS_SUPER`, `LASERSTRIKE`, `ANTIMATTERBOMB`, `CHRONOAMBUSH`

`CHRONOSPHERE`, `SUBORBITALSTRIKE`, `NANOSWARM`, `SPYPLANE`, `OBSERVATION`, `AIRSTRIKE_NUKE`

`ICBM_NUKE`, `CARPETBOMB_NUKE`, `ARTILLERYBARRAGE_NUKE`, `SUPERHACK`, `SYSTEMHACK`

`CARPETBOMB_NAPALM`, `DRAGONSTAR`, `SPIDERMINES`, `EARTHSHAKER`, `IRONCURTAIN`, `PARADROP_TANK`

`MORTARBARRAGE`, `NAPALMBOMB`, `PARADROP_LARGE`, `DEMOTRAPS`, `FRENZY_GLA`, `GPSSCRAMBLER`

`JUNKREPAIR`, `ROCKETBARRAGE`, `CARPETBOMB_CLUSTER`, `SUICIDEPLANE`, `ARTILLERYBARRAGE_GLA`

`VIRUS`, `CHEMTRAILS`, `AIRSTRIKE_GLA`, `CHEMICALBOMB`, `TOXINDROP`, `JUMPJET`

`ATTACK_DAMAGE_AREA2`, `ATTACK_DAMAGE_AREA3`, `ATTACK_DAMAGE_AREA4`

`JUMPJET` and `ATTACK_DAMAGE_AREA2` / `3` / `4` are new; the extra ATTACK_DAMAGE_AREA slots let a
unit use a different damage-area cursor per weapon.

## Veterancy Levels

Added two extra veterancy levels above HEROIC, for a total of 6 (`REGULAR`, `VETERAN`, `ELITE`, `HEROIC`, `LEVEL_FOUR`, `LEVEL_FIVE`).

* `LEVEL_FOUR`
* `LEVEL_FIVE`

Used by the object veterancy parameters (`MaxVeterancyLevel`, `ExperienceRequired`, `ExperienceValue`, `SkillPointValue`), the [`SetMaxVeterancyLevel`](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Objects-&-Modules#experiencescalarupgrade) upgrade, and the `HealthBonus_Four` / `HealthBonus_Five` GameData parameters.

## Rider Slots (Combat Bike)

Extended `RiderChangeContain` from 8 to 16 rider slots. Corresponding enums were added for the new slots (9-16):

* ArmorSetFlags: `RIDER9` ... `RIDER16`
* WeaponSetFlags: `RIDER9` ... `RIDER16`
* ModelConditions: `RIDER9` ... `RIDER16`
* ObjectStatus: `STATUS_RIDER9` ... `STATUS_RIDER16`

See [RiderChangeContain](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Objects-&-Modules#riderchangecontain).

# New Enum Types

## TintStatus Types
TintStatus is used to define color tint effects for various game mechanics (e.g. Frenzy).
Most entries are unused by default.

The colors for each type can be defined in in GameData.ini. The TintStatus types that are used in vanilla ZH have their default values hardcoded, but they can be overriden in GameData.ini.

Values:
* `DISABLED` - Used for Disabled units
* `IRRADIATED`
* `POISONED`
* `GAINING_SUBDUAL_DAMAGE` - Used for units being disabled
* `FRENZY` - Used for Frenzy
* `SHIELDED`
* `DEMORALIZED`
* `BOOST`
* `EXTRA1`
* `EXTRA2`
* `EXTRA3`
* `EXTRA4`
* `EXTRA5`
* `EXTRA6`
* `EXTRA7`
* `EXTRA8`


