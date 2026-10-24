# Storms

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

# Headlights

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

# Planar mirrors

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

# HQ sky

Cloud shadows drift softly over the ground, change shape as they go and never repeat, in place of
one tiled cloud texture sliding across the map. Each frame a shader draws the clouds into a map over
the ground the camera sees, and terrain, roads, bridges, units and buildings darken from it. Two
cloud shapes at unrelated sizes and angles add up to each cloud, a slow warp bends them, and a
finer layer frays their edges. Each layer drifts at its own speed, so clouds form and fade instead
of sliding as one sheet. Needs the Direct3D 9 build and Cloud shadows on.

* `HQSky = Yes` - (No brings back the tiled cloud texture. Also `HQ sky` on the Graphics
page, greyed out while Cloud shadows is off. Needs `CheckHQSky` in `OptionsMenu.wnd` for the
menu control.)

Tuned in the mod's `GameData.ini`:

* `SkyCloudSize = 600` - (World units across a typical cloud.)
* `SkyCloudCoverage = 0.45` - (Share of the ground in shadow. 0 is a clear sky, 1 overcast.)
* `SkyCloudSoftness = 0.25` - (How wide the fade at a cloud's edge is. Low gives crisp edges.)
* `SkyCloudShadowStrength = 0.6` - (How much sun a thick cloud blocks. 0 for none, 1 as dark as a cast
shadow.)
* `SkyCloudShadowTint = R:235 G:242 B:255` - (The hue of the sunlight a cloud takes away. White gives
neutral grey.)
* `SkyCloudWindSpeed = 11` - (World units a second the clouds drift. 0 holds them still.)
* `SkyCloudWindAngle = 56` - (Degrees the clouds drift towards, 0 along the map's x. The default
matches the old clouds.)
* `SkyCloudChurn = 0.3` - (How fast shapes change as they drift. 0 slides them as one sheet.)
* `SkyCloudBillow = 0.5` - (How far shapes bulge and curl. High values twist them into streaks.)
* `SkyCloudDetail = 0.4` - (Ragged detail at the edges. 0 gives smooth blobs.)

Notes:
* Night keeps the clouds off, as before.
* Clouded ground keeps its sky light. Clouds darken cast shadows by the same factor as the ground,
so object shadows keep their contrast under clouds. Unit highlights fade under clouds too. With
shadow maps off, clouds darken everything as the old clouds did.
* `CONTRA_CLOUDSUN=0` brings back clouds that darken everything, to compare the two looks.
* Trees and the water's reflected sky keep their old look.
* `CONTRA_SKYCLOUDS=0` keeps the tiled texture, to rule the HQ sky out of a rendering fault.

# Colour grading

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

# Shader water

Lakes, seas and rivers are shaded per pixel, with refraction, reflection, sun glint, foam and
vertex waves. Its options and `Water.ini` parameters are on [Water](Water.md).

