# Ground noise

`Ground Lighting` (`UseLightMap`) no longer multiplies the terrain by `TSNoiseUrb`, a 256 texel grey
cloud that repeats every 31 tiles. The game builds a noise texture at load instead and reads it at
three scales, each turned against the last, so no view shows the pattern repeat. Broad patches,
finer mottling and a slight warm or cool tint break up the tiling of the terrain textures. The
average brightness matches the old texture. Roads and blend tiles get the same noise, so they meet
the terrain without a seam.

Tuned in the mod's `GameData.ini`:

* `GroundNoiseStrength = 0.12` - (How far the ground's brightness strays from its average. 0 gives an
even tone.)
* `GroundNoiseSize = 1000` - (World units across the broadest patches. The finer layers are 2.2 and 5
times smaller.)
* `GroundNoiseTint = 0.03` - (How far patches lean warm or cool. 0 keeps them grey.)
* `GroundNoiseBrightness = 0.9` - (The ground's average brightness under the noise. 0.9 matches
`TSNoiseUrb`; 1.0 leaves the terrain as bright as with `Ground Lighting` off.)

Notes:
* The pattern repeats only across four `GroundNoiseSize` spans, 4000 world units at the default.
* The Direct3D 8 build and the lower terrain detail settings keep `TSNoiseUrb`.
* On cards without shader model 2.0a, terrain and road shadows turn off while `Ground Lighting` is on.

# Height blending

Where two terrain textures meet, the taller parts of each push into the other instead of a soft
10-unit fade. Stones and clumps stand proud of the texture beside them, and the edge follows them.
Three-texture blend tiles blend the same way. Needs the Direct3D 9 build.

* `HeightBlend = Yes` - (No brings back the soft fade. Also `Height blending` on the Graphics
page. Needs `CheckHeightBlend` in `OptionsMenu.wnd` for the menu control.)

Tuned in the mod's `GameData.ini`:

* `TerrainHeightBlendStrength = 2` - (How far the taller texture pushes into the other's side. 0 keeps
the edge where the fade would put it.)
* `TerrainHeightBlendSharpness = 4` - (How narrow the edge is. 1 is as wide as the soft fade.)

Heights come from `<texture>_hgt.dds` in `Art\Textures`, named after the `Terrain.ini` texture as
`_nrm.dds` is for normal maps, e.g. `NTGrass1_hgt.dds`. The game does not look for it in
`Art\Terrain` beside the texture. Its red channel is the height, black low and white high, and it
must be at least as large as the texture; simplest is the same size. Textures without one take their own brightness as height, measured against the texture's
average, so a bright texture does not simply cover a dark one.

Notes:
* The lower terrain detail settings keep the soft fade.
* Roads draw as before.

# Stochastic terrain

Painted areas of terrain break up the visible repeat of their textures. The ground is split into
hex cells, each cell shifts and turns the texture by its own random amount, and every point blends
the three nearest cells. It is the tiling the water uses under standing water, painted onto dry
ground with the WorldBuilder **Stochastic Terrain** brush (Tools menu and toolbar). Needs the
Direct3D 9 build and a pixel shader 2.0a card.

The brush's options:

* Brush Width and Feather - (Where the effect applies, and how softly it fades out at the edge.)
* Seed - (Picks the cells' shifts and turns. Randomize picks a new one. Each stroke stamps its seed
on the cells it raises, so neighbouring areas can look different. Seed 0 (shown as Random) gives
every stroke its own random seed. Where two seeds meet the change follows the cell edges, so it
shows no seam.)
* Blending Rate - (How softly the cells blend. Low gives visible patches of turned texture, high a
smooth mix. Also stamped per stroke.)

The brush shows its own overlay: a blue circle where it paints at full strength, a green one where its
feather ends, and yellow outlines round the hex cells the stroke's seed takes over. Each cell takes the
seed painted nearest its centre, so a seed changes whole cells, which can reach past the brush or miss
a small one entirely.

Painting only raises the effect, and holding Shift while dragging erases it. Painting over ground
that is already fully painted keeps its strength but stamps the new stroke's seed and blending rate,
so repainting a spot changes its look. The paint saves in the map's own `StochasticTerrain` chunk.
Older builds and the retail game skip the chunk and draw the terrain as before, and saving the map in
an older WorldBuilder drops the paint.

The cell spacing is `ShaderWaterStochasticSize` in `Water.ini`, 100 when that is 0.

Notes:
* Cliffs keep their own texturing, as under water.
* Terrain chunks with paint draw through the seabed shaders, which take three point lights instead
of eight.
* Flat terrain mode and the third texture of three-texture blend tiles draw without it.

