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

## Scripts as JSON

* Export Scripts saves `.json` as well as `.scb`; pick **Script JSON** in the save dialog.
* Import Scripts opens either kind. A `.json` file imports exactly like the `.scb` it came from, so
scripts can be edited in a text editor and brought back.
* An unedited `.json` converts back to the original `.scb` byte for byte, so `SkirmishScripts.scb`
and other script files survive the round trip.

## Editing a script file

Import Scripts asks whether to edit the file or merge it into the map.

* Edit - (Replaces the map's players, teams and scripts with the file's, as one step Undo reverses.
Every player the file names is created, with the faction its name implies, such as
`SkirmishAmerica` to `FactionAmerica`. Export Scripts then offers to save straight back to the
file, in its own format.)
* Merge - (Adds the scripts to the map's players. A team whose owner player is missing asks for a
new owner.)

Edit works best on an empty map, because objects on the map keep their old teams.

Every import ends with a report of the players it created and each team whose owner player was
missing, with where that team went.

Editing the JSON:
* Each script holds `or`, a list of OR clauses. The conditions inside one clause are AND-ed, as in
the editor.
* A new condition or action needs only `name` and `parameters`, for example
`{"name": "QUICK_GAME_ENABLED", "parameters": []}`. The game matches it by name.
* A comparison parameter's `comparison` name (`GREATER_EQUAL` and the like) wins over its `int`.
* Leave `nameTable` alone; new names are added to it on import.
* A file that fails to convert shows why, with the line for a JSON syntax error, and imports
nothing.

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
`Water.ini` again. Offers to create `map.ini` when the map has none. `AdditiveBlending` switches
between the old additive water and shader water; enable it for the old water.)
* Water tuning > Terrain and sky - (The second tab tunes the `TerrainGlint`, `GroundNoise`,
`TerrainHeightBlend` and `SkyCloud` keys of [Direct3D 9 Features](dx9feat.md)
the same way. A changed key is saved to the `GameData` block of `map.ini`, which the game reads per
map; the reset button returns the map to `GameData.ini`. An effect that is off under Level Of Detail >
FX Shaders shows no change.)

The same editor opens any INI file from its own File menu. It completes names from the game's INI
data, narrowed to the values each key takes, and Ctrl+Space opens a searchable picker. It also
checks the file's structure.

