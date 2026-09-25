# Direct3D 9 Features

Graphics features that need the Direct3D 9 build. The Direct3D 8 build ignores their settings.
Options live in `Options.ini` and the advanced display options; tuning keys live in the mod's
`GameData.ini`. Bloom, vertical sync and laser ground glow work on both builds and are on
[contraZH Changes](contraZH-Changes.md#rendering).

Cheat builds reload `Data\INI\GameData.ini` about half a second after it is saved, so these tuning
keys can be adjusted with a map running: `UnitSpecularIntensity`, `UnitSpecularPower`,
`UnitBumpHeight`, `UnitNormalMapStrength`, `TerrainNormalMapStrength`, `UnitEmissiveIntensity`,
`UnitEmissiveNightIntensity`, `SoftParticleDistance`, `AmbientOcclusionRadius`,
`AmbientOcclusionStrength`, the `GroundNoise` keys and the `Flame`, `Haze`, `Electric` and `Laser` tuning keys. Other `GameData.ini` keys keep their
value until a restart. The saved values win over a map's `map.ini` until the map loads again. A
deleted key keeps its value until a restart, and a file with an error applies only the keys above
the error until the next save.

## Shadow mapping

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
* Units and buildings also darken under the drifting cloud shadows, like the ground beneath them.
This follows the cloud map setting and is off at night.

## Specular highlights

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

## Surface detail (normal mapping)

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
a normal map. 0 leaves them flat, so only textures with normal maps get detail. The bumps come
from a slightly blurred copy of the texture and stay smooth up close, and sharp brightness edges
such as paint lines and team colour borders emboss only softly.)
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
ground glow and the police car's lights. Also `Dynamic lights` in the advanced display options.
Needs `CheckDynamicLights` in `OptionsMenu.wnd` for the menu control.)
* `PixelLights = Yes` - (No keeps dynamic lights on the old per-vertex lighting. Also `Per-pixel
lights` in the advanced display options, greyed out while dynamic lights are off. Needs
`CheckPixelLights` in `OptionsMenu.wnd` for the menu control.)

Notes:
* Every draw picks its own lights, so a busy battle can light the whole screen per pixel:
  * The terrain draws in patches of 32 by 32 cells, each taking nine lights.
  * Each vehicle, structure or soldier takes the eight brightest where it stands.
  * Each bridge takes nine.
  * Roads take the nine lights nearest the middle of the view.
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

* `SoftParticles = Yes` - (No draws sprites with hard edges. Also `Soft particles` in the advanced
display options. Needs `CheckSoftParticles` in `OptionsMenu.wnd` for the menu control.)

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

Tuned in the mod's `GameData.ini`. The same keys in a `ParticleSystem` block override them for
that system alone:

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
* Streaks, projectile streams, volume particles and terrain-conforming particles stay plain.
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

Listed and tuned in the mod's `GameData.ini`:

* `ElectricParticleTextures = TeslaBlast.tga ...` - (Textures whose systems turn electric. Each line
adds to the list, so a long list can span several lines. Read at launch.)
* `ElectricArcs = 1.5` - (Arc brightness. 0 turns the arcs off.)
* `ElectricArcSharpness = 10` - (Lower gives broad glowing bands, higher thin threads.)
* `ElectricNoiseSize = 40` - (World units across one tile of arc noise. Smaller gives more, closer arcs.)
* `ElectricJitter = 0.03` - (How far the texture jumps each crackle, in texture widths.)
* `ElectricFlicker = 0.6` - (How far brightness swings, as a fraction. 0 is steady.)
* `ElectricRate = 15` - (Crackles per second. 0 freezes the arcs.)

A `W3DLaserDraw` module takes the same six tuning keys. Each one it sets overrides `GameData.ini`
for that beam alone, and the keys it leaves out keep `GameData.ini`'s values.

```
ParticleSystem EMPRing
  ...
  ElectricShader = Yes
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
catch more light, and the laser shader's pulses brighten the ground as they pass. Beams drawn plain
or electric light the ground steadily. The Direct3D 8
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

Notes:
* The light lights the ground's own colour, measured against the map's terrain lighting, so red
ground turns redder and a dark night map lights up as much as a bright day.
* The light reaches `GroundGlowRadius` on the module, else `LaserGroundGlowRadius`, else 1.25 times
the laser's `OuterBeamWidth`, at least 15. It is counted from the beam in three dimensions.
* Lasers no longer take dynamic lights, so the per-pixel lights stay free for explosions and
muzzle flashes.
* Water covers the glow on ground beneath it. Units and buildings are not lit.
* Launch with `CONTRA_LASERGLOW=0` to go back to the dynamic lights.

## Ambient occlusion

Creases, corners, and the ground where units and buildings stand fall into soft shade, so objects sit
on the terrain instead of floating over it. Needs the Direct3D 9 build, shader model 2.0a and
anti-aliasing off.

* `AmbientOcclusion = Yes` - (No turns the shade off. Also `Ambient occlusion` in the advanced display
options, greyed out while anti-aliasing is on. Needs `CheckAmbientOcclusion` in `OptionsMenu.wnd` for
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
* `Strength` - (How far it bends the scene, in world units. Around 2 to 6 reads well.)
* `Duration` - (How long the ring takes to reach `Radius`, in milliseconds. It fades as it goes.)

```
FXList FX_NukeExplosion
  Shockwave
    Radius   = 300
    Width    = 40
    Strength = 4
    Duration = 900
  End
End
```

Notes:
* At most 16 rings show at once; a new one replaces the oldest.
* The ring lies flat at the effect's height, so on steep ground it is centred but not draped.
