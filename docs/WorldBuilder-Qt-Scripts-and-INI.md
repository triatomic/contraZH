# Script editor

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

# map.ini and INI editing

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

