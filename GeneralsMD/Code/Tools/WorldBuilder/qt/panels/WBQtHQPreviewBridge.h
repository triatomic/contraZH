// WBQtHQPreviewBridge.h -- opaque facade for the Qt HQ map preview dialog (File > Map.ini >
// Generate HQ tga).
//
// The MFC side holds the captured render and shades it. The Qt side shows the result and the
// controls. Shading controls ask for a new image at once; capture controls need a new render.
#ifndef WB_QT_HQ_PREVIEW_BRIDGE_H
#define WB_QT_HQ_PREVIEW_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// Colours are RGB, 0 to 255.
typedef struct WBQtHQPreviewParams
{
	float relief;
	float elevation;
	float waterFalloff;
	int depthTint;
	int shallow[3];
	int deep[3];
} WBQtHQPreviewParams;

enum
{
	WBQT_HQ_AREA_MAP = 0,
	WBQT_HQ_AREA_PLAYABLE = 1,
	WBQT_HQ_AREA_CUSTOM = 2
};

typedef struct WBQtHQCaptureParams
{
	int objects;
	int trees;
	int roads;
	int colorGrade;
	int renderedWater;	// the editor's flat water surface
	int shaderWater;	// the shader water, which wins over renderedWater
	int clouds;
	int macroTexture;
	int stochastic;		// stochastic filtering over all the ground
	int timeOfDay;		// 0 keeps the current one, 1 to 4 morning to night
	int area;			// WBQT_HQ_AREA_*
	int customX0;		// custom area corners in cells, without the border
	int customY0;
	int customX1;
	int customY1;
	int supersample;
	int size;
} WBQtHQCaptureParams;

// ====== Qt -> MFC (implemented in src/WBQtHQPreviewBridge.cpp) ======

// The size and supersampling of the current render, after any reduction.
int  WBQtHQPreview_Size(void);
int  WBQtHQPreview_Supersample(void);
int  WBQtHQPreview_MaxCapture(void);
// The map without its border, and the playable boundary, in cells.
void WBQtHQPreview_GetMapCells(int *width, int *height, int *playableWidth, int *playableHeight);
// The area at its own proportions within a size^2 square. Frames stretch the area over the whole square.
void WBQtHQPreview_FitSize(const WBQtHQCaptureParams *capture, int size, int *width, int *height);

void WBQtHQPreview_GetDefaults(WBQtHQPreviewParams *params, WBQtHQCaptureParams *capture);
// The settings last saved with, or the defaults.
void WBQtHQPreview_GetLast(WBQtHQPreviewParams *params, WBQtHQCaptureParams *capture);
// Renders the map again. Returns 1 on success.
int  WBQtHQPreview_Render(const WBQtHQCaptureParams *capture);
// Why the last render failed.
void WBQtHQPreview_GetError(char *buf, int size);
// The live view: Begin sets the scene up, Frame draws it into size^2 BGRA pixels, top row north, and End puts the editor back.
int  WBQtHQPreview_LiveBegin(const WBQtHQCaptureParams *capture);
int  WBQtHQPreview_LiveFrame(unsigned char *bgra, int size);
// Draws a frame of the given size straight into the window, a native child of the dialog, without reading it back.
int  WBQtHQPreview_LivePresent(void *window, int size);
void WBQtHQPreview_LiveEnd(void);
// Fills Size()^2 BGRA pixels, top row north.
void WBQtHQPreview_Compose(const WBQtHQPreviewParams *params, unsigned char *bgra);
// Writes the tga and remembers the settings. Returns 1 on success.
int  WBQtHQPreview_Save(const WBQtHQPreviewParams *params, const WBQtHQCaptureParams *capture);

// ====== MFC only (implemented in src/WBQtHQPreviewBridge.cpp) ======

// Runs the dialog over the map. Returns 1 when saved, 0 when cancelled, -1 on failure.
int  WBQtHQPreview_Run(void *view, const char *mapPath);

// ====== MFC -> Qt (implemented in qt/panels/WBQtHQPreviewDialog.cpp) ======

// Modal. Returns 1 when the user saved.
int  WBQtHQPreview_Show(const char *tgaPath);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_HQ_PREVIEW_BRIDGE_H
