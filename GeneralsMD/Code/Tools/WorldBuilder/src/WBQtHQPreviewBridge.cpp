// WBQtHQPreviewBridge.cpp -- the MFC side of the Qt HQ map preview dialog. See WBQtHQPreviewBridge.h.
#include "StdAfx.h"

#ifdef RTS_HAS_QT

#include "Lib/BaseType.h"
#include "WorldBuilderDoc.h"
#include "wbview3d.h"
#include "MapPreview.h"
#include "qt/panels/WBQtHQPreviewBridge.h"

#include <stdio.h>

static const char *HQ_PREVIEW_SECTION = "HQMapPreview";

static MapPreview *s_preview = NULL;
static WbView3d *s_view = NULL;
static CString s_mapPath;
static CString s_error;
static WbView3d::TopViewCapture s_liveView;

static void toParams(const WBQtHQPreviewParams *in, HQPreviewParams *out)
{
	out->relief = in->relief;
	out->elevation = in->elevation;
	out->waterFalloff = in->waterFalloff;
	out->depthTint = in->depthTint != 0;
	for (Int k = 0; k < 3; k++)
	{
		out->shallow[k] = in->shallow[k];
		out->deep[k] = in->deep[k];
	}
}

static void toCapture(const WBQtHQCaptureParams *in, HQCaptureParams *out)
{
	out->objects = in->objects != 0;
	out->trees = in->trees != 0;
	out->roads = in->roads != 0;
	out->colorGrade = in->colorGrade != 0;
	out->renderedWater = in->renderedWater != 0;
	out->shaderWater = in->shaderWater != 0;
	out->clouds = in->clouds != 0;
	out->macroTexture = in->macroTexture != 0;
	out->stochastic = in->stochastic != 0;
	out->timeOfDay = in->timeOfDay;
	out->area = in->area;
	out->customX0 = in->customX0;
	out->customY0 = in->customY0;
	out->customX1 = in->customX1;
	out->customY1 = in->customY1;
	out->supersample = in->supersample;
	out->size = in->size;
}

static Real getProfileReal(const char *key, Real fallback)
{
	CString text = ::AfxGetApp()->GetProfileString(HQ_PREVIEW_SECTION, key, "");
	float value = 0.0f;
	return (!text.IsEmpty() && sscanf(text, "%f", &value) == 1) ? value : fallback;
}

static void writeProfileReal(const char *key, Real value)
{
	CString text;
	text.Format("%g", value);
	::AfxGetApp()->WriteProfileString(HQ_PREVIEW_SECTION, key, text);
}

static int getProfileInt(const char *key, int fallback)
{
	return ::AfxGetApp()->GetProfileInt(HQ_PREVIEW_SECTION, key, fallback);
}

static void writeProfileInt(const char *key, int value)
{
	::AfxGetApp()->WriteProfileInt(HQ_PREVIEW_SECTION, key, value);
}

// Colours are stored as 0xRRGGBB.
static void getProfileColor(const char *key, int rgb[3])
{
	const Int packed = getProfileInt(key, -1);
	if (packed >= 0)
	{
		rgb[0] = (packed >> 16) & 0xff;
		rgb[1] = (packed >> 8) & 0xff;
		rgb[2] = packed & 0xff;
	}
}

static void writeProfileColor(const char *key, const int rgb[3])
{
	writeProfileInt(key, (rgb[0] << 16) | (rgb[1] << 8) | rgb[2]);
}

int WBQtHQPreview_Size(void)
{
	return s_preview ? s_preview->getHQSize() : HQ_PREVIEW_SIZE;
}

int WBQtHQPreview_Supersample(void)
{
	return s_preview ? s_preview->getHQSupersample() : HQ_SUPERSAMPLE;
}

int WBQtHQPreview_MaxCapture(void)
{
	return HQ_MAX_CAPTURE;
}

void WBQtHQPreview_GetMapCells(int *width, int *height, int *playableWidth, int *playableHeight)
{
	Int w = 0;
	Int h = 0;
	Int pw = 0;
	Int ph = 0;
	MapPreview::getHQMapCells(&w, &h, &pw, &ph);
	*width = w;
	*height = h;
	*playableWidth = pw;
	*playableHeight = ph;
}

void WBQtHQPreview_GetDefaults(WBQtHQPreviewParams *params, WBQtHQCaptureParams *capture)
{
	HQPreviewParams p;
	MapPreview::getDefaultHQParams(&p);
	params->relief = p.relief;
	params->elevation = p.elevation;
	params->waterFalloff = p.waterFalloff;
	params->depthTint = p.depthTint ? 1 : 0;
	for (Int k = 0; k < 3; k++)
	{
		params->shallow[k] = p.shallow[k];
		params->deep[k] = p.deep[k];
	}

	HQCaptureParams c;
	MapPreview::getDefaultHQCapture(&c);
	capture->objects = c.objects ? 1 : 0;
	capture->trees = c.trees ? 1 : 0;
	capture->roads = c.roads ? 1 : 0;
	capture->colorGrade = c.colorGrade ? 1 : 0;
	capture->renderedWater = c.renderedWater ? 1 : 0;
	capture->shaderWater = c.shaderWater ? 1 : 0;
	capture->clouds = c.clouds ? 1 : 0;
	capture->macroTexture = c.macroTexture ? 1 : 0;
	capture->stochastic = c.stochastic ? 1 : 0;
	capture->timeOfDay = c.timeOfDay;
	capture->area = c.area;
	capture->supersample = c.supersample;
	capture->size = c.size;

	// A custom area starts as the whole map.
	int playableWidth = 0;
	int playableHeight = 0;
	capture->customX0 = 0;
	capture->customY0 = 0;
	WBQtHQPreview_GetMapCells(&capture->customX1, &capture->customY1, &playableWidth, &playableHeight);
}

void WBQtHQPreview_GetLast(WBQtHQPreviewParams *params, WBQtHQCaptureParams *capture)
{
	WBQtHQPreview_GetDefaults(params, capture);
	params->relief = getProfileReal("Relief", params->relief);
	params->elevation = getProfileReal("Elevation", params->elevation);
	params->waterFalloff = getProfileReal("WaterFalloff", params->waterFalloff);
	params->depthTint = getProfileInt("DepthTint", params->depthTint);
	getProfileColor("ShallowWater", params->shallow);
	getProfileColor("DeepWater", params->deep);

	// The area is per map, so it is not remembered.
	capture->objects = getProfileInt("Objects", capture->objects);
	capture->trees = getProfileInt("Trees", capture->trees);
	capture->roads = getProfileInt("Roads", capture->roads);
	capture->colorGrade = getProfileInt("ColorGrade", capture->colorGrade);
	capture->renderedWater = getProfileInt("RenderedWater", capture->renderedWater);
	capture->shaderWater = getProfileInt("ShaderWater", capture->shaderWater);
	capture->clouds = getProfileInt("Clouds", capture->clouds);
	capture->macroTexture = getProfileInt("MacroTexture", capture->macroTexture);
	capture->stochastic = getProfileInt("Stochastic", capture->stochastic);
	capture->timeOfDay = getProfileInt("TimeOfDay", capture->timeOfDay);
	capture->supersample = getProfileInt("Supersample", capture->supersample);
	capture->size = getProfileInt("Size", capture->size);
}

int WBQtHQPreview_Render(const WBQtHQCaptureParams *capture)
{
	if (s_preview == NULL || s_view == NULL || capture == NULL)
	{
		return 0;
	}
	HQCaptureParams c;
	toCapture(capture, &c);
	if (s_preview->prepareHQ(s_view, c))
	{
		s_error.Empty();
		return 1;
	}
	s_error = s_view->getTopViewError();
	if (s_error.IsEmpty())
	{
		s_error = "the area is empty";
	}
	return 0;
}

int WBQtHQPreview_LiveBegin(const WBQtHQCaptureParams *capture)
{
	if (s_view == NULL || capture == NULL)
	{
		return 0;
	}
	HQCaptureParams c;
	toCapture(capture, &c);
	if (!MapPreview::getHQTopView(c, &s_liveView))
	{
		s_error = "the area is empty";
		return 0;
	}
	if (!s_view->beginTopView(s_liveView, TRUE))
	{
		s_error = s_view->getTopViewError();
		return 0;
	}
	return 1;
}

int WBQtHQPreview_LiveFrame(unsigned char *bgra, int size)
{
	if (s_view == NULL || bgra == NULL || !s_view->renderTopView(size, bgra))
	{
		s_error = (s_view != NULL) ? s_view->getTopViewError() : "the 3D view is not ready";
		return 0;
	}
	return 1;
}

static void fitSize(const WbView3d::TopViewCapture &view, int size, int *width, int *height)
{
	const Real w = view.x1 - view.x0;
	const Real h = view.y1 - view.y0;
	const Real scale = size / max(w, h);
	*width = max(1, (int)(w*scale + 0.5f));
	*height = max(1, (int)(h*scale + 0.5f));
}

void WBQtHQPreview_FitSize(const WBQtHQCaptureParams *capture, int size, int *width, int *height)
{
	*width = size;
	*height = size;
	HQCaptureParams c;
	WbView3d::TopViewCapture view;
	toCapture(capture, &c);
	if (MapPreview::getHQTopView(c, &view))
	{
		fitSize(view, size, width, height);
	}
}

int WBQtHQPreview_LivePresent(void *window, int size)
{
	if (s_view == NULL || window == NULL)
	{
		s_error = "the 3D view is not ready";
		return 0;
	}
	int width = 0;
	int height = 0;
	fitSize(s_liveView, size, &width, &height);
	Real area[4];
	area[0] = 0.5f - 0.5f*width/size;
	area[1] = 0.5f - 0.5f*height/size;
	area[2] = 0.5f + 0.5f*width/size;
	area[3] = 0.5f + 0.5f*height/size;
	if (!s_view->presentTopView(size, window, area))
	{
		s_error = s_view->getTopViewError();
		return 0;
	}
	return 1;
}

void WBQtHQPreview_LiveEnd(void)
{
	if (s_view != NULL)
	{
		s_view->endTopView();
	}
}

void WBQtHQPreview_GetError(char *buf, int size)
{
	if (buf != NULL && size > 0)
	{
		strncpy(buf, s_error, size - 1);
		buf[size - 1] = 0;
	}
}

void WBQtHQPreview_Compose(const WBQtHQPreviewParams *params, unsigned char *bgra)
{
	if (s_preview == NULL || params == NULL || bgra == NULL)
	{
		return;
	}
	HQPreviewParams p;
	toParams(params, &p);
	s_preview->composeHQ(p, bgra);
}

int WBQtHQPreview_Save(const WBQtHQPreviewParams *params, const WBQtHQCaptureParams *capture)
{
	if (s_preview == NULL || params == NULL || capture == NULL)
	{
		return 0;
	}
	const Int size = s_preview->getHQSize();
	std::vector<UnsignedByte> pixels(size*size*4);
	WBQtHQPreview_Compose(params, &pixels[0]);
	if (!MapPreview::writeHQ(s_mapPath, &pixels[0], size))
	{
		return 0;
	}
	writeProfileReal("Relief", params->relief);
	writeProfileReal("Elevation", params->elevation);
	writeProfileReal("WaterFalloff", params->waterFalloff);
	writeProfileInt("DepthTint", params->depthTint);
	writeProfileColor("ShallowWater", params->shallow);
	writeProfileColor("DeepWater", params->deep);
	writeProfileInt("Objects", capture->objects);
	writeProfileInt("Trees", capture->trees);
	writeProfileInt("Roads", capture->roads);
	writeProfileInt("ColorGrade", capture->colorGrade);
	writeProfileInt("RenderedWater", capture->renderedWater);
	writeProfileInt("ShaderWater", capture->shaderWater);
	writeProfileInt("Clouds", capture->clouds);
	writeProfileInt("MacroTexture", capture->macroTexture);
	writeProfileInt("Stochastic", capture->stochastic);
	writeProfileInt("TimeOfDay", capture->timeOfDay);
	writeProfileInt("Supersample", capture->supersample);
	writeProfileInt("Size", capture->size);
	return 1;
}

int WBQtHQPreview_Run(void *view, const char *mapPath)
{
	if (view == NULL || mapPath == NULL)
	{
		return -1;
	}
	// The dialogs would share one top view, so a second one waits for the first to close.
	if (s_preview != NULL)
	{
		return 0;
	}
	MapPreview preview;
	s_preview = &preview;
	s_view = (WbView3d *)view;
	s_mapPath = mapPath;

	CString tgaPath = mapPath;
	tgaPath.Replace(".map", ".tga");
	const int result = WBQtHQPreview_Show(tgaPath);
	s_view->endTopView();
	s_preview = NULL;
	s_view = NULL;
	return result;
}

#endif // RTS_HAS_QT
