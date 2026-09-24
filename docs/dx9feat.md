# Direct3D 9 Features

Graphics features that need the Direct3D 9 build. The Direct3D 8 build ignores their settings.
Options live in `Options.ini` and the advanced display options; tuning keys live in the mod's
`GameData.ini`. Bloom, vertical sync and laser ground glow work on both builds and are on
[contraZH Changes](contraZH-Changes.md#rendering).

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
a normal map. 0 leaves them flat, so only textures with normal maps get detail.)
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
light the ground in smooth circles and follow its bumps. Vehicles and structures near them are lit
the same way. Needs the Direct3D 9 build and shader model 2.0a; other cards keep the old lighting.

* `DynamicLights = Yes` - (No turns off every dynamic light: explosion and muzzle-flash pulses, laser
ground glow and the police car's lights. Also `Dynamic lights` in the advanced display options.
Needs `CheckDynamicLights` in `OptionsMenu.wnd` for the menu control.)
* `PixelLights = Yes` - (No keeps dynamic lights on the old per-vertex lighting. Also `Per-pixel
lights` in the advanced display options, greyed out while dynamic lights are off. Needs
`CheckPixelLights` in `OptionsMenu.wnd` for the menu control.)

Notes:
* Each frame the lights nearest the middle of the view are drawn per pixel, nine on the terrain
and eight on vehicles and structures. A light whose reach covers the middle counts as nearest. The
rest keep the old lighting, which lights the terrain by its corners, so large ones look blocky.
* Infantry, flat terrain and roads keep the old lighting.
* Launch with `CONTRA_PIXELLIGHTS=1` to limit per-pixel lights to the terrain, or `0` to turn them off.

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

```
ParticleSystem TankDragonMuzzleFlame
  ...
  FlameShader = Yes
End
```

Notes:
* Slave systems follow their master, and a system a particle carries follows that particle's system.
* Streaks, projectile streams, volume particles and terrain-conforming particles stay plain.
* On a card without shader model 2.0a, flames keep their shading but lose the soft fade.
* Launch with `CONTRA_FLAMESHADER=1` to drop the shimmer, or `0` to turn flame shading off.

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

## Shader water

Lakes, seas and rivers are shaded per pixel, with refraction, reflection, sun glint, foam and
vertex waves. Its options and `Water.ini` parameters are on [Water](Water.md).

## Hardware instancing

Copies of the same vehicle, structure or prop draw together in one call per mesh piece, in the
shadow map and in the main view along with their shadow and highlight passes. Needs the Direct3D 9
build and a shader model 3 card; other cards draw as before.

Notes:
* These still draw one at a time: infantry and other skinned meshes, fading units, camera-facing
sprites, objects lit by point or dynamic lights, and objects under shroud, jamming, frozen or heat
vision overlays.
* The `CONTRA_INSTANCING` environment variable helps track down rendering faults: `0` turns it off,
`1` limits it to the shadow map, `2` adds main view objects without shadow or highlight passes, and
`3` (the default) covers everything.

## GPU skinning

Infantry and other skinned meshes bend on the graphics card instead of the CPU, in the shadow map
and in the main view along with their shadow and highlight passes. Needs the Direct3D 9 build and a
shader model 2 card; other cards skin on the CPU as before.

Notes:
* These still skin on the CPU: meshes following more than 70 bones, camera-facing or sorted meshes,
objects lit by point or dynamic lights, and objects under shroud, jamming, frozen or heat vision
overlays.
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
