# Direct3D 9 Features

Graphics features that need the Direct3D 9 build. The Direct3D 8 build ignores their settings.
Options live in `Options.ini` and the advanced display options; tuning keys live in the mod's
`GameData.ini`. Bloom, vertical sync and laser ground glow work on both builds and are on
[contraZH Options.ini](contraZH-Options.md#rendering).

Cheat builds reload `Data\INI\GameData.ini` about half a second after it is saved, so these tuning
keys can be adjusted with a map running: `UnitSpecularIntensity`, `UnitSpecularPower`,
`UnitBumpHeight`, `UnitNormalMapStrength`, `TerrainNormalMapStrength`, the `TerrainGlint` keys, `UnitEmissiveIntensity`,
`UnitEmissiveNightIntensity`, `SoftParticleDistance`, `AmbientOcclusionRadius`,
`AmbientOcclusionStrength`, the `GroundNoise`, `TerrainHeightBlend` and `SkyCloud` keys, `TerrainAtlasBorder` and the `Flame`, `Haze`, `Electric`, `Laser`, `Cryo` and `Disruption` tuning keys, the `SandStorm` and `SnowStorm` keys, the `Headlight` keys but `HeadlightShader`, the `PlanarMirror` keys but `PlanarMirrorShader`, the `MotionBlur` keys, and the `ColorLut` keys. Other `GameData.ini` keys keep their
value until a restart. The saved values win over a map's `map.ini` until the map loads again. A
deleted key keeps its value until a restart, and a file with an error applies only the keys above
the error until the next save.

## Pages

* [Lighting & Surfaces](dx9-Lighting.md) - shadow mapping, specular, terrain glint, normal and glow maps, per-pixel lights, ambient occlusion
* [Terrain](dx9-Terrain.md) - ground noise, height blending and stochastic tiling
* [Effect Shaders](dx9-Effect-Shaders.md) - soft particles, flame, electric, laser, cryo, disruption, shockwaves and motion blur
* [Weather, Sky & Reflections](dx9-Environment.md) - storms, headlights, planar mirrors, sky, colour grading and water
* [Performance](dx9-Performance.md) - supersampling, instancing, GPU skinning and presentation

## Shader implementation

An effect picks up its shader in one of five places: the `ParticleSystem` block, the `W3DLaserDraw`
module, a `W3DModelDraw` module, a `W3DStormDraw` module, or an `FXList` nugget. Tuning defaults live in `GameData.ini`, and some places can override
them for one effect. Each shader's own section, linked in the table, lists its keys.

| Effect | Shader | Set in | Turn on with | Own tuning |
|---|---|---|---|---|
| Fire sprites | [Flame](dx9-Effect-Shaders.md#flame-shading) | `ParticleSystem` | `FlameShader = Yes` | `Flame` and `Haze` keys |
| Fire trails | [Flame](dx9-Effect-Shaders.md#flame-shading) | `ParticleSystem` | `Type = STREAK` and `FlameShader = Yes` | `Flame` keys |
| Fire beams | [Flame](dx9-Effect-Shaders.md#flame-shading) | `W3DLaserDraw` | `FlameShader = Yes` | `Flame` keys |
| Burning, crackling or frozen models | [Flame](dx9-Effect-Shaders.md#flame-shading), [Electric](dx9-Effect-Shaders.md#electric-shading), [Cryo](dx9-Effect-Shaders.md#cryo-shading) | `W3DModelDraw` | `FlameShader`, `ElectricShader` or `CryoShader = Yes` | That shader's keys |
| Sparks and flares | [Electric](dx9-Effect-Shaders.md#electric-shading) | `ParticleSystem` | `ElectricShader = Yes` | `ElectricParticleScale` |
| Laser trails | [Laser](dx9-Effect-Shaders.md#laser-shading) | `ParticleSystem` | `Type = STREAK` and `LaserShader = Yes` | None |
| Laser beams | [Laser](dx9-Effect-Shaders.md#laser-shading) | `W3DLaserDraw` | On by default | `Laser` keys |
| Tesla and lightning bolts | [Electric](dx9-Effect-Shaders.md#electric-shading) | `W3DLaserDraw` | `ElectricShader = Yes` | `Electric` keys |
| Freeze rays | [Cryo](dx9-Effect-Shaders.md#cryo-shading) | `W3DLaserDraw` | `CryoShader = Yes` | `Cryo` keys |
| Frost trails, puffs and flares | [Cryo](dx9-Effect-Shaders.md#cryo-shading) | `ParticleSystem` | `CryoShader = Yes` | `CryoParticleScale` |
| Blast ring | [Shockwave](dx9-Effect-Shaders.md#shockwave-fxlistini) | `FXList` | A `Shockwave` block | The block's keys |
| Jammer fields and distortion auras | [Disruption](dx9-Effect-Shaders.md#disruption-shading) | `ParticleSystem`, `W3DModelDraw`, `W3DLaserDraw` | `DisruptionShader = Yes` or `Only` | `Disruption` keys |
| Distortion disc with no art | [Disruption](dx9-Effect-Shaders.md#disruption-shading) | `FXList` | A `Disruption` block | The block's keys |
| Sandstorm or snowstorm | [Storm](dx9-Environment.md#storms) | `FXList`, `W3DStormDraw` | A `Storm` block or the module | `SandStorm` and `SnowStorm` keys |
| Vehicle headlights | [Headlights](dx9-Environment.md#headlights) | Automatic, `W3DModelDraw` | A `HEADLIGHT` mesh in the model | `Headlight` keys, per model too |
| Mirror floors, puddles, pools and glass | [Planar mirrors](dx9-Environment.md#planar-mirrors) | `W3DModelDraw` | `PlanarMirror = Yes` | `PlanarMirror` keys, per model too |
| Soft edges on sprites | [Soft particles](dx9-Effect-Shaders.md#soft-particles) | Automatic | Nothing | None |
| Blur on fast rotors and aircraft | [Motion blur](dx9-Effect-Shaders.md#motion-blur) | Automatic, `W3DModelDraw` | Nothing | `MotionBlur` keys |
| Glow around bright effects | [Bloom](contraZH-Options.md#bloom) | `ParticleSystem` | `Shader = ADDITIVE` | None |

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
| Bumps on a unit, structure or bridge | `<texture>_nrm.dds` | [Surface detail](dx9-Lighting.md#surface-detail-normal-mapping) |
| Lit windows, lamps and exhausts | `<texture>_emi.dds` | [Glow masks](dx9-Lighting.md#glow-masks) |
| Bumps on terrain | `<terrain texture>_nrm.dds` | [Surface detail](dx9-Lighting.md#surface-detail-normal-mapping) |
| Bumps on a road | `<road texture>_nrm.dds` | [Surface detail](dx9-Lighting.md#surface-detail-normal-mapping) |
| Stones pushing through blends | `<terrain texture>_hgt.dds` | [Height blending](dx9-Terrain.md#height-blending) |
| Shine on one terrain type | `GlintStrength` and `GlintGloss` in `Terrain.ini` | [Terrain glint](dx9-Lighting.md#terrain-glint) |

### Troubleshooting

* The Direct3D 8 build ignores every shader in these pages.
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
