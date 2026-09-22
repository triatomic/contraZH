# contraZH Changes

This page describes changes made in [contraZH](https://github.com/triatomic/contraZH), a fork of
GeneralsGameCode_Modding. Everything here is additional to upstream; the rest of this wiki still
applies unchanged.

Almost all of it is client-side presentation and input handling, read from `Options.ini` and
defaulting to retail behaviour, so an untouched `Options.ini` plays exactly as before. Two things sit
outside that: [Gameplay Fixes](#gameplay-fixes), which correct retail bugs in the simulation itself
and are always on, and a small number of simulation rules read from the mod's own `GameData.ini`,
each of which states its default where it is described.

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

## Guard mode holds its ground

Guarding units used to be interrupted by any hit they took. Being shot while returning to the guard
post, or while attacking inside the guard circle, kicked the unit into chasing the attacker, so the
move and the fire fought each other and the unit stuttered. That transition is gone; a guarding unit
finishes its return, or its current attack, and picks up the attacker through its normal enemy scan.

Units that deploy to fire, the Nuke Cannon above all, had it worse, because every one of those
interruptions packed them up. Three more cases now leave them deployed. When a target dies and
another enemy is still inside the guard circle, the unit fires at the next one without leaving its
attack state. A guard order at the spot the unit already stands on no longer issues a zero-length
move that would pack it up. And a unit still unpacking when its target walks out of range reverses
the unpack instead of finishing it. A manually deployed unit keeps its stance in all of these.

This changes the simulation. Ported from CookieLandProjects/CLP_AI.

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

## USE_OWNER_OBJECT fires from the owner

An `OCLSpecialPower` with `CreateLocation = USE_OWNER_OBJECT` is meant to make the object that owns
the power carry out the delivery itself, with no new transport spawned. That is how vanilla Generals
behaves and what the retail INI comments describe. Zero Hour instead spawned a fresh transport next
to the owner and left the owner idle, because a `Real angle` overload added to the OCL entry point
swallowed the "do not create the owner" flag as a lifetime of zero frames.

The owner now performs the delivery, as in Generals. Only powers that set `USE_OWNER_OBJECT`, which
are scripted or mod-defined, are affected; every other `CreateLocation` is untouched. This changes the
simulation for those powers, so replays that rely on them will not play back identically.

## More Generals Challenge personas

ChallengeMode.ini stopped at twelve personas, `GeneralPersona0` through `GeneralPersona11`. A
thirteenth was quietly dropped by the parser, and because the block was abandoned at that point every
persona after it was lost too. The ceiling is now twenty four.

The menu draws a persona on the button named `GeneralPosition<N>` in ChallengeMenu.wnd, so a new
persona needs a matching control added to that layout to be visible. A slot the layout has no button
for is now skipped instead of crashing the game on entering the Challenge menu, so the code and the
layout can be updated independently.

Notes:
* Raising the ceiling does not add any generals by itself. The personas and the buttons are both data.
* The twelve retail personas are untouched, and a layout that still carries exactly twelve buttons
behaves as it always did.

## Dying infantry do not block or catch clicks

A soldier in its death fall stayed on the pathfind grid until its `SlowDeathBehavior` sink began,
so for a few seconds it was a solid obstacle to any vehicle that could not crush it, such as radar
vans and drones. A dying soldier now leaves the grid the moment it dies and no longer blocks a
mover. Crushers still shove corpses aside as before, and dying vehicles still block until they turn
into hulks.

A dying unit also kept catching the mouse, so a click on a corpse never reached the unit or ground
behind it. Anything effectively dead now drops its pick bit, unless it is `ALWAYS_SELECTABLE`.

## Jammed units deselect properly

A jam weapon left its target in the selection list. The unit appeared selected but the control bar did
not respond, forcing the player to click elsewhere and reselect. Jamming now deselects the unit as it
lands.

Only jamming does this. `UNSELECTABLE` on its own blocks a new selection click, which is its job, but
it no longer empties the player's current selection - a slaved drone, a docking unit or a building you
have just sold used to take the whole selection with it.

A jammed unit also always gets control back. Whether the jam had worn off was worked out from the
unit's current max health, so anything that moved that number - a promotion, a health upgrade - moved
the threshold out from under the unit and left it jammed with nothing left to heal. The jam state is
now tracked directly, and a unit that stops being jammable heals off a jam it already has instead of
keeping it forever.

This changes selection state, so it affects replays.

## Portable addons no longer block building

Placing a building over your own units normally shoves them out of the footprint, but a carrier
with a `PORTABLE_STRUCTURE` addon - an Overlord with a Gattling Cannon, or any of the mod's
multi-addon vehicles - refused the placement outright. The addon stays a live collision object
riding on its carrier, and the build check treated it as a held, immovable unit rather than as
part of the vehicle that would have driven off. Contained objects are now skipped by that check
and only their carrier is judged, so an enemy or otherwise stuck carrier still blocks as before.
This changes build legality, so it affects replays.

## Units chasing a moving target now shoot it

Ordering a fast unit onto a slower moving one used to produce a twitch instead of an attack. The
unit drove up, and on the frame it tried to aim, the target had drifted a fraction past the edge of
its range, so it gave up and drove up again. Against anything that kept moving it never got a shot
off and simply died under return fire - Cyber Shredders, terrorist bikers and Demolishers were the
usual victims. Players worked around it by issuing a move order and then attacking again.

Underneath it the attacker was driving to the wrong place. Weapon range is measured between the
edges of two units, but the spot a unit walked to while closing was measured from its target's
centre and ignored how wide either of them was. The gap between those two numbers is roughly the
attacker's own radius, so a unit arrived at a position it had itself judged to be in range, was told
it was not, and set off again. The bigger the attacker and the smaller its target the worse it got,
which is why a tank chasing infantry was the clearest case. Both now measure the same way.

On top of that, a unit that has already closed tolerates a small amount of drift, about one
pathfinding cell, before deciding the target has escaped. Deciding whether to *start* an attack
still uses the exact weapon range, so nothing gains reach, and the shot itself allows the same
slack - otherwise a unit committed to a shot that was then silently discarded, leaving it locked on
a target it never damaged. When a unit does fall behind it also stops as soon as a shot opens up,
rather than walking out a firing position its target has already left.

Units without turrets gain the most, since they have to stop and turn the hull to fire at all. This
changes when units fire, so it affects replays.

# Game Setup

## Random army per faction

The army list in Skirmish, LAN and online game setup gains one `Random <faction>` entry per base
faction, listed right after `Random`: with the retail generals that is `Random USA`, `Random China`
and `Random GLA`. Picking one starts the game as a random general of that faction only, for a human
slot or an AI slot alike. Which generals belong to a faction comes from the `BaseSide` line of their
`PlayerTemplate.ini` entry, so a mod that adds a faction gets its own entry without a code change.

Notes:
* The labels are string table lookups named `GUI:Random<BaseSide>` (`GUI:RandomUSA`,
`GUI:RandomChina`, `GUI:RandomGLA`). A missing key falls back to `Random <BaseSide>`.
* Limit Armies still applies: the random pick only considers generals the checkbox allows, and an
entry disappears when none of its generals are allowed.
* The choice travels in the game options like plain `Random` does. All players in a LAN or online
game need this build, and a replay made with one of these entries needs it too.

## Host camera height

The LAN game setup gains a `Max Camera Height` checkbox and number field for the host. When checked,
every player in that game zooms out to that height (210 to 1000) instead of `GameData.ini`'s
`MaxCameraHeight`, and personal Options settings are ignored. The value travels in the game options
like starting cash, so clients need this build. Generals Online keeps its `/maxcameraheight` lobby
command; a lobby left at GO's default (310) plays the mod's own limit. Replays keep the limit they were
played with. The `Shift + Ctrl + Z` zoom-limit cheat is unaffected.

# Options.ini

These are read at startup. Most of them can also be changed in game from Options, with the
`Game Options` button in the title bar, and take effect when you press Accept. `NewRadar` and
`BlipSize` still need a restart.

## Camera

* `UseCustomMaxCameraHeight = No` - (`Yes` lets `MaxCameraHeight` below replace `GameData.ini`'s
`MaxCameraHeight`, 670 in Contra. Also editable in Options as `Max Camera Height`, where it applies
without a restart.)
* `MaxCameraHeight = 670` - (210 to 1000. Used in single player, skirmish and campaign. LAN and
online games with two or more humans use the host's limit, or `GameData.ini`'s when the host set
none.)

## Display

* `HealthBarDisplayMode = Classic` - (`Classic` | `Damaged` | `Always`. `Damaged` shows a bar only on
hurt objects, `Always` shows one on everything.)
* `AlliedDecalMode = House` - (`Hidden` | `House` | `Army`. Draws the ground decal of an ally's
general power (nuke, scud storm, carpet bomb, gunship, paradrop and the like) once it fires, so allies
see each other's targeting. `House` tints it in the ally's player color, `Army` in the faction color
of the general they picked, `Hidden` keeps retail behavior where only your own decals show. Enemy
decals stay hidden. Applies to the next power fired. The Game Options combo needs `ComboBoxAlliedDecals`
and `AlliedDecalsLabel` windows in `OptionsMenu.wnd`; without them the key still works from the file.)
* `NumericalHealth = No` - (Yes prints the hit points beside the health bar. Follows
`HealthBarDisplayMode`, so the number appears exactly where a bar does.)
* `SelectionCircle = No` - (Yes draws a green ring on the ground under selected objects. Retail draws
nothing there; selection is only a brief tint flash on the model.)
* `DefensesRangeCircle = No` - (Yes draws a ring showing the attack range of an armed structure while
you are positioning it, so you can see what a defense covers before committing to the spot. Any
structure with a weapon, not only base defenses. The ring is the widest reach of any weapon the
structure can ever field, including the ones it only gets from an upgrade. Walls and other line built
pieces each get their own ring.)
* `ObjectDecals = Yes` - (No suppresses the ground decals objects ask for with `DisplayDecal`.
On by default, since a template only gets one when it asks. Independent of the 2D and 3D shadow
settings, because the decal is an aura marker rather than a shadow.)
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
* `BorderlessWindow = No` - (Yes runs the game in a frameless window at the selected resolution,
centred on the monitor, so it covers the screen when the resolution matches the desktop. Toggled
by the Borderless checkbox beside the Resolution label in the Options menu and applied on Accept,
like a resolution change. `-win` is unaffected and still gives a captioned window.)

Note: a blue bar under the health bar shows the progress of the unit or upgrade at the head of a
building's production queue. It appears while the building is selected or moused over, or all the
time when `HealthBarDisplayMode = Always`. Own buildings only.

Note: the same blue bar on a supply truck, worker or Chinook shows how full it is, from the
supplies it carries against the most it can hold. It appears whenever the gatherer is carrying
something, under the same selection and ownership rules as the production bar.

Note: `NewRadar` cannot hide roads the way the RA3 minimap does. Roads are painted into the terrain
textures themselves rather than drawn as their own radar layer, so by the time the radar samples a
road cell it is indistinguishable from the ground around it. Bridges do still draw in their own
colour, since those come from the bridge list rather than the terrain.

Note: `SelectionCircle` and `DefensesRangeCircle` need a mod-side `PlainRingSelection.tga` — a white
or greyscale ring with alpha, tinted at runtime. Until it exists the ring simply does not draw.

Note: the `DefensesRangeCircle` checkbox needs a `CheckDefensesRangeCircle` window in
`OptionsMenu.wnd`; without it the key still works from the file.

Note: `DisplayDecal` likewise needs a mod-side `.tga`, named by `DecalTexture`. There is no default
texture for it, so an object that asks for a decal without naming one simply does not draw it.

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

## Reverse move hotkey

Caps Lock arms a reverse move for the whole selection, whatever their command sets hold. The next
terrain click sends the same order as the `REVERSE_MOVE` command button, so the unit drives to the
spot in reverse. Pressing Caps Lock again, or arming attack move, drops the mode.

* `TOGGLE_REVERSEMOVE` in CommandMap.ini rebinds it. The Caps Lock default only fills an empty slot.
* `KEY_CAPS` is new as a CommandMap key name. The press still toggles the Caps Lock state.
* While armed, the cursor shows the `ReverseMove` block from Mouse.ini, e.g.
`MouseCursor ReverseMove` with `Image = SCCMove` and `Texture = SCCMove`. A Mouse.ini without that
block keeps the plain move cursor.

The order needs a locomotor with `CanMoveBackwards = Yes`, and `ReverseMoveIgnoreAngleThreshold`
in GameData decides whether a goal in front of the unit is also driven to in reverse.

## Smart selection

Shows a row of small cameos, three fifths of a command button, above the command bar. A selection
of different unit types gets one cameo per type with a count of how many are selected; a selection
of a single type gets one cameo per object. A cameo standing for one object shows a small health
bar instead of a count, and a light orange clip bar above it when the object shows ammo pips, split
into one segment per shot. A cameo also shows the veterancy chevron, as the portrait does; a cameo
standing for a whole type shows the highest rank among the units of that type in the selection. The
row holds 16 cameos; anything beyond that gets none. A count past 999 gets no badge. Objects that
are reskins of one type share that type's cameo.

* `SmartSelection = Yes` - (No hides the row and unbinds its keys.)
* `SmartSelectionUseMouse = Yes` - (Yes keeps only a cameo's units on double click, No on
Ctrl+Shift+click.)

* Left click a cameo to show its command set in the bar. A cameo that stands for a whole type
shows the type's set, and every cameo of that type pushes in. A cameo that stands for one object
drives the bar as if that object alone were selected, so a transport shows its passengers and a
factory its queue, and only its cameo pushes in. The whole group stays selected, so orders given on
the map still go to everyone, but a command off the bar goes to the focused object, or the focused
type, alone. Plain orders are the exception and always go to everyone, even when armed off the bar:
move, attack move, guard, stop, scatter, cheer, formation, waypoints, enter, dock, repair and heal.
A building placed from a focused dozer is built by it, and the other selected dozers go to help.
* Right click the pushed in cameo to go back to the group's common commands. Right click any
other cameo to drop its unit, or its whole type, from the selection.
* Double click a cameo (or Ctrl+Shift+click it with `SmartSelectionUseMouse = No`) to keep only its
unit, or its whole type, and drop everything else.
* Tab and Shift+Tab (`SMART_SELECTION_NEXT_TYPE` / `SMART_SELECTION_PREV_TYPE` in
CommandMap.ini) step the focus through the row, skipping cameos that are already pushed in.

### Command group row

A second row of small cameos sits on the command bar frame, one per command group (Ctrl+1
to Ctrl+0) that still has live members, in key order 1 to 9 then 0. Each cameo shows the group's
most common unit type, the group number top left and the live member count bottom right; the
count drops as members die. The row shows whenever any group has members, even with nothing
selected, and the smart selection row lifts one row above it while both are shown.

* `SmartCommandGroup = Yes` - (No hides the row. `SmartSelection = No` hides it too.)

* Left click a cameo to select its group, as the number key does.
* Double click a cameo to also center the camera on the group, as double tapping the number key
does.

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
* Shift+click queues five units at once, from either the mouse or the hotkey. Shift+click on a
queue entry cancels every queued unit of that type in the factory. Shift+click on a passenger
cameo unloads every passenger of that type from the transport.

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

## Command line

* `-loadreplay <file>` starts the game straight into a replay with the full game context, so the
menus and Options are loaded as usual. `<file>` is a name inside the Replays folder or an absolute
path. A replay that cannot be read, or whose map is not installed, shows a message box in the
shell instead of starting a broken game.
* `-loadsave <file>` likewise accepts an absolute path as well as a name inside the Save folder,
and a save that cannot be read shows a message box instead of failing silently.

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

### Bloom

A soft glow around additive particle effects (fire, muzzle flashes, tracers, lasers, explosions)
and around model meshes whose W3D material uses the `Add` blend mode, such as building lights and
glowing panels. Smoke and other alpha blended effects do not glow. Built from DX8 render targets and
fixed function blur passes, so it needs no shader support.

* `Bloom = No` - (Yes turns the glow on. Also the `Glow around additive effects` checkbox in
Game Options, where it applies on Accept without a restart.)
* `BloomStrength = 0.5` - (0 to 1. How bright the glow is. 0 is the same as off. Edited as a
percentage, `Strength %`, in Game Options.)
* `BloomDebug = No` - (Yes replaces the scene with the blurred glow buffer on black, full strength.
Only the additive effects and meshes that feed the bloom show up, so it tells at a glance whether
the effect is running and what is feeding it. The UI still draws on top. `Debug view` in Game
Options.)

The Game Options controls need the `BloomGroupLabel`, `CheckBloom`, `TextEntryBloomStrength` and
`CheckBloomDebug` windows in `OptionsMenu.wnd`; without them the keys still work from the file.

Notes:
* The glow source is a second draw of the additive particles into an offscreen target, so the
effect costs fill rate on particle heavy scenes in proportion to how many additive particles are
on screen.
* Off while `AntiAliasing` is above 1, because DirectX 8 cannot redirect a multisampled scene into a
texture. Turning anti-aliasing off brings it back without a restart.
* Particles hidden behind terrain or buildings cast no glow, since the second draw shares the
scene's depth buffer.

### Shadow mapping

Shadows cast from the sun into a shadow map, replacing the stencil shadow volumes on vehicles and
buildings and the blob decals under infantry. Each shadow takes the shape of its object, including
the cutouts in trees, fences and other alpha tested or blended meshes, and falls on terrain, roads,
bridges, units and buildings, with soft filtered edges. Needs the Direct3D 9 build and a shader
model 2 card.

* `ShadowMap = Yes` - (No goes back to the stencil volumes and blob decals. Also the
`Shadow mapping` checkbox in Game Options, where it applies on Accept without a restart.)

`3D Shadows` and `2D Shadows` still decide which objects cast: 3D covers the objects authored with
volume shadows (vehicles, buildings, trees) and 2D the ones authored with decal shadows (mostly
infantry). With both off there are no shadows, so the detail presets keep controlling shadows as
before.

The Game Options control needs the `CheckShadowMap` window in `OptionsMenu.wnd`; without it the key
still works from the file.

Notes:
* Decals that use the shadow type for something other than a shadow keep drawing, such as the fake
structure marker and the glow under shells. Only decal textures whose name starts with `shadow` are
replaced.
* Units under shroud, and stealthed units, cast no shadow, so a shadow cannot give away a unit the
player cannot see.
* Additive and other glow passes cast nothing. Opaque meshes cast solid shapes even when their
texture has an alpha channel, since unit textures often keep reflection masks there.
* The map is 4096 texels across and follows the ground in view, so detail drops as the camera zooms
out.
* The sun is never lower than `ShadowMapMinSunElevation` degrees, 30 unless the mod's
`GameData.ini` sets it, keeping the direction the map's lighting sets. Maps light their objects with
low suns that the stencil volumes shortened per object, and one shared sun cannot, so without the
floor shadows streaked several times their caster's height. At 30 degrees a shadow reaches at most
about 1.7 times its caster's height. 0 turns the floor off.
* Terrain casts too, so cliffs and hills shadow the ground below them. Only the terrain loaded
around the camera casts, and the flat terrain mode does not yet.

### Specular highlights

Vehicles and structures catch a per-pixel highlight from the sun, on top of their usual lighting.
The highlight follows the map's own sun direction and colour, scales with how bright the model's
texture is so metal shines and dark paint barely does, and disappears where the sun's shadow falls
when shadow mapping is on. Infantry stay matte. Needs the Direct3D 9 build and a shader model 2
card.

* `Specular = Yes` - (No turns the highlights off. Also the `Specular highlights` checkbox in the
advanced display options, where it applies on Accept without a restart.)

The Game Options control needs the `CheckSpecular` window in `OptionsMenu.wnd`; without it the key
still works from the file. How the highlight looks is set in the mod's `GameData.ini`:

* `UnitSpecularIntensity = 0.35` - (How bright the highlight is. 0 turns it off.)
* `UnitSpecularPower = 24` - (How tight it is. Higher values give a smaller, sharper highlight.
Around 4 to 128 is useful; past that the highlight shrinks to nothing.)

`SpecularDebug = Yes` in `Options.ini` tints everything the pass covers a faint magenta and shows
the highlight 8 times brighter in magenta, to check where it runs and where it lands.
* Additive meshes on skinned models (infantry and other bone deformed meshes) do not glow. Their
vertices only exist for the duration of the normal draw, so there is nothing left to draw again.

### Laser ground glow

Each laser beam lights the terrain along its whole length with a row of small dynamic lights
(up to twelve per beam), so the ground under the beam picks up the beam color. The lights are
terrain only, so units and buildings do not light up. They take the beam color (house colored
when the laser asks for it), switch on and off with the beam and follow a continuous beam as its
target moves. Roads are not lit, since road dynamic lighting is disabled in the engine.

* `LaserRef = No` - (Yes turns the glow on. Also the `Lasers light the ground` checkbox in Game
Options, where it applies on Accept without a restart.)

`GameData.ini` tunes the color and intensity for every laser, and a `W3DLaserDraw` module can
override each for its own laser with `GroundGlowColor` and `GroundGlowIntensity`. A module value
above zero (or not black) wins, then the `GameData.ini` value, else the default named below.

* `LaserGroundGlowColor = R:0 G:0 B:0` - (Light color. Black derives it from the beam: every
beam layer's color weighed by its width, times the average color of the laser texture, so a
textured laser with white ini colors still glows in its texture's hue. The color is normalized
to full brightness either way.)
* `LaserGroundGlowIntensity = 70%` - (How strongly the color is added to the ground.)

Each light reaches as far as the laser's `OuterBeamWidth`, or `GroundGlowRadius` on the
`W3DLaserDraw` module when that is set. The terrain is lit per vertex on a 10 unit grid, so that
reach is floored at 15 to keep the lit vertices a strip rather than dots. The lights sit one
radius apart with at most twelve per beam; a beam longer than that widens its lights until they
meet and dims them by as much. Each light also reaches up to 6 units past its radius for a
softer edge.

* The Game Options checkbox needs a `CheckLaserRef` window in `OptionsMenu.wnd`; without it the
key still works from the file.
* The terrain lights up to 64 dynamic lights a frame, for every kind of dynamic light.

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

# GameData.ini

## Subdual damage defaults

Subdual, jamming, frozen and chrono tuning no longer has to be repeated on every ActiveBody. GameData.ini
accepts any number of `SubdualDamageDefaults` blocks, each optionally limited to a `KindOf`
list and kept off a `ForbiddenKindOf` list, and values may be written against max health:

```
SubdualDamageDefaults
  SubdualDamageCap        = MaxHealth * 2
  SubdualDamageHealRate   = 500
  SubdualDamageHealAmount = MaxHealth / 16.25
End
```

An ActiveBody key still wins over the global block, an explicit `SubdualDamageCap = 0` still
means immune, and an ActiveBody with no key at all now falls back to the global block instead
of zero. The `MaxHealth` forms work on ActiveBody too, and ActiveBody gains `ChronoDamageHealRate`
and `ChronoDamageHealAmount` for per-unit chrono tuning. Details in
[GameData](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/GameData#subdual-damage-defaults).

## Subdual frozen

`SUBDUAL_FROZEN` is a third subdual damage pool beside retail subdual and jamming. It disables
the unit the way retail subdual does, through its own `DISABLED_FROZEN` type, and sets a `FROZEN`
condition state while it holds. It gets the same customization as jamming: `FrozenDamageCap`,
`FrozenDamageHealRate` and `FrozenDamageHealAmount` on ActiveBody or in `SubdualDamageDefaults`,
an armor coefficient, a `Frozen` health-bar icon from Animation2D.ini, `SoundFrozen` /
`SoundUnfrozen` per unit with `UnitFrozen` / `UnitUnfrozen` in MiscAudio.ini as fallback, and a
`FrozenOverlay*` texture block in GameData.ini. The jamming and frozen overlays stack on one unit.
Details in [Subdual Frozen](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Objects-&-Modules#subdual-frozen).

## BatchParticles

* `BatchParticles = No` - (Default. `Yes` draws consecutive particle systems that share a texture,
blend mode and billboard mode in a single call.)

Every particle system used to be its own draw call. With `BatchParticles = Yes`, plain particle
systems that look alike are gathered into one 512-point buffer and drawn together, which
TheSuperHackers measured at 15 to 30 percent cheaper particle rendering. Streak, volume and
terrain-conforming systems are never batched and draw exactly as before.

Notes:
* Independent of the option, a particle system with nothing on screen is now skipped outright
instead of being walked and drawn empty.
* Ported from TheSuperHackers commit `de20ae0cb` (Ronin and Mauller).

## SkipTranslucencySort

* `SkipTranslucencySort = No` - (Default. `Yes` draws translucent triangles in submission order
instead of depth-sorting them every frame.)

Every frame the renderer copies every translucent triangle on screen (particles, tank tracks, water,
roads and all other blended surfaces), computes its depth, sorts the whole set and resubmits it in
back-to-front order. With thousands of particle systems in view that CPU pass is a real share of
the frame. `Yes` skips it entirely, so translucent things draw in the order the engine happens to
submit them. The cost is layering errors where translucent effects overlap: smoke can appear behind
an explosion glow that is really in front of it. Opaque and alpha-tested geometry is unaffected.

Notes:
* The same key exists per detail level in GameLOD.ini (`StaticGameLOD` blocks), so `Low` and
`Medium` can skip sorting while `High` keeps it. A detail level can only add the skip: sorting is
skipped when either the GameData key or the applied level says `Yes`.
* There is no Options menu checkbox for this.

## BackToFront

* `BackToFront = No` - (Default. `Yes` draws whole particle systems farthest first when
`SkipTranslucencySort = Yes`.)

Without the sorter, particle systems draw in the order they were created, so a new explosion glow
lands on top of the older smoke that is really in front of it. With `BackToFront = Yes` the renderer
takes the mean position of each system's visible particles, which it already visits to cull them,
and draws the systems farthest from the camera first. That removes most of the layering errors
between different effects. Particles inside one system still draw in list order, and streaks, tank
tracks and water are not reordered.

Notes:
* Does nothing while `SkipTranslucencySort = No`; the sorter already orders every triangle.
* Depth ordering interleaves textures more, so `BatchParticles` gathers slightly smaller batches.

## ForceFireAllWeapons

* `ForceFireAllWeapons = No` - (Default. `Yes` makes an attack ground order fire every weapon that
can hit the ground, on every turret, instead of only the primary weapon.) Goes in the `AIUpdate`
block next to `TurretsLinked`.

Attack ground, whether from a Ctrl+click on terrain or a script, always selects the primary weapon
and fires nothing else. Until now the only way around it was `TurretsLinked = Yes`, which fires
every slot regardless of what it can hit, anti-air included, and also chains every turret to one
target during ordinary attacks. `ForceFireAllWeapons = Yes` only changes attack ground. Each weapon
whose anti mask includes ground fires once it is ready and the target is inside its own range;
weapons that can only hit aircraft, projectiles or mines stay quiet. A turret that does not hold
the current weapon turns to the point on its own and fires with its own ground weapon, so a two
turret unit no longer needs `TurretsLinked` to use both.

Notes:
* Attacking a unit or building, forced or not, still picks the single best weapon.
* A weapon slot that fires in sync with another slot keeps that rule and fires only when its lead
does. A locked weapon, which is how attack ground special powers work, fires alone as before.
* The weapons beyond the lead skip their `PreAttackDelay` wind-up, the same as linked turrets do.
* A slot whose `AutoChooseSources` excludes `FROM_PLAYER` is skipped for player orders, the same
way normal weapon choice skips it.

## NoOccupantFriendlyFire

* `NoOccupantFriendlyFire = No` - (Default. `Yes` spares the container a passenger is riding in from
that passenger's own splash damage.)

An object has never been able to hurt itself with its own splash damage, but a passenger firing out
of a transport or a garrisoned building is a separate object, so it hurts the thing it is riding in.
Anti-tank infantry are the usual victims of this: a Tank Hunter in a bunker firing at something
beside the wall knocks down the bunker holding it. With `NoOccupantFriendlyFire = Yes` the splash
skips the container the shooter is inside, the same way it already skips the shooter.

Notes:
* The whole containment chain is skipped, not just the immediate container, so infantry inside a
bunker riding an Overlord spare both.
* Weapons that already damage their own firer - `RadiusDamageAffects = SELF`, which is how suicide
attacks are built - are untouched and still destroy the container.
* Directly ordering the passenger to attack its own container still damages it. Splash on a nearby
target is what changes, not a deliberate shot.
* Turning this on changes the simulation, so a replay must be played back with the same setting it
was recorded with.

## Transport load slowdown

* `TransportLoadSpeedPenalty = 0%` - (Default. The fraction of its `Speed` a container loses when it
is completely full.)
* `TransportLoadTurnRatePenalty = 0%` - (Default. The same for `TurnRate`.)
* `TransportLoadAccelerationPenalty = 0%` - (Default. The same for `Acceleration`.)
* `TransportLoadLiftPenalty = 0%` - (Default. The same for `Lift`.)
* `TransportLoadPenaltyKindOf` - (Default: every kind. Only occupants with at least one of these
`KindOf` bits count toward the load.)
* `TransportLoadPenaltyForbidKindOf` - (Default: none. Occupants with any of these `KindOf` bits
never count toward the load.)

A transport in retail moves at exactly the same speed whether it is empty or packed, so there is no
cost to filling one up and no reason to send a half-loaded one anywhere. These make a container
heavier the more it is carrying: at a full load it loses the whole percentage, at half a load it
loses half of it, and as passengers leave it gets the speed back.

The penalty is worked out from what is inside at that moment rather than tallied up as passengers
come and go, so an emptied transport is back to exactly its original speed with nothing left over.

Fullness is counted in slots rather than bodies, so a unit that takes three slots weighs three times
as much as one that takes a single slot.

```
GameData
  TransportLoadSpeedPenalty        = 40%
  TransportLoadAccelerationPenalty = 25%
End
```

The two `KindOf` keys set the game-wide default for which passengers weigh anything, so a mod can
exclude, say, `INFANTRY` everywhere without touching each transport. A container's own
`LoadPenaltyKindOf` and `LoadPenaltyForbidKindOf` replace the global values for that container.

Individual containers can override any of these, exclude particular passengers from counting, or opt
out of the whole thing - see
[Load slowdown from occupants](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Objects-&-Modules#load-slowdown-from-occupants).

Notes:
* Each percentage covers the damaged variant of its value, so `SpeedDamaged` is scaled by the same
amount as `Speed`. A damaged transport is slowed once, not twice.
* The slowdown survives a change of locomotor, so an upgrade that grants `SET_NORMAL_UPGRADED`, or a
unit falling back to `SET_PANIC`, keeps it.
* Containers that cannot move are unaffected. A garrisoned building has no locomotor, so the keys
parse but do nothing there.
* A loaded transport travelling with a group holds the group to its speed, exactly as any other slow
unit does.
* Turning this on changes the simulation, so a replay must be played back with the same setting it
was recorded with. Left at the `0%` default nothing changes at all.

# SpecialPower.ini

## StartCooldownOnFirstShot

* `StartCooldownOnFirstShot = No` - (Default. `Yes` delays `ReloadTime` until the unit has fired the
shots the power ordered.)

A special power normally starts its cooldown the moment the player uses it. When the power's OCL has
an `Attack` nugget, the unit still has to line up and shoot, so it spends part of that cooldown
before firing anything.

Set `StartCooldownOnFirstShot = Yes` and the cooldown starts after the unit finishes shooting
instead. While the game waits for those shots, the power is locked: it reports itself as not ready,
so the cameo greys out and the player cannot use it again. If `NumberOfShots` is more than one, the
power stays locked until the unit fires the last one.

```
SpecialPower SpecialPowerDig
  Enum                     = SPECIAL_HELIX_NAPALM_BOMB
  ReloadTime               = 60000
  StartCooldownOnFirstShot = Yes
End
```

Notes:
* If the unit fires at least one shot, the cooldown starts even when it does not fire the rest. A
move order that interrupts the attack, an empty clip, and the unit dying all end the shooting, so the
power never waits for a shot that will not come.
* If the unit fires **nothing**, the game treats the use as cancelled and gives the power back ready.
This covers ordering the unit away before it shoots, the target disappearing, and the unit dying
first. The player does not get the credits back, because the power charges them when it is used.
* Should the engine miss a cancel, the power gives up waiting after `ReloadTime` and starts its
cooldown, so it can never get stuck. This also catches a power whose OCL has no `Attack` nugget,
which is a mistake in the data; the log says which power it was.
* A power with `SharedSyncedTimer` ignores this field. The player owns that timer, not the building
that fired, so no single unit's shots can start it.

# ObjectCreationList.ini

## Attack nugget: FireRegardlessOfOrders

* `FireRegardlessOfOrders = No` - (Default. `Yes` fires every `NumberOfShots` no matter what the
unit is ordered to do meanwhile.)

An `Attack` nugget normally orders the unit that fired the special power to attack the target point
with `WeaponSlot` for `NumberOfShots`. That is an ordinary attack order, so any move, attack or stop
given while it runs replaces it, and the barrage ends after however many shots got out.

With `FireRegardlessOfOrders = Yes` the shots are queued on the unit instead of being an order. Every
frame the weapon in `WeaponSlot` is ready, one shot is fired at the target point straight from the
launch bone, until the count is used up. The unit stays fully responsive: it moves, retargets and
shoots its other weapons exactly as ordered while the barrage carries on. The delivery decal stays
until the last queued shot is away.

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
* The weapon fires without turning or aiming, so it should not sit on a turret. `PreAttackDelay` is
not observed.
* Range is not checked, so the shots keep landing after the unit has driven beyond the weapon's
`AttackRange`.
* Boarding a transport, garrison or tunnel that does not let its passengers fire drops the rest of
the barrage.
* Timing comes from the weapon as usual: `DelayBetweenShots` between shots, and a clip reload in the
middle when `NumberOfShots` is larger than `ClipSize`. A weapon that runs dry with
`AutoReloadsClip = No` drops the remaining shots.
* Firing the power again replaces the queue: new target point, count reset.
* A dying unit drops its remaining shots. The queue survives a save and load.

# Animation2D.ini

## Texture

An `Animation` block can name a texture file directly for a frame with
`Texture = <file> [width height]`, instead of pointing at a `MappedImage`. The whole file is the
frame. This is what the low-power and jammed health-bar icons use, and it is the quickest way to
add a one-image icon: drop a `.tga` in `Art\Textures\` and reference it, with no `MappedImage`
entry to write.

* `Texture = jammer.tga` - (The file to draw. Width and height default to `32 32`.)
* `Texture = jammer.tga 64 64` - (Explicit draw size. Match the file's real pixel size, or the
image is scaled to fit.)

`Texture` and `Image` lines can be mixed in one block. Each fills the next frame in order, so
`NumberImages` must count both kinds. The file name doubles as the image name, so every animation
naming the same file shares one image, and an existing `MappedImage` of that name is reused rather
than replaced. The file resolves like any texture, so a `.dds` of the same name anywhere in the
archives wins over a loose `.tga`.

A static icon is `NumberImages = 1`, `AnimationMode = ONCE` and `AnimationDelay = 0`. Icons only
change what is drawn, so this does not affect replays.

# Turret modules

## MaxPhysicalPitch

* `MaxPhysicalPitch = 90` - (Default. The highest pitch a turret with `AllowsPitch = Yes` will aim at,
in degrees.)

The counterpart of `MinPhysicalPitch`. A turret's pitch is the angle to its target plus the arc from
`GroundUnitPitch`, and retail only clamps the low end. On a Fire Base that is fine, since a building
never aims at anything past its range, but a vehicle can be ordered to attack a target far outside
its range, so nothing stopped its cannon from climbing skyward on the way. `MaxPhysicalPitch` caps the
aim at the given angle; the default of straight up leaves data that does not mention it exactly as before.

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

# FireWeaponWhenDamagedBehavior

## NoHealthLoss

* `NoHealthLoss = No` - (Default. `Yes` refunds the health taken by any hit that passes the
`DamageTypes` and `DamageAmount` gate.)

The module is often used as a trigger rather than as a reaction to real harm: `DamageTypes` and
`DamageAmount` name a damage type and a minimum amount that mean "do this now", the way a `MELEE`
hit of 1 point fires the pilot-killing weapon on an infiltrated vehicle. The trigger hit still cost
the object that much health, so the signal always came with a bite. With `NoHealthLoss = Yes` the
health lost to a qualifying hit is given straight back, before the module picks and fires its
reaction weapon.

Notes:
* Every qualifying hit is refunded, not only the ones that fire something. A hit that arrives while
the reaction weapon is reloading, or in a damage state that has no weapon assigned, is still free.
* Qualifying damage can no longer kill the object or move it between pristine, damaged, really
damaged and rubble, so the reaction weapon is always chosen for the state the object was already in.
* The hit still registers as a hit: its damage effects play and it still raises the owner's "under
attack" warning.
* Damage types outside `DamageTypes`, and hits below `DamageAmount`, are unaffected and drain health
normally.

# StealthUpdate

## New StealthForbiddenConditions

Four new values for `StealthForbiddenConditions` (and `OverrideStealthForbiddenConditions` on
`StealthUpgrade`), alongside the retail set of `ATTACKING`, `MOVING`, `USING_ABILITY`,
`FIRING_PRIMARY`, `FIRING_SECONDARY`, `FIRING_TERTIARY`, `NO_BLACK_MARKET`, `TAKING_DAMAGE`,
`RIDERS_ATTACKING` and `FIRING_WEAPON_FOUR` to `FIRING_WEAPON_EIGHT`.

* `RIDERS_FIRING_PRIMARY`, `RIDERS_FIRING_SECONDARY`, `RIDERS_FIRING_TERTIARY` - the transport
cannot stealth while any passenger fired the named weapon slot this frame or the last. They work
like the transport's own `FIRING_*` conditions but look at the riders, and, like `RIDERS_ATTACKING`,
only apply to a container whose `PassengersAllowedToFire = Yes`. `RIDERS_ATTACKING` reveals for as
long as a rider holds an attack order, even between shots; these reveal only on the shots
themselves, so a container with a slow-firing rider can re-cloak in between.
* `UNIT_CREATED` - the object cannot stealth in the frame it finishes producing a unit. This covers
a `ProductionUpdate` queue completing, a spawner or drone carrier releasing a spawn, and a dozer
finishing a structure.

As with `TAKING_DAMAGE`, each of these breaks stealth for a single frame; the object's `StealthDelay`
then decides how long it stays visible before it may cloak again.

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

## TOGGLE_FIRE_WEAPON

Fires a weapon exactly as `FIRE_WEAPON` does, but a second click stops it again. Retail has no way
to call off a `FIRE_WEAPON` order: it runs until `MaxShotsToFire` is spent, so a jammer set to sixty
shots is committed to all sixty.

```
CommandButton Slth_Command_JammerStationActivate_HumanPlayer
  Command          = TOGGLE_FIRE_WEAPON
  Options          = CHECK_LIKE     ; required, or the button never renders as toggled on
  WeaponSlot       = SECONDARY
  MaxShotsToFire   = 60
  TextLabel        = CONTROLBAR:GLAJamm
  ButtonImage      = SUJPulse
  ButtonBorderType = ACTION
  DescriptLabel    = CONTROLBAR:ToolTipGLAFireJamm
End
```

Notes:
* The button reads the unit's actual state rather than remembering a click, so it also switches off
by itself once `MaxShotsToFire` is spent.
* Stopping ends only the firing. A move order given alongside it survives, unlike the stop command,
which clears everything.
* The button stays clickable while the weapon reloads between shots. A plain `FIRE_WEAPON` button
greys out there, which would otherwise take the cancel away for most of a burst.
* A selection where only some units are firing resolves one way for the whole group: if any of them
is firing, the click stops all of them.

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
click still cancels the entry, and Shift+click still cancels every unit of its type.

# Structure Multi-Select

Structures of one type can now be selected together, the same way units can. Retail dropped a
selected structure the moment anything else was picked.

* The select-matching key (E by default) picks every structure of the same type on screen, or
across the map with Alt held.
* Shift+click and Shift+double click add a structure of the same type to the selection.
* Shift plus a team number (0 to 9) adds that team if it holds only structures of that type.
* Mixing structures with units, or structures of different types, still drops the old selection.

With several producers selected the bar shows their shared commands. A build or upgrade command
that every selected producer has is enabled even without `OK_FOR_MULTI_SELECT`. The queue shows the
nine entries closest to finishing across all of them.

* Click a unit or upgrade to queue it on the producer that would finish it first.
* Shift+click a unit to queue five, each on whichever producer would finish it first. A producer
whose queue or parking is full hands the rest to the next one, and the batch stops where the money
or the unit's per player limit runs out.
* Shift+click an upgrade to queue it on the five producers that would finish it first.
* Shift+click a queue entry to cancel that type in every selected producer.

# New Behavior Modules

## TimeOfDayOverrideUpdate

Switches the map's time of day. It is not a superweapon of its own: it rides along on something that
already exists, so any existing special power can be made to bring on the night.

It works two ways. Attached to the structure that fires a special power it switches when that power
is launched, and `Duration` decides how long the change lasts. Attached instead to an object the
power creates, with `ActivateOnCreate`, it switches as soon as that object exists and holds the
change until it dies - which for something like a storm that lingers over its target means the night
lasts exactly as long as the storm does, with no duration to keep in step with it.

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
* `FallbackTimeOfDay = AFTERNOON` - (Where a timed switch returns to when the map's own time of day
is already `TimeOfDay`, since going back to it would leave the world where the switch put it. Only
a revert target, never something the power switches to.)
* `Duration = 0` - (In milliseconds. `0` makes the switch permanent, and firing again toggles it
back. Any other value reverts to the original time of day after that long. Ignored when
`ActivateOnCreate` is set, since the object's own lifetime is the duration.)
* `ActivateOnCreate = No` - (Yes switches the moment the object carrying the module is created and
puts it back when that object dies, instead of waiting for a special power to fire. For an object
created by a superweapon this ties the change to the weapon's effect rather than to its launch.)

The switch is a real time of day change, not a lighting tint, so it brings everything night owns
with it: the map author's own night lighting, night model variants such as headlights, the night
ambient sounds, the night sky and water, and the night player indicator colours. Maps store lighting
for all four times of day, so a daytime map already carries the night palette its author chose.

Notes:
* Without `ActivateOnCreate` the switch lands when the power is **launched**, not when it hits.
* A map that already sits at `TimeOfDay` is left alone. The power still fires, it just has no time
of day change to make, so a night bringing superweapon never brings daylight instead.
* Time of day is global, so it changes the view for every player and every observer, not just the
firing player.
* Several objects can hold the change at once. A second one arriving joins the first rather than
starting over, and the world only goes back when the last of them lets go, so overlapping strikes
never cut each other's night short.
* It happens in the simulation on all machines at once, so it is multiplayer and replay safe. Nothing
about it feeds back into the simulation - lighting and models are presentation only.
* A timed switch that is still running when the firing structure dies reverts rather than sticking.

Switching at runtime also relights the trees, building bibs, bridges and roads, which the engine
previously left with their baked daytime lighting. Roads needed one more step: they were relit, but
the new colours only reached the screen when a road moved in or out of view, so they kept their
daytime look until the camera happened to travel far enough. The debug time of day hotkey picks up
the same fixes, and now refreshes player indicator colours as well.

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

## Engine differences in the online build

The online build follows the official Generals Online client in more than the network
stack. These differences apply to the whole executable, skirmish and campaign included,
and none of them exist in a normal Contra build.

### 60 Hz simulation

The game logic runs at 60 frames per second instead of 30, matching the official
`gen_online_60hz` client. Unit speeds, reload times and every duration written in
milliseconds are unchanged, because the engine converts them from the frame rate.
Code that counted raw frames (turret turn rates, slow death arcs, particle keyframes,
script timers, deploy animations) keeps its retail timing through a second 30 Hz frame
counter, the same way Generals Online does it.

Two things follow from this that the player should know:

* Replays are only compatible within the same build. A replay from the 30 Hz build
  will not play back in the 60 Hz build or the other way round; the game refuses it with
  the usual version mismatch message.
* The lowest render frame rate cap is 60, because the render rate can never sit below
  the logic rate. The FPS presets that were below 60 are gone from the hotkey cycle.

### Frame rate cap during online matches

During an online match the render cap comes from Generals Online's `settings.json`
(the same file the official client reads), so the whole match runs the way the official
client would. The moment the match ends, the cap, the FPS limit switch and the camera
scroll speed all return to Contra's own option values. The shell, skirmish and campaign
never see the Generals Online values.

### Observer overlay

For spectators the player list gains four columns: science points, kills, losses and
power surplus (drawn red while a player is short on power), plus the army name. A
notification feed slides in on the left for general promotions, superweapon and
generals power use, rank 3 and rank 5, an income of 10k a minute, GLA players gaining
power, and a player losing all dozers and command centers. The control bar toggle
hotkey hides and shows both together.

`Options.ini` keys, all optional:

| Key | Default | Meaning |
|---|---|---|
| `ObserverNotificationFontSize` | `10` | Feed font size; `0` disables the feed |
| `ObserverNotificationSpecialPowerUsage` | `yes` | Announce power use |
| `ObserverNotificationSpecialPowerPurchase` | `yes` | Announce promotions |
| `ObserverNotificationMilestone` | `yes` | Announce rank, income, power and dozer milestones |

### Widescreen layouts and camera

The user interface is laid out for a 1280x720 reference instead of 800x600, and the
maximum camera height rises with the aspect ratio so a 16:9 screen sees as much of the
map vertically as a 4:3 screen does. The layouts themselves come from a Generals Online
install: the game prefers window files under `GeneralsOnlineGameData\` when they exist.
Without that folder the control bar and sliders draw at the wrong scale, and with it
the options menu is Generals Online's, which lacks Contra's own option controls. See
`PORT_NOTES.md` for the status of the Contra layouts.

### Replay analytics

`-headless -replay <file> -exportStats` writes a gzip compressed JSON summary of the
match next to the replay (players, build, kill, capture, energy and rank events, plus a
time series), and `-statsUrl <url>` posts it. The flag refuses to run outside headless
replay simulation.

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
