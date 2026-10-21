# Supersampling

`Supersampling` on the Graphics page offers `150%` and `200%`. The world renders to a target that
many percent of the screen's size each way and is filtered down to the screen with a box filter,
which smooths edges, alpha-tested cutouts, particles and shader output alike. The interface and
the mouse draw after that at the screen's own size, so text stays sharp. Unlike MSAA the larger
target is single sampled, so the scene's depth stays readable and ambient occlusion, soft
particles, the storm haze and every other depth-reading effect keep working, now supersampled too.

Notes:
* 200% draws four times the pixels. Zoomed in on a big fight it costs about what MSAA 8x does.
* Supersampling takes MSAA's place; the Anti-aliasing box greys out while it is on.
* `Options.ini` stores it as `SuperSampling = 150` or `200`.
* The `CONTRA_SSAA` environment variable set to a percent overrides the option for one run.
* The combo needs a layout with the `ComboBoxSuperSampling` row, which `build/add_ssaa_wnd.py` adds
to a loose `OptionsMenu.wnd`.

# Hardware instancing

Copies of the same vehicle, structure or prop draw together in one call per mesh piece, in the
shadow map and in the main view along with their shadow and highlight passes. Needs the Direct3D 9
build and a shader model 3 card; other cards draw as before.

Notes:
* These still draw one at a time: infantry and other skinned meshes, fading units, camera-facing
sprites, objects lit by point lights or by more dynamic lights than they draw per pixel, and
objects under shroud, jamming, frozen or heat vision overlays.
* The `CONTRA_INSTANCING` environment variable helps track down rendering faults: `0` turns it off,
`1` limits it to the shadow map, `2` adds main view objects without shadow or highlight passes, and
`3` (the default) covers everything.

# GPU skinning

Infantry and other skinned meshes bend on the graphics card instead of the CPU, in the shadow map
and in the main view along with their shadow and highlight passes. Needs the Direct3D 9 build and a
shader model 2 card; other cards skin on the CPU as before.

Notes:
* These still skin on the CPU: meshes following more than 70 bones, camera-facing or sorted meshes,
objects lit by point lights or by more dynamic lights than they draw per pixel, and objects under
shroud, jamming, frozen or heat vision overlays.
* The `CONTRA_GPU_SKINNING` environment variable helps track down rendering faults: `0` turns it
off, `1` limits it to the shadow map, `2` adds main view skins without shadow or highlight passes,
and `3` (the default) covers everything.

# Faster translucent effects

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

# Flip model presentation

The Direct3D 9 build runs on a Direct3D 9Ex device. Windowed and borderless modes present through a
flip model swap chain, which skips the desktop compositor's copy and lets a borderless window at
desktop resolution flip straight to the screen. MSAA still works; the scene renders to a
multisampled target that resolves into the swap chain each frame.

Notes:
* Fullscreen keeps the classic swap chain.
* Terrain and tree textures ignore the texture reduction setting.
* The `CONTRA_D3D9EX` environment variable set to `0` returns to a plain Direct3D 9 device.

