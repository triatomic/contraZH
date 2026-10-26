# Debug and Cheat Features

The debug overlays and cheat commands require builds made with `RTS_DEBUG_CHEATS=ON`.
The health bar, pause and frame-step controls below also work in normal release builds.
Keys use US keyboard labels. On other layouts, use the same physical keys.
These are the engine's default bindings; `CommandMap.ini` can override them unless noted.

## Shader debug panel

`Ctrl + Shift + Tab` shows or hides a small shader panel near the upper-right corner during a
match or replay. Click a row to toggle its effect immediately. The X hides the panel; the rest
of the screen remains usable and the game keeps running.

The panel controls bloom, laser glow, shadow maps, specular highlights and emissive glow,
normal maps, water reflections, shoreline foam, soft particles, flame, electric, laser and cryo
shaders, dynamic and pixel lights, ambient occlusion, height blending, HQ sky clouds and planar mirrors.

The minus and plus buttons change bloom threshold in 5% steps and bloom size in 25% steps.
Threshold runs from 0% to 100% and requires Direct3D 9. Size runs from 25% to 400%.

Changes affect rendering only and the panel does not save `Options.ini`. Hiding the panel keeps
the chosen states. A shader disabled by hardware limits or a launch-time environment variable
cannot be enabled through the panel. The panel closes when the match or UI resources reset.
The shortcut is also bindable as `CHEAT_TOGGLE_SHADER_PANEL` in cheat builds.

## Debug name overlays

Names drawn above every object on screen, props and wreckage included.

* `Ctrl + [` - (Cycles three ways: off, the object's template (INI) name in white, then the
model's subobject names in green as well.)
* `Ctrl + ]` - (Particle systems running on that object, in blue, with the FXList that
spawned them in amber. Each label has a line to the system's emitter position.)
* `Ctrl + '` - (The `CommandSet` the object uses, in yellow)
* `Ctrl + ;` - (The weapons the object is armed with, in red, under the command set)
* `Ctrl + -` - (The `Armor` the object currently uses, in light blue, under the weapons)
* `Ctrl + /` - (The W3D model each draw module has loaded, in teal, as a callout: a leader line runs
from the model out to one side and the name sits at the end of it)
* `Ctrl + ,` - (Laser beams: the laser object's name, which for a weapon is its `LaserName`, in pink
at the middle of the beam)
* `Ctrl + .` - (Laser beams: the `W3DLaserDraw` module tags, in lavender, under the laser name)

* Subobject names (hull, turret, wheels, ...) are for `ShowSubObject` / `HideSubObject`. Up to 16,
then "and N more".
* Command set, weapons and armor are read from the live object, so they show the set or block the
engine actually picked. Empty cases show `<none>`, `<no weapons>` or `<no armor>`.
* Weapons are listed as in a `WeaponSet` block, e.g. `PRIMARY NapalmMissileWeapon`.
* Models are the ones the current condition states picked, so damaged, night and snow variants show.
A module with no model in its current state gets no label. Each leader starts at the centre of its
model's bounds and goes out on the side that model is on; labels that would overlap are pushed up
above each other. Up to 16 per object.
* Laser labels that would overlap are pushed up above each other. A second beam of the same laser
landing on an existing label is not labelled again. With a laser overlay on, the object overlays
skip laser objects, which sit on their firer.

Also bindable in `CommandMap.ini` as `CHEAT_SHOW_OBJECT_NAME`, `CHEAT_SHOW_PARTICLE_NAMES`,
`CHEAT_SHOW_COMMAND_SET`, `CHEAT_SHOW_WEAPON_SET`, `CHEAT_SHOW_ARMOR_SET`, `CHEAT_SHOW_MODEL_NAMES`,
`CHEAT_SHOW_LASER_NAME` and `CHEAT_SHOW_LASER_BEAM_BLOCK`.

* `ParticleNameLingerMS = 0` - (Options.ini. Milliseconds a particle name stays on screen after its
system has gone. 0 or absent shows names only while the system is alive.)

Notes:
* Without a linger, one-shot effects flash past unreadably. Lingering callouts point to the
last recorded emitter position.
* Delayed systems get callouts after their first world-transform update.
* The particle overlay is expensive; leave it off when not in use.

## Window name overlay

* `Shift + Ctrl + /` - (Outlines every visible GUI window in amber and labels it with its `.wnd`
name. The window under the cursor gets a cyan outline and a readout beside the cursor listing it and
its parents, root first, with its screen position and size.)

* Top level windows show the full `File.wnd:Name`; child windows show only the part after the colon.
* A label that would cover one already drawn is skipped. Hover the window to read its name.
* Also bindable in `CommandMap.ini` as `CHEAT_SHOW_WINDOW_NAMES`.

## Pathfinding overlay

The engine's AI debug overlay, which retail only had in debug builds. `Ctrl + A` cycles the modes
and names each one on screen. `DEMO_TOGGLE_AI_DEBUG` in CommandMap.ini.

* Paths - (A yellow line along the last path computed; hovering a unit switches it to that unit's
path. Each search also leaves the cells it explored: blue for the frontier, magenta for visited, red
when the search failed.)
* Terrain - (Cell types: red cliff, blue water, green impassable, magenta broken bridge, orange
rubble, yellow building, cyan pinched. Shore clearance in white where a cell touches water, cliff or
the map edge and gray one cell further in.)
* Cells - (Unit occupancy: red goal claims, white parked units, green moving units, yellow both.)
* Ground paths - (The shared path a group follows when it moves in columns.)
* Zones - (Connected zones, drawn when the zones are rebuilt.)

Notes:
* Single player only.
* Terrain mode also switches off column group moves, an original debugging hook. Judge group
movement in Cells or Paths mode.
* With the overlay on, the camera can scroll past the map edge.

## Pathfinding log

* `Keypad +` - (Starts writing `pathfinding.txt` next to the exe. `CHEAT_PATHFIND_LOG_ON`)
* `Keypad -` - (Stops writing. `CHEAT_PATHFIND_LOG_OFF`)

While on, a `PFSTAT` line lands once a second whenever the pathfinder did anything: requests waiting,
requests refused by a full queue, searches and how many stopped at the cell cap, coarse hits and
misses, cell records used, the largest search, records left, zone count, searches by kind (find,
closest, patch, move-away, attack, safe, ground), zone screen refusals, blocked repaths, and units
shoved off invalid cells. The file explains each field in its header. The first start in a session
replaces the file, later starts append.

## Other cheat hotkeys

* ``Shift + Ctrl + ` `` - (Toggles instant build for human players. When enabled, also gives the
local player +999999 credits, this general's own sciences, max rank and a revealed map.
Single player only. `CHEAT_KITCHEN_SINK` in CommandMap.ini)
* `Shift + Ctrl + Z` - (Toggles the camera zoom limit)
* `Ctrl + F1` - (Toggles back face culling, drawing every face double sided when off. Contra also
binds this key to `SAVE_VIEW1`, which this cheat build shadows, so camera bookmark 1 is not saved.
`CHEAT_TOGGLE_FACE_CULLING` in CommandMap.ini)
* `Shift + Ctrl + F1` - (D3D9 only. Lists the last frame's draw calls in the top right corner, one
line per vertex and pixel shader pair with its draw and primitive counts, busiest first. `fixed`
means the fixed function pipeline. Refreshes twice a second. Takes the key from `CHEAT_RUNSCRIPT1` in
CommandMapDemo.ini. `CHEAT_SHOW_SHADER_DRAWS` in CommandMap.ini)
* `Shift + Ctrl + Tab` - (Opens or hides the shader debug panel. Click an option to toggle its
rendering effect live. `CHEAT_TOGGLE_SHADER_PANEL` in CommandMap.ini)
* `Alt + V` - (Copies the template (INI) name of the selected objects to the clipboard, one per
line with duplicates folded, and confirms with a message. `CHEAT_COPY_OBJECT_NAME` in CommandMap.ini)

Notes:
* Disabling the combined cheat resets rank to 1 and wipes purchased sciences. It does not remove
the cash or hide the revealed map.
* The combined cheat grants only this general's own sciences.

## Individual cheats and map scripts

Contra's `CommandMapDemo.ini` supplies these bindings in cheat builds. All are single player only.

| Key | Action | CommandMap name |
|---|---|---|
| `Alt + F` | Toggle instant build for the local player. | `CHEAT_INSTANT_BUILD` |
| `Ctrl + =` | Add 10000 credits. Use the main keyboard's equals key. | `CHEAT_ADD_CASH` |
| `Ctrl + R` | Reveal the map permanently for the local player. | `CHEAT_DESHROUD` |
| `Shift + Ctrl + Alt + K` | Grant every grantable science, including other generals' sciences. | `CHEAT_GIVE_ALL_SCIENCES` |
| `Shift + Ctrl + P` | Add one science purchase point. | `CHEAT_GIVE_SCIENCEPURCHASEPOINTS` |
| `Ctrl + S` | Toggle special power cooldown delays. | `CHEAT_TOGGLE_SPECIAL_POWER_DELAYS` |
| `Shift + Ctrl + Space` | Switch control to the next non-neutral player. | `CHEAT_SWITCH_TEAMS` |
| `Shift + Ctrl + X` | Kill the selected objects. | `CHEAT_KILL_SELECTION` |
| `Alt + G` | Toggle Hand-of-God mode, which kills an object when you select it. | `CHEAT_TOGGLE_HAND_OF_GOD_MODE` |
| `Alt + H` | Toggle the object health debug display. | `CHEAT_SHOW_HEALTH` |
| `Alt + T` | Toggle on-screen message text. | `CHEAT_TOGGLE_MESSAGE_TEXT` |
| `Shift + Ctrl + F2` through `F9` | Run the map script `KEY_F2` through `KEY_F9`, if present. | `CHEAT_RUNSCRIPT2` through `CHEAT_RUNSCRIPT9` |

`Shift + Ctrl + F1` opens the shader draw list instead of running `KEY_F1`. Rebind
`CHEAT_RUNSCRIPT1` to another key to run that script.

## Camera and scene controls

* `Delete` - (Cycles Default, Free, Chase, Perspective and Orthographic camera modes, then returns
to Default. Chase and Perspective need a selected object and are skipped without one.
`CHEAT_CYCLE_CAMERA_MODE` in CommandMap.ini)
* `Ctrl + Home` - (Cycles the skybox through the map default, Morning and Moon. Forces the sky to
draw. `CHEAT_CYCLE_SKYBOX` in CommandMap.ini)
* `Ctrl + End` - (Cycles terrain rendering through normal, black background and green screen.
Hides terrain, roads, trees, shroud, water and sky while keeping units visible.
`CHEAT_CYCLE_TERRAIN_MODE` in CommandMap.ini)

Camera modes hide the HUD and clear terrain shroud without revealing hidden enemy units.
The camera and scene controls affect only this client's view and also work in multiplayer and replays.
See [Camera Controls](CameraControls.md) for movement, mouse gestures, sun lighting and `CapsLock`
selection controls.

## Controls available without cheats

* ``Ctrl + ` `` - (Cycles `HealthBarDisplayMode` through Classic, Damaged and Always for the current
session. Does not save to Options.ini. `CYCLE_HEALTH_BAR_MODE` in CommandMap.ini)
* `Ctrl + \` or `Shift + P` - (Pauses or resumes a single player game without opening the Esc menu.
Sound stops, music plays on, and the camera still moves. `TOGGLE_PAUSE_ALT` in CommandMap.ini)
* `Shift + O` - (Advances one simulation frame, then pauses. Single player only.
`STEP_FRAME_ALT` in CommandMap.ini)

Observers can also use `P` to pause and `O` to step one frame.

## Debug views

The Graphics page of the Options menu has five debug view checkboxes: `Bloom debug view`,
`Specular debug view`, `Normal map debug view`, `Lights debug view` and `Occlusion debug view`.
Each one draws a single render pass alone so it can be judged; the shader pages describe what each
shows. A release build hides the five checkboxes and ignores their `Options.ini` keys (`BloomDebug`,
`SpecularDebug`, `NormalMapDebug`, `DynamicLightsDebug`, `AmbientOcclusionDebug`), so a key left on
by a cheat build cannot stick in a release build.
