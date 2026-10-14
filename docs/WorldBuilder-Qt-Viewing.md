# Viewing the map

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

# Direct3D 9 effects

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
[Stochastic terrain](dx9-Terrain.md#stochastic-terrain).

