# ParticleSystem.ini

## ConformToTerrain

* `ConformToTerrain = No` - (Default. `Yes` opts a single effect into terrain conforming.)

Ground aligned, non-billboarded, flat particles are built as a mesh from the terrain heightmap, like
decals, instead of a flat quad that cuts through hills.

Notes:
* Opt-in per effect; retail effects are unchanged.
* Cost grows with particle size squared. Past 160 cells per side the mesh samples every Nth cell.

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
