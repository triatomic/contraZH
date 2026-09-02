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
* `AddVeterancyLevel = 0` - (Promote the selected units by this many veterancy levels; negative demotes. Capped to the valid range.)
* `AddSalvageTier = 0` - (Change the selected salvagers' crate-upgrade tier by this much; negative removes. Capped 0..2.)
* `ProductionSpeedMultiplier = 0.0` - (Build-speed multiplier for the local player; > 1 builds faster. 0 means the field is absent / no change.)
* `SetSelectedOwner = <who>` - (Give the selected objects to another player. One of `ENEMIES`, `NEUTRAL`, `ALLIES` or `SELF`. See [Changing who owns the selection](#changing-who-owns-the-selection).)
* `AddHealth = 100000` - (Add this much health to the selected objects, raising their maximum as well as their current health; a negative amount damages them instead. The value may be left blank to use the default. See [Adding health](#adding-health).)
* `KillSelected = No` - (If Yes, kill the selected objects outright.)

## Adding health

`AddHealth` raises the maximum health of everything selected and adds the same amount to their
current health, so the units come out tougher rather than merely repaired.

```
ChatCommand hp
  AddHealth =
End
```

The amount may be left blank, which uses the default of 100000 - far above any unit's own maximum,
so the selection becomes effectively unkillable. Give it a smaller number for a realistic buff:

```
ChatCommand hp1000
  AddHealth = 1000
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
* `AddVeterancyLevel`, `AddSalvageTier`, `SetSelectedOwner`, `AddHealth` and `KillSelected` act on the currently selected units.
* `KillSelected` is the same kill the `DEMO_TOGGLE_HAND_OF_GOD_MODE` debug key performs, but it needs no debug build and takes the whole selection at once.
