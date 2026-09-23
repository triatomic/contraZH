# contraZH Changes

Changes in [contraZH](https://github.com/triatomic/contraZH), a fork of GeneralsGameCode_Modding.
Everything here is additional to upstream; the rest of this wiki still applies.

* Most changes are client-side, read from `Options.ini`, and default to retail behaviour. An
untouched `Options.ini` plays exactly as before.
* [Gameplay Fixes](#gameplay-fixes) are always on and fix retail simulation bugs.
* A few simulation rules are read from the mod's `GameData.ini`; each states its default.
* Synced with TheSuperHackers/GeneralsGameCode and GeneralsGameCode_Modding as of August 2026. The
only casualty was the texture filter option, replaced by TheSuperHackers' version - see
[Rendering](#rendering) for the renamed `AnisotropyLevel` key.

# Gameplay Fixes

Always on, no `Options.ini` key. All are retail bugs upstream still carries. Each entry says whether
it changes the simulation (older replays no longer play back identically) or only what is drawn.

## Guard mode holds its ground

* A guarding unit that gets hit finishes its return or its current attack instead of chasing the
attacker and stuttering. It picks up the attacker through its normal enemy scan.
* Deploy-to-fire units (Nuke Cannon) stay deployed when:
  * their target dies and another enemy is inside the guard circle
  * they are ordered to guard the spot they already stand on
  * a target leaves range while they are still unpacking (the unpack reverses)
* Manually deployed units keep their stance.

Changes the simulation. Ported from CookieLandProjects/CLP_AI.

## Weapon bonus no longer restarts the reload

A bonus changing mid-reload (Propaganda, horde, continuous fire, promotion) used to reset the reload
to zero, which could stall a unit or let it skip the reload (Nuke Cannon plus Propaganda toggling).
The reload now keeps the fraction already served; only the remaining time scales.

## Snipe survives a weapon set change

Jarmen Kell's snipe wind-up was lost when his weapon set changed (salvage, promotion, bike). The
charge now carries over and finishes on schedule.

Note: dropping out of snipe mode entirely is data - a set change drops the weapon lock unless the
set is marked `WeaponLockSharedAcrossSets`.

## Stealth kills no longer reveal the killer

The floating bounty text is shown only to players who can see the killer: its owner, allies, and
replay observers. Stealthed snipers and stealthed transports no longer give themselves away. A
detected stealth unit still shows it.

Presentation only; replays unaffected.

## Drones stay on their leash

* The leash (twice the guard range from the master) was never checked while the master had a
victim, so drones wandered off indefinitely.
* The leash is now checked first. A drone past it drops its attack and returns.
* Unchanged: normal fights within the master's range, drones without guard range, Stinger Site
stingers.

Changes the simulation.

Note: a drone that auto-acquires targets can still start fights after returning; that is data.

## USE_OWNER_OBJECT fires from the owner

`OCLSpecialPower` with `CreateLocation = USE_OWNER_OBJECT` now has the owner carry out the delivery,
as in Generals, instead of spawning a new transport. Only powers using that value are affected.

Changes the simulation for those powers.

## More Generals Challenge personas

* The persona limit is now 24 (`GeneralPersona0` to `GeneralPersona23`). A 13th used to be dropped
along with everything after it.
* Each persona needs a `GeneralPosition<N>` button in ChallengeMenu.wnd to show. Slots without a
button are skipped instead of crashing.
* The retail twelve are untouched.

## Dying infantry do not block or catch clicks

* A dying soldier leaves the pathfind grid immediately instead of blocking non-crushing vehicles
until it sinks. Dying vehicles still block until they become hulks.
* Dead objects no longer catch mouse clicks, unless `ALWAYS_SELECTABLE`.

## Jammed units deselect properly

* Jamming deselects the unit, so the control bar no longer appears stuck.
* `UNSELECTABLE` alone blocks new selection clicks but no longer empties the current selection.
* The jam state is tracked directly, so a promotion or health upgrade can no longer leave a unit
jammed forever.

Changes selection state; affects replays.

## Portable addons no longer block building

Placing a building over your own `PORTABLE_STRUCTURE` carrier (Overlord with Gattling Cannon, etc.)
now shoves it aside like any unit instead of refusing placement. Stuck or enemy carriers still
block.

Changes build legality; affects replays.

## Units chasing a moving target now shoot it

* Fast units ordered onto slow moving targets used to twitch forever without firing.
* The approach point and weapon range now measure edge to edge the same way.
* An engaged unit tolerates about one pathfinding cell of drift before chasing again. Starting an
attack still uses exact range, so nothing gains reach.
* A unit that falls behind stops as soon as a shot opens up.
* Turretless units benefit most.

Changes when units fire; affects replays.

# Game Setup

## Random army per faction

* The army list gets a `Random <faction>` entry per base faction (`Random USA`, `Random China`,
`Random GLA`) after `Random`, for human and AI slots.
* Factions come from `BaseSide` in `PlayerTemplate.ini`, so new mod factions get an entry
automatically.

Notes:
* Labels are `GUI:Random<BaseSide>` (`GUI:RandomUSA`, `GUI:RandomChina`, `GUI:RandomGLA`), falling
back to `Random <BaseSide>`.
* Limit Armies still applies; an entry disappears when none of its generals are allowed.
* All LAN/online players need this build, and so do replays using these entries.

## Host camera height

* The LAN host gets a `Max Camera Height` checkbox and field (210 to 1000) that overrides everyone's
`MaxCameraHeight` for that game.
* Clients need this build. Replays keep the limit they were played with.
* Generals Online keeps its `/maxcameraheight` lobby command; GO's default (310) means the mod's own
limit.
* The `Shift + Ctrl + Z` zoom-limit cheat is unaffected.

# Options.ini

Read at startup. Most can also be changed in game under Options > `Game Options` and apply on
Accept. `NewRadar` and `BlipSize` need a restart.

## Camera

* `UseCustomMaxCameraHeight = No` - (`Yes` lets `MaxCameraHeight` replace `GameData.ini`'s value, 670
in Contra. Also `Max Camera Height` in Options.)
* `MaxCameraHeight = 670` - (210 to 1000. Single player, skirmish and campaign. Multiplayer uses the
host's limit, or `GameData.ini`'s.)

## Display

* `HealthBarDisplayMode = Classic` - (`Classic` | `Damaged` | `Always`. `Damaged` shows bars only on
hurt objects, `Always` on everything.)
* `AlliedDecalMode = House` - (`Hidden` | `House` | `Army`. Shows allies' general power decals once
fired, tinted in player colour (`House`) or faction colour (`Army`). `Hidden` is retail. Enemy
decals stay hidden. Needs `ComboBoxAlliedDecals` and `AlliedDecalsLabel` in `OptionsMenu.wnd` for
the menu control.)
* `NumericalHealth = No` - (Yes prints hit points beside the health bar, wherever a bar shows.)
* `SelectionCircle = No` - (Yes draws a green ring under selected objects.)
* `DefensesRangeCircle = No` - (Yes shows an armed structure's widest attack range, upgrades
included, while placing it. Wall pieces get one ring each.)
* `ObjectDecals = Yes` - (No hides ground decals requested with `DisplayDecal`. Independent of shadow
settings.)
* `SmartPips = No` - (Yes keeps ammo and passenger pips visible on own units whenever there is
something to show.)
* `BuildTimerDisplayMode = None` - (`None` | `Seconds` | `Auto`. Countdowns on build queue and special
power cameos. `Auto` switches to MM:SS past a minute.)
* `NewRadar = No` - (Yes gives an RA3 style radar: outlined blips, larger structures, dark
waterline, two-tone ground, 256x256 grid.)
* `BlipSize = Large` - (`Small` | `Large`. `Small` is 3/5 px unit/structure, `Large` 5/7. Only with
`NewRadar`.)
* `BorderlessWindow = No` - (Yes runs a frameless centred window at the selected resolution. Also
the Borderless checkbox beside Resolution. `-win` still gives a captioned window.)

Notes:
* A blue bar under a building's health bar shows production progress; on supply gatherers it shows
cargo. Own objects, when selected or moused over, or always with `HealthBarDisplayMode = Always`.
* `NewRadar` cannot hide roads, since they are painted into the terrain textures. Bridges still
draw.
* `SelectionCircle` and `DefensesRangeCircle` need a mod-side `PlainRingSelection.tga` (white or
greyscale ring with alpha).
* `DisplayDecal` needs the `.tga` named by `DecalTexture`; there is no default.
* The `DefensesRangeCircle` checkbox needs `CheckDefensesRangeCircle` in `OptionsMenu.wnd`.

## Hotkey overlay

Draws each cameo's hotkey letter on the cameo.

* `KeyboardOverlay = No` - (Yes shows the letters.)
* `KeyboardOverlayRed = 255` - (Letter colour, 0-255 per channel.)
* `KeyboardOverlayGreen = 255`
* `KeyboardOverlayBlue = 255`
* `KeyboardOverlayBackdrop = Yes` - (Draw a translucent plate behind the letter.)
* `KeyboardOverlayBackdropRed = 0` - (Backdrop colour, 0-255 per channel.)
* `KeyboardOverlayBackdropGreen = 0`
* `KeyboardOverlayBackdropBlue = 0`
* `KeyboardOverlayBackdropOpacity = 128` - (0-255; 128 is 50%.)

Shows the hotkey actually registered, so a dropped duplicate is not advertised.

## Grid hotkeys

Keys each cameo by **its slot in the command bar** instead of its ampersand letter, so keys stay put.

* `GridHotkeys = No` - (Yes keys cameos by slot position.)
* `GridHotkeyLayout = QWERTYUIOASDFGHJKL` - (One key per slot, in reading order.)
* `GridHotkeyColumns = 9` - (Command bar width, needed to map the column-ordered slots to the
row-ordered layout. 0 disables remapping.)
* `NonGridHotkeys =` - (Keys to leave out of the grid, e.g. `SGX`. Separators are ignored, so `SGX`,
`S,G,X` and `S G X` are all the same.)

An excluded slot falls back to its string file letter, or gets no hotkey if that letter is also a
grid key.

## Reverse move hotkey

Caps Lock arms a reverse move for the whole selection; the next terrain click drives there in
reverse, like the `REVERSE_MOVE` button. Pressing Caps Lock again or arming attack move cancels it.

* `TOGGLE_REVERSEMOVE` in CommandMap.ini rebinds it. The Caps Lock default only fills an empty slot.
* `KEY_CAPS` is a new CommandMap key name. The Caps Lock state still toggles.
* The cursor uses a `MouseCursor ReverseMove` block from Mouse.ini if present (e.g. `Image = SCCMove`,
`Texture = SCCMove`), else the move cursor.

Needs a locomotor with `CanMoveBackwards = Yes`. `ReverseMoveIgnoreAngleThreshold` in GameData
decides whether goals in front are also reversed to.

## Smart selection

A row of small cameos above the command bar: one per unit type with a count, or one per object for
a single-type selection. Single-object cameos show health, ammo clip segments and veterancy. Up to
16 cameos; counts past 999 show no badge. Reskins share a cameo.

* `SmartSelection = Yes` - (No hides the row and unbinds its keys.)
* `SmartSelectionUseMouse = Yes` - (Yes keeps only a cameo's units on double click, No on
Ctrl+Shift+click.)

* Left click: show that type's (or object's) command set. The whole group stays selected; map
orders go to everyone, bar commands go to the focused object or type. Plain orders (move, attack
move, guard, stop, scatter, cheer, formation, waypoints, enter, dock, repair, heal) always go to
everyone. A building placed from a focused dozer is built by it, with the other dozers helping.
* Right click the pushed-in cameo: back to the group's common commands. Right click another cameo:
drop it from the selection.
* Double click (or Ctrl+Shift+click with `SmartSelectionUseMouse = No`): keep only that cameo's
units.
* Tab and Shift+Tab (`SMART_SELECTION_NEXT_TYPE` / `SMART_SELECTION_PREV_TYPE` in CommandMap.ini)
step the focus through the row.

### Command group row

A row on the command bar frame with one cameo per non-empty command group (1-9, then 0). Each shows
the group's most common unit, its number and live member count. Shown whenever any group has
members.

* `SmartCommandGroup = Yes` - (No hides the row. `SmartSelection = No` hides it too.)

* Left click selects the group; double click also centres the camera on it.

## Input

* `CastMode = Normal` - (`Normal` | `QuickCast` | `QuickCastWithIndicator`. `QuickCast` fires a
targeted ability at the cursor on the hotkey press. `QuickCastWithIndicator` aims with a decal while
held and fires on release.)

Notes:
* Superweapons, structure placement, rally points and beacons are excluded from quick cast.
* A cast pressed during cooldown is queued and fires when the ability is ready, never earlier.
* Shift+click queues five units. Shift+click a queue entry cancels all of that type; Shift+click a
passenger cameo unloads all of that type.

## Clipboard paste

`Ctrl + V` pastes into any text field: in game chat (Enter, Shift+Enter for allies), lobby chat,
names, and so on.

Notes:
* Pastes obey the field's filters; hidden fields still show asterisks.
* Text is truncated to fit the field.
* A multi-line paste stops at the first line break.

## Command line

* `-loadreplay <file>` starts straight into a replay with menus and Options loaded. `<file>` is a
name in the Replays folder or an absolute path. Unreadable replays or missing maps show a message
box.
* `-loadsave <file>` also accepts an absolute path, and shows a message box on failure.

## Rendering

TheSuperHackers' texture filter option replaced this fork's version in the August 2026 merge:

* `TextureFilter = Bilinear` - (`None` | `Point` | `Bilinear` | `Trilinear` | `Anisotropic`. Also in
the in-game options menu.)
* `AnisotropyLevel = 2` - (`2` | `4` | `8` | `16`. Only with `TextureFilter = Anisotropic`.
**Renamed from the pre-merge `AnisotropicLevel`** — update Options.ini by hand; the old key is
silently ignored.)

Notes:
* An unknown `TextureFilter` value falls back to `None`, so typos are visible. `AnisotropyLevel`
rounds down to a valid step.
* The level is clamped to what the device supports.
* Fork fixes on top: settings survive a device reset (alt-tab, window toggle), and a driver that
supports anisotropic for only min or mag filtering falls back to linear for the other.

### Bloom

Soft glow around additive particles (fire, muzzle flashes, tracers, lasers, explosions) and `Add`
blended meshes such as building lights. Smoke and alpha blended effects do not glow. Needs no shader
support.

* `Bloom = No` - (Yes turns the glow on. Also `Glow around additive effects` in Game Options,
applied on Accept.)
* `BloomStrength = 0.5` - (0 to 1. 0 is off. `Strength %` in Game Options.)
* `BloomDebug = No` - (Yes shows only the blurred glow buffer on black, to see what feeds it.
`Debug view` in Game Options.)

Menu controls need `BloomGroupLabel`, `CheckBloom`, `TextEntryBloomStrength` and `CheckBloomDebug`
in `OptionsMenu.wnd`.

Notes:
* Costs fill rate in proportion to on-screen additive particles.
* Off while `AntiAliasing` is above 1 on the Direct3D 8 build. The Direct3D 9 build glows with MSAA
on too, and blurs through a pixel shader where the card has one.
* Particles hidden behind terrain or buildings do not glow.
* Additive meshes on skinned models (infantry and other bone-deformed meshes) do not glow.

### Shadow mapping

Sun shadows from a shadow map replace stencil volumes on vehicles and buildings and blob decals
under infantry. Shadows match their object's shape, alpha cutouts included, fall on everything, and
have soft edges. Needs the Direct3D 9 build and a shader model 2 card.

* `ShadowMap = Yes` - (No restores stencil volumes and blob decals. Also `Shadow mapping` in Game
Options, applied on Accept. Needs `CheckShadowMap` in `OptionsMenu.wnd` for the menu control.)

`3D Shadows` and `2D Shadows` still pick the casters: 3D for volume-shadow objects (vehicles,
buildings, trees), 2D for decal-shadow objects (mostly infantry). Both off means no shadows.

Notes:
* Only decals whose texture name starts with `shadow` are replaced; other shadow-type decals keep
drawing.
* Shrouded and stealthed units cast no shadow.
* Additive and glow passes cast nothing. Opaque meshes cast solid even with an alpha channel.
* The map is 4096 texels and follows the ground in view, so detail drops when zoomed out. When the
camera tilts toward the horizon, the map keeps the ground nearest the camera and distant shadows
fade.
* The sun is kept at least `ShadowMapMinSunElevation` degrees high (default 30, set in the mod's
`GameData.ini`; 0 disables). At 30 degrees shadows reach at most about 1.7 times the caster's height.
* Terrain casts too, but only terrain loaded around the camera, and not in flat terrain mode.
* Objects whose shadow cannot reach the screen are left out of the map.

### Specular highlights

Vehicles and structures get a per-pixel sun highlight, following the map's sun, brighter on bright
texture areas, and hidden in shadow when shadow mapping is on. Infantry stay matte. Needs the
Direct3D 9 build and a shader model 2 card.

* `Specular = Yes` - (No turns highlights off. Also `Specular highlights` in the advanced display
options, applied on Accept. Needs `CheckSpecular` in `OptionsMenu.wnd` for the menu control.)

Tuned in the mod's `GameData.ini`:

* `UnitSpecularIntensity = 0.35` - (How bright the highlight is. 0 turns it off.)
* `UnitSpecularPower = 24` - (How tight it is. Higher values give a smaller, sharper highlight.
Around 4 to 128 is useful; past that the highlight shrinks to nothing.)

`SpecularDebug = Yes` in `Options.ini` tints covered surfaces magenta and shows the highlight 8x
brighter.

### Surface detail (normal mapping)

Bump detail in sunlight on vehicles, structures and terrain. On units and structures it shades
both diffuse light and the specular highlight, fades in shadow, and costs no extra draw. Infantry
stay flat. Needs the Direct3D 9 build and shader model 2.0a; other cards get plain highlights and
flat terrain.

* `NormalMaps = Yes` - (No turns the detail off. Also `Surface detail` in the advanced display
options, applied on Accept. Needs `CheckNormalMaps` in `OptionsMenu.wnd` for the menu control.)

Units and structures use a normal map when one exists and otherwise derive bumps from texture
brightness (light = raised, so painted markings emboss too). A normal map sits beside its texture in
`Art\Textures` with `_nrm` added, e.g. `avtank_nrm.dds` for `avtank.tga`:

* Tangent space, in the DirectX convention (green points down the texture).
* DDS only, DXT5 or uncompressed. Other formats are not looked for.
* Laid out on the same UVs as the texture it belongs to.

Tuned in the mod's `GameData.ini`:

* `UnitBumpHeight = 0.15` - (How far, in world units, full brightness rises on textures without
a normal map. 0 leaves them flat, so only textures with normal maps get detail.)
* `UnitNormalMapStrength = 1.0` - (Scales the tilt of authored normal maps. Above 1 exaggerates
them, below 1 softens them.)

Terrain uses normal maps only, never derived bumps:

* Named after the `Terrain.ini` texture, e.g. `NTGrass1_nrm.dds` for `NTGrass1.tga`.
* At least as large as the part of the texture the game reads; simplest is the same size.
* Terrain without one stays flat.
* Flat regardless: the third texture where three meet, flat terrain mode, roads, water reflections.

* `TerrainNormalMapStrength = 2.0` - (Scales the tilt of terrain normal maps. Terrain is seen
from further away than units, so it defaults stronger.)

`NormalMapDebug = Yes` in `Options.ini` paints terrain flat grey with only the bump shading, 4x
stronger.

### Shader water

Lakes, seas and rivers are shaded per pixel, with refraction, reflection, sun glint, foam and
vertex waves. Its options and `Water.ini` parameters are on [Water](Water.md).

### Hardware instancing

Copies of the same vehicle, structure or prop draw together in one call per mesh piece, in the
shadow map and in the main view along with their shadow and highlight passes. Needs the Direct3D 9
build and a shader model 3 card; other cards draw as before.

Notes:
* These still draw one at a time: infantry and other skinned meshes, fading units, camera-facing
sprites, objects lit by point or dynamic lights, and objects under shroud, jamming, frozen or heat
vision overlays.
* The `CONTRA_INSTANCING` environment variable helps track down rendering faults: `0` turns it off,
`1` limits it to the shadow map, `2` adds main view objects without shadow or highlight passes, and
`3` (the default) covers everything.

### GPU skinning

Infantry and other skinned meshes bend on the graphics card instead of the CPU, in the shadow map
and in the main view along with their shadow and highlight passes. Needs the Direct3D 9 build and a
shader model 2 card; other cards skin on the CPU as before.

Notes:
* These still skin on the CPU: meshes following more than 70 bones, camera-facing or sorted meshes,
objects lit by point or dynamic lights, and objects under shroud, jamming, frozen or heat vision
overlays.
* The `CONTRA_GPU_SKINNING` environment variable helps track down rendering faults: `0` turns it
off, `1` limits it to the shadow map, `2` adds main view skins without shadow or highlight passes,
and `3` (the default) covers everything.

### Flip model presentation

The Direct3D 9 build runs on a Direct3D 9Ex device. Windowed and borderless modes present through a
flip model swap chain, which skips the desktop compositor's copy and lets a borderless window at
desktop resolution flip straight to the screen. MSAA still works; the scene renders to a
multisampled target that resolves into the swap chain each frame.

Notes:
* Fullscreen keeps the classic swap chain.
* Terrain and tree textures ignore the texture reduction setting.
* The `CONTRA_D3D9EX` environment variable set to `0` returns to a plain Direct3D 9 device.

### Faster translucent effects

Additive effects such as fire, glows, lasers and muzzle flashes skip depth sorting and draw in
batches after the sorted smoke and other alpha blended effects. Sorted effects that share a texture
and shader draw together instead of one draw per overlapping piece. Models with alpha blended
materials (soft texture alpha, glass, canopies) sort back to front as whole objects and draw
straight from video memory, between the sorted effects. Heavy battles with lots of smoke, fire and
translucent models keep a higher frame rate. Direct3D 9 build only.

Notes:
* Fire and glows now always show on top of smoke, even smoke in front of them.
* A translucent model's own triangles draw in file order, so a complex one seen through itself
may layer wrongly.
* Skinned translucent models and alpha tested models draw as before.
* Multiply and screen blended effects still sort with the alpha blended ones.
* The `CONTRA_BLENDSORT` environment variable helps track down rendering faults: `0` sorts every
translucent triangle and draws each piece on its own, `1` adds the additive batches and shared
draws, and `2` (the default) also sorts translucent models as whole objects.

### Laser ground glow

Each laser beam lights the terrain along its length with up to twelve dynamic lights in the beam's
colour (house coloured when the laser asks). Terrain only; units, buildings and roads are not lit.

* `LaserRef = No` - (Yes turns the glow on. Also `Lasers light the ground` in Game Options, applied
on Accept. Needs `CheckLaserRef` in `OptionsMenu.wnd` for the menu control.)

Colour and intensity come from `GameData.ini`, overridable per `W3DLaserDraw` module with
`GroundGlowColor` and `GroundGlowIntensity`. A non-zero module value wins, then `GameData.ini`, then
the default:

* `LaserGroundGlowColor = R:0 G:0 B:0` - (Light colour. Black derives it from the beam layers'
colours weighted by width, times the laser texture's average colour, normalised to full brightness.)
* `LaserGroundGlowIntensity = 70%` - (How strongly the colour is added to the ground.)

Notes:
* Each light's reach is the laser's `OuterBeamWidth`, or `GroundGlowRadius` on the module, at least
15, plus a 6 unit soft edge. Long beams spread and dim their twelve lights.
* The terrain takes up to 64 dynamic lights a frame, of all kinds.

# ParticleSystem.ini

## ConformToTerrain

* `ConformToTerrain = No` - (Default. `Yes` opts a single effect into terrain conforming.)

Ground aligned, non-billboarded, flat particles are built as a mesh from the terrain heightmap, like
decals, instead of a flat quad that cuts through hills.

Notes:
* Opt-in per effect; retail effects are unchanged.
* Cost grows with particle size squared. Past 160 cells per side the mesh samples every Nth cell.

# GameData.ini

## Subdual damage defaults

Subdual, jamming, frozen and chrono tuning can be set once in GameData.ini instead of on every
ActiveBody. Any number of `SubdualDamageDefaults` blocks, each optionally limited by `KindOf` /
`ForbiddenKindOf`, with values relative to max health:

```
SubdualDamageDefaults
  SubdualDamageCap        = MaxHealth * 2
  SubdualDamageHealRate   = 500
  SubdualDamageHealAmount = MaxHealth / 16.25
End
```

* An ActiveBody key still wins; `SubdualDamageCap = 0` still means immune.
* An ActiveBody without the key falls back to the global block instead of zero.
* `MaxHealth` forms work on ActiveBody too, which also gains `ChronoDamageHealRate` and
`ChronoDamageHealAmount`.

Details in [GameData](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/GameData#subdual-damage-defaults).

## Subdual frozen

`SUBDUAL_FROZEN` is a third subdual pool beside subdual and jamming. It disables via
`DISABLED_FROZEN` and sets a `FROZEN` condition state. Customisable like jamming:

* `FrozenDamageCap`, `FrozenDamageHealRate`, `FrozenDamageHealAmount` on ActiveBody or in
`SubdualDamageDefaults`
* armor coefficient
* `Frozen` health-bar icon in Animation2D.ini
* `SoundFrozen` / `SoundUnfrozen` per unit, `UnitFrozen` / `UnitUnfrozen` in MiscAudio.ini as
fallback
* `FrozenOverlay*` texture block in GameData.ini (stacks with the jamming overlay)

Details in [Subdual Frozen](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Objects-&-Modules#subdual-frozen).

## BatchParticles

* `BatchParticles = No` - (Default. `Yes` draws consecutive particle systems that share a texture,
blend mode and billboard mode in a single call.)

Batches plain particle systems into one 512-point buffer, 15 to 30 percent cheaper per
TheSuperHackers. Streak, volume and terrain-conforming systems are never batched.

Notes:
* Particle systems with nothing on screen are now skipped regardless of the option.
* Ported from TheSuperHackers commit `de20ae0cb` (Ronin and Mauller).

## SkipTranslucencySort

* `SkipTranslucencySort = No` - (Default. `Yes` draws translucent triangles in submission order
instead of depth-sorting them every frame.)

Skips the per-frame CPU sort of every translucent triangle, which is costly with many particles.
The cost is layering errors where translucent effects overlap. Opaque and alpha-tested geometry is
unaffected.

Notes:
* Also settable per detail level in GameLOD.ini (`StaticGameLOD` blocks). Sorting is skipped when
either the GameData key or the active level says `Yes`.
* No Options menu checkbox.

## BackToFront

* `BackToFront = No` - (Default. `Yes` draws whole particle systems farthest first when
`SkipTranslucencySort = Yes`.)

Orders whole particle systems by distance, which removes most layering errors between effects.
Particles within one system, streaks, tank tracks and water are not reordered.

Notes:
* Does nothing while `SkipTranslucencySort = No`.
* `BatchParticles` batches get slightly smaller.

## ForceFireAllWeapons

* `ForceFireAllWeapons = No` - (Default. `Yes` makes an attack ground order fire every weapon that
can hit the ground, on every turret, instead of only the primary weapon.) Goes in the `AIUpdate`
block next to `TurretsLinked`.

Unlike `TurretsLinked`, this only affects attack ground and skips weapons that cannot hit ground.
Each turret aims and fires its own ground weapon when ready and in range.

Notes:
* Attacking a unit or building still picks the single best weapon.
* Synced slots still follow their lead; a locked weapon (attack ground special powers) fires alone.
* Weapons beyond the lead skip `PreAttackDelay`, as with linked turrets.
* Slots whose `AutoChooseSources` excludes `FROM_PLAYER` are skipped for player orders.

## NoOccupantFriendlyFire

* `NoOccupantFriendlyFire = No` - (Default. `Yes` spares the container a passenger is riding in from
that passenger's own splash damage.)

Stops e.g. a Tank Hunter in a bunker knocking down its own bunker.

Notes:
* Covers the whole containment chain (infantry in a bunker on an Overlord spare both).
* `RadiusDamageAffects = SELF` weapons (suicide attacks) still destroy the container.
* A direct order to attack the container still damages it.
* Changes the simulation; replays must use the same setting.

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

A container loses the penalty in proportion to how full it is, counted in slots, and regains it as
passengers leave.

```
GameData
  TransportLoadSpeedPenalty        = 40%
  TransportLoadAccelerationPenalty = 25%
End
```

A container's own `LoadPenaltyKindOf` and `LoadPenaltyForbidKindOf` replace the global `KindOf`
keys. Per-container overrides and opt-outs: see
[Load slowdown from occupants](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Objects-&-Modules#load-slowdown-from-occupants).

Notes:
* Damaged values (`SpeedDamaged`) scale by the same amount; no double penalty.
* Survives locomotor changes (`SET_NORMAL_UPGRADED`, `SET_PANIC`).
* Immobile containers (garrisoned buildings) are unaffected.
* A loaded transport slows its group like any slow unit.
* Changes the simulation; replays must use the same setting. At `0%` nothing changes.

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

# ObjectCreationList.ini

## Attack nugget: FireRegardlessOfOrders

* `FireRegardlessOfOrders = No` - (Default. `Yes` fires every `NumberOfShots` no matter what the
unit is ordered to do meanwhile.)

Normally new orders cancel the `Attack` nugget's barrage. With `Yes` the shots are queued on the
unit and fired from the launch bone whenever `WeaponSlot` is ready, while the unit stays fully
controllable. The delivery decal stays until the last shot.

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
* No turning or aiming, so do not use a turret weapon. `PreAttackDelay` is ignored.
* Range is not checked.
* Entering a container that blocks passenger fire drops the rest.
* Timing follows the weapon (`DelayBetweenShots`, clip reloads). Running dry with
`AutoReloadsClip = No` drops the rest.
* Firing again replaces the queue. Death drops it. Save/load keeps it.

# Animation2D.ini

## Texture

An `Animation` frame can name a texture file directly with `Texture = <file> [width height]`
instead of a `MappedImage`. Quickest way to add a one-image icon: drop a `.tga` in `Art\Textures\`
and reference it.

* `Texture = jammer.tga` - (The file to draw. Width and height default to `32 32`.)
* `Texture = jammer.tga 64 64` - (Explicit draw size. Match the file's real pixel size, or the
image is scaled to fit.)

Notes:
* `Texture` and `Image` lines can be mixed; `NumberImages` counts both.
* The file name is the image name, shared across animations; an existing `MappedImage` of that name
is reused.
* A `.dds` of the same name wins over a loose `.tga`.
* Static icon: `NumberImages = 1`, `AnimationMode = ONCE`, `AnimationDelay = 0`.
* Presentation only; replays unaffected.

# Turret modules

## MaxPhysicalPitch

* `MaxPhysicalPitch = 90` - (Default. The highest pitch a turret with `AllowsPitch = Yes` will aim at,
in degrees.)

Counterpart of `MinPhysicalPitch`. Stops a vehicle's cannon pointing skyward at out-of-range
targets.

# RiderChangeContain

Two opt-in fields on the combat bike contain module, both default `No`.

## SurviveScuttle

* `SurviveScuttle = No` - (Default. `Yes` keeps the transport alive after its rider dismounts.)

The abandoned bike topples into its `ScuttleStatus` pose and stays. A new valid rider stands it back
up.

Notes:
* Not box-selectable, but can be right-clicked, so a rider can be ordered onto it.
* Enemies can still destroy it; only its owner can mount it.
* `ScuttleDelay` is unused.

## SilentScuttle

* `SilentScuttle = No` - (Default. `Yes` scuttles the bike without announcing a loss.)

A dismount scuttle no longer triggers "Unit Lost", a radar ping or an "under attack" warning. A bike
killed by an enemy still announces. Kill credit and score are unchanged. Not needed with
`SurviveScuttle = Yes`.

# FireWeaponWhenDamagedBehavior

## NoHealthLoss

* `NoHealthLoss = No` - (Default. `Yes` refunds the health taken by any hit that passes the
`DamageTypes` and `DamageAmount` gate.)

For using the module as a trigger (e.g. a 1 point `MELEE` hit) without the trigger hit costing
health.

Notes:
* Every qualifying hit is refunded, even when no weapon fires.
* Qualifying hits can no longer kill the object or change its damage state.
* The hit still plays damage effects and raises "under attack".
* Other damage types and smaller hits drain health normally.

# StealthUpdate

## New StealthForbiddenConditions

New values for `StealthForbiddenConditions` (and `OverrideStealthForbiddenConditions` on
`StealthUpgrade`):

* `RIDERS_FIRING_PRIMARY`, `RIDERS_FIRING_SECONDARY`, `RIDERS_FIRING_TERTIARY` - no stealth while a
passenger fired that slot this frame or last. Only for `PassengersAllowedToFire = Yes`. Unlike
`RIDERS_ATTACKING`, they reveal only on shots, so the container can re-cloak between them.
* `UNIT_CREATED` - no stealth in the frame it finishes producing a unit (production queue, spawner
or drone release, dozer finishing a structure).

Each breaks stealth for one frame; `StealthDelay` decides when it can cloak again.

# New CommandButton Commands

## HOLD_FIRE

Suppresses **automatic target acquisition only**; explicit attack orders still fire. Covers the
unit, its addons and sub-turrets, and contained infantry.

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

Note: garrisoned buildings cannot hold fire (most have no AI module). Infantry inside a *unit* are
covered.

## TOGGLE_FIRE_WEAPON

Like `FIRE_WEAPON`, but a second click stops it.

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
* The button reflects the unit's real state, switching off once `MaxShotsToFire` is spent.
* Stopping ends only the firing; a move order survives.
* Stays clickable while reloading between shots.
* In a mixed selection, if any unit is firing, the click stops all.

## AUTO_FILL

Selects nearby infantry and orders them into the selected container.

```
CommandButton Command_AutoFill
  Command       = AUTO_FILL
  Options       = OK_FOR_MULTI_SELECT
  TextLabel     = CONTROLBAR:AutoFill
  ButtonImage   = SNAutoFill
  DescriptLabel = CONTROLBAR:TooltipAutoFill
End
```

Takes uncontained infantry not already in the group, nearest first. They break off whatever they are
doing.

## Queue reorder

Off by default; enable with `QueueReorder = Yes` in the `GameData` block of GameData.ini.

* Ctrl+click a queue cameo moves it one place earlier. The displaced entry restarts its build time;
finished units of a batch stay finished.
* No effect on the first entry or a finished unit waiting to exit.
* Plain click still cancels; Shift+click still cancels all of that type.

# Structure Multi-Select

Structures of one type can be selected together.

* The select-matching key (E by default) picks same-type structures on screen, or map-wide with Alt.
* Shift+click and Shift+double click add a same-type structure.
* Shift plus a team number (0 to 9) adds that team if it holds only that structure type.
* Mixing with units or other structure types still replaces the selection.

The bar shows shared commands. Build and upgrade commands every producer has are enabled even without
`OK_FOR_MULTI_SELECT`. The queue shows the nine entries closest to finishing.

* Click a unit or upgrade: queue it on the producer that finishes first.
* Shift+click a unit: queue five, spread across producers, stopping at money or unit limits.
* Shift+click an upgrade: queue it on the five fastest producers.
* Shift+click a queue entry: cancel that type everywhere.

# New Behavior Modules

## TimeOfDayOverrideUpdate

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

# Drag Selection

## EasyMilitaryDrag

* `EasyMilitaryDrag = No` - (Yes leaves builders out of a drag selection, so boxing over a base picks
up the army without dragging workers along.)

Builders are `KINDOF_DOZER` and `KINDOF_IGNORES_SELECT_ALL`, as for Select All.

Notes:
* Ctrl while dragging selects **only** builders.
* A drag that would select only structures ignores the filter.

# Debug and Cheat Features

Only in builds made with `RTS_DEBUG_CHEATS=ON`.

## Debug name overlays

Names drawn above every object on screen, props and wreckage included.

* `Ctrl + [` - (Cycles three ways: off, the object's template (INI) name in white, then the
model's sub object names in green as well.)
* `Ctrl + ]` - (Particle systems running on that object, in blue, with the FXList that
spawned them in amber)
* `Ctrl + '` - (The `CommandSet` the object uses, in yellow)
* `Ctrl + ;` - (The weapons the object is armed with, in red, under the command set)
* `Ctrl + /` - (The `Armor` the object currently uses, in light blue, under the weapons)

* Sub object names (hull, turret, wheels, ...) are for `ShowSubObject` / `HideSubObject`. Up to 16,
then "and N more".
* Command set, weapons and armor are read from the live object, so they show the set or block the
engine actually picked. Empty cases show `<none>`, `<no weapons>` or `<no armor>`.
* Weapons are listed as in a `WeaponSet` block, e.g. `PRIMARY NapalmMissileWeapon`.

Also bindable in `CommandMap.ini` as `CHEAT_SHOW_OBJECT_NAME`, `CHEAT_SHOW_PARTICLE_NAMES`,
`CHEAT_SHOW_COMMAND_SET`, `CHEAT_SHOW_WEAPON_SET` and `CHEAT_SHOW_ARMOR_SET`.

* `ParticleNameLingerMS = 0` - (Options.ini. Milliseconds a particle name stays on screen after its
system has gone. 0 or absent shows names only while the system is alive.)

Notes:
* Without a linger, one-shot effects flash past unreadably.
* The particle overlay is expensive; leave it off when not in use.

## Other cheat hotkeys

* ``Ctrl + ` `` - (Instant build, +999999 credits, this general's own sciences, max rank,
reveals the map)
* `Ctrl + \` - (Toggles rendering off and on; the simulation keeps running)
* `Shift + Ctrl + Z` - (Toggles the camera zoom limit)
* `Ctrl + F1` - (Toggles back face culling, drawing every face double sided when off. Contra also
binds this key to `SAVE_VIEW1`, so it saves camera bookmark 1 too. `CHEAT_TOGGLE_FACE_CULLING` in
CommandMap.ini)
* ``Shift + Ctrl + ` `` - (Cycles `HealthBarDisplayMode` live)

Notes:
* The health bar cycle is **not** a cheat and works in a normal release build too.
* A second press of the combined cheat resets rank via `resetRank()`, wiping purchased sciences.
* Sciences granted are only this general's own tree.

# Generals Online (experimental)

Optional build with the [Generals Online](https://github.com/GeneralsOnlineDevelopmentTeam/GameClient)
online system, the community GameSpy replacement: login, lobbies, matchmaking and stats over REST +
WebSocket, peer to peer over Valve's GameNetworkingSockets (ICE with STUN/TURN).

* Off by default; a normal build compiles all of it out.
* `-DRTS_BUILD_GENERALS_ONLINE=ON` replaces the GameSpy path: Online runs GO's version check and
login, and the WOL screens become GO's. LAN and skirmish are unchanged.

Notes:
* Needs a Generals Online backend; without one login fails and returns to the main menu. Using the
official `playgenerals.online` service with Contra needs coordination with the GO team.
* Upstream crash reporting, anti-cheat and hardware fingerprinting are compiled out.
* See `PORT_NOTES.md` beside the GeneralsOnline sources for details and deviations.

## Engine differences in the online build

These apply to the whole executable, skirmish and campaign included.

### 60 Hz simulation

Logic runs at 60 fps, matching the official `gen_online_60hz` client. Speeds, reloads and
millisecond durations are unchanged. Raw-frame code (turret turn rates, death arcs, particle
keyframes, script timers, deploy animations) keeps retail timing via a 30 Hz counter.

* Replays only play back in the same build; 30 Hz and 60 Hz replays are rejected by each other.
* The lowest render frame rate cap is 60.

### Frame rate cap during online matches

During an online match the render cap comes from Generals Online's `settings.json`. After the match,
the cap, FPS limit switch and scroll speed return to Contra's values. Shell, skirmish and campaign
never use the GO values.

### Observer overlay

Spectators get extra player columns (science points, kills, losses, power surplus in red when short,
army name) and a notification feed for promotions, power and superweapon use, rank 3 and 5, 10k/min
income, GLA power gains, and losing all dozers and command centers. The control bar toggle hides
both.

`Options.ini` keys, all optional:

| Key | Default | Meaning |
|---|---|---|
| `ObserverNotificationFontSize` | `10` | Feed font size; `0` disables the feed |
| `ObserverNotificationSpecialPowerUsage` | `yes` | Announce power use |
| `ObserverNotificationSpecialPowerPurchase` | `yes` | Announce promotions |
| `ObserverNotificationMilestone` | `yes` | Announce rank, income, power and dozer milestones |

### Widescreen layouts and camera

The UI targets 1280x720 instead of 800x600, and max camera height scales with aspect ratio. Window
layouts come from `GeneralsOnlineGameData\` when present. Without it, the control bar and sliders
draw at the wrong scale; with it, the options menu lacks Contra's controls. See `PORT_NOTES.md`.

### Replay analytics

`-headless -replay <file> -exportStats` writes a gzipped JSON match summary next to the replay, and
`-statsUrl <url>` posts it. Only works in headless replay simulation.

## Runtime requirements

An `RTS_BUILD_GENERALS_ONLINE=ON` build needs these DLLs **next to the executable**, or it fails at
startup with "the application was unable to start correctly". The build copies them automatically;
this matters when copying the exe by hand.

| File | Provides |
|---|---|
| `GameNetworkingSockets.dll` | Valve GameNetworkingSockets - the peer to peer transport |
| `libprotobuf.dll` | Protocol Buffers, used by GameNetworkingSockets |
| `abseil_dll.dll` | Abseil, used by Protocol Buffers |
| `libcrypto-3.dll`, `libssl-3.dll` | OpenSSL 3 - TLS for the service and DTLS for peer traffic |
| `libcurl.dll` | HTTP and the WebSocket client |
| `zlib1.dll` | compression, used by libcurl |
| `discord-rpc.dll` | Discord rich presence (optional at runtime, but the import is not) |

* All are 32 bit and must come from one matching set. A mismatched `libprotobuf.dll` and
`abseil_dll.dll` corrupt the heap before `WinMain`; replace them all together.
* Some GO installs ship a **64 bit** `zlib1.dll`, which breaks `libcurl`. Use the 32 bit one.

A normal Contra build needs none of these.
