# Selection and objects

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
rebuild count and flags. Follow Object moves the view to the selected entry. Import reads only the
`SkirmishBuildList` matching the player's faction side, so an `AIData.ini` with every general's list
fills each player in a skirmish script file with its own list. A file with one list imports it into
any player.)
* Select Similar works on roads and bridges.

