# Direct3D 9 Features

Graphics features that need the Direct3D 9 build. The Direct3D 8 build ignores their settings.
Options live in `Options.ini` and the advanced display options; tuning keys live in the mod's
`GameData.ini`. Bloom, vertical sync and laser ground glow work on both builds and are on
[contraZH Changes](contraZH-Changes.md#rendering).

Cheat builds reload `Data\INI\GameData.ini` about half a second after it is saved, so these tuning
keys can be adjusted with a map running: `UnitSpecularIntensity`, `UnitSpecularPower`,
`UnitBumpHeight`, `UnitNormalMapStrength`, `TerrainNormalMapStrength`, the `TerrainGlint` keys, `UnitEmissiveIntensity`,
`UnitEmissiveNightIntensity`, `SoftParticleDistance`, `AmbientOcclusionRadius`,
`AmbientOcclusionStrength`, the `GroundNoise`, `TerrainHeightBlend` and `SkyCloud` keys, `TerrainAtlasBorder` and the `Flame`, `Haze`, `Electric`, `Laser`, `Cryo` and `Disruption` tuning keys, the `SandStorm` and `SnowStorm` keys, the `Headlight` keys but `HeadlightShader`, the `PlanarMirror` keys but `PlanarMirrorShader`, and the `ColorLut` keys. Other `GameData.ini` keys keep their
value until a restart. The saved values win over a map's `map.ini` until the map loads again. A
deleted key keeps its value until a restart, and a file with an error applies only the keys above
the error until the next save.

## Shader implementation

An effect picks up its shader in one of five places: the `ParticleSystem` block, the `W3DLaserDraw`
module, a `W3DModelDraw` module, a `W3DStormDraw` module, or an `FXList` nugget. Tuning defaults live in `GameData.ini`, and some places can override
them for one effect. Each shader's own section below lists its keys.

| Effect | Shader | Set in | Turn on with | Own tuning |
|---|---|---|---|---|
| Fire sprites | [Flame](#flame-shading) | `ParticleSystem` | `FlameShader = Yes` | `Flame` and `Haze` keys |
| Fire trails | [Flame](#flame-shading) | `ParticleSystem` | `Type = STREAK` and `FlameShader = Yes` | `Flame` keys |
| Fire beams | [Flame](#flame-shading) | `W3DLaserDraw` | `FlameShader = Yes` | `Flame` keys |
| Burning, crackling or frozen models | [Flame](#flame-shading), [Electric](#electric-shading), [Cryo](#cryo-shading) | `W3DModelDraw` | `FlameShader`, `ElectricShader` or `CryoShader = Yes` | That shader's keys |
| Sparks and flares | [Electric](#electric-shading) | `ParticleSystem` | `ElectricShader = Yes` | `ElectricParticleScale` |
| Laser trails | [Laser](#laser-shading) | `ParticleSystem` | `Type = STREAK` and `LaserShader = Yes` | None |
| Laser beams | [Laser](#laser-shading) | `W3DLaserDraw` | On by default | `Laser` keys |
| Tesla and lightning bolts | [Electric](#electric-shading) | `W3DLaserDraw` | `ElectricShader = Yes` | `Electric` keys |
| Freeze rays | [Cryo](#cryo-shading) | `W3DLaserDraw` | `CryoShader = Yes` | `Cryo` keys |
| Frost trails, puffs and flares | [Cryo](#cryo-shading) | `ParticleSystem` | `CryoShader = Yes` | `CryoParticleScale` |
| Blast ring | [Shockwave](#shockwave-fxlistini) | `FXList` | A `Shockwave` block | The block's keys |
| Jammer fields and distortion auras | [Disruption](#disruption-shading) | `ParticleSystem`, `W3DModelDraw`, `W3DLaserDraw` | `DisruptionShader = Yes` or `Only` | `Disruption` keys |
| Distortion disc with no art | [Disruption](#disruption-shading) | `FXList` | A `Disruption` block | The block's keys |
| Sandstorm or snowstorm | [Storm](#storms) | `FXList`, `W3DStormDraw` | A `Storm` block or the module | `SandStorm` and `SnowStorm` keys |
| Vehicle headlights | [Headlights](#headlights) | Automatic, `W3DModelDraw` | A `HEADLIGHT` mesh in the model | `Headlight` keys, per model too |
| Mirror floors, puddles, pools and glass | [Planar mirrors](#planar-mirrors) | `W3DModelDraw` | `PlanarMirror = Yes` | `PlanarMirror` keys, per model too |
| Soft edges on sprites | [Soft particles](#soft-particles) | Automatic | Nothing | None |
| Glow around bright effects | [Bloom](contraZH-Changes.md#bloom) | `ParticleSystem` | `Shader = ADDITIVE` | None |

"None" means the effect always uses the `GameData.ini` values.

A `ParticleSystem` that leaves the shader key out is `Auto`:

* `FlameShader` turns on when the system rides a projectile whose weapon has `DamageType = FLAME`.
* `ElectricShader` turns on when the `ParticleName` texture is listed in `ElectricParticleTextures`.
* `LaserShader` turns on when a streak's `ParticleName` texture is listed in `LaserParticleTextures`.
* `CryoShader` turns on when the `ParticleName` texture is listed in `CryoParticleTextures`.

The texture lists suit a texture many systems share. `Yes` suits a single system, and `No` opts one
out of a list.

### What a particle system can take

| `Type` | `Shader` | Takes |
|---|---|---|
| `PARTICLE` | `ADDITIVE`, `ALPHA` | Flame, electric or cryo, the soft fade, and disruption |
| `PARTICLE` | `ALPHA_TEST`, `MULTIPLY` | Nothing |
| `STREAK` | Any but `MULTIPLY` | Laser, cryo, or flame with `FlameShader = Yes` |
| `VOLUME_PARTICLE`, `SMUDGE`, `DRAWABLE` | Any | Nothing |

* Cryo wins over every other shader. A system, beam or model with cryo on draws as ice.
* Flame comes next. A system, beam or model that is both flame and electric draws as flame.
* A beam with `ElectricShader = Yes` draws as electric, whatever its `LaserShader`.
* A streak takes flame only with `FlameShader = Yes`. `Auto` leaves it alone, so trails behind flame
shells keep their look.
* Only `FlameShader = Auto` follows a master system. Slave systems need their own `ElectricShader` or
`LaserShader`.
* Terrain-conforming particles stay plain.
* Disruption is a pass of its own behind the art, so it combines with any of the shaders above.

### Models and terrain

Models and terrain pick up a shader when a matching texture sits beside theirs in `Art\Textures`, or
from `Terrain.ini` for the glint. A model's `W3DModelDraw` module also switches on four effect
shaders for its translucent meshes, the ones with an additive or alpha-blended material:

* `FlameShader = No` - (Default. `Yes` shades the meshes as fire.)
* `ElectricShader = No` - (Default. `Yes` shades them as electricity.)
* `CryoShader = No` - (Default. `Yes` shades them as ice, the way a cryo sprite draws.)
* `DisruptionShader = No` - (Default. `Yes` or `Only` bends the scene behind them, and combines
with any of the three above.)

Like disruption, the three shaders only touch translucent meshes (additive or alpha-blended
materials). An opaque mesh skips the sorter and stays plain.

The module takes the six `Flame` keys from `FlameWarp` to `FlameRise`, the six `Electric` keys and
the nine `Cryo` keys from `CryoTint` to `CryoShardSize`. Each one it sets overrides `GameData.ini`
for that model alone. Opaque meshes stay as they are, and the laser shader needs a beam, so a model
cannot take it.

```
Draw = W3DModelDraw ModuleTag_01
  DefaultConditionState
    Model = EXShieldDome
  End
  ElectricShader = Yes
  ElectricArcs = 2.5
End
```

| Effect | Add | Section |
|---|---|---|
| Bumps on a unit, structure or bridge | `<texture>_nrm.dds` | [Surface detail](#surface-detail-normal-mapping) |
| Lit windows, lamps and exhausts | `<texture>_emi.dds` | [Glow masks](#glow-masks) |
| Bumps on terrain | `<terrain texture>_nrm.dds` | [Surface detail](#surface-detail-normal-mapping) |
| Bumps on a road | `<road texture>_nrm.dds` | [Surface detail](#surface-detail-normal-mapping) |
| Stones pushing through blends | `<terrain texture>_hgt.dds` | [Height blending](#height-blending) |
| Shine on one terrain type | `GlintStrength` and `GlintGloss` in `Terrain.ini` | [Terrain glint](#terrain-glint) |

### Troubleshooting

* The Direct3D 8 build ignores every shader on this page.
* `FlameShaders`, `ElectricShaders`, `LaserShaders` and `CryoShaders` in `Options.ini` default to Yes.
No turns that shader off everywhere.
* Flame haze, shockwaves and disruption need `Heat Effects` on. Bloom needs `Bloom = Yes`.
* The system's `Type` and `Shader` must allow the shader, as in the table above.
* `ParticleSystem.ini` changes and the texture lists apply on the next launch. `GameData.ini` tuning
reloads in cheat builds.
* `CONTRA_FLAMESHADER`, `CONTRA_ELECTRICSHADER`, `CONTRA_LASERSHADER`, `CONTRA_CRYOSHADER` or
`CONTRA_DISRUPTSHADER` set to 0 turns that shader off. `CONTRA_STORMSHADER=0` turns storms off, `CONTRA_HEADLIGHTSHADER=0` brings back the headlight meshes, and `CONTRA_PLANARMIRROR=0` draws mirror meshes plain.
* `LaserDebug = Yes` in `GameData.ini` draws shaded beams dark, so it shows which beams took the
laser shader.

## Shadow mapping

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

## Specular highlights

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

## Terrain glint

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

## Surface detail (normal mapping)

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

## Glow masks

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

## Per-pixel dynamic lights

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

## Soft particles

Smoke, dust, fire and explosion sprites fade out as they near the surface behind them, so they no
longer cut a hard line into the ground or the buildings they pass through. Needs the Direct3D 9
build and a shader model 2 card.

* `SoftParticles = Yes` - (No draws sprites with hard edges. Also `Soft particles` on the Shaders
page. Needs `CheckSoftParticles` in `OptionsMenu.wnd` for the menu control.)

Tuned in the mod's `GameData.ini`:

* `SoftParticleDistance = 12` - (How far in front of a surface, in world units, a sprite starts to
fade. 0 turns the fade off.)

Notes:
* With anti-aliasing off, sprites fade against everything already drawn. With it on, they fade
against the ground only, since a multisampled depth buffer cannot be read.
* Ground-aligned and alpha-tested sprites keep their edges.
* Launch with `CONTRA_SOFTPARTICLES=2` to fade against the ground only, or `0` to turn the fade off.

## Flame shading

Flame weapon fire flickers, licks and breaks up at its edges, and glows white-hot where it is
brightest. The air behind it shimmers. Blue, green and other coloured flames keep their colour.
Needs the Direct3D 9 build and a shader model 2 card; the shimmer also needs `Heat Effects` on.

* `FlameShaders = Yes` - (No draws flames as plain sprites. Options.ini only, no menu control.)

Picked per particle system in `ParticleSystem.ini`:

* `FlameShader = Auto` - (Default. On when the system rides a projectile whose weapon has
`DamageType = FLAME`, such as the Dragon tank, Immolator and flame tower sprays. `Yes` turns it on
for any system, such as muzzle flames, burning buildings and fire fields. `No` turns it off.)

Picked per beam in a `W3DLaserDraw` module, or per model in a `W3DModelDraw` module:

* `FlameShader = No` - (Default. `Yes` shades the beam or the model's translucent meshes as fire. On
a beam it wins over the laser and electric shaders.)

Tuned in the mod's `GameData.ini`. The same keys in a `ParticleSystem` block override them for
that system alone, and a beam or model takes the six `Flame` keys the same way:

* `FlameWarp = 0.04` - (How far the noise pushes the texture lookup, in texture widths. Higher licks
more.)
* `FlameHeat = 2.2` - (How fast bright parts run to white. 0 keeps the flame's own colour.)
* `FlameFlicker = 0.3` - (How far brightness swings, as a fraction. 0 is steady.)
* `FlameBreakup = 1` - (How much the faint fringe breaks up. 0 keeps clean edges.)
* `FlameNoiseSize = 20` - (World units across one tile of flame noise. Smaller gives finer tongues.)
* `FlameRise = 0.8` - (Noise tiles the flame pattern climbs per second.)
* `HazeBend = 1.2` - (How far the haze bends the scene, in world units at the flame. The main
strength control; past about 4 edges smear.)
* `HazeSize = 1.5` - (Haze sprite size as a multiple of the flame sprite's.)
* `HazeLift = 0.3` - (How far the haze sits above the flame, as a multiple of the sprite's size.)
* `HazeNoiseSize = 14` - (World units across one tile of haze noise. Larger gives slower, broader
waves.)
* `HazeRise = 1.1` - (Noise tiles the shimmer climbs per second.)
* `HazeMask = 2` - (How quickly faint parts of the flame reach full haze strength.)

```
ParticleSystem TankDragonMuzzleFlame
  ...
  FlameShader = Yes
  FlameFlicker = 0.5
  HazeBend = 2.5
End
```

Notes:
* A key a `ParticleSystem` block leaves out keeps the `GameData.ini` value.
* Systems with settings of their own draw apart from other flames, so keep overrides to the systems
that need them.
* The `GameData.ini` keys reload while the game runs in cheat builds, as described at the top of this
page. `ParticleSystem.ini` overrides take effect on the next launch.
* Slave systems follow their master, and a system a particle carries follows that particle's system.
* A streak burns only with `FlameShader = Yes`, since `Auto` would turn every trail behind a flame
shell to fire. Streaks, beams and models take the flame shading without the shimmer behind it.
* Projectile streams, volume particles and terrain-conforming particles stay plain.
* On a card without shader model 2.0a, flames keep their shading but lose the soft fade.
* Launch with `CONTRA_FLAMESHADER=1` to drop the shimmer, or `0` to turn flame shading off.

## Electric shading

Tesla, lightning and EMP sparks and flares crackle. Thin arcs jump across each sprite many times a
second, in its own colour taken halfway to white, while the sprite jitters and its brightness strobes.
Ported from Red Alert 3's tesla shader. Needs the Direct3D 9 build and a shader model 2 card.
Each key is pictured on [Electric & Laser Shading](Electric-&-Laser-Shading.md#electric-shading).

* `ElectricShaders = Yes` - (No draws electric sprites plain. Options.ini only, no menu control.)

Picked per particle system in `ParticleSystem.ini`:

* `ElectricShader = Auto` - (Default. On when the system's `ParticleName` texture is listed in
`GameData.ini`'s `ElectricParticleTextures`. `Yes` turns it on for any system, `No` turns it off.)

Picked per beam in a `W3DLaserDraw` module:

* `ElectricShader = No` - (Default. `Yes` shades the beam as electricity instead of as a laser, for
tesla and lightning bolts. The beam keeps its texture, which tiles along it as before.)

Picked per model in a `W3DModelDraw` module:

* `ElectricShader = No` - (Default. `Yes` shades the model's translucent meshes as electricity. The
module takes the six tuning keys below.)

Listed and tuned in the mod's `GameData.ini`:

* `ElectricParticleTextures = TeslaBlast.tga ...` - (Textures whose systems turn electric. Each line
adds to the list, so a long list can span several lines. Read at launch.)
* `ElectricArcs = 1.5` - (Arc brightness. 0 turns the arcs off.)
* `ElectricArcSharpness = 10` - (Lower gives broad glowing bands, higher thin threads.)
* `ElectricNoiseSize = 40` - (World units across one tile of arc noise. Smaller gives more, closer arcs.)
* `ElectricJitter = 0.03` - (How far the texture jumps each crackle, in texture widths.)
* `ElectricFlicker = 0.6` - (How far brightness swings, as a fraction. 0 is steady.)
* `ElectricRate = 15` - (Crackles per second. 0 freezes the arcs.)
* `ElectricParticleScale = 100%` - (How large electric sprites draw, as a percentage of the particle's
own size. With electric shading off they keep their own size.)

A `W3DLaserDraw` module takes the six keys from `ElectricArcs` to `ElectricRate`. Each one it sets
overrides `GameData.ini` for that beam alone, and the keys it leaves out keep `GameData.ini`'s values.
A `ParticleSystem` takes `ElectricParticleScale` alone, and uses `GameData.ini`'s other keys.

```
ParticleSystem EMPRing
  ...
  ElectricShader = Yes
  ElectricParticleScale = 80%
End

Draw = W3DLaserDraw ModuleTag_Draw
  ...
  ElectricShader = Yes
  ElectricArcs = 2.5
  ElectricRate = 30
End
```

Notes:
* Arcs lie in the view plane and are sized in world units, so ground-aligned rings and bolts, which
the pitched camera sees at a slant, crackle with the same arcs as upright sprites. Big effects carry
more arcs than small sparks.
* Past the default camera's distance, `CameraHeight` over the sine of `CameraPitch`, arcs widen with
depth, so they stay visible at the top of the screen and when zoomed out.
* A system that is both flame and electric draws as flame.
* Streaks such as `TeslaTrail.tga`, volume particles, terrain-conforming particles and multiplied
sprites stay plain.
* Launch with `CONTRA_ELECTRICSHADER=0` to turn electric shading off.

## Laser shading

Laser beams burn white-hot along their axis and glow out in their own colour. The core wavers in
width, pulses of brightness run along the beam towards its target, and the flat edges of the beam
melt into glow. Beams also fade where they meet the ground, like soft particles. Needs the Direct3D 9
build and a shader model 2 card. Each key is pictured on
[Electric & Laser Shading](Electric-&-Laser-Shading.md#laser-shading).

* `LaserShaders = Yes` - (No draws lasers plain. Options.ini only, no menu control.)

Beams from `W3DLaserDraw` get it by default. Turned off per beam in the draw module:

* `LaserShader = Yes` - (Default. `No` draws the beam plain, for beams that are not lasers, such as
the hacker's `EXBinaryStream32.tga` data stream.)

Laser streaks from particle systems are picked in `ParticleSystem.ini`:

* `LaserShader = Auto` - (Default. On when the system is `Type = STREAK` and its `ParticleName` texture
is listed in `GameData.ini`'s `LaserParticleTextures`. `Yes` turns it on for any streak system, `No`
turns it off.)

Listed and tuned in the mod's `GameData.ini`:

* `LaserParticleTextures = EXRedLaser.dds ...` - (Streak textures whose systems draw as lasers. Each
line adds to the list, so a long list can span several lines. Read at launch.)
* `LaserCore = 1.2` - (Core brightness. 0 turns the core off.)
* `LaserCoreWidth = 0.25` - (Core width, as a fraction of the beam's half width.)
* `LaserShimmer = 0.3` - (How far the core's width wavers, as a fraction. 0 keeps it steady, 1 at most.)
* `LaserPulse = 0.4` - (How far brightness swings along the beam, as a fraction. 0 is steady.)
* `LaserPulseSize = 120` - (World units across one tile of pulse noise. Smaller gives more, closer pulses.)
* `LaserPulseSpeed = 400` - (World units a second the pulses travel. 0 freezes them.)
* `LaserDebug = No` - (Yes subtracts shaded beams from the scene instead of adding them, so they
show dark on any background. Plain beams stay bright, which also shows which beams are shaded.)

A `W3DLaserDraw` module takes the six tuning keys from `LaserCore` to `LaserPulseSpeed`. Each one it
sets overrides `GameData.ini` for that beam alone, and the keys it leaves out keep `GameData.ini`'s
values. Laser streaks from particle systems always use `GameData.ini`'s.

```
ParticleSystem Red_BurstLaserTrail
  ...
  Type = STREAK
  LaserShader = Yes
End

Draw = W3DLaserDraw ModuleTag_Draw
  ...
  LaserCore = 2
  LaserPulseSpeed = 800
End
```

Notes:
* A beam's own texture still gives it its colour and shape. The shader adds the core, the pulses and
the soft edges on top.
* Beams made of several `NumBeams` layers get a core in every layer, so the axis runs brightest.
* Pulses are fixed along the beam in the world, so they keep flowing smoothly while the shooter moves.
* Sprite particles, such as laser muzzle flares, stay plain.
* Launch with `CONTRA_LASERSHADER=0` to turn laser shading off.

## Laser ground glow

Laser beams light the ground per pixel, with one light shaped like the beam. The light fades with
the distance to the nearest point on the beam, so a beam skimming the ground lights a bright strip
and a beam climbing into the sky lights only the ground near the shooter. Slopes facing the beam
catch more light, and the laser shader's pulses brighten the ground as they pass. Beams drawn plain,
electric or cryo light the ground steadily, and cryo beams light it in their ice colour. The Direct3D 8
build keeps the strip of dynamic lights described in
[Laser ground glow](contraZH-Changes.md#laser-ground-glow). Needs a shader model 2 card. Each key is
pictured on [Electric & Laser Shading](Electric-&-Laser-Shading.md#laser-ground-glow).

Uses the same switches and keys as the Direct3D 8 glow: `LaserRef`, `DynamicLights`,
`LaserGroundGlowColor`, `LaserGroundGlowIntensity`, `LaserGroundGlowRadius`, and `GroundGlowColor`,
`GroundGlowIntensity` and `GroundGlowRadius` on the `W3DLaserDraw` module. Shaped further in the
mod's `GameData.ini`:

* `LaserGroundGlowFalloff = 2` - (How fast the light fades with distance from the beam, as a power.
Higher gives a tight bright core, lower a broad wash.)
* `LaserGroundGlowWrap = 0.5` - (Light on ground facing away from the beam, from 0 to 1. 0 lights
only slopes facing it, 1 lights all ground evenly.)
* `LaserGroundGlowDebug = No` - (Yes darkens the ground by as much as the glow would light it, so the
light's reach and shape show as a shadow.)
* `LaserGroundGlowOverlap = Yes` - (Yes lights the ground under overlapping beams in one pass, so the
overlap brightens modestly. No lights each beam on its own, and overlaps compound.)

Notes:
* The light lights the ground's own colour, measured against the map's terrain lighting, so red
ground turns redder and a dark night map lights up as much as a bright day.
* The light reaches `GroundGlowRadius` on the module, else `LaserGroundGlowRadius`, else 1.25 times
the laser's `OuterBeamWidth`, at least 15. It is counted from the beam in three dimensions.
* Beams whose glows overlap light the ground together. Each colour channel takes the root of the sum
of the beams' squared light, so two equal beams light 1.4 times as much as one, three light 1.7
times, and crossing beams leave no crease. A lone beam lights as before.
* Up to seven overlapping beams combine. Past that, the seven brightest light the spot and the rest
go dark. Combining needs a shader model 2.0a card. Without one, or when the overlap covers too much
ground for one draw, each beam lights on its own.
* Lasers no longer take dynamic lights, so the per-pixel lights stay free for explosions and
muzzle flashes.
* Water covers the glow on ground beneath it. Units and buildings are not lit.
* Launch with `CONTRA_LASERGLOW=0` to go back to the dynamic lights.

## Cryo shading

Freeze rays turn to ice. The beam's colour is pulled toward an ice tint at its own brightness, a
blue-white core runs along its axis, frost bands drift slowly towards the target, and jagged ice teeth
cut into both edges. Frost trails from particle systems draw the same way. Frost puffs and flares take
the tint, thin cracks split their faint fringe into shards, and glints twinkle across them. Cryo wins
over the laser, electric and flame shaders wherever it is on. Needs the Direct3D 9 build and a shader
model 2 card.

* `CryoShaders = Yes` - (No draws cryo effects plain. Options.ini only, no menu control.)

Picked per beam in a `W3DLaserDraw` module:

* `CryoShader = No` - (Default. `Yes` shades the beam as ice, whatever its `LaserShader` and
`ElectricShader`.)

Picked per model in a `W3DModelDraw` module:

* `CryoShader = No` - (Default. `Yes` shades the model's translucent meshes as ice, the way a cryo
sprite draws. The module takes the nine tuning keys from `CryoTint` to `CryoShardSize`.)

Picked per particle system in `ParticleSystem.ini`:

* `CryoShader = Auto` - (Default. On when the system's `ParticleName` texture is listed in
`GameData.ini`'s `CryoParticleTextures`. `Yes` turns it on for any system, `No` turns it off. Streaks
draw as freeze rays, sprites as frost.)

Listed and tuned in the mod's `GameData.ini`:

* `CryoParticleTextures = CryoFlare.tga EXCryoRing.tga ...` - (Textures whose systems turn to ice.
Each line adds to the list, so a long list can span several lines. Read at launch.)
* `CryoTint = R:150 G:215 B:255` - (The ice colour. Only its hue counts, so the effect keeps its
brightness.)
* `CryoTintStrength = 0.8` - (How far the effect's own colour moves to the tint, from 0 to 1. 0 keeps
the texture's colour.)
* `CryoCore = 1` - (Core brightness. 0 turns the core off.)
* `CryoCoreWidth = 0.3` - (Core width, as a fraction of the beam's half width.)
* `CryoFrost = 0.5` - (How strongly the frost bands whiten the beam. 0 turns them off.)
* `CryoFrostSize = 200` - (World units across one tile of frost noise. Smaller gives more, closer
bands.)
* `CryoFrostSpeed = 60` - (World units a second the bands drift towards the target. 0 freezes them.)
* `CryoShards = 0.4` - (How far ice teeth cut into a beam, as a fraction of its half width, and how far
cracks reach into a sprite's fringe. 0 keeps edges whole.)
* `CryoShardSize = 8` - (World units from one tooth or shard to the next.)
* `CryoGlints = 2` - (Glint brightness on sprites. 0 turns glints off.)
* `CryoGlintSize = 1.5` - (World units between glints.)
* `CryoGlintRate = 2` - (About how many times a second each glint twinkles. 0 freezes them.)
* `CryoParticleScale = 100%` - (How large cryo sprites and trails draw, as a percentage of the
particle's own size. With cryo shading off they keep their own size.)

A `W3DLaserDraw` module takes the nine tuning keys from `CryoTint` to `CryoShardSize`. Each one it
sets overrides `GameData.ini` for that beam alone, and the keys it leaves out keep `GameData.ini`'s
values. A `ParticleSystem` takes `CryoParticleScale` alone, and uses `GameData.ini`'s other keys.

```
ParticleSystem FrostPuff
  ...
  CryoShader = Yes
  CryoParticleScale = 60%
End

Draw = W3DLaserDraw ModuleTag_Draw
  ...
  CryoShader = Yes
  CryoTint = R:120 G:200 B:255
  CryoShards = 0.6
End
```

Notes:
* The beam's texture still gives it its shape. With `CryoTintStrength` below 1 its colour shows through
the tint.
* Teeth and frost bands are fixed along the beam, counted from the shooter, so they travel with it.
* Cracks and glints are fixed in the world, so they hold still while the camera pans and shift as
the sprite drifts through them.
* The ground glow under a cryo beam holds steady and takes the beam's tint. With cryo shading off the
beam and its glow keep their own colour.
* Terrain-conforming particles, volume particles and multiplied sprites stay plain.
* Launch with `CONTRA_CRYOSHADER=0` to turn cryo shading off.

## Disruption shading

A shape ripples the scene behind it and pulls its colours apart, like a jammed video signal. The
shape can be a model's translucent meshes, a particle system's sprites, a strip along a laser beam, or
a plain disc from an `FXList`. Its texture is the mask, so the scene bends most where the texture is
brightest and not at all where it is black. Three movements add up, each with its own strength:

* Rings travel outwards through the shape.
* A noise field wobbles the scene, like strong heat haze.
* Bands across the screen jump sideways in fits.

Red bends further than green and blue less, which fringes every bent edge with colour. Needs the
Direct3D 9 build, a shader model 2 card and `Heat Effects` on.

Picked per entry in a `W3DModelDraw` module, a `W3DLaserDraw` module or a `ParticleSystem`:

* `DisruptionShader = No` - (Default. `Yes` bends the scene behind the shape and draws the shape's own
art over it. `Only` bends the scene and leaves the art undrawn, so the shape is a pure mask.)

A `W3DLaserDraw` module also takes:

* `DisruptionWidth = 0` - (Width of the strip that bends the scene, in world units. 0 takes the
beam's widest width, which is often too thin to see.)

Tuned in the mod's `GameData.ini`:

* `DisruptionRingStrength = 3` - (How far the rings push the scene, in world units. 0 turns them off.)
* `DisruptionRingSize = 40` - (World units from one ring to the next.)
* `DisruptionRingSpeed = 60` - (World units a second the rings travel outwards. 0 freezes them.)
* `DisruptionWobble = 1.5` - (How far the noise pushes the scene, in world units. 0 turns it off.)
* `DisruptionWobbleSize = 30` - (World units across one tile of wobble noise. Smaller is finer.)
* `DisruptionWobbleSpeed = 1.5` - (Noise tiles the wobble crosses per second.)
* `DisruptionGlitch = 4` - (How far a band jumps sideways at most, in world units. 0 turns the bands
off.)
* `DisruptionGlitchSize = 12` - (Height of a band, in pixels on a 1080p screen, scaled to other
resolutions.)
* `DisruptionGlitchRate = 12` - (Jumps per second. 0 freezes the bands.)
* `DisruptionChroma = 0.5` - (How much further red bends than green, and how much less blue does, as
a fraction. 0 keeps the colours together.)
* `DisruptionChromaSpread = 0.5` - (World units red and blue part along the rings' direction even
where nothing bends. 0 fringes only bent pixels.)
* `DisruptionMask = 2` - (How quickly the shape's brightness reaches full strength. Higher lets
fainter parts of the texture bend the scene.)

Every entry that takes `DisruptionShader` also takes these twelve keys. Each one it sets overrides
`GameData.ini` for that entry alone, and the keys it leaves out keep `GameData.ini`'s values.

```
Draw = W3DModelDraw ModuleTag_01
  DefaultConditionState
    Model = EXGLAJammer
  End
  DisruptionShader       = Yes
  DisruptionRingStrength = 5
  DisruptionGlitch       = 8
End

ParticleSystem JammerSparks
  ...
  DisruptionShader = Only
  DisruptionWobble = 3
End

Draw = W3DLaserDraw ModuleTag_Draw
  ...
  DisruptionShader = Yes
  DisruptionWidth  = 30
End
```

An `FXList` draws a disc with no art through a `Disruption` block. The disc is brightest at its
middle and fades to its rim.

* `Radius` - (The disc's radius, in world units.)
* `Duration` - (How long the disc lasts, in milliseconds.)
* `Fade = 20%` - (Share of the duration spent fading in, and again fading out.)
* The twelve tuning keys above.

```
FXList FX_JammerPulse
  Disruption
    Radius                 = 425
    Duration               = 1200
    Fade                   = 20%
    DisruptionRingStrength = 5
  End
End
```

Notes:
* Disruption bends a copy of the scene taken before anything translucent draws. Particles, beams and
translucent models stay crisp over it, and overlapping shapes do not bend each other.
* On a model only translucent meshes take part, the ones with an additive or alpha-blended material.
With `Only`, the whole model goes undrawn.
* A model's rings start at each mesh's own origin, a sprite's at the middle of its texture, and a
disc's at its middle. On a beam they run along its length and push across it.
* Rings keep time with the game clock, so an effect that is replaced every second ripples without
a jump.
* A beam needs a `Texture`, which masks its strip. Without one it draws as a plain beam, even with
`Only`.
* On a particle system, `ALPHA_TEST` and `MULTIPLY` sprites, streaks, volume particles and
terrain-conforming particles take no disruption, and draw their own art even with `Only`.
* The bend follows the mask at the size the sprite draws, so `CryoParticleScale` and
`ElectricParticleScale` scale it too. On a model it fades with the object's opacity.
* An `FXList` disc lies flat and ignores depth, like a shockwave. At most 32 show at once.
* With `Heat Effects` off or on the Direct3D 8 build, `Yes` and `Only` both draw the plain art, so
the effect never vanishes.
* Launch with `CONTRA_DISRUPTSHADER=0` to turn disruption off.

## Storms

A sandstorm or snowstorm stands on the ground inside an upright cylinder. The storm is haze that
fills the cylinder and grains or flakes that blow through it, and the shaders make both, so no
particle system is involved. The haze follows the terrain, thins out towards the storm's edge and
top, and bunches into gusts that drift with the wind. Units and buildings inside it fade with
their depth in the haze. Needs the Direct3D 9 build and a shader model 3 card that reads textures
in the vertex shader. Older cards draw no storm.

A storm comes from one of two places:

* A `Storm` block in an `FXList` starts a storm where the effect plays. It stays there for its
`Duration`.
* A `W3DStormDraw` module on an object keeps a storm around the object. The storm follows the
object and dies down once the object is gone.

Both take these keys. A key left out takes the value `GameData.ini` holds for the storm's `Type`.
There each key carries the type's name in front, so `HazeDensity` is `SandStormHazeDensity` for sand
and `SnowStormHazeDensity` for snow, and the percent keys take a plain number such as
`SandStormEdgeFade = 35`. The two columns list what `GameData.ini` starts with. A storm reads
`GameData.ini` every frame, so in a cheat build a saved change shows on a running storm:

| Key | `SAND` | `SNOW` | Meaning |
|---|---|---|---|
| `Type` | | | `SAND` or `SNOW`. Default `SAND`. |
| `Radius` | 300 | 300 | World units from the storm's middle to its edge. |
| `Height` | 120 | 150 | World units the storm stands above the ground. |
| `EdgeFade` | 35% | 35% | Share of the radius over which the storm thins out to nothing. |
| `FadeTime` | 3000 | 3000 | Milliseconds the storm takes to build up, and again to die down. |
| `HazeColor` | R:194 G:158 B:107 | R:217 G:224 B:235 | Colour of the haze, before the map's lighting. |
| `HazeDensity` | 0.8 | 0.35 | How much 100 world units of haze hide. 0 draws no haze. |
| `HazeMaxOpacity` | 80% | 55% | The most the haze may hide, however deep. |
| `HazeNoiseSize` | 300 | 350 | World units across one tile of gust noise. Smaller is finer. |
| `Gusts` | 70% | 50% | How unevenly the haze and grains bunch up. 0% is an even fog. |
| `WindAngle` | 0 | 0 | Direction the wind blows towards, in degrees. 0 is east, 90 is north. |
| `WindSpeed` | 140 | 45 | World units a second. |
| `FallSpeed` | 4 | 30 | World units a second the grains sink. |
| `Turbulence` | 5 | 9 | World units the grains swirl off their path. |
| `GrainColor` | R:219 G:189 B:140 | R:255 G:255 B:255 | Colour of the grains, before the map's lighting. |
| `GrainCount` | 6000 | 5000 | Grains in view at once, up to 40000. 0 draws no grains. |
| `GrainSize` | 1.0 | 1.8 | World units across a grain. |
| `GrainStreak` | 0.06 | 0.02 | Seconds of travel a grain smears along. 0 draws round grains. |
| `GrainOpacity` | 60% | 85% | |

A `Storm` block also takes:

* `Duration` - (How long the storm lasts, in milliseconds, with both fades inside it.)

```
FXList FX_SandstormStrike
  Storm
    Type        = SAND
    Radius      = 350
    Duration    = 30000
    WindAngle   = 45
    HazeDensity = 1.0
  End
End

Object SnowstormEmitter
  Draw = W3DStormDraw ModuleTag_Storm
    Type       = SNOW
    Radius     = 500
    GrainCount = 12000
  End
  ...
End
```

Notes:
* Storms are visual only. They change no vision, damage or speed. They are not saved, so a loaded
game has lost its `FXList` storms, while `W3DStormDraw` storms build up again.
* Grains fill a square 640 world units wide around the middle of the view, so `GrainCount` sets how
thick they look on screen whatever the storm's size.
* Sand keeps close to the ground and snow fills the storm's height evenly.
* The haze stops at whatever the scene's depth holds, so it thins in front of tall buildings and
hills. Without a readable depth buffer it stops at the terrain alone and draws over objects.
* Storms draw after particles, so effects inside a storm are hazed like the ground under them.
* A `W3DStormDraw` storm dies down while its object is under the shroud. An `FXList` storm ignores
the shroud.
* At most 8 storms show at once, and further ones do not start.
* Launch with `CONTRA_STORMSHADER=0` to turn storms off.

## Headlights

At night a model shows every mesh whose name holds `HEADLIGHT`, such as `HEADLIGHT01`. With this
feature those meshes stay hidden and a shader draws each headlight in their place, as two parts:

* The beam is a soft cone in the air. It dims along its length and towards its edge, and fades out
where it meets the ground or a model.
* The pool is the light the lamp throws. It brightens the terrain, models and buildings inside the
lamp's cone, by their own colours.

Each headlight takes its place and size from its mesh. The beam runs along the side of the mesh
that is long and leads away from the model's middle. The narrow end of a cone is the lamp, and a
shape with no narrow end has its lamp at the end nearer the model's middle. In both cases
the cone's width gives the beam's width at the far end. A mesh may hold several cones. Each cone
that stands apart draws as a lamp of its own, and cones that lie within half their radius of each
other, such as twin lamps set side by side, draw as one. The models need no INI change, and
headlights still show only at night. An opaque `HEADLIGHT` mesh, which blends nothing, is a lamp body
rather than light, so it keeps its own look and throws no beam. With `RotrHack = Yes`, an additive
`HEADLIGHT` mesh does the same. Rise of the Reds draws lit windows and glows that way. `HeadlightPerConeAim = Yes` instead gives each cone its own direction, for meshes whose lamps
aim different ways, such as a floodlight rig on a roof. The lamp is the middle of a cone's narrow end, and cones
that aim the same way with lamps within one radius of each other draw as one. A mesh with a lamp that
has no narrow end keeps the rule above.

Needs the Direct3D 9 build and a shader model 3 card. Elsewhere, and with `HeadlightShader = No`,
the models show their headlight meshes as before. The pool has no shadows, so a lamp also lights
ground that a hill or building hides from it.

The keys live in `GameData.ini`. All but `HeadlightShader`, `HeadlightShaderForbiddenKindOf`, `HeadlightPerConeAim` and `RotrHack` reload in cheat builds:

| Key | Default | Meaning |
|---|---|---|
| `HeadlightShader` | `Yes` | `No` keeps the headlight meshes. Read at launch. |
| `HeadlightShaderForbiddenKindOf` | none | Objects of any of these kinds keep their headlight meshes, such as `STRUCTURE` for buildings that use `HEADLIGHT` meshes as lit windows. Read at launch. `GameData.ini` only. |
| `HeadlightColor` | `R:255 G:242 B:209` | Colour of the beam and the pool. |
| `HeadlightBeamIntensity` | 0.35 | Brightness of the beam. 0 draws no beam. |
| `HeadlightBeamLength` | 1.0 | Beam length, in mesh lengths. |
| `HeadlightBeamWidth` | 1.0 | Beam width at the far end, in mesh widths. |
| `HeadlightBeamFalloff` | 1.5 | How fast the beam dims along its length. 1 dims evenly, more dims sooner. |
| `HeadlightBeamSoftness` | 8.0 | World units over which the beam fades into what it touches. |
| `HeadlightPoolIntensity` | 0.8 | Brightness of the pool. 0 draws no pool. |
| `HeadlightPoolRange` | 2.5 | How far the light reaches, in mesh lengths. |
| `HeadlightPoolAngle` | 26 | Degrees from the middle of the light to its edge. |
| `HeadlightPoolPitch` | 11.5 | Degrees the light tilts down from the mesh, so a level lamp reaches the ground. |
| `HeadlightPoolFalloff` | 1.5 | How fast the pool dims with distance. |
| `HeadlightPoolClampBrightness` | `Yes` | Where pools overlap, the ground takes the brightest one alone, so lamps side by side do not burn it white. `No` stacks them. `GameData.ini` only. |
| `HeadlightPerConeAim` | `No` | `Yes` aims each cone of a `HEADLIGHT` mesh its own way, as above. Read at launch. `GameData.ini` only. |
| `RotrHack` | `No` | `Yes` keeps additive `HEADLIGHT` meshes, such as Rise of the Reds' lit windows, as they are, with no beam. Read at launch. `GameData.ini` only. |

A model can override any of these for itself. The same keys go in its `W3DModelDraw` module, or in
a module built on it such as `W3DTankDraw`, beside `OkToChangeModelColor`. A key left out takes the
`GameData.ini` value, so a module names only what differs. `HeadlightShader = No` there keeps that
model's headlight meshes while the rest of the game uses the shader. It cannot turn the shader on
for one model while `GameData.ini` has it off. A negative `HeadlightPoolPitch` tilts the light up.
Module keys are read at launch.

```
Draw = W3DTruckDraw ModuleTag_01
  HeadlightColor         = R:190 G:215 B:255
  HeadlightPoolIntensity = 1.4
  HeadlightPoolAngle     = 34
  DefaultConditionState
    Model = AVOmega
  End
End
```

## Planar mirrors

Chosen meshes of a model act as level mirrors. They reflect the scene above them: units, buildings,
trees and the sky. Suited to polished floors, wet plazas, metal decks, puddles, pools and glass.

* An opaque mesh keeps its texture, lighting and shadows. The reflection blends over it.
* A translucent mesh shows what lies under it, bent by its normal map, below its own texture. The
reflection lies over both.
* The reflection's share is `PlanarMirrorReflectivity` seen from above and rises towards all of it
at a grazing angle.
* A `<texture>_nrm.dds` normal map beside the mesh's texture ripples the reflection and what shows
through. Without one the mirror is flat.

The game mirrors the scene in at most two heights each frame, those of the mirrors nearest the
middle of the view. Mirrors within half a unit of each other in height share one. A mirror at
any other height reflects the sky alone. The mirror's height is the top of the mesh, so the mesh
should be flat and level. A mirror that turns up on screen shows the sky for its first frame.

Each height costs one more draw of the scene at half resolution, cut down to the mirrors' part of
the screen. Water reflections are separate and cost their own draw.

Needs the Direct3D 9 build and a pixel shader 2.0a card. Elsewhere, with `PlanarMirrorShader = No`,
and with water reflections turned off in the options (`WaterReflections = No` in `Options.ini`), the
meshes draw as before.

Mark the model in its `W3DModelDraw` module, or a module built on it, beside `OkToChangeModelColor`:

| Key | Default | Meaning |
|---|---|---|
| `PlanarMirror` | `No` | `Yes` makes the meshes below mirrors. It wins over `FlameShader`, `ElectricShader` and `CryoShader` on those meshes. |
| `PlanarMirrorMeshes` | every mesh | Mesh names, without the model's name, such as `FLOOR01 POOL`. Case does not matter. |
| `PlanarMirrorOverrideTexture` | `No` | `Yes` draws the meshes as clear glass in place of their texture, whatever the mesh. They show what lies under them, bent by the normal map, with the reflection over it. A mesh drawn with alpha keeps its texture's alpha as its outline. |

The look keys live in `GameData.ini`. All but `PlanarMirrorShader` reload in cheat builds:

| Key | Default | Meaning |
|---|---|---|
| `PlanarMirrorShader` | `Yes` | `No` draws every mirror mesh plain. Read at launch. |
| `PlanarMirrorReflectivity` | 0.35 | Share of the reflection seen from straight above, 0 to 1. |
| `PlanarMirrorTint` | `R:255 G:255 B:255` | Multiplies the reflection. Darker for tinted glass or dull metal. |
| `PlanarMirrorDistortion` | 0.01 | How far the normal map bends the reflection and what shows through, in screen widths. |
| `PlanarMirrorFrost` | 0 | 0 clear to 1 frosted. Blurs the reflection, and blurs and clouds what shows through glass and translucent mirrors. |

The same four look keys in the model's module override `GameData.ini` for that model. Module keys
are read at launch.

Glass draws after the opaque scene, from one copy of the screen taken at the first glass or
translucent mirror. An object that draws later and stands under the glass does not show through it.
A glass mesh takes no shadows or highlights on itself, though it still casts its shadow.

```
Draw = W3DModelDraw ModuleTag_01
  PlanarMirror             = Yes
  PlanarMirrorMeshes       = FLOOR POOL
  PlanarMirrorReflectivity = 0.6
  DefaultConditionState
    Model = CBPlaza
  End
End
```

## Supersampling

`Supersampling` on the Shaders page offers `150%` and `200%`. The world renders to a target that
many percent of the screen's size each way and is filtered down to the screen with a box filter,
which smooths edges, alpha-tested cutouts, particles and shader output alike. The interface and
the mouse draw after that at the screen's own size, so text stays sharp. Unlike MSAA the larger
target is single sampled, so the scene's depth stays readable and ambient occlusion, soft
particles, the storm haze and every other depth-reading effect keep working, now supersampled too.

Notes:
* 200% draws four times the pixels. Zoomed in on a big fight it costs about what MSAA 8x does.
* Supersampling takes MSAA's place; the Anti-aliasing box greys out while it is on.
* `Options.ini` stores it as `SuperSampling = 150` or `200`.
* The `CONTRA_SSAA` environment variable set to a percent overrides the option for one run.
* The combo needs a layout with the `ComboBoxSuperSampling` row, which `build/add_ssaa_wnd.py` adds
to a loose `OptionsMenu.wnd`.

## Ambient occlusion

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

## Ground noise

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

## Height blending

Where two terrain textures meet, the taller parts of each push into the other instead of a soft
10-unit fade. Stones and clumps stand proud of the texture beside them, and the edge follows them.
Three-texture blend tiles blend the same way. Needs the Direct3D 9 build.

* `HeightBlend = Yes` - (No brings back the soft fade. Also `Height blending` on the Shaders
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

## Stochastic terrain

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

## HQ sky

Cloud shadows drift softly over the ground, change shape as they go and never repeat, in place of
one tiled cloud texture sliding across the map. Each frame a shader draws the clouds into a map over
the ground the camera sees, and terrain, roads, bridges, units and buildings darken from it. Two
cloud shapes at unrelated sizes and angles add up to each cloud, a slow warp bends them, and a
finer layer frays their edges. Each layer drifts at its own speed, so clouds form and fade instead
of sliding as one sheet. Needs the Direct3D 9 build and Cloud shadows on.

* `HQSky = Yes` - (No brings back the tiled cloud texture. Also `HQ sky` on the Shaders
page, greyed out while Cloud shadows is off. Needs `CheckHQSky` in `OptionsMenu.wnd` for the
menu control.)

Tuned in the mod's `GameData.ini`:

* `SkyCloudSize = 600` - (World units across a typical cloud.)
* `SkyCloudCoverage = 0.45` - (Share of the ground in shadow. 0 is a clear sky, 1 overcast.)
* `SkyCloudSoftness = 0.25` - (How wide the fade at a cloud's edge is. Low gives crisp edges.)
* `SkyCloudShadowStrength = 0.35` - (How dark a thick cloud's shadow is. 0 for none. The default matches
the old clouds' darkest.)
* `SkyCloudShadowTint = R:235 G:242 B:255` - (The shadow's hue. White gives neutral grey.)
* `SkyCloudWindSpeed = 11` - (World units a second the clouds drift. 0 holds them still.)
* `SkyCloudWindAngle = 56` - (Degrees the clouds drift towards, 0 along the map's x. The default
matches the old clouds.)
* `SkyCloudChurn = 0.3` - (How fast shapes change as they drift. 0 slides them as one sheet.)
* `SkyCloudBillow = 0.5` - (How far shapes bulge and curl. High values twist them into streaks.)
* `SkyCloudDetail = 0.4` - (Ragged detail at the edges. 0 gives smooth blobs.)

Notes:
* Night keeps the clouds off, as before.
* Trees and the water's reflected sky keep their old look.
* `CONTRA_SKYCLOUDS=0` keeps the tiled texture, to rule the HQ sky out of a rendering fault.

## Colour grading

A colour table gives a map its look: warm for a desert, cold for snow, muted for a city. After each
view's 3D scene is drawn, a shader reads every pixel's colour and replaces it with the colour the
table holds for it. The interface, the cursor and the health bars draw afterwards and keep their
colours. The grade changes no lighting. It recolours the lit picture, so it applies alike to terrain,
units, effects and team colours. Needs the Direct3D 9 build and pixel shader 2.0a.

Set in the mod's `GameData.ini` for every map, or in the `GameData` block of a map's `map.ini` for
that map alone:

The table:

* `ColorLut = None` - (The table's file name, such as `lut_desert.tga`. `None` uses no table.)
* `ColorLutStrength = 1` - (How much of the table's result is taken. 0 leaves the scene as it is.)
* `ColorLutChroma = 1` - (How much of the table's hue is taken. 0 takes only its brightness.)
* `ColorLutLuma = 1` - (How much of the table's brightness is taken. 0 takes only its hues, so units
stay as bright or dark as they were.)

Colour, before the table:

* `ColorLutBrightness = 1` - (Multiplies the scene.)
* `ColorLutContrast = 1` - (Spreads the scene around mid grey. 0 is flat grey.)
* `ColorLutSaturation = 1` - (Colourfulness. 0 is black and white.)
* `ColorLutTint = R:255 G:255 B:255` - (Colour the scene is multiplied by.)
* `ColorLutVibrance = 0` - (Saturates dull colours more than vivid ones, so terrain gains colour and
team colours hold. -1 to 1, and negative mutes them.)
* `ColorLutTechnicolor = 0` - (Strength of the two-strip Technicolor film look, 0 to 1.)

Levels, after the table:

* `ColorLutBlackPoint = 0` - (Level that becomes black. Raise it to crush the shadows.)
* `ColorLutWhitePoint = 1` - (Level that becomes white. Lower it to brighten the highlights.)
* `ColorLutGamma = 1` - (Midtone brightness. Above 1 brightens and below 1 darkens.)
* `ColorLutOutputBlack = 0` - (What black comes out as. Raise it for the lifted, matte look. The
shroud lifts with it.)
* `ColorLutOutputWhite = 1` - (What white comes out as. Lower it to dim the highlights.)

Finish, last:

* `ColorLutVignette = 0` - (How dark the view gets towards its edges, 0 to 1.)
* `ColorLutVignetteRadius = 2` - (Distance from the view's centre, in half its height, where the
vignette reaches full strength. 2 reaches the corners of a 16:9 view.)
* `ColorLutGrain = 0` - (Film grain over the scene, new every frame. 0.1 to 0.2 is a light grain.)
* `ColorLutDither = 1` - (Noise that breaks up the banding a grade leaves in smooth gradients, in
8 bit colour steps. 0 is off.)

Every key but `ColorLutStrength`, `ColorLutChroma` and `ColorLutLuma` works without a table, so a map
can take a small correction alone. The dither draws only while some other key grades the scene.

A `map.ini` that grades one map:

```ini
GameData
  ColorLut = lut_snow.tga
  ColorLutStrength = 0.8
  ColorLutSaturation = 0.9
End
```

Preset tables, in `Art\Textures`:

| File | Map style | Look |
|---|---|---|
| `lut_desert.tga` | Desert | Warm midtones and highlights over cool shadows. |
| `lut_snow.tga` | Snow | Blue shadows, clean whites and muted colour. |
| `lut_naval.tga` | Naval | Deep blue-teal shadows and midtones under neutral highlights. |
| `lut_island.tga` | Island | Vivid colour, lush greens and warm sunlight. |
| `lut_urban.tga` | Urban | Muted colour with hard contrast. |
| `lut_future.tga` | Future | Violet shadows, cyan highlights and hard contrast. |
| `lut_neutral.tga` | None | Changes nothing. The starting point for a new table. |

Seventeen more looks come from the MultiLUT atlas of [OtisFX](https://github.com/FransBouma/OtisFX),
a ReShade shader pack by Frans Bouma under the MIT licence:

| File | Look |
|---|---|
| `lut_otis_hollywood.tga` | Teal shadows and warm highlights, as in action films. |
| `lut_otis_blue.tga` | Strong cold blue cast. |
| `lut_otis_coollight.tga` | Slight cool cast with soft contrast. |
| `lut_otis_flatgreen.tga` | Flat and green-grey, a military drab. |
| `lut_otis_redliftmatte.tga` | Lifted, reddish shadows with a matte finish. |
| `lut_otis_crossprocess.tga` | Cross-processed film: yellow highlights over blue shadows. |
| `lut_otis_azurered.tga` | Two tones, warm red over azure. |
| `lut_otis_vogue.tga` | Pale and clean, with cool shadows. |
| `lut_otis_sepia.tga` | Brown monochrome, for old footage. |
| `lut_otis_bw.tga`, `lut_otis_bwcontrast.tga` | Black and white, at medium and high contrast. |
| `lut_otis_color1.tga`, `color2`, `color5` to `color8` | Six mild colour casts, warm to cool. |

A ReShade `lut.png` of 1024 by 32 has the same layout, so it works once saved as a TGA.

A table is a 24 or 32 bit TGA strip of N slices, N*N wide and N tall, with N up to 64. The presets
are 1024 by 32. Red runs across a slice, green down it and blue from slice to slice. To make one,
paste `lut_neutral.tga` beside a screenshot in an image editor, grade both together, and save the
strip alone under a new name in `Art\Textures`. The game reads the file as it is, so the texture
detail setting never shrinks it.

Notes:
* The render tuner's Colour grade tab previews each key on a screenshot and saves the keys into
`GameData.ini` or a `map.ini`. `lut_presets.py` beside it writes the preset tables and splits a
MultiLUT atlas.
* Vibrance and Technicolor follow the [SweetFX](https://github.com/CeeJayDK/SweetFX) shaders of the
same names, and the hue and brightness shares follow ReShade's `LUT.fx`.
* The dither hides steps up to its own size. A strong contrast or gamma can widen the scene's own
8 bit steps past that, and a higher `ColorLutDither` trades them for visible noise.
* A map's value holds until the next map loads. A `map.ini` that sets no `ColorLut` key takes the
`GameData.ini` ones.
* View filters, such as the black and white one, draw first, and the grade applies over them.
* WorldBuilder shows no grade.
* A file that is missing or has the wrong shape draws no table, and a cheat build names it in
`d3d9render.txt`.
* A loose table saved again under the same name shows within half a second, so a table can be
graded with the map running.
* `CONTRA_COLORLUT=0` turns the grade off, to rule it out of a rendering fault.

## Shader water

Lakes, seas and rivers are shaded per pixel, with refraction, reflection, sun glint, foam and
vertex waves. Its options and `Water.ini` parameters are on [Water](Water.md).

## Hardware instancing

Copies of the same vehicle, structure or prop draw together in one call per mesh piece, in the
shadow map and in the main view along with their shadow and highlight passes. Needs the Direct3D 9
build and a shader model 3 card; other cards draw as before.

Notes:
* These still draw one at a time: infantry and other skinned meshes, fading units, camera-facing
sprites, objects lit by point lights or by more dynamic lights than they draw per pixel, and
objects under shroud, jamming, frozen or heat vision overlays.
* The `CONTRA_INSTANCING` environment variable helps track down rendering faults: `0` turns it off,
`1` limits it to the shadow map, `2` adds main view objects without shadow or highlight passes, and
`3` (the default) covers everything.

## GPU skinning

Infantry and other skinned meshes bend on the graphics card instead of the CPU, in the shadow map
and in the main view along with their shadow and highlight passes. Needs the Direct3D 9 build and a
shader model 2 card; other cards skin on the CPU as before.

Notes:
* These still skin on the CPU: meshes following more than 70 bones, camera-facing or sorted meshes,
objects lit by point lights or by more dynamic lights than they draw per pixel, and objects under
shroud, jamming, frozen or heat vision overlays.
* The `CONTRA_GPU_SKINNING` environment variable helps track down rendering faults: `0` turns it
off, `1` limits it to the shadow map, `2` adds main view skins without shadow or highlight passes,
and `3` (the default) covers everything.

## Faster translucent effects

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

## Flip model presentation

The Direct3D 9 build runs on a Direct3D 9Ex device. Windowed and borderless modes present through a
flip model swap chain, which skips the desktop compositor's copy and lets a borderless window at
desktop resolution flip straight to the screen. MSAA still works; the scene renders to a
multisampled target that resolves into the swap chain each frame.

Notes:
* Fullscreen keeps the classic swap chain.
* Terrain and tree textures ignore the texture reduction setting.
* The `CONTRA_D3D9EX` environment variable set to `0` returns to a plain Direct3D 9 device.

## Shockwave (FXList.ini)

An expanding ring on the ground that bends everything behind it, like the air a big blast pushes
out. Needs the Direct3D 9 build, a shader model 2 card and `Heat Effects` on.

* `Radius` - (How far the ring travels, in world units.)
* `Width` - (How thick the ring is, in world units.)
* `Strength` - (How far it bends the scene, in world units. Around 2 to 6 reads well. In world units
the bend shrinks on screen as the camera pulls back, and is lost when zoomed far out.)
* `StrengthInPixels = No` - (Yes counts `Strength` in pixels on a 1080p screen instead, scaled to
other resolutions, so the bend looks the same at every zoom. Around 6 to 12 reads well.)
* `Duration` - (How long the ring takes to reach `Radius`, in milliseconds. It fades as it goes.)

```
FXList FX_NukeExplosion
  Shockwave
    Radius           = 300
    Width            = 40
    Strength         = 8
    StrengthInPixels = Yes
    Duration         = 900
  End
End
```

Notes:
* At most 16 rings show at once; a new one replaces the oldest.
* The ring lies flat at the effect's height, so on steep ground it is centred but not draped.
