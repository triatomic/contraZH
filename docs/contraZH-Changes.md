# contraZH Changes

This page describes changes made in [contraZH](https://github.com/triatomic/contraZH), a fork of
GeneralsGameCode_Modding. Everything here is additional to upstream; the rest of this wiki still
applies unchanged.

Almost all of it is client-side presentation and input handling, read from `Options.ini` and
defaulting to retail behaviour, so an untouched `Options.ini` plays exactly as before. The exception
is [Gameplay Fixes](#gameplay-fixes), which correct retail bugs in the simulation itself and are
always on.

As of August 2026 the fork is synced with TheSuperHackers/GeneralsGameCode and
GeneralsGameCode_Modding again (their `Core/` restructure included). Everything on this page
survived the merge unchanged, with one exception: the texture filter option was superseded by the
version TheSuperHackers landed — see [Rendering](#rendering) for the renamed `AnisotropyLevel` key
and the extended value list.

# Gameplay Fixes

Unlike the rest of this page these are not optional and have no `Options.ini` key. They are all retail
bugs that upstream still carries.

Some of them change the simulation - the timing of a reload or a snipe, the path a drone flies - so
any replay recorded before them will not play back identically. Others only change what is drawn, and
leave replay playback alone. Each entry says which kind it is.

## Weapon bonus no longer restarts the reload

A weapon bonus changing mid-reload used to throw away the progress already made and start the timer
again from zero. Since a bonus can come and go freely - walking in and out of a Propaganda aura, a
horde forming and breaking up, continuous fire ramping up, a promotion landing - a unit could be held
at the start of its reload indefinitely, and with the right timing the reload could be skipped
outright. The China Nuke Cannon was the clearest case: toggling Propaganda on it let it fire well
ahead of schedule.

The reload now keeps the fraction it had already served. A unit halfway through a reload stays
halfway through it; gaining rate of fire shortens what is left and losing it lengthens what is left,
but neither hands back nor takes away time already spent. Toggling a bonus on and off again leaves
the reload exactly where it was, so there is nothing left to exploit.

## Snipe survives a weapon set change

Jarmen Kell's snipe is a wind-up rather than an instant shot, and anything that changed his weapon
set part way through threw that wind-up away. Picking up scrap is the usual way to hit it, since
salvage grants a weapon upgrade, but a veterancy promotion does the same thing, as does mounting or
dismounting the combat bike. The snipe either fired with no wind-up at all or dropped out of the
attack entirely.

The charge now carries over to the replacement weapon and finishes on its original schedule.

Note: a related half of this is data rather than code. A weapon set change also drops the weapon lock
and reverts to the primary weapon unless the set is marked `WeaponLockSharedAcrossSets`, which is
what makes Kell fall out of snipe mode rather than merely lose the charge.

## Stealth kills no longer reveal the killer

With a cash bounty in effect, a kill floats the money earned above the unit that made it. That text
was drawn for everyone, including players who could not see the unit at all, so a stealthed sniper
announced its own position the moment it fired successfully. Infantry shooting from inside a stealthed
transport gave away the transport the same way, since the text appears over whoever scored the kill.

The text is now shown only to players who can legitimately see the killer - its owner, their allies,
and observers watching a replay. Everyone else sees nothing, exactly as they see nothing of the unit
itself. A unit that is stealthed but has been detected still shows it, because at that point it is
visible anyway.

This one is presentation only. The bounty is awarded identically for every player whether or not the
text is drawn, so replays recorded before it still play back identically.

## Drones stay on their leash

A drone flies out to whatever its master is shooting at, and is meant to be pulled back once it
strays too far from the master. The check that does the pulling sits at the end of a list of
priorities, and the "go attack the master's victim" step above it returns before ever reaching it, so
for as long as the master had a victim the leash simply did not exist. A master keeps its last victim
recorded after it stops shooting, so a Humvee that fired once and then drove away left its drone
behind indefinitely, chasing across the map and often parked on top of the enemy still attacking it.

The leash is now checked before the drone commits to the chase, using the same distance it always
meant to use - twice the guard range, from the master. A drone that strays past it breaks off and
comes home, including one already in the middle of an attack, which the old code could not do even
when it tried: an internally issued move order is layered on top of an attack rather than replacing
it, and the attack resumes a few seconds later. The drone is now taken out of the attack properly
before being sent back.

Nothing changes in the ordinary case, where the master shoots at something within its own weapon
range and the drone never approaches the leash. Drones that have no guard range are untouched, as are
Stinger Site stingers, which use the same module but never take the attack path at all.

This one changes the simulation, so replays recorded before it will not play back identically.

Note: a drone set to acquire targets on its own can still pick a fight after being pulled home, since
that is a separate mechanism from following the master's victim. That is a data decision rather than
an engine one.

# Options.ini

These are read once at startup. Changing them needs a restart.

## Display

* `HealthBarDisplayMode = Classic` - (`Classic` | `Damaged` | `Always`. `Damaged` shows a bar only on
hurt objects, `Always` shows one on everything.)
* `NumericalHealth = No` - (Yes prints the hit points beside the health bar. Follows
`HealthBarDisplayMode`, so the number appears exactly where a bar does.)
* `SelectionCircle = No` - (Yes draws a green ring on the ground under selected objects. Retail draws
nothing there; selection is only a brief tint flash on the model.)
* `SmartPips = No` - (Yes keeps ammo and passenger pips on screen instead of showing them only while
the unit is selected or moused over. Own units only. Nothing is drawn when there is nothing to
report, so the pips read as "still loaded" and "carrying someone" at a glance.)
* `BuildTimerDisplayMode = None` - (`None` | `Seconds` | `Auto`. Countdown numbers on build queue and
special power cameos. `Auto` switches to MM:SS past a minute.)
* `NewRadar = No` - (Yes redraws the radar in the style of the RA3 minimap. Unit and structure
blips get a dark outline so they read against any terrain, structures draw a little larger than
units, and the waterline gets a dark contour instead of fading into the land. Ground is flattened
to two tones, an olive for natural terrain and a pale grey for rock and man made surfaces, mixed
where the two meet so edges shade across rather than seam, so the map reads as regions rather than
as a patchwork of texture. The radar grid doubles to 256x256 to
make room for the detail, so the radar is sharper as well. No keeps the retail radar exactly as it
was.)
* `BlipSize = Large` - (`Small` | `Large`. How big the object blips draw. `Small` is 3 pixels for a
unit and 5 for a structure, `Large` is 5 and 7. Larger blips are easier to pick out at a glance but
run together sooner when units are packed in. Ignored unless `NewRadar` is on.)

Note: `NewRadar` cannot hide roads the way the RA3 minimap does. Roads are painted into the terrain
textures themselves rather than drawn as their own radar layer, so by the time the radar samples a
road cell it is indistinguishable from the ground around it. Bridges do still draw in their own
colour, since those come from the bridge list rather than the terrain.

Note: `SelectionCircle` needs a mod-side `PlainRingSelection.tga` — a white or greyscale ring with
alpha, tinted green at runtime. Until it exists the ring simply does not draw.

## Hotkey overlay

Draws each command bar cameo's hotkey letter on the cameo.

* `KeyboardOverlay = No` - (Yes shows the letters.)
* `KeyboardOverlayRed = 255` - (Letter colour, 0-255 per channel.)
* `KeyboardOverlayGreen = 255`
* `KeyboardOverlayBlue = 255`
* `KeyboardOverlayBackdrop = Yes` - (Draw a translucent plate behind the letter.)
* `KeyboardOverlayBackdropRed = 0` - (Backdrop colour, 0-255 per channel.)
* `KeyboardOverlayBackdropGreen = 0`
* `KeyboardOverlayBackdropBlue = 0`
* `KeyboardOverlayBackdropOpacity = 128` - (0-255; 128 is 50%.)

The letter shown is whatever actually got registered in the hotkey manager, not the button's label,
so a colliding hotkey that was dropped at registration is not advertised as working.

## Grid hotkeys

Keys each cameo by **where it sits in the command bar** rather than by the letter its string file
marked with an ampersand, so a key stays in the same place whatever is being built.

* `GridHotkeys = No` - (Yes keys cameos by slot position.)
* `GridHotkeyLayout = QWERTYUIOASDFGHJKL` - (One key per slot, in reading order.)
* `GridHotkeyColumns = 9` - (Command bar width. The command bar numbers its slots down each column
while the layout string is written across each row, so this is needed to map between them. 0
disables remapping.)
* `NonGridHotkeys =` - (Keys to leave out of the grid, e.g. `SGX`. Separators are ignored, so `SGX`,
`S,G,X` and `S G X` are all the same.)

An excluded slot falls back to its string file letter, so the key is freed for the game's own use
while the button still works the way it did before grid hotkeys existed. If that letter is also a
live grid letter the slot gets no hotkey instead, since `addHotKey` keeps whichever slot registered
first and silently drops the other.

## Input

* `CastMode = Normal` - (`Normal` | `QuickCast` | `QuickCastWithIndicator`. `QuickCast` fires a
targeted ability at the cursor on the hotkey press instead of arming it for a second click.
`QuickCastWithIndicator` holds to aim with a decal and fires on release.)

Notes:
* Superweapons, structure placement, rally points and beacons are deliberately excluded — firing one
at an unintended spot cannot be undone.
* A cast requested while the ability is recharging is remembered and fired the moment the logic side
says it is ready, rather than being thrown away. The cooldown itself is untouched: readiness is
asked of `SpecialPowerModule::isReady` every frame rather than predicted, so a queued cast can never
fire earlier than a manual one could.
* Shift+click queues or cancels five units at once, from either the mouse or the hotkey.

## Clipboard paste

`Ctrl + V` pastes the clipboard into any text field. Retail had no paste at all, so a link or a
callout had to be retyped by hand - most keenly felt in the in game chat, opened with Enter, or
Shift+Enter for allies only.

It works in every text entry, not just chat: lobby chat, game and player names, and so on.

Notes:
* Pasted text obeys the same rules as typing. A field that only takes numbers, letters or ASCII
filters the paste the same way, and hidden fields keep showing asterisks.
* Text longer than the space left is truncated to fit, the same way typing stops at the limit.
* A multi-line paste stops at the first line break, so half a pasted paragraph cannot become a
chat message on its own.

## Rendering

The texture filter option shipped here pre-merge was this fork's adaptation of an unreleased
TheSuperHackers branch. The August 2026 upstream merge replaced it with the version TheSuperHackers
landed, which renames one key and extends the values:

* `TextureFilter = Bilinear` - (`None` | `Point` | `Bilinear` | `Trilinear` | `Anisotropic`. The
engine has supported the better modes since retail, but nothing ever called
`WW3D::Set_Texture_Filter`, so they were unreachable. Now also selectable from the in-game options
menu.)
* `AnisotropyLevel = 2` - (`2` | `4` | `8` | `16`. Only used when `TextureFilter = Anisotropic`.
Retail hardcoded this to 2, the lowest anisotropic filtering goes. **Renamed from the pre-merge
`AnisotropicLevel`** — update Options.ini by hand; the old key is silently ignored.)

Notes:
* An unrecognised `TextureFilter` value now falls back to `None` (point sampling) rather than the
default, so a typo shows up as a visibly unfiltered picture instead of being silently absorbed.
`AnisotropyLevel` still rounds down to a valid step.
* The level is clamped to whatever the device reports supporting. DirectX rejects a value above the
cap and silently keeps the previous setting rather than reporting an error, so asking for 16x on
hardware that tops out at 8x degrades cleanly instead of doing nothing.
* Two fork fixes are layered on top of the merged version: the filter mode and anisotropy level are
re-applied after a device reset (alt-tab, fullscreen/windowed toggle) instead of silently reverting
to defaults, and when the driver supports anisotropic filtering for only one of
minification/magnification the other falls back to linear per capability, instead of both dropping
to point sampling as the merged code did.

# ParticleSystem.ini

## ConformToTerrain

* `ConformToTerrain = No` - (Default. `Yes` opts a single effect into terrain conforming.)

A ground aligned particle drawn as a quad can only ever be a flat plane, so a wide one cuts through
a hillside no matter how its corners are placed. With `ConformToTerrain = Yes`, ground aligned,
non-billboarded particles with no volume depth are instead built as a mesh from the terrain's own
heightmap cells, the same way projected decals are, so the particle inherits the ground geometry
exactly.

Notes:
* Conforming is **opt-in per effect**: retail effects render exactly as before unless an INI sets
`ConformToTerrain = Yes`. (It was briefly opt-out; the default flipped to `No` so only effects
checked against the new renderer pay its cost or change appearance.)
* Cost is quadratic in particle size. Past 160 terrain cells per side the mesh samples every Nth cell
instead, so a very large particle stops getting more expensive without bound. The trade is a coarser
terrain fit, which is not visible on the effects that actually reach that size.

# RiderChangeContain

Two opt-in fields on the combat bike contain module. Both default to `No`, so data that does not
mention them behaves exactly as before.

## SurviveScuttle

* `SurviveScuttle = No` - (Default. `Yes` keeps the transport alive after its rider dismounts.)

Normally a bike whose rider gets off topples over, waits out `ScuttleDelay` and is destroyed. With
`SurviveScuttle = Yes` it still topples into its `ScuttleStatus` pose, but is never killed - it just
lies there. Walking a new valid rider onto it stands it back up: the toppled model condition and the
unselectable and immobile statuses are cleared, and the bike behaves normally again.

Notes:
* The abandoned bike cannot be box-selected, but it can still be right-clicked as a destination, so
ordering a rider onto it works. It is only unselectable, not uninteractable.
* It stays a legitimate target - enemies can shoot and destroy it. Only its owner can mount it.
* `ScuttleDelay` becomes dead config, since the timer it feeds is never started.

## SilentScuttle

* `SilentScuttle = No` - (Default. `Yes` scuttles the bike without announcing a loss.)

A dismount is a deliberate act, but the scuttle still killed the bike as an anonymous death, so EVA
called out "Unit Lost", dropped a radar ping and could raise an "under attack" warning every time a
rider got off. With `SilentScuttle = Yes` the bike is flagged as scuttling for the moment it dies,
and the three announcements skip an object carrying that flag. A bike destroyed by an enemy still
announces normally.

The flag only silences the announcements. The bike is still not credited to anyone as a kill and is
not added to either player’s score, exactly as an ordinary scuttle behaves today.

The two fields are independent. `SurviveScuttle = Yes` never reaches the kill at all, so it does not
need `SilentScuttle`.

# New CommandButton Commands

## HOLD_FIRE

Suppresses **automatic target acquisition only**. A holding unit still fires when the player
explicitly orders an attack. Covers the unit, its addon and sub-turrets, and any infantry contained
inside it.

```
CommandButton Command_HoldFire
  Command       = HOLD_FIRE
  Options       = CHECK_LIKE     ; required, or the button never renders as toggled on
  TextLabel     = CONTROLBAR:HoldFire
  ButtonImage   = SNHoldFire
  DescriptLabel = CONTROLBAR:TooltipHoldFire
End
```

Per-object parameter:
* `HoldFireAllowsRetaliation = Yes` - (Default. `No` stops the unit returning fire even when attacked.)

Note: the flag lives on `AIUpdateInterface`, so garrisoned buildings cannot hold fire — most have no
AI module. Infantry inside a *unit* are covered.

## AUTO_FILL

Selects nearby infantry and orders them to board the selected container.

```
CommandButton Command_AutoFill
  Command       = AUTO_FILL
  Options       = OK_FOR_MULTI_SELECT
  TextLabel     = CONTROLBAR:AutoFill
  ButtonImage   = SNAutoFill
  DescriptLabel = CONTROLBAR:TooltipAutoFill
End
```

Only infantry that are not already contained and not already members of the group are considered,
searched nearest-first per container. What they are currently doing does not matter - infantry on
the move or in a fight break off and board, the same as if the order had been given by hand.

## Queue reorder

Off by default; a mod enables it with `QueueReorder = Yes` in the `GameData` block of
GameData.ini. With it off, clicking the queue behaves exactly like retail, Ctrl held or
not.

Ctrl+clicking any cameo in the build queue - unit or upgrade - moves it one position
earlier, swapping it with the entry directly before it. The displaced entry loses the
build time spent on it and starts over when it reaches the front again; already produced
units of a quantity batch stay produced. Ctrl+click on the first entry does nothing, and
a finished unit that is only waiting to exit the factory cannot be displaced. A plain
click still cancels the entry, and Shift+click still cancels a batch of units.

# Drag Selection

## EasyMilitaryDrag

* `EasyMilitaryDrag = No` - (Yes leaves builders out of a drag selection, so boxing over a base picks
up the army without dragging workers along.)

Covers `KINDOF_DOZER` and `KINDOF_IGNORES_SELECT_ALL`, the same kinds Select All already
disqualifies.

Notes:
* Holding Ctrl while dragging inverts it, selecting **only** the builders.
* If a drag would otherwise select nothing but structures, the filter is dropped for that drag and
everything under the box is selected, so dragging over a group of workers still works.

# Debug and Cheat Features

These require a build made with `RTS_DEBUG_CHEATS=ON`. They are compiled out of a normal release
build entirely.

## Debug name overlays

Overlays that draw names above every object on screen, selected or not, including props and
wreckage that never get a health bar.

* `Ctrl + [` - (Cycles three ways: off, the object's template (INI) name in white, then the
model's sub object names in green as well.)
* `Ctrl + ]` - (Particle systems running on that object, in blue, with the FXList that
spawned them in amber)
* `Ctrl + '` - (The `CommandSet` the object uses, in yellow)
* `Ctrl + ;` - (The weapons the object is armed with, in red, under the command set)
* `Ctrl + /` - (The `Armor` the object currently uses, in light blue, under the weapons)

The sub object list is what the W3D model is actually built from - hull, turret, wheels,
housecolor and so on - so it is useful for finding the name to use in `ShowSubObject` or
`HideSubObject`. Up to 16 are listed, with a trailing "and N more" when the model has more.

The command set is read from the object rather than its template, so a unit whose buttons were
swapped at runtime shows the set it is actually using, which is not always the one its template
names. Objects with no command set at all show `<none>`. It works on its own, and with the object
name overlay also on the two are drawn side by side.

The weapon overlay lists one line per occupied weapon slot, written the way the INI writes it
inside a `WeaponSet` block - `PRIMARY NapalmMissileWeapon` for a `Weapon = PRIMARY
NapalmMissileWeapon` line. Which `WeaponSet` block is live depends on the conditions the object
currently matches (veterancy, player upgrades, rider slot), so reading the weapons back is the
direct way to see which block the engine actually picked. Unarmed objects show `<no weapons>`.

The armor overlay shows the `Armor` line of whichever `ArmorSet` block the object currently
matches, the same idea one level down: which block is live depends on the conditions it meets
(veterancy, player upgrades, second life), so reading the armor back is the direct way to see
which one the engine picked. Objects with no armor, normal for props and rubble, show
`<no armor>`.

Also bindable in `CommandMap.ini` as `CHEAT_SHOW_OBJECT_NAME`, `CHEAT_SHOW_PARTICLE_NAMES`,
`CHEAT_SHOW_COMMAND_SET`, `CHEAT_SHOW_WEAPON_SET` and `CHEAT_SHOW_ARMOR_SET`.

* `ParticleNameLingerMS = 0` - (Options.ini. Milliseconds a particle name stays on screen after its
system has gone. 0 or absent shows names only while the system is alive.)

Notes:
* Many effects are one-shot bursts that die within a frame or two, so without a linger their names
flash past unreadably.
* The particle overlay scans every live particle system once per drawn object, every frame, so it is
best switched on only while looking for something.

## Other cheat hotkeys

* ``Ctrl + ` `` - (Instant build, +999999 credits, this general's own sciences, max rank,
reveals the map)
* `Ctrl + \` - (Toggles rendering off and on; the simulation keeps running)
* `Shift + Ctrl + Z` - (Toggles the camera zoom limit)
* ``Shift + Ctrl + ` `` - (Cycles `HealthBarDisplayMode` live)

Notes:
* The health bar cycle is **not** a cheat and works in a normal release build too.
* The combined cheat's second press resets rank, which calls `resetRank()` and wipes purchased
sciences — it returns you to a fresh general, not to what you had before.
* Sciences granted are only this general's own tree, walked from the three purchase command sets the
player template names, not every science in the game.

# Generals Online (experimental)

The engine can be built with the online system from
[Generals Online](https://github.com/GeneralsOnlineDevelopmentTeam/GameClient), the
community replacement for GameSpy: modern login, lobbies, matchmaking and stats over a
REST + WebSocket backend, with peer to peer play over Valve's GameNetworkingSockets
(ICE with STUN/TURN fallback) instead of the retail NAT negotiation.

This is a build-time option, off by default. A normal build is completely unchanged -
every ported line is compiled out. Building with `-DRTS_BUILD_GENERALS_ONLINE=ON`
replaces the GameSpy online path: the Online button runs the Generals Online version
check and login, and the WOL screens become their Generals Online counterparts. LAN and
skirmish are untouched either way.

Notes:
* The client needs a Generals Online backend to actually play online; without one it
fails gracefully at login and returns to the main menu. Connecting to the official
`playgenerals.online` service with a Contra client is subject to coordination with the
Generals Online team.
* Crash reporting, anti-cheat and hardware fingerprinting from upstream are compiled
out by default in this fork.
* The port is documented in detail in `PORT_NOTES.md` next to the GeneralsOnline
sources, including every deliberate deviation from upstream.

## Runtime requirements

A build made with `RTS_BUILD_GENERALS_ONLINE=ON` links against several libraries that
are not part of the game. Their DLLs must sit **next to the executable**, or the game
dies at startup before it draws anything - usually with a bare "the application was
unable to start correctly", because the failure happens in the loader rather than in
game code.

The build copies them automatically as a post build step, so a build directory is
always complete. The list matters when you copy an executable somewhere by hand:

| File | Provides |
|---|---|
| `GameNetworkingSockets.dll` | Valve GameNetworkingSockets - the peer to peer transport |
| `libprotobuf.dll` | Protocol Buffers, used by GameNetworkingSockets |
| `abseil_dll.dll` | Abseil, used by Protocol Buffers |
| `libcrypto-3.dll`, `libssl-3.dll` | OpenSSL 3 - TLS for the service and DTLS for peer traffic |
| `libcurl.dll` | HTTP and the WebSocket client |
| `zlib1.dll` | compression, used by libcurl |
| `discord-rpc.dll` | Discord rich presence (optional at runtime, but the import is not) |

All seven are 32 bit, and they are versions of each other: `libprotobuf.dll` and
`abseil_dll.dll` in particular have to come from the same set. Mixing a protobuf with a
mismatched abseil corrupts the heap inside a static initializer and the game dies
before `WinMain`, which looks nothing like a version problem. If you replace one of
these, replace all of them together.

Note also that some Generals Online installs ship a **64 bit** `zlib1.dll` for their
own tooling. Copying that one into the game folder breaks `libcurl` with an invalid
image error - the game needs the 32 bit one.

Nothing here is required by a normal Contra build, which has no online stack compiled
in and no extra dependencies.
