# Electric & Laser Shading

What each electric and laser key does, with pictures. Picking systems, switches and Options.ini
settings are on [Direct3D 9 Features](dx9-Effect-Shaders.md#electric-shading).

All keys go in the mod's `GameData.ini`. Cheat builds reload them about half a second after the file
is saved, except the `ElectricParticleTextures` and `LaserParticleTextures` lists, which are read at
launch.

A `W3DLaserDraw` module can also set any electric or laser tuning key. The value it sets overrides
`GameData.ini` for that beam alone. `ElectricShader = Yes` on the module shades the beam as
electricity instead of as a laser.

The pictures run the game's shader math offline, on a stand-in flare and beam texture, at the scale
the default camera sees them. Arc shapes, strengths and colours match the game. The textures are
not Contra's own.

## Pages

* [Electric Shading](Electric-Shading.md) - arcs, jitter, crackle and distance keys
* [Laser Shading](Laser-Shading.md) - core, pulse and ground glow keys

## Tuner

The separate `Fxtuner` repository contains `fx_tuner.py`, which opens a window with sliders for fluid,
electric and laser keys and a live, animated preview beside them. It needs Python with numpy,
Pillow and PyQt5. Run the commands below from that repository.

The `Fluid` tab previews currents, bubble rings, edge breakup and glow. It supports pool textures,
colour, particle alpha and alpha/additive blend comparisons. All eight global fluid controls use
the same load, save, copy and reset paths as the other shaders. See
[Fluid shading](contraZH-ParticleSystems.md#fluid-shading) for keys and example settings.

```
python fx_tuner.py
```

* It loads the keys from the game's `GameData.ini` at start. The path is at the top of the window.
* `Save to GameData.ini` writes the slider values in place, keeping each line's comment, and adds
keys the file lacks. It backs the file up once per session. Cheat builds pick the change up within
a second, so the game shows it with a map running.
* `Copy lines` puts the keys on the clipboard instead.
* The `Contra` tab lists every GameData key contraZH adds, with a line of help each and no preview.
Keys a shader tab also shows stay in step with it. Its own keys are written only once changed.
* Preview-only controls set the sprite size, camera distance, colours and beam height. `Texture...`
loads an extracted `.tga` or `.dds` in place of the stand-in flare or beam.
* `python fx_tuner.py --docs` redraws the documentation pictures.
