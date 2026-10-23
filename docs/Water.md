# Water

Lakes, seas and rivers are shaded per pixel. The seabed ripples through the waves and fades into
the water colour with depth, the surface mirrors the cliffs, trees, units and buildings around it
against the map's skybox, the sun glints off the waves, and foam gathers along the shore and on swell
crests. Unit, building and cliff shadows darken the water when shadow mapping is on. Needs the
Direct3D 9 build and shader model 2.0a; other cards get the old water.

Shader model 3 cards draw the full look. The water texture's pattern gives way to a flat colour
that deepens with depth, four layers of waves light that colour and bend the seabed, and small sun
sparkles twinkle over the waves beside the broad glint. `ShaderWaterTexturePattern` brings the
texture's pattern back. Shader model 2.0a cards keep the water texture and two wave layers.

## Pages

* [Water.ini](Water-ini.md) - the main and advanced `Water.ini` keys by look
* [Key Reference](Water-Key-Reference.md) - every key with its format, default and range
* [Textures & Notes](Water-Textures.md) - normal, swell and foam textures, and notes

# Options.ini

* `ShowSoftWaterEdge = Yes` - (`Smooth water` in the options menu. On picks shader water, off the
old flat water.)
* `WaterReflections = Yes` - (`Water reflections` in the options menu, applied on Accept. No keeps
only the skybox in the water. The mirror draws the scene a second time at half resolution. Needs
`CheckWaterReflections` in `OptionsMenu.wnd` for the menu control.)
* `ShorelineFoam = Yes` - (`Shoreline foam` in the options menu. A band of surf runs along the
waterline, washing up and back out of step along the shore, and breaks into the foam web as the
water deepens. It follows `ShaderWaterFoamStrength`, `ShaderWaterShoreFoamDepth` and
`ShaderWaterShoreFoamSurge` shape it, and `ShaderWaterFoamDepth = 0` turns it off.
Shader model 3 lakes and seas only, not rivers. Needs `CheckShorelineFoam` in `OptionsMenu.wnd` for
the menu control.)
