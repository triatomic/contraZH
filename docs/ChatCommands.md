# Chat Commands

Added a system for user-defined "chat commands" - cheat/debug-style commands that the local player can trigger by typing them into the in-game chat window, prefixed with a slash (`/money`). They are defined in an optional `ChatCommands.ini` file.

## Enabling the chat window in Singleplayer

Chat commands work in **singleplayer and skirmish** games only. By default the chat window cannot be opened there, so you need to enable it in `GameData.ini`:

* `EnableSingleplayerChatwindow = Yes`

With this set, the in-game chat window can be opened (Enter key) in singleplayer/skirmish. (See [GameData](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/GameData#singleplayer-chat-window).)

## Usage

Open the chat window and type the command name prefixed with a slash, for example `/money`. The first word of the message, with the slash removed, is matched against the defined command names. If it matches a `ChatCommand` block, the command is executed for the local player and the message is not sent as chat. A message that does not start with a slash is always sent as ordinary chat.

The name is matched case-insensitively, so `/MONEY` works as well as `/money`. The slash is only how the message is recognised as a command - it is not part of the name in `ChatCommands.ini`.

## ChatCommands.ini

Each command is defined as a `ChatCommand <name> ... End` block. The `<name>` is what the player types to trigger it. All attributes are optional - a command can do one or several things at once.

* `AddMoney = 0` - (Signed amount of credits to add to the player. Default = 0)
* `AddRank = 0` - (Number of ranks (general's promotions) to grant, capped at the max rank. Default = 0)
* `ReadyTimers = No` - (If Yes, set all of the player's special power timers to ready.)
* `SpawnObjectAtCursor = <ObjectTemplate>` - (Spawn the given object template for the local player at the mouse cursor. The player can type an object name after the command to spawn that instead, in which case the name here is only the default. The name may be left blank to always require a typed one. See [Spawning a typed object](#spawning-a-typed-object).)
* `TogglePrerequisites = No` - (If Yes, toggles ignoring unit/building build prerequisites. Science requirements still apply.)
* `ToggleInfiniteEnergy = No` - (If Yes, toggles infinite power for the local player.)
* `GrantAllUpgrades = No` - (If Yes, grants the local player all player-type upgrades.)
* `AddVeterancyLevel = 0` - (Acts on the selection. Promote the selected units by this many veterancy levels; negative demotes. Capped to the valid range.)
* `AddSalvageTier = 0` - (Acts on the selection. Change the selected salvagers' crate-upgrade tier by this much; negative removes. Capped 0..2.)
* `ProductionSpeedMultiplier = 0.0` - (Build-speed multiplier for the local player; > 1 builds faster. 0 means the field is absent / no change.)
* `SetSelectedOwner = <who>` - (Acts on the selection. Give the selected objects to another player. One of `ENEMIES`, `NEUTRAL`, `ALLIES` or `SELF`. See [Changing who owns the selection](#changing-who-owns-the-selection).)
* `AddHealth = 100000` - (Acts on the selection. Add this much health, raising the maximum as well as the current health; a negative amount damages instead. The value may be left blank to use the default. See [Adding health](#adding-health).)
* `KillSelected = No` - (Acts on the selection. If Yes, kill the selected objects outright.)
* `KillSelectedPilots = No` - (Acts on the selection. If Yes, kill the crew of the selected vehicles, leaving them unmanned. See [Sniping the crew](#sniping-the-crew).)
* `ControlPlayer = No` - (If Yes, take control of the player named after the command. See [Controlling another player](#controlling-another-player).)
* `SubdueSelected = No` - (Acts on the selection. If Yes, disable the selected objects the way an EMP does, for as long as their own data allows. See [Disabling with an EMP](#disabling-with-an-emp).)

## Adding health

`AddHealth` raises the maximum health of everything selected and adds the same amount to their
current health, so the units come out tougher rather than merely repaired. Leaving the amount blank
uses the default, which is past any unit's own maximum and so makes the selection unkillable:

```
ChatCommand hp
  AddHealth =
End
```

A negative amount damages the selection instead, as ordinary unresistable damage, so an amount
larger than what the object has left kills it properly - with its death animation, explosion and
score - rather than parking it at zero health:

```
ChatCommand hurt
  AddHealth = -1000
End
```

Anything without a body, such as a piece of terrain scenery, is skipped.

## Controlling another player

`ControlPlayer` hands your own controls to another player, so their base and units answer to you and
the view, shroud, radar and command bar all follow. It is the quickest way to check what an opponent
is building, or to play both sides of a test.

```
ChatCommand control
  ControlPlayer = Yes
End
```

The player is named after the command, either by number - counting the playable players from 1, as
the score screen lists them - or by name:

```
/control 2
/control PlyrChina
```

`/control` on its own reports that it needs a name, and an unknown name says so. The neutral player
that owns the map's civilian objects cannot be controlled. Switch back the same way, with your own
number.

## Disabling with an EMP

`SubdueSelected` fills the selected objects' subdual damage, which is what an EMP weapon does, so
they sit disabled and powerless until it wears off.

```
ChatCommand emp
  SubdueSelected = Yes
End
```

How long that lasts is the object's own business: subdual damage drains at the rate its data sets,
and the amount is capped by its `SubdualDamageCap`, so filling the pool holds it for as long as that
data allows. An object with no subdual settings at all cannot be disabled this way and is left
alone - this is a property of the unit, not of the command.

The damage is dealt as unresistable, so armor cannot shrug the pulse off the way it can reduce the
vehicle and building subdual types.

## Sniping the crew

`KillSelectedPilots` deals the same damage as Jarmen Kell's shot, so the selected vehicles lose
their crew and are left unmanned for anyone to take over.

```
ChatCommand decrew
  KillSelectedPilots = Yes
End
```

An unmanned vehicle turns grey, is disabled and passes to the neutral player, exactly as one sniped
in play. A combat bike is scuttled instead, since it carries a rider rather than a crew - and if it
is moving at the time it simply explodes. Anything that is not a vehicle is left alone, so mixed
selections are safe.

## Changing who owns the selection

`SetSelectedOwner` hands whatever is currently selected to another player, which is the quickest way
to get a unit onto the other side and watch it fight, or to take a captured one back.

```
ChatCommand giveenemy
  SetSelectedOwner = ENEMIES
End
```

`ENEMIES` and `ALLIES` pick the first player holding that relationship to you, so on a map with
several opponents the objects always go to the same one. `NEUTRAL` gives them to the neutral player,
which is how civilian structures are owned. `SELF` gives them back to you.

The selection is cleared as the owner changes, since objects you no longer own cannot stay selected.
Nothing happens if the map has no player with the relationship asked for - a one-on-one skirmish has
no ally to give anything to.

## Spawning a typed object

`SpawnObjectAtCursor` accepts an object name typed after the command, so a single command can
spawn anything instead of needing one command per object:

```
/spawntank                       spawns the object named in the INI
/spawntank AmericaTankCrusader   spawns that object instead
```

The typed name is matched case-insensitively, so `americatankcrusader` also works. If no object of
that name exists, a message says so and nothing is spawned.

A command counts as a spawn command as soon as `SpawnObjectAtCursor` is present, so the value can be
left blank for a command that always expects a typed name:

```
ChatCommand spawn
  SpawnObjectAtCursor =
End
```

That command needs a name: `/spawn AmericaTankCrusader` spawns, while `/spawn` on its own reports what it expects instead.

## Example

```
ChatCommand money
  AddMoney = 10000
End

ChatCommand maxrank
  AddRank = 5
  GrantAllUpgrades = Yes
End

ChatCommand fast
  ProductionSpeedMultiplier = 5.0
  ToggleInfiniteEnergy = Yes
End

ChatCommand spawntank
  SpawnObjectAtCursor = AmericaTankCrusader
End
```

With these defined (and `EnableSingleplayerChatwindow = Yes`), typing `/money` in the chat window grants 10000 credits, etc.

Notes:
* Commands run only for the local player, in singleplayer/skirmish. They are intended for testing/debugging and custom cheat setups.
* The fields marked "acts on the selection" above read the whole selection, including objects the
player does not own, so a unit given away with `SetSelectedOwner` can be selected and handed back.
* `KillSelected` performs the same kill as the hand-of-god debug mode, which reaches it through a
message that only exists in a debug build. The chat command calls it directly, so it works in a
release build and takes the whole selection at once.
