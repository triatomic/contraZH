# ChatCommands.ini

New fields for upstream's [chat commands](ChatCommands.md). Commands still run only in
singleplayer and skirmish, typed with a slash prefix.

## New fields

* `SpawnObjectAtCursor = <ObjectTemplate>` - (Also takes an object name typed after the command, and the value may be left blank. See [Spawning a typed object](#spawning-a-typed-object).)
* `SetSelectedOwner = <who>` - (Acts on the selection. Give the selected objects to another player. One of `ENEMIES`, `NEUTRAL`, `ALLIES` or `SELF`. See [Changing who owns the selection](#changing-who-owns-the-selection).)
* `AddHealth = 100000` - (Acts on the selection. Add this much health, raising the maximum as well as the current health; a negative amount damages instead. The value may be left blank to use the default. See [Adding health](#adding-health).)
* `KillSelected = No` - (Acts on the selection. If Yes, kill the selected objects outright.)
* `KillSelectedPilots = No` - (Acts on the selection. If Yes, kill the crew of the selected vehicles, leaving them unmanned. See [Sniping the crew](#sniping-the-crew).)
* `ControlPlayer = No` - (If Yes, take control of the player named after the command. See [Controlling another player](#controlling-another-player).)
* `SubdueSelected = No` - (Acts on the selection. If Yes, disable the selected objects the way an EMP does. See [Disabling with an EMP](#disabling-with-an-emp).)

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

The player is named after the command, either by number - counting the playable players from 1, in
the order the map defines them - or by name:

```
/control 2
/control PlyrChina
```

The neutral player that owns the map's civilian objects cannot be controlled. Switch back the same
way, with your own number.

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

Notes:
* The fields marked "acts on the selection" above read the whole selection, including objects the
player does not own, so a unit given away with `SetSelectedOwner` can be selected and handed back.
* `KillSelected` performs the same kill as the hand-of-god debug mode, which reaches it through a
message that only exists in a debug build. The chat command calls it directly, so it works in a
release build and takes the whole selection at once.
