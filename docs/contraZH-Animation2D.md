# Animation2D.ini

## Texture

An `Animation` frame can name a texture file directly with `Texture = <file> [width height]`
instead of a `MappedImage`. Quickest way to add a one-image icon: drop a `.tga` in `Art\Textures\`
and reference it.

* `Texture = jammer.tga` - (The file to draw. Width and height default to `32 32`.)
* `Texture = jammer.tga 64 64` - (Explicit draw size. Match the file's real pixel size, or the
image is scaled to fit.)

```
Animation Jammed
  NumberImages   = 1
  Texture        = jammer.tga
  AnimationMode  = ONCE
  AnimationDelay = 0
End
```

Notes:
* `Texture` and `Image` lines can be mixed; `NumberImages` counts both.
* The file name is the image name, shared across animations; an existing `MappedImage` of that name
is reused.
* A `.dds` of the same name wins over a loose `.tga`.
* Static icon: `NumberImages = 1`, `AnimationMode = ONCE`, `AnimationDelay = 0`.
* Presentation only; replays unaffected.
