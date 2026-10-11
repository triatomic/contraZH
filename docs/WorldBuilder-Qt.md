# WorldBuilder Qt

A port of the Zero Hour WorldBuilder map editor from MFC to Qt. It keeps the classic workflow and
adds a dark theme, faster search and repair tools for maps made against other mods, and editors
for `map.ini` and other INI files. It builds from this repository, so it reads the same game data
and draws the same Direct3D 9 effects as the Contra game build.

It builds on Adriane's WorldBuilder, whose additions are listed in [Adriane's
changelog](https://github.com/triatomic/contraZH/blob/main/GeneralsMD/Code/Tools/WorldBuilder/docs/ChangeLog%20-%20Adriane.txt).

## Running

WorldBuilder loads game data from its own folder, so it must sit inside a Zero Hour install. Copy
the exe and the Qt DLLs next to it into the game folder and run it from there. The Qt runtime ships
with the build; no separate Qt install is needed to run it.

| Exe | Renderer |
|---|---|
| `WorldBuilderZH_Qt.exe` | Direct3D 8 |
| `WorldBuilderZH_Qt_D3D9.exe` | Direct3D 9, draws the D3D9 effects |

The Direct3D 9 exe loads its shaders from the `shaders\` folder in the game folder, the same one the
Contra game build uses. Shaders from a different build of the source tree load but draw terrain,
roads and water wrong.

## Interface

* Theme - (Dark, light, or following Windows. Stored in `WorldBuilder.ini`.)
* Tool windows remember their size and position, and stay on screen. Window > Reset Window Positions
puts them back.
* Undo history depth is a setting; the default is 64 steps.
* Long combo boxes filter as you type when "Search in combo boxes" is ticked in the Entity Finder.
The NewSearch box beside it turns on live filtering in the tree pickers.
* Shift+F5 resets the Direct3D device.
* Brush size has hotkeys, and the brush panels accept values past the slider's cap.
* Open Map shows a preview for every map, including maps packed in a `.big`. A map with no `.tga`
gets a generated one.

## Selection and objects

* Shift+Ctrl+drag over empty ground removes the boxed objects from the selection. The box draws red.
* Clicking a tree selects the tree under the cursor. Objects hidden under the ground are not picked
through it.
* Object Options - (A category header places every object in the category at once, with manual
spacing. Leaves tinted green are objects the map's `map.ini` adds; orange ones are stock objects
it redefines.)
* Object Properties - (A Listen button plays the object's attached sound. Scale sets the object's
visual size on top of the template `Scale`, for the whole selection.)
* Placing an object whose template has `InstanceScaleFuzziness` gives it a random scale in that range.
* Build List - (Replace swaps the selected building for another and keeps its position, angle,
rebuild count and flags. Follow Object moves the view to the selected entry.)
* Select Similar works on roads and bridges.

## Viewing the map

Under View > Models:

* Show Full Model - (Draws the units an object brings with it, such as the Overlord's turret or a
Stinger Site's soldiers. The game creates those at runtime, so WorldBuilder hid them before.)
* Animate Models - (Plays looping and infantry idle animations in the viewport.)
* Animation Scrubber - (Poses the selected object at any point through its animations with a slider,
so moving bones and what rides on them can be checked. A scrubbed object never plays.)

Objects a `map.ini` defines draw as the game draws them: hidden sub-objects stay hidden, and
particle-only objects show their particles. Render Particles previews particle systems live.

Sound:

* View > Listen To Map - (Plays the map's ambient sounds through the game's audio as the camera
moves: enabled ones, permanent ones, all of them, or none.)
* View > Ranges & Radii > Show Playing Sounds - (Marks the sounds currently playing.)

The Debug menu tints the terrain by pathfinding cell. Cliff shows slopes too steep to cross in red,
Water shows water cells in blue, and Objects shows structure footprints in yellow. Passability
merges the ticked layers into one red, to answer whether a unit can stand on a cell.

## Direct3D 9 effects

Level Of Detail > FX Shaders switches the game's D3D9 effects in the Direct3D 9 exe:

* Shadow Mapping - (Needs View > Models > Show Shadows. Objects cast into the shadow map and the
terrain receives it.)
* Bloom - (The only item in the Direct3D 8 exe.)
* Effect Shaders - (Flame, electric, laser and cryo, with soft particles.)
* HQ Sky Cloud Shadows - (Needs Show Clouds.)
* Terrain Normal Maps, Terrain Height Blend, Specular & Glint

Each item starts from the player's `Options.ini` and is then remembered in `WorldBuilder.ini`.
Ambient occlusion, shockwaves and laser ground glow are left out.

The Stochastic Terrain brush paints hex-cell tiling onto dry ground to hide texture repeats. See
[Stochastic terrain](dx9feat.md#stochastic-terrain).

## Script editor

* The tree filters as you type (with NewSearch on) and matches names, comments and every
parameter.
* Show chips narrow the tree to warnings, active or inactive, and difficulty.
* F2 renames, Ctrl+D duplicates and Delete deletes, after a confirmation that can be turned off.
* Ctrl+C and Ctrl+V copy conditions and actions between scripts.
* The detail pane links to the scripts a script calls and is called by. Clicking a unit or waypoint
selects it in the 3D view and centres the camera on it.
* Ctrl+H opens find and replace for parameter values across all scripts, or only the selected one.
Script names, folders and comments are never touched.
* F4 toggles the editor. Clear All empties the script list and can be undone.

A map made against another mod can name objects your game data lacks. Such a script shows them on
a [Missing] line. Clicking a name opens the Edit dialog on the row that uses it, and the object
picker preselects the closest existing name. F3 and Shift+F3 step through other close matches.

Replace Missing Unit and the Team Builder and Build List repairs use the same name matching.
Replace All shows a report of every swap, which can be corrected before applying.

## map.ini and INI editing

File > Map.ini:

* Open map.ini (internal) - (Edits the file inside WorldBuilder. Object names the game data does not
know are underlined, and right-click offers the closest matches.)
* Reload map.ini and Check map.ini - (Apply or check the file without reopening the map. The report
lists every block that failed to parse; only those blocks are dropped.)
* Auto-reload map.ini on change - (Reloads when the file is saved outside WorldBuilder.)
* Water tuning - (Tunes the map's water against the 3D view. Each row is a `WaterTransparency` key
from [Water](Water.md), stepped with the - and + buttons or typed. A changed key is saved to the
`WaterTransparency` block of the map's `map.ini`; the reset button removes it, so the map follows
`Water.ini` again. Offers to create `map.ini` when the map has none.)
* Water tuning > Terrain and sky - (The second tab tunes the `TerrainGlint`, `GroundNoise`,
`TerrainHeightBlend`, `TerrainAtlasBorder` and `SkyCloud` keys of [Direct3D 9 Features](dx9feat.md)
the same way. A changed key is saved to the `GameData` block of `map.ini`, which the game reads per
map; the reset button returns the map to `GameData.ini`. An effect that is off under Level Of Detail >
FX Shaders shows no change.)

The same editor opens any INI file from its own File menu. It completes names from the game's INI
data, narrowed to the values each key takes, and Ctrl+Space opens a searchable picker. It also
checks the file's structure.

## Map Generator

Map Generator > Generate Map builds a playable skirmish map into the open document. It lays Perlin
terrain with cliffs and textures, symmetric start positions on flat pads, equal supply sources per
player, roads between the starts, and scattered trees and rocks. Trees, rocks and textures come from
the loaded game data, so it works with any mod. The map grows when the player count needs room.
Randomize rerolls the settings. One Undo restores the previous map.

## MCP server

WorldBuilder can be driven by an AI assistant through a Model Context Protocol (MCP) server. The
server runs as `python -m tools.worldbuilder_mcp` from the repository root and talks to a running
editor. It can open maps, place and edit objects, paint terrain, and edit teams, waypoints and
scripts. The MCP menu turns the editor's side on or off. Setup is in the [MCP
readme](https://github.com/triatomic/contraZH/blob/main/tools/worldbuilder_mcp/README.md).

## Building

WorldBuilder is 32-bit and needs Qt 5.15.2 for 32-bit MSVC (`msvc2019`). Run inside an x86 MSVC
environment:

```
cmake --preset win32-qt-d3d9 -DCMAKE_PREFIX_PATH="C:/Qt/5.15.2/msvc2019"
cmake --build --preset win32-qt-d3d9 --target z_worldbuilder
cmake --build --preset win32-qt-d3d9 --target rts_shaders
```

Use the `win32-qt` preset for the Direct3D 8 exe. The [Qt build
guide](https://github.com/triatomic/contraZH/blob/main/GeneralsMD/Code/Tools/WorldBuilder/docs/QT-BUILD.md)
covers the Qt install and the debugging switches.
