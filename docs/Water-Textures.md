# Textures

All sit in `Art\Textures` itself, not in a subfolder.

* `TWWater01_nrm.dds` for `TWWater01.tga` - (Normal map that replaces the built-in waves, named like
unit normal maps. It tiles every `ShaderWaterWaveScale` units, in the DirectX convention. DXT
compressed with mipmaps; the game reads no other DDS layout.)
* `TWWater01_hgt.dds` - (Swell heights, greyscale DXT with mipmaps, mid grey is the resting level.
The height is read from alpha in a DXT5 file whose alpha varies, and from green otherwise. Without
it the swell uses the built-in waves.)

* `WaterFoam.dds` - (Foam for every water texture, replacing the built-in cell web. Greyscale, read
from red, bright where foam is dense; a tiling DXT with mipmaps. One pattern covers 150 world units up
close. `TWWater01_foam.dds` beside a water texture overrides it for that texture alone.)

`scripts/water_maps.py` builds the normal and swell textures from any image (needs Python with numpy
and Pillow).

# Notes

* Many maps pick their own `StandingWaterTexture` in `map.ini`, such as `twwater01trop.tga` on
tropical maps, `twwater01ice.tga` on winter maps and `twlava.tga` for lava. To change such a map's
water, replace that texture; its `_nrm.dds` and `_hgt.dds` follow its name.
* The sky in the reflection is the map's skybox (`SkyboxTexture*` in `WaterTransparency`), dimmed by
the map's lighting.
* One water height is mirrored at a time, that of the flat water under the middle of the view. Water
at other heights and sloping rivers show the skybox alone, as does a camera looking nearly level.
* Particles, decals and shadows are left out of the reflection, and only objects near the view are
mirrored.
* Objects standing in or over the water are kept out of the refraction, so hulls and props don't
smear into the waves.
* Effects drawn after the water (smoke, fire, translucent models) are not bent by the waves.
* `AdditiveBlending = Yes` water keeps the old look.
* Lava maps should set `IsWater = No` in the `WaterTransparency` block of their `map.ini`, so the
lava keeps the old look instead of reflecting the sky and gathering foam. The next map gets shader
water back.
* The `CONTRA_WATER` environment variable picks the water: `0` the old water, `1` pixel shader 2.0a
water, `2` shader model 3 water with vertex waves on each water area's own grid, `3` (the default)
with them on the grids around the camera.

