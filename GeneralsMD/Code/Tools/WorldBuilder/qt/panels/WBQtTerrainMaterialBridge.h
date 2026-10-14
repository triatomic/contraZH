// WBQtTerrainMaterialBridge.h -- self-contained opaque facade for the Qt Terrain Material panel.
//
// Mirrors the style of the shared WBQtPanelBridge.h but is kept in its own header so this
// migration does not have to touch that shared file. Carries ONLY int/double/char*/void* (no
// Qt or MFC types), so the MFC bridge TU and the Qt panel TU can share these declarations
// without including each other's headers. Both src/WBQtTerrainMaterialBridge.cpp (MFC side,
// defines the reverse callbacks) and qt/panels/WBQtTerrainMaterialPanel.cpp (Qt side, calls
// them) include it.
//
// The MFC TerrainMaterial dialog stays created as the hidden OFF fallback and owns the
// favorites tree (m_favTreeView, which the favorites .ini persistence uses) plus the width /
// height edit boxes the popup sliders write. The selection statics (m_currentFgTexture /
// m_currentBgTexture) and the paint/copy/pathing/mirror flags are class statics the TileTool /
// FloodFillTool / EyedropperTool read, so the Qt panel drives those through the same setters /
// MFC handlers and the tools keep working unchanged. The texture list + tile-swatch pixels are
// read straight off WorldHeightMapEdit's statics (no MFC dialog needed).
#ifndef WB_QT_TERRAIN_MATERIAL_BRIDGE_H
#define WB_QT_TERRAIN_MATERIAL_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// --- Texture tree enumeration. Mirrors TerrainMaterial::addTerrain() -- each texture class
//     yields its tree category path (up to 4 "\"-joined segments) + a display leaf label
//     ("NN% name") + whether it should be shown at all (blend-edge tiles are skipped). ------
int  WBQtTerrainMaterial_GetTexClassCount(void);
// Fills pathOut with the "\"-joined category path and leafOut with the "NN% name" leaf label.
// Returns 1 if this class should be listed, 0 if it should be skipped (blend edge / no data).
int  WBQtTerrainMaterial_GetTexClassEntry(int texClass, char *pathOut, char *leafOut, int cap);
// The lower-cased UI name, for the search filter (matches TerrainMaterial::OnSearch).
int  WBQtTerrainMaterial_GetTexClassUiName(int texClass, char *nameOut, int cap);
// The Terrain.ini name of a class. Returns 0 for eval textures and blend edges.
int  WBQtTerrainMaterial_GetTexClassTerrainName(int texClass, char *nameOut, int cap);
// The class' Terrain.ini keys as the game parsed them. A glintGloss of 0 means GameData's gloss.
// Returns 0 for eval textures.
int  WBQtTerrainMaterial_GetTerrainKeys(int texClass, int *blendEdges, int *restrictConstruction, double *glintStrength, double *glintGloss);

// --- WorldBuilder-only brushes from WorldBuilderQt\*.ini (TerrainBrushes). An alias is a named
//     tree entry that paints another terrain's texture class. -------------------------------
int  WBQtTerrainMaterial_GetBrushAliasCount(void);
// Fills the alias' texture class, category path, "NN% name" leaf and bare name. Returns 0 if its
// terrain has no loaded texture.
int  WBQtTerrainMaterial_GetBrushAliasEntry(int index, int *texClassOut, char *pathOut, char *leafOut, char *nameOut, int cap);
// Saves a brush to WorldBuilderQt\TerrainBrushes.ini and loads it. Pass a texture (relative to
// the terrain texture folder, also appended to the loose Data\INI\Terrain.ini) or a terrain
// name. Returns the texture class it paints, or -1 with the reason in errorOut. A success can
// still carry a warning in errorOut. The Terrain.ini keys apply only with a texture; a negative
// glintStrength or a glintGloss of zero or less leaves that key out.
int  WBQtTerrainMaterial_CreateBrush(const char *name, const char *category, const char *terrain, const char *texture,
	int blendEdges, int restrictConstruction, double glintStrength, double glintGloss, char *errorOut, int cap);
// The game-relative terrain texture folder ("Art/Terrain/").
int  WBQtTerrainMaterial_GetTerrainTextureDir(char *dirOut, int cap);
// Whole-texture previews as BGRA rows, bottom row first, sized up to GetPreviewMaxExtent on a
// side. Each returns the side length in pixels, or 0 if there is nothing to show.
int  WBQtTerrainMaterial_GetPreviewMaxExtent(void);
int  WBQtTerrainMaterial_GetTexClassPreview(int texClass, unsigned char *bgraOut, int cap);
// The texture is named relative to the terrain texture folder and need not be loaded yet.
int  WBQtTerrainMaterial_GetTexturePreview(const char *texture, unsigned char *bgraOut, int cap);

// --- Selection. The fg/bg class indices are the statics the tools read. Selecting a class
//     validates the size (canFitTexture) and returns 0 (with the class still set) if it does
//     not fit, so the panel can warn -- exactly like TerrainMaterial::OnNotify. -------------
int  WBQtTerrainMaterial_GetFgTexClass(void);
int  WBQtTerrainMaterial_GetBgTexClass(void);
// Returns 1 if the class fits, 0 if it is too large (the fg class is set either way).
int  WBQtTerrainMaterial_SelectFgTexClass(int texClass);
void WBQtTerrainMaterial_SwapTextures(void);
// The leaf name shown in the label (last path element of the fg class UI name).
int  WBQtTerrainMaterial_GetFgLeafName(char *nameOut, int cap);

// --- Tile swatch pixels. Each class' preview tile is a fixed square DIB (TILE_PIXEL_EXTENT
//     wide, 32bpp BGRA, top-down), the same pixels TerrainSwatches::DrawMyTexture blits. -----
int  WBQtTerrainMaterial_GetSwatchExtent(void);	// TILE_PIXEL_EXTENT (pixels per side)
// Copies the class' TILE_PIXEL_EXTENT^2 * 4 BGRA bytes into bgraOut. Returns 1 on success, 0
// if the class has no tile data (caller should draw a solid placeholder).
int  WBQtTerrainMaterial_GetSwatchPixels(int texClass, unsigned char *bgraOut, int cap);

// --- Brush size / z-height. Route through the same setWidth / BigTileTool path the MFC popup
//     sliders + size edit use, so the tool and the (hidden) MFC edit boxes stay in sync. -----
int  WBQtTerrainMaterial_GetWidth(void);
void WBQtTerrainMaterial_SetWidth(int width);
int  WBQtTerrainMaterial_GetHeight(void);
void WBQtTerrainMaterial_SetHeight(int height);
int  WBQtTerrainMaterial_GetMinTileSize(void);
int  WBQtTerrainMaterial_GetMaxTileSize(void);
int  WBQtTerrainMaterial_GetMinZHeight(void);
int  WBQtTerrainMaterial_GetMaxZHeight(void);
double WBQtTerrainMaterial_GetFeetPerCell(void);	// MAP_XY_FACTOR
// 1 when the active tool is the single-cell TileTool (the multi-tile-only controls -- brush
// size, copy mode, pattern paint, mirror, no-mixing -- are disabled), mirroring setToolOptions.
int  WBQtTerrainMaterial_IsSingleCell(void);

// --- Pathing (passable / impassable) painting. Mirrors OnPassableCheck / OnPassable /
//     OnImpassable: enabling paints pathing (and flips the impassable overlay). -------------
void WBQtTerrainMaterial_SetPaintPathing(int on);
int  WBQtTerrainMaterial_IsPaintingPathing(void);
void WBQtTerrainMaterial_SetPassable(int passable);	// 1 = passable, 0 = impassable
int  WBQtTerrainMaterial_IsPassable(void);

// --- Pattern paint mode (multi-tile only). Mirrors OnTogglePaintMode / OnPaintModeCombo. ---
int  WBQtTerrainMaterial_GetPaintModeCount(void);
int  WBQtTerrainMaterial_GetPaintModeName(int index, char *nameOut, int cap);
void WBQtTerrainMaterial_SetPatternPaint(int on);
int  WBQtTerrainMaterial_IsPatternPaint(void);
void WBQtTerrainMaterial_SetPaintMode(int mode);	// 1-based, index+1 like the MFC combo
int  WBQtTerrainMaterial_GetPaintMode(void);
void WBQtTerrainMaterial_SetPaintDensity(int density);	// 0-100 (mode "Scatter B")
int  WBQtTerrainMaterial_GetPaintDensity(void);

// --- No-mixing toggle (multi-tile only). Mirrors OnToggleNoMixing. -------------------------
void WBQtTerrainMaterial_SetNoMixing(int on);
int  WBQtTerrainMaterial_IsNoMixing(void);

// --- Water-only painting. Texture paint skips cells outside the water areas. ----------------
void WBQtTerrainMaterial_SetPaintWaterOnly(int on);
int  WBQtTerrainMaterial_IsPaintWaterOnly(void);

// --- Copy mode (texture / terrain / raise-only + select / apply + rotation). Mirrors
//     OnCopyMode / OnCopyModeTerrain / OnRaiseOnly / OnCopySelect / OnCopyApply / OnRotate*. -
void WBQtTerrainMaterial_SetCopyTextureMode(int on);
int  WBQtTerrainMaterial_IsCopyTextureMode(void);
void WBQtTerrainMaterial_SetCopyTerrainMode(int on);
int  WBQtTerrainMaterial_IsCopyTerrainMode(void);
void WBQtTerrainMaterial_SetRaiseOnly(int on);
int  WBQtTerrainMaterial_IsRaiseOnly(void);
void WBQtTerrainMaterial_SetCopySelectMode(void);
int  WBQtTerrainMaterial_IsCopySelectMode(void);
void WBQtTerrainMaterial_SetCopyApplyMode(void);
int  WBQtTerrainMaterial_IsCopyApplyMode(void);
void WBQtTerrainMaterial_SetCopyRotation(int degrees);	// 0 / 90 / 180 / 270
int  WBQtTerrainMaterial_GetCopyRotation(void);

// --- Mirror toggles (multi-tile only). Mirrors OnToggleMirror / OnToggleMirrorX/Y/XY, which
//     drive BOTH BigTileTool and FloodFillTool. ----------------------------------------------
void WBQtTerrainMaterial_ToggleMirror(void);
int  WBQtTerrainMaterial_GetMirror(void);
void WBQtTerrainMaterial_ToggleMirrorX(void);
int  WBQtTerrainMaterial_GetMirrorX(void);
void WBQtTerrainMaterial_ToggleMirrorY(void);
int  WBQtTerrainMaterial_GetMirrorY(void);
void WBQtTerrainMaterial_ToggleMirrorXY(void);
int  WBQtTerrainMaterial_GetMirrorXY(void);

// --- Favorites tree. The single source of truth stays the MFC dialog's m_favTreeView (the
//     .ini persistence walks it), so the Qt panel enumerates / adds / deletes / imports
//     through it via TerrainMaterial::qtFav*. -------------------------------------------------
int  WBQtTerrainMaterial_GetFavoriteCount(void);
// Fills nameOut with the favorite's label; texClassOut with its stored texture class index.
int  WBQtTerrainMaterial_GetFavorite(int index, char *nameOut, int cap, int *texClassOut);
// Adds the given class (by its tree leaf label) as a favorite (mirrors OnSetFavorite). Returns
// 1 if added, 0 if it already existed (a beep is issued MFC-side).
int  WBQtTerrainMaterial_AddFavorite(int texClass, const char *label);
void WBQtTerrainMaterial_DeleteFavorite(int index);
// Imports the favorites .ini from the current map folder (mirrors OnImportFavoritesFromMapFolder,
// but without the confirmation MessageBox -- the panel asks first). Returns the new count.
int  WBQtTerrainMaterial_ImportFavorites(void);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_TERRAIN_MATERIAL_BRIDGE_H
