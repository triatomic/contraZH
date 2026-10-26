# ParticleSystem.ini

## Bloom

`Bloom = Yes` is the default. Set `Bloom = No` on a particle system to exclude its particles
from the bloom pass while keeping their normal appearance.
The FX tuner's INI entries mode exposes `Bloom` under each particle system's `System` controls.

```ini
ParticleSystem MyBeamEffect
  Shader = ADDITIVE
  Bloom = No
  ; Other particle settings go here.
End
```

The global `Bloom` setting must be enabled for particles or objects to glow. Only additive
particles feed bloom; alpha-blended effects and smudges do not. The flag applies to sprites,
streaks and other additive particle types supported by the existing bloom renderer.
`ParticleSystemExtend` inherits the parent's flag and can override it with `Bloom = Yes` or `No`.
FX lists that spawn the particle system use its flag; there is no separate FXList key.

## D3D9 billboard rendering

* D3D9 builds camera-facing particle quads on the GPU when shader model 3 hardware is available.
* Texture atlases, particle colours, rotation, soft fading and shader effects keep their existing settings.
* Instancing works with `SkipTranslucencySort = No` and preserves triangle sorting with other translucent geometry.
* Each half of a sorted particle quad keeps its place in the sorted order; bloom, haze and disruption passes can instance independently.
* Streaks, volume particles, ground-aligned particles and D3D8 builds use the existing renderer.
* Set the environment variable `CONTRA_PARTICLE_INSTANCING=0` before starting the game to compare with CPU-built quads.

## ConformToTerrain

* `ConformToTerrain = No` - (Default. `Yes` opts a single effect into terrain conforming.)

Ground aligned, non-billboarded, flat particles are built as a mesh from the terrain heightmap, like
decals, instead of a flat quad that cuts through hills.

Notes:
* Opt-in per effect; retail effects are unchanged.
* Flat ground becomes one quad; only uneven ground is meshed cell by cell.
* Only the part on screen is meshed. Past 160 cells per side the mesh stops at coarser cells.

## Snow move effects

A vehicle showing its `SNOW` model state swaps its move particles for the system of the same name
ending in `Snow`.

```
ParticleSystem RocketBuggyDust      ; Dust = RocketBuggyDust in the draw module
ParticleSystem RocketBuggyDustSnow  ; used instead on snow
```

* `W3DTankDraw` - (`TreadDebrisLeft` and `TreadDebrisRight`. The defaults become
`TrackDebrisDirtLeftSnow` and `TrackDebrisDirtRightSnow`.)
* `W3DTruckDraw` - (`Dust`, `DirtSpray` and `PowerslideSpray`.)
* `W3DTankTruckDraw` - (All five keys above.)

Notes:
* A name without a `Snow` system keeps its normal effect, so nothing has to be defined.
* Follows the unit's own snow state, including a per-object weather override set in WorldBuilder.

## ParticleSystemExtend

See [Extend Blocks](contraZH-Extend.md#particlesystemini).
