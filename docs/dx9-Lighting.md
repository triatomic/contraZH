# Shadow mapping

Sun shadows from a shadow map replace stencil volumes on vehicles and buildings and blob decals
under infantry. Shadows match their object's shape, alpha cutouts included, fall on everything, and
have soft edges. Needs the Direct3D 9 build and a shader model 2 card.

* `ShadowMap = Yes` - (No restores stencil volumes and blob decals. Also `Shadow mapping` on the
Shaders page, applied on Accept. Needs `CheckShadowMap` in `OptionsMenu.wnd` for the menu control.)
* `ShadowMapResolution = 4096` - (The shadow map's width and height: 512, 1024, 2048 or 4096. Other
values round up to the next of these. Also `Shadows` on the Shaders page of the Options menu, applied
on Accept without a restart. Larger gives sharper edges and uses more video memory, 64 MB at 4096.
Needs `ButtonShaders` and `WinShaders` in `OptionsMenu.wnd`, which `build/add_shaders_wnd.py` adds;
`build/move_shader_checks_wnd.py` moves the shader checkboxes onto that page.)

`3D Shadows` and `2D Shadows` still pick the casters: 3D for volume-shadow objects (vehicles,
buildings, trees), 2D for decal-shadow objects (mostly infantry). Both off means no shadows.

* `ShadowsAlwaysOn = No` - (Yes lets objects with no `Shadow` in their Draw module, such as chain
link fences, cast into the shadow map. Needs `3D Shadows`. With shadow mapping off they stay
shadowless. Set in the mod's `GameData.ini`, read at launch.)

Models that only have alpha or translucent meshes, or too many vertices for a stencil volume, now
cast into the shadow map instead of casting nothing.

Notes:
* Only decals whose texture name starts with `shadow` are replaced; other shadow-type decals keep
drawing.
* Shrouded and stealthed units cast no shadow.
* Additive and glow passes cast nothing. Opaque meshes cast solid even with an alpha channel.
* The map follows the ground in view, so detail drops when zoomed out. When the
camera tilts toward the horizon, the map keeps the ground nearest the camera and distant shadows
fade.
* The sun is kept at least `ShadowMapMinSunElevation` degrees high (default 30, set in the mod's
`GameData.ini`; 0 disables). At 30 degrees shadows reach at most about 1.7 times the caster's height.
* Terrain casts too, but only terrain loaded around the camera, and not in flat terrain mode.
* Bridges cast as well as receive, with railings and trusses cut out by their texture.
* Objects whose shadow cannot reach the screen are left out of the map.
* Units and buildings also darken under the drifting cloud shadows, like the ground beneath them.
This follows the cloud map setting and is off at night.

# Specular highlights

Vehicles, structures and bridges get a per-pixel sun highlight, following the map's sun, brighter on bright
texture areas, and hidden in shadow when shadow mapping is on. Infantry stay matte. Needs the
Direct3D 9 build and a shader model 2 card.

* `Specular = Yes` - (No turns highlights and the terrain glint off. Also `Specular highlights` on the
Shaders page, applied on Accept. Needs `CheckSpecular` in `OptionsMenu.wnd` for the menu
control.)

Tuned in the mod's `GameData.ini`:

* `UnitSpecularIntensity = 0.35` - (How bright the highlight is. 0 turns it off.)
* `UnitSpecularPower = 24` - (How tight it is. Higher values give a smaller, sharper highlight.
Around 4 to 128 is useful; past that the highlight shrinks to nothing.)

`SpecularDebug = Yes` in `Options.ini` tints covered surfaces magenta and shows the highlight 8x
brighter.

# Terrain glint

The ground glints where it mirrors the sun towards the camera, so it lights up when the view faces
the sun. The glint takes the map's sun colour, so night maps glint faintly, and shadows and clouds
hide it. Roads and the third texture where three meet glint with the terrain, and terrain normal
maps break it up. Needs the Direct3D 9 build and shader model 2.0a.

* Follows `Specular` above. No turns both off.

Tuned in the mod's `GameData.ini`:

* `TerrainGlintIntensity = 0.25` - (How bright the glint is. 0 turns it off.)
* `TerrainGlintGloss = 12` - (How tight it is. Higher values give a smaller, sharper glint, and 1
spreads it over all ground facing the sun.)
* `TerrainGlintAlbedo = 0.5` - (How far the glint follows the ground's brightness. 0 glints dark and
bright ground alike, and 1 leaves dark ground almost dull.)

Each terrain texture can set its own in `Terrain.ini`, and where two textures blend, their glints
blend too:

```
Terrain SnowFlatType1
  Texture = TSSnow01a.tga
  Class = SNOW_FLAT
  GlintStrength = 2.0
  GlintGloss = 10
End
```

* `GlintStrength = 1.0` - (Multiplies `TerrainGlintIntensity` for this texture. 0 leaves it dull.)
* `GlintGloss` - (This texture's gloss. Left out, it takes `TerrainGlintGloss`.)

| Ground | GlintStrength | GlintGloss |
|---|---|---|
| Grass, field | 0.3 | 4 |
| Sand, desert | 0.8 | 6 |
| Rock | 0.6 | 10 |
| Snow | 2.0 | 10 |
| Asphalt, concrete | 1.0 | 16 |
| Metal | 3.0 | 48 |

Notes:
* Flat terrain mode and water reflections do not glint. Ground under standing water loses it below
the waterline.
* Roads glint by the `GameData.ini` keys alone.
* `Terrain.ini` changes apply when a map loads. Executables and WorldBuilders from before these keys
reject a `Terrain.ini` that has them.
* Launch with `CONTRA_TERRAINGLINT=0` to turn it off.

# Surface detail (normal mapping)

Bump detail in sunlight on vehicles, structures, bridges, terrain and roads. On units, structures
and bridges it shades both diffuse light and the specular highlight, fades in shadow, and costs no
extra draw. Infantry stay flat. Needs the Direct3D 9 build and shader model 2.0a; other cards get plain highlights and
flat terrain.

* `NormalMaps = Yes` - (No turns the detail off. Also `Surface detail` on the Shaders
page, applied on Accept. Needs `CheckNormalMaps` in `OptionsMenu.wnd` for the menu control.)

Units, structures and bridges use a normal map when one exists and otherwise derive bumps from
texture brightness (light = raised, so painted markings emboss too). A normal map sits beside its texture in
`Art\Textures` with `_nrm` added, e.g. `avtank_nrm.dds` for `avtank.tga`:

* Tangent space, in the DirectX convention (green points down the texture).
* DDS only, DXT5 or uncompressed. Other formats are not looked for.
* Laid out on the same UVs as the texture it belongs to.

Tuned in the mod's `GameData.ini`:

* `UnitBumpHeight = 0.15` - (How far, in world units, full brightness rises on unit and structure
textures without a normal map. Bridges take `RoadBumpHeight` below instead. 0 leaves them flat,
so only textures with normal maps get detail. The bumps come from a slightly blurred copy of the
texture and stay smooth up close, and sharp brightness edges such as paint lines and team colour
borders emboss only softly.)
* `UnitNormalMapStrength = 1.0` - (Scales the tilt of authored normal maps. Above 1 exaggerates
them, below 1 softens them.)

Terrain uses normal maps only, never derived bumps:

* Named after the `Terrain.ini` texture, e.g. `NTGrass1_nrm.dds` for `NTGrass1.tga`.
* In `Art\Textures`, not `Art\Terrain` beside the texture.
* At least as large as the part of the texture the game reads; simplest is the same size.
* Terrain without one stays flat.
* The third texture where three meet bumps with the rest.
* Flat regardless: flat terrain mode, water reflections.

Roads use a normal map when one exists and otherwise derive bumps from brightness, as units do:

* Named after the `TerrainRoads.ini` texture, e.g. `TRStreet_nrm.dds` for `TRStreet.tga`.
* In `Art\Textures`, laid out on the road texture's UVs, in the units' format above.
* Normal map strength follows `TerrainNormalMapStrength`, and `NormalMapDebug` shows both kinds.
* Derived bumps read the texture's green channel, which tracks brightness on grey roads.
* Flat in water reflections.

* `RoadBumpHeight = 1.1` - (How far, in world units, full brightness rises on road and bridge
textures without a normal map. 0 leaves them flat. Lane markings emboss softly, as paint does on
units.)

* `TerrainNormalMapStrength = 2.0` - (Scales the tilt of terrain normal maps. Terrain is seen
from further away than units, so it defaults stronger.)

`NormalMapDebug = Yes` in `Options.ini` paints terrain flat grey with only the bump shading, 4x
stronger.

# Glow masks

Vehicles and structures can have lit windows, lamps and exhausts that ignore sunlight, shadow and
cloud, and shine brighter at night. With bloom on, the glowing parts also bloom. Needs the Direct3D
9 build and a shader model 2 card. They draw with the specular pass, so `Specular = No` turns them off too.

A glow mask sits beside its texture in `Art\Textures` with `_emi` added, e.g. `abbarracks_emi.dds`
for `abbarracks.tga`:

* Black where nothing glows; the colour is the light added on top of the lit texture.
* DDS only, laid out on the same UVs as the texture it belongs to.
* Textures without one cost nothing.

Tuned in the mod's `GameData.ini`:

* `UnitEmissiveIntensity = 0.5` - (How bright the masks are by day. 0 turns them off by day.)
* `UnitEmissiveNightIntensity = 1.5` - (The same at night.)

Notes:
* Skinned meshes glow but do not bloom.
* Meshes with a glow mask are not hardware instanced while bloom is on.

# Per-pixel dynamic lights

Explosion flashes, muzzle flashes, laser glow and other dynamic lights are drawn per pixel, so they
light the ground in smooth circles and follow its bumps. Roads, bridges, vehicles, structures and
infantry near them are lit the same way. Needs the Direct3D 9 build and shader model 2.0a; other
cards keep the old lighting.

* `DynamicLights = Yes` - (No turns off every dynamic light: explosion and muzzle-flash pulses, laser
ground glow and the police car's lights. Also `Dynamic lights` on the Shaders page.
Needs `CheckDynamicLights` in `OptionsMenu.wnd` for the menu control.)
* `PixelLights = Yes` - (No keeps dynamic lights on the old per-vertex lighting. Also `Per-pixel
lights` on the Shaders page, greyed out while dynamic lights are off. Needs
`CheckPixelLights` in `OptionsMenu.wnd` for the menu control.)

Notes:
* Every draw picks its own lights, so a busy battle can light the whole screen per pixel:
  * The terrain draws in patches of 32 by 32 cells, each taking eight lights, or four with standing water.
  * Each vehicle, structure or soldier takes the eight brightest where it stands.
  * Each bridge takes eight.
  * Roads take the eight lights nearest the middle of the view.
* The lights nearest the middle of the view go first, up to 64 at once. A light whose reach covers
the middle counts as nearest.
* A light that would overfill a terrain patch stays on the old lighting, which lights the terrain
by its corners, so large ones look blocky. Objects light those past their eight the old way too.
* Infantry and other units without a sun highlight stay matte. They take only the lights.
* Roads, bridges, the third texture where three meet, and flat terrain mode had no dynamic lighting
before, so they show none without shader model 2.0a.
* Launch with `CONTRA_PIXELLIGHTS=1` to limit per-pixel lights to the ground, or `0` to turn them off.

# Ambient occlusion

Creases, corners, and the ground where units and buildings stand fall into soft shade, so objects sit
on the terrain instead of floating over it. Needs the Direct3D 9 build, shader model 2.0a and
anti-aliasing off.

* `AmbientOcclusion = Yes` - (No turns the shade off. Also `Ambient occlusion` on the Shaders
page, greyed out while anti-aliasing is on. Needs `CheckAmbientOcclusion` in `OptionsMenu.wnd` for
the menu control.)

Tuned in the mod's `GameData.ini`:

* `AmbientOcclusionRadius = 12` - (How far, in world units, geometry shades what is near it. Larger
values spread the shade wider and soften it.)
* `AmbientOcclusionStrength = 1.0` - (How dark the shade gets. 0 turns it off.)

`AmbientOcclusionDebug = Yes` in `Options.ini` shows the shade in grey in place of the terrain, units
and buildings. Water, decals, particles and the interface still draw over it.

Notes:
* The shade reads the scene's depth, which a multisampled depth buffer hides, so it is off whenever
anti-aliasing is on.
* It falls on the terrain, units, buildings and trees. Water, decals, particles and translucent models
draw over it unshaded.
* It darkens the whole colour, lit or not, including glow masks and highlights.
* Launch with `CONTRA_SSAO=0` to turn it off. `CONTRA_SOFTPARTICLES` other than 1 turns it off too,
since that also removes the readable scene depth.

