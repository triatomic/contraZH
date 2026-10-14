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

`scripts/fx_tuner.py` opens a window with a slider for every electric and laser key and a live, animated
preview beside them. It needs Python with numpy, Pillow and PyQt5.

```
python scripts/fx_tuner.py
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
* `python scripts/fx_tuner.py --docs` redraws every picture in these pages into `docs/images`.
