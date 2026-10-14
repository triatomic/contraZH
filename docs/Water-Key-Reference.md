# Key reference

Every key `Water.ini` accepts, with its format, default and useful values. Colours are written
`R:0-255 G:0-255 B:0-255`, with ` A:0-255` added for RGBA. Booleans are `Yes` or `No`. Textures are
file names in `Art\Textures`.

The game enforces only the limits marked **(hard)**, clamping values past them. The other ranges
are where the water still looks right; values outside them load, but the look breaks down.

## WaterTransparency

One block. `map.ini` can override any of these keys for its map.

### Main keys

| Key | Format | Default | Range | Values |
|---|---|---|---|---|
| `StandingWaterColor` | RGB | `R:255 G:255 B:255` | `0` - `255` each **(hard)** | White tints lakes and rivers by the map's light times the `WaterSet` `DiffuseColor`. Black draws them unlit. Any other colour is used as the tint. |
| `ShaderWaterOpacity` | number | `0.95` | `0` - `1` | Opacity of deep water. `0` uses `TransparentWaterMinOpacity`. Above `1` over-brightens. |
| `ShaderWaterClarity` | number | `1.0` | `0.1` - `10` | Scales `TransparentWaterDepth` for how deep the seabed shows. |
| `ShaderWaterDeepColor` | RGB | unset | `0` - `255` each **(hard)** | Colour of deep water. Unset takes the average colour of `StandingWaterTexture`. Shader model 3 only. |
| `ShaderWaterWaveScale` | number | `160` | `1` **(hard)** - `2000` | World units one ripple pattern covers. Below `50` the ripples shimmer, above `2000` they are too broad to see. |
| `ShaderWaterWaveStrength` | number | `0.2` | `0` - `2` | Ripple steepness. `0` is flat. Above `2` the surface turns to glitter. |
| `ShaderWaterSpecular` | number | `1.0` | `0` - `5` | `0` turns the sun glint off. Above `5` the glint washes out to white. |
| `ShaderWaterSparkle` | number | `2.0` | `0` **(hard)** - `10` | Brightness of the small sun sparkles. `0` turns them off. Shader model 3 only. |
| `ShaderWaterReflection` | number | `3.0` | `0` - `10` | `0` turns the sky reflection off. Reflection is capped at 80% **(hard)**, so higher values only spread that cap to steeper views. |
| `ShaderWaterSwellHeight` | number | `3.0` | `0` - `10` | Height of the vertex waves in world units. `0` turns them off. Waves shrink in water shallower than twice this. Above `10` they cut into hulls. |
| `ShaderWaterFoamStrength` | number | `0.5` | `0` **(hard)** - `2` | Brightness of the foam. `0` turns it off. |
| `ShaderWaterAutoMeasure` | Yes/No | `Yes` | - | Calms water by its distance from shore. `No` gives all water the open look. Shader model 3 only. |
| `ShaderWaterEnclosedCalm` | number | `1.0` | `0` - `1` **(hard)** | How much calmer enclosed water is. `0` gives it the open look. |
| `ShaderWaterZoomCompensation` | Yes/No | `Yes` | - | Keeps the ripples and sun specks as they look up close at any zoom and camera pitch. Shader model 3 only. |

### Advanced keys

| Key | Format | Default | Range | Values |
|---|---|---|---|---|
| `TransparentWaterDepth` | number | `3.0` | `0` - `50` | Depth over which the seabed fades out and the surface fades in from the shore. `0` gives a hard shore edge. |
| `TransparentWaterMinOpacity` | number | `1.0` | `0` - `1` | Opacity of deep water for the old water, and for shader water when `ShaderWaterOpacity` is `0`. Above `1` over-brightens. |
| `StandingWaterTexture` | texture | `TWWater01.tga` | - | Surface texture of lakes, seas and rivers. Its `_nrm.dds` and `_hgt.dds` follow its name. |
| `AdditiveBlending` | Yes/No | `No` | - | `Yes` adds the water onto the scene and keeps the old water, with no shader water. |
| `IsWater` | Yes/No | `Yes` | - | `No` draws the map's water as the old water, with no shader water, for lava and other liquids that should not reflect, refract or foam. |
| `RadarWaterColor` | RGB | `R:140 G:140 B:255` | `0` - `255` each **(hard)** | Colour of water on the radar. |
| `SkyboxTextureN` | texture | `TSMorningN.tga` | - | North face of the skybox, which shader water reflects. Also `SkyboxTextureE`, `S`, `W` and `T` (top), defaulting to `TSMorningE.tga` and so on. |
| `ShaderWaterTexturePattern` | number | `0` | `0` - `1` **(hard)** | How much of the water texture's pattern shows over the water colour. `0` is the plain colour, `1` the full pattern. Shader model 3 only. |
| `ShaderWaterFoamDepth` | number | `6` | `0` - `30` | Depth where shore foam fades out. `0` turns foam off, crest foam included. |
| `ShaderWaterShoreFoamDepth` | number | `2.0` | `0.1` **(hard)** - `10` | Depth the shoreline foam band reaches at the top of its surge. Shader model 3 only. |
| `ShaderWaterShoreFoamSurge` | number | `0.4` | `0` - `0.9` **(hard)** | Share of the shoreline foam band the surge pulls back. `0` holds it still. |
| `ShaderWaterSpecularSpread` | number | `1.0` | `0.1` **(hard)** - `16` | Widens the sun glint so it shows at more view angles. Below `1` it narrows. Above `16` the glint turns into a haze over the whole surface. |
| `ShaderWaterVirtualSun` | Yes/No | `No` | - | `Yes` glints off a sun ahead of the camera at the map sun's height, so the glint shows from every view. |
| `ShaderWaterRefraction` | number | `0.015` | `0` - `0.1` | Fraction of the screen the waves bend the seabed by. `0` turns it off. Above `0.05` smears. |
| `ShaderWaterWaveShading` | number | `1.0` | `0` **(hard)** - `3` | How much the waves light and shade the water's colour. `0` leaves it flat. Shader model 3 only. |
| `ShaderWaterOpenReach` | number | `400` | `1` **(hard)** - `2550` | World units from shore at which water counts as open. Distances past `2550` count as `2550`. |
| `ShaderWaterStochasticSize` | number | `420` | `0`, or `30` - `1000` | World units between the cells that shift the patterns to hide their tiling. `0` turns it off. Below `30` the patterns blur, above `1000` the pattern shows within a cell. |
| `ShaderWaterStochasticSeabed` | Yes/No | `Yes` | - | `Yes` also hex-tiles the terrain under standing water. Needs pixel shader 2.0a. |
| `ShaderWaterSwellScale` | number | `700` | `1` **(hard)** - `3000` | World units one swell pattern covers. Below `200` the swell looks choppy, above `3000` it is too broad to see. |
| `ShaderWaterSwellSpeed` | number | `30` | `-200` - `200` | World units a second the swell drifts. `0` holds it still. Negative reverses it. |
| `ShaderWaterPlanarStrength` | number | `0.3` | `0` - `1` | Reflection the mirrored scene adds on top of the sky's. The 80% cap applies to the sum. |
| `ShaderWaterPlanarDistortion` | number | `0.02` | `0` - `0.1` | Fraction of the screen the waves bend the mirrored scene by. `0` keeps it sharp. |
| `ShaderWaterClearReflections` | Yes/No | `Yes` | - | Shadows leave the reflections as bright as around them. Shader model 3 only. |
| `ShaderWaterSoftShadows` | Yes/No | `Yes` | - | Shadows in the water blur with depth and sway with the ripples. Shader model 3 only. |
| `WaterAnimationFps` | whole number | `30` | `0`, or `30` - `60` **(hard)** | Moves the water as if the game ran at that rate. `0` moves it every frame. Values from `1` to `29` count as `30`, and above `60` as `60`. |

## WaterSet

One block per time of day: `WaterSet MORNING`, `AFTERNOON`, `EVENING` and `NIGHT`. Only
`DiffuseColor` touches the lakes, seas and rivers above. The rest drive the old mirrored water
types and the animated water grid, which shader water does not use.

| Key | Format | Default | Range | Values |
|---|---|---|---|---|
| `DiffuseColor` | RGBA | `R:0 G:0 B:0 A:0` | `0` - `255` each **(hard)** | Tint of lakes and rivers when `StandingWaterColor` is white. Alpha is the old water's opacity. |
| `TransparentDiffuseColor` | RGBA | `R:0 G:0 B:0 A:0` | `0` - `255` each **(hard)** | Colour and alpha of the old pixel shader sea. |
| `SkyTexture` | texture | none | - | Sky plane the old mirrored water reflects. |
| `SkyTexelsPerUnit` | number | `0` | `0.1` - `10` | Sky texture texels per world unit. Higher repeats the texture more. |
| `UScrollPerMS` | number | `0` | `-0.1` - `0.1` | Sky plane drift along one axis, in world units a millisecond. |
| `VScrollPerMS` | number | `0` | `-0.1` - `0.1` | Sky plane drift along the other axis, in world units a millisecond. |
| `Vertex00Color` | RGBA | `R:0 G:0 B:0 A:0` | `0` - `255` each **(hard)** | Colour at one corner of the sky plane. Also `Vertex01Color`, `Vertex10Color` and `Vertex11Color` for the other three. |
| `WaterTexture` | texture | none | - | Texture of the animated water grid. |
| `WaterRepeatCount` | whole number | `0` | `1` - `100` | Times `WaterTexture` repeats across the water grid. |

