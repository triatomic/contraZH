# Terrain brushes

Terrain Material's New... button opens the New Terrain Brush dialog. A brush is a texture-tree entry
that does one of three things:

* Alias - (Paints an existing terrain under a new name, in its own folder.)
* Re-file - (Moves an existing terrain to another folder. The brush takes the terrain's name.)
* New texture - (Adds a terrain from a `.tga` in `Art\Terrain`, for the editor and the game.)

The dialog previews the whole texture of the picked terrain, or of the first new texture file.

## Brush files

New appends each brush to `WorldBuilderQt\TerrainBrushes.ini` in the game folder. WorldBuilder reads
every `.ini` in `WorldBuilderQt\` at startup; the game never reads them. A hand edit takes effect on
the next start.

```
Brush MyRoadPatch          ; alias
  Terrain = AsphaltType1
  Category = Urban\Roads
End

Brush GrassType2           ; re-file
  Category = Grass\Green
End
```

* Category - (Folder path in the texture tree, levels split by `\` or `/`. It replaces the `Class`
folder. An alias or new texture without one goes under Custom.)
* Terrain - (Terrain.ini name the brush paints. Maps store that name, so they load in any build.
Left out, the brush re-files the terrain of its own name.)
* Brush names use letters, digits, `_`, `-` and `.`.

## New textures

A new texture also appends a `Terrain` block to the loose `Data\INI\Terrain.ini`, so maps painted
with it draw in-game.

* Loose file only - (When `Terrain.ini` sits only inside a `.big`, New refuses. Extract it into
`Data\INI\` first; a new loose copy would hide every terrain in the archive.)
* Multiplayer - (Terrain INI is part of the multiplayer INI check, so every player needs the edited
`Terrain.ini`.)
* File - (24- or 32-bit `.tga`. Each side is cut into 64-pixel tiles, at most 10, so a side over 640
pixels is refused. The name may not contain spaces, `;`, `=` or quotes.)
* Browse - (Copies a file from outside `Art\Terrain` into it, replacing those characters with `_`.
A name already in `Art\Terrain` is skipped, never overwritten.)
* Several files - (Browse takes many at once. Each becomes its own brush named after its file, all
in the chosen category with the same keys. A failure skips that file and is listed; the rest are
created.)

## Terrain.ini keys

| Box | Key | In-game effect |
|---|---|---|
| Blend edges | `BlendEdges` | Custom blend-edge texture, listed in Blend Material, not the tree |
| Restrict construction | `RestrictConstruction` | Nothing can be built on it |
| Glint strength | `GlintStrength` | Multiplies GameData's `TerrainGlintIntensity`; 0 is dull |
| Glint gloss | `GlintGloss` | Glint tightness; unset takes GameData's `TerrainGlintGloss` |

* New texture - (Ticked boxes are written into its new block.)
* Existing terrain - (The boxes show its keys as the game parsed them. Blend edges is read-only.)
* Save to Terrain.ini - (Writes only the keys that changed, into the terrain's last block in the
loose `Terrain.ini`. A terrain defined only in an archive gets an override block of the same name.
An unticked key is written as its default, `No`, `1` or `0`, so an earlier block cannot bring an
old value back.)
* Later files - (Save refuses when a file in `Data\INI\Terrain\` also defines the terrain, because
the game loads it after `Terrain.ini`. Edit that file instead.)
* Create with unsaved key edits asks whether to save them too.

