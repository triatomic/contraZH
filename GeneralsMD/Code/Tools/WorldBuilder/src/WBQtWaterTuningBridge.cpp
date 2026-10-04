// WBQtWaterTuningBridge.cpp -- MFC side of the Qt Water tuning window.
//
// Holds the tables of WaterTransparency and GameData keys the window shows, and writes each change
// into the live data so the 3D view draws it. Whole body behind RTS_HAS_QT; empty TU when Qt is OFF.

#include "StdAfx.h"

#ifdef RTS_HAS_QT

#include "Lib/BaseType.h"
#include "Common/FileSystem.h"
#include "Common/GlobalData.h"
#include "GameClient/Water.h"
#include "W3DDevice/GameClient/BaseHeightMap.h"
#include "WorldBuilderDoc.h"
#include "wbview3d.h"
#include "qt/panels/WBQtWaterTuningBridge.h"

namespace
{
	struct WaterKey
	{
		const char *key;
		const char *help;
		int kind;
		float lo;
		float hi;
		float step;
		int advanced;
		Real WaterTransparencySetting::*real;
		Bool WaterTransparencySetting::*flag;
		RGBColor WaterTransparencySetting::*color;
		Bool rebuildsTerrain;	// the terrain caches this key in its shoreline tiles
	};

#define WATER_REAL(key, member, lo, hi, step, advanced, help) \
	{ key, help, WBQT_WATER_FLOAT, lo, hi, step, advanced, &WaterTransparencySetting::member, NULL, NULL, FALSE }
#define WATER_SHORE(key, member, lo, hi, step, advanced, help) \
	{ key, help, WBQT_WATER_FLOAT, lo, hi, step, advanced, &WaterTransparencySetting::member, NULL, NULL, TRUE }
#define WATER_BOOL(key, member, advanced, help) \
	{ key, help, WBQT_WATER_BOOL, 0.0f, 1.0f, 1.0f, advanced, NULL, &WaterTransparencySetting::member, NULL, FALSE }
#define WATER_COLOR(key, member, advanced, help) \
	{ key, help, WBQT_WATER_COLOR, 0.0f, 255.0f, 1.0f, advanced, NULL, NULL, &WaterTransparencySetting::member, FALSE }

	// IsWater, then the keys of the FX tuner's Water tab in its order, ranges and steps.
	const WaterKey s_keys[] =
	{
		WATER_BOOL("IsWater", m_isWater, 0,
			"No draws the old water without shaders, for lava and the like."),
		WATER_REAL("ShaderWaterOpacity", m_shaderWaterOpacity, 0.0f, 1.0f, 0.01f, 0,
			"Deep water opacity. 1 hides the seabed. 0 uses TransparentWaterMinOpacity."),
		WATER_REAL("ShaderWaterClarity", m_shaderWaterClarity, 0.1f, 10.0f, 0.1f, 0,
			"Scales TransparentWaterDepth. Higher sees deeper into the shallows."),
		WATER_COLOR("ShaderWaterDeepColor", m_shaderWaterDeepColor, 0,
			"Colour of deep water on shader model 3 cards. Unticked leaves the key out, so Water.ini decides."),
		WATER_REAL("ShaderWaterWaveScale", m_shaderWaterWaveScale, 1.0f, 2000.0f, 1.0f, 0,
			"World units one ripple pattern covers. Higher gives broader waves."),
		WATER_REAL("ShaderWaterWaveStrength", m_shaderWaterWaveStrength, 0.0f, 2.0f, 0.01f, 0,
			"Ripple steepness. Drives glint, reflection and bending."),
		WATER_REAL("ShaderWaterSpecular", m_shaderWaterSpecular, 0.0f, 5.0f, 0.05f, 0,
			"Scales the sun glint. 0 turns it off."),
		WATER_REAL("ShaderWaterSparkle", m_shaderWaterSparkle, 0.0f, 10.0f, 0.1f, 0,
			"Brightness of the small sun sparkles on the fine waves, on shader model 3 cards. 0 turns them off."),
		WATER_REAL("ShaderWaterReflection", m_shaderWaterReflection, 0.0f, 10.0f, 0.05f, 0,
			"Scales the sky reflection. 0 turns it off. All reflection caps at 80%."),
		WATER_REAL("ShaderWaterSwellHeight", m_shaderWaterSwellHeight, 0.0f, 10.0f, 0.1f, 0,
			"Height of the vertex waves on lakes and seas, in world units. 0 turns them off."),
		WATER_REAL("ShaderWaterFoamStrength", m_shaderWaterFoamStrength, 0.0f, 2.0f, 0.05f, 0,
			"Brightness of the foam. 0 turns it off."),
		WATER_BOOL("ShaderWaterAutoMeasure", m_shaderWaterAutoMeasure, 0,
			"Calms water by its distance from shore, so ponds, harbours and rivers are calmer than open sea."),
		WATER_REAL("ShaderWaterEnclosedCalm", m_shaderWaterEnclosedCalm, 0.0f, 1.0f, 0.01f, 0,
			"How much calmer enclosed water is than open water, 0 to 1: smaller waves, less swell and fewer sparkles."),
		WATER_BOOL("ShaderWaterZoomCompensation", m_shaderWaterZoomCompensation, 0,
			"Keeps the ripples and sun specks as they look up close at any zoom and camera pitch."),

		WATER_SHORE("TransparentWaterDepth", m_transparentWaterDepth, 0.0f, 50.0f, 0.1f, 1,
			"Depth over which the seabed fades out and the shore fades in. 0 gives a hard shore edge."),
		WATER_SHORE("TransparentWaterMinOpacity", m_minWaterOpacity, 0.0f, 1.0f, 0.01f, 1,
			"Deep water opacity of the old water, and of shader water when ShaderWaterOpacity is 0."),
		WATER_REAL("ShaderWaterFoamDepth", m_shaderWaterFoamDepth, 0.0f, 30.0f, 0.5f, 1,
			"Depth where shore foam fades out. 0 turns foam off, crest foam included."),
		WATER_REAL("ShaderWaterShoreFoamDepth", m_shaderWaterShoreFoamDepth, 0.1f, 10.0f, 0.1f, 1,
			"Depth the shoreline foam band reaches at the top of its surge. Gentle shores show it wider."),
		WATER_REAL("ShaderWaterShoreFoamSurge", m_shaderWaterShoreFoamSurge, 0.0f, 0.9f, 0.05f, 1,
			"Share of the shoreline foam band the surge pulls back. 0 holds the band still."),
		WATER_REAL("ShaderWaterSpecularSpread", m_shaderWaterSpecularSpread, 0.1f, 16.0f, 0.1f, 1,
			"Widens the sun glint to more view angles. Below 1 narrows it."),
		WATER_BOOL("ShaderWaterVirtualSun", m_shaderWaterVirtualSun, 1,
			"Glints off a sun ahead of the camera at the map sun's height."),
		WATER_REAL("ShaderWaterRefraction", m_shaderWaterRefraction, 0.0f, 0.1f, 0.001f, 1,
			"How far the waves bend the seabed, as a fraction of the screen."),
		WATER_REAL("ShaderWaterWaveShading", m_shaderWaterWaveShading, 0.0f, 3.0f, 0.05f, 1,
			"How much the waves light and shade the water's own colour on shader model 3 cards. 0 leaves it flat."),
		WATER_REAL("ShaderWaterTexturePattern", m_shaderWaterTexturePattern, 0.0f, 1.0f, 0.01f, 1,
			"How much of the water texture's pattern shows over the water colour. 0 is the plain colour, 1 the full pattern."),
		WATER_REAL("ShaderWaterOpenReach", m_shaderWaterOpenReach, 1.0f, 2550.0f, 10.0f, 1,
			"World units from shore at which water counts as fully open. The enclosed look fades into the open look over this distance."),
		WATER_REAL("ShaderWaterStochasticSize", m_shaderWaterStochasticSize, 0.0f, 1000.0f, 1.0f, 1,
			"World units between the cells that shift the textures to hide tiling. 0 turns it off."),
		WATER_BOOL("ShaderWaterStochasticSeabed", m_shaderWaterStochasticSeabed, 1,
			"Hex cells also hide the tiling of the terrain under standing water."),
		WATER_REAL("ShaderWaterSwellScale", m_shaderWaterSwellScale, 1.0f, 3000.0f, 1.0f, 1,
			"World units one swell pattern covers. Higher gives longer swells."),
		WATER_REAL("ShaderWaterSwellSpeed", m_shaderWaterSwellSpeed, -200.0f, 200.0f, 1.0f, 1,
			"World units a second the swell drifts. 0 holds it still."),
		WATER_REAL("ShaderWaterPlanarStrength", m_shaderWaterPlanarStrength, 0.0f, 1.0f, 0.01f, 1,
			"Reflection the mirrored scene adds on top of the sky's."),
		WATER_REAL("ShaderWaterPlanarDistortion", m_shaderWaterPlanarDistortion, 0.0f, 0.1f, 0.001f, 1,
			"How far the waves bend the mirrored scene, as a fraction of the screen."),
		WATER_BOOL("ShaderWaterClearReflections", m_shaderWaterClearReflections, 1,
			"Shadows leave the sky and mirrored scene in the water as bright as around them."),
		WATER_BOOL("ShaderWaterSoftShadows", m_shaderWaterSoftShadows, 1,
			"Shadows in the water blur with depth and sway with the ripples."),
	};

	const int s_keyCount = sizeof(s_keys) / sizeof(s_keys[0]);

	struct RenderKey
	{
		const char *key;
		const char *group;
		const char *help;
		int kind;
		float lo;
		float hi;
		float step;
		Real GlobalData::*real;
		Int GlobalData::*integer;
		RGBColor GlobalData::*color;
		AsciiString GlobalData::*text;
	};

#define RENDER_REAL(group, key, member, lo, hi, step, help) \
	{ key, group, help, WBQT_WATER_FLOAT, lo, hi, step, &GlobalData::member, NULL, NULL, NULL }
#define RENDER_INT(group, key, member, lo, hi, step, help) \
	{ key, group, help, WBQT_WATER_FLOAT, lo, hi, step, NULL, &GlobalData::member, NULL, NULL }
#define RENDER_COLOR(group, key, member, help) \
	{ key, group, help, WBQT_WATER_COLOR, 0.0f, 255.0f, 1.0f, NULL, NULL, &GlobalData::member, NULL }
#define RENDER_TEXT(group, key, member, help) \
	{ key, group, help, WBQT_WATER_TEXT, 0.0f, 0.0f, 0.0f, NULL, NULL, NULL, &GlobalData::member }

	// The GameData keys of the FX tuner's Glint, Ground, Blend, Sky and Colour grade tabs in its order, ranges and steps.
	const RenderKey s_renderKeys[] =
	{
		RENDER_REAL("Glint", "TerrainGlintIntensity", m_terrainGlintIntensity, 0.0f, 2.0f, 0.01f,
			"How bright the sun's glint on the ground is. 0 turns it off."),
		RENDER_REAL("Glint", "TerrainGlintGloss", m_terrainGlintGloss, 1.0f, 128.0f, 0.5f,
			"How tight the glint is. Higher gives a smaller, sharper glint, and 1 spreads it over all ground facing the sun."),
		RENDER_REAL("Glint", "TerrainGlintAlbedo", m_terrainGlintAlbedo, 0.0f, 1.0f, 0.05f,
			"How far the glint follows the ground's brightness. 0 glints dark and bright ground alike, and 1 leaves dark ground almost dull."),

		RENDER_REAL("Ground", "GroundNoiseStrength", m_groundNoiseStrength, 0.0f, 0.5f, 0.005f,
			"How far the ground's brightness strays from its average. 0 gives an even tone."),
		RENDER_REAL("Ground", "GroundNoiseSize", m_groundNoiseSize, 100.0f, 5000.0f, 10.0f,
			"World units across the broadest patches."),
		RENDER_REAL("Ground", "GroundNoiseTint", m_groundNoiseTint, 0.0f, 0.2f, 0.005f,
			"How far patches lean warm or cool. 0 keeps them grey."),
		RENDER_REAL("Ground", "GroundNoiseBrightness", m_groundNoiseBrightness, 0.5f, 1.3f, 0.01f,
			"The ground's average brightness under the noise."),

		RENDER_REAL("Blend", "TerrainHeightBlendStrength", m_terrainHeightBlendStrength, 0.0f, 6.0f, 0.05f,
			"How far the taller texture pushes into the other's side of a blend. 0 keeps the edge where the soft fade would put it."),
		RENDER_REAL("Blend", "TerrainHeightBlendSharpness", m_terrainHeightBlendSharpness, 1.0f, 12.0f, 0.1f,
			"How narrow the blend's edge is. 1 is as wide as the soft fade."),
		RENDER_INT("Blend", "TerrainAtlasBorder", m_terrainAtlasBorder, 4.0f, 32.0f, 4.0f,
			"Texels copied around each texture in the terrain atlas, in steps of 4. Wider keeps high anisotropy clean but fits fewer textures."),

		RENDER_REAL("Sky", "SkyCloudSize", m_skyCloudSize, 150.0f, 3000.0f, 10.0f,
			"World units across a typical cloud."),
		RENDER_REAL("Sky", "SkyCloudCoverage", m_skyCloudCoverage, 0.0f, 1.0f, 0.01f,
			"Share of the ground in shadow. 0 is a clear sky, 1 overcast."),
		RENDER_REAL("Sky", "SkyCloudSoftness", m_skyCloudSoftness, 0.02f, 1.0f, 0.01f,
			"How wide the fade at a cloud's edge is. Low gives crisp edges."),
		RENDER_REAL("Sky", "SkyCloudShadowStrength", m_skyCloudShadowStrength, 0.0f, 1.0f, 0.01f,
			"How dark a thick cloud's shadow is. 0 for none."),
		RENDER_COLOR("Sky", "SkyCloudShadowTint", m_skyCloudShadowTint,
			"The shadow's hue. White gives neutral grey. Unticked leaves the key out, so GameData.ini decides."),
		RENDER_REAL("Sky", "SkyCloudWindSpeed", m_skyCloudWindSpeed, 0.0f, 100.0f, 0.5f,
			"World units a second the clouds drift. 0 holds them still."),
		RENDER_REAL("Sky", "SkyCloudWindAngle", m_skyCloudWindAngle, 0.0f, 360.0f, 1.0f,
			"Degrees the clouds drift towards, 0 along the map's x."),
		RENDER_REAL("Sky", "SkyCloudChurn", m_skyCloudChurn, 0.0f, 1.0f, 0.01f,
			"How fast shapes change as they drift. 0 slides them as one sheet."),
		RENDER_REAL("Sky", "SkyCloudBillow", m_skyCloudBillow, 0.0f, 1.5f, 0.01f,
			"How far shapes bulge and curl. High values twist them into streaks."),
		RENDER_REAL("Sky", "SkyCloudDetail", m_skyCloudDetail, 0.0f, 1.0f, 0.01f,
			"Ragged detail at the edges. 0 gives smooth blobs."),

		RENDER_TEXT("Colour grade", "ColorLut", m_colorLut,
			"The colour table's file name in Art\\Textures, such as lut_desert.tga. None uses no table."),
		RENDER_REAL("Colour grade", "ColorLutStrength", m_colorLutStrength, 0.0f, 1.0f, 0.01f,
			"How much of the table's result is taken. 0 leaves the scene as it is."),
		RENDER_REAL("Colour grade", "ColorLutChroma", m_colorLutChroma, 0.0f, 1.0f, 0.01f,
			"How much of the table's hue is taken. 0 takes only its brightness."),
		RENDER_REAL("Colour grade", "ColorLutLuma", m_colorLutLuma, 0.0f, 1.0f, 0.01f,
			"How much of the table's brightness is taken. 0 takes only its hues, so units stay as bright as they were."),
		RENDER_REAL("Colour grade", "ColorLutBrightness", m_colorLutBrightness, 0.0f, 2.0f, 0.01f,
			"Multiplies the scene, before the table."),
		RENDER_REAL("Colour grade", "ColorLutContrast", m_colorLutContrast, 0.0f, 2.0f, 0.01f,
			"Spreads the scene around mid grey, before the table. 0 is flat grey."),
		RENDER_REAL("Colour grade", "ColorLutSaturation", m_colorLutSaturation, 0.0f, 2.0f, 0.01f,
			"Colourfulness, before the table. 0 is black and white."),
		RENDER_COLOR("Colour grade", "ColorLutTint", m_colorLutTint,
			"Colour the scene is multiplied by, before the table. Unticked leaves the key out, so GameData.ini decides."),
		RENDER_REAL("Colour grade", "ColorLutVibrance", m_colorLutVibrance, -1.0f, 1.0f, 0.01f,
			"Saturates dull colours more than vivid ones, so terrain gains colour and team colours hold. Negative mutes them."),
		RENDER_REAL("Colour grade", "ColorLutTechnicolor", m_colorLutTechnicolor, 0.0f, 1.0f, 0.01f,
			"Strength of the two-strip Technicolor film look."),
		RENDER_REAL("Colour grade", "ColorLutBlackPoint", m_colorLutBlackPoint, 0.0f, 0.5f, 0.005f,
			"Level that becomes black, after the table. Raise it to crush the shadows."),
		RENDER_REAL("Colour grade", "ColorLutWhitePoint", m_colorLutWhitePoint, 0.5f, 1.0f, 0.005f,
			"Level that becomes white, after the table. Lower it to brighten the highlights."),
		RENDER_REAL("Colour grade", "ColorLutGamma", m_colorLutGamma, 0.2f, 3.0f, 0.01f,
			"Midtone brightness. Above 1 brightens and below 1 darkens."),
		RENDER_REAL("Colour grade", "ColorLutOutputBlack", m_colorLutOutputBlack, 0.0f, 0.5f, 0.005f,
			"What black comes out as. Raise it for the lifted, matte look. The shroud lifts with it."),
		RENDER_REAL("Colour grade", "ColorLutOutputWhite", m_colorLutOutputWhite, 0.5f, 1.0f, 0.005f,
			"What white comes out as. Lower it to dim the highlights."),
		RENDER_REAL("Colour grade", "ColorLutVignette", m_colorLutVignette, 0.0f, 1.0f, 0.01f,
			"How dark the view gets towards its edges."),
		RENDER_REAL("Colour grade", "ColorLutVignetteRadius", m_colorLutVignetteRadius, 0.5f, 4.0f, 0.05f,
			"Distance from the view's centre, in half its height, where the vignette reaches full strength. 2 reaches the corners of a 16:9 view."),
		RENDER_REAL("Colour grade", "ColorLutGrain", m_colorLutGrain, 0.0f, 1.0f, 0.01f,
			"Film grain over the scene, new every frame. 0.1 to 0.2 is a light grain."),
		RENDER_REAL("Colour grade", "ColorLutDither", m_colorLutDither, 0.0f, 8.0f, 0.25f,
			"Noise that breaks up the banding a grade leaves in smooth gradients, in 8 bit colour steps. 0 is off."),
	};

	const int s_renderKeyCount = sizeof(s_renderKeys) / sizeof(s_renderKeys[0]);

	// The GameData.ini values, taken before the first write. Nothing else in WorldBuilder changes these keys.
	float s_renderBase[s_renderKeyCount][3];
	AsciiString s_renderBaseText[s_renderKeyCount];
	Bool s_renderBaseTaken = FALSE;

	void readRenderKey(const RenderKey &k, const GlobalData *data, float v[3])
	{
		v[0] = v[1] = v[2] = 0.0f;
		if (k.text != NULL)
		{
			return;
		}
		if (k.color != NULL)
		{
			const RGBColor &c = data->*(k.color);
			v[0] = c.red * 255.0f;
			v[1] = c.green * 255.0f;
			v[2] = c.blue * 255.0f;
		}
		else if (k.integer != NULL)
		{
			v[0] = (float)(data->*(k.integer));
		}
		else
		{
			v[0] = data->*(k.real);
		}
	}

	void writeRenderKey(const RenderKey &k, GlobalData *data, const float v[3])
	{
		if (k.text != NULL)
		{
			return;
		}
		if (k.color != NULL)
		{
			RGBColor &c = data->*(k.color);
			c.red = v[0] / 255.0f;
			c.green = v[1] / 255.0f;
			c.blue = v[2] / 255.0f;
		}
		else if (k.integer != NULL)
		{
			data->*(k.integer) = (Int)(v[0] + 0.5f);
		}
		else
		{
			data->*(k.real) = v[0];
		}
	}

	Bool takeRenderBase()
	{
		if (TheWritableGlobalData == NULL)
		{
			return FALSE;
		}
		if (!s_renderBaseTaken)
		{
			for (int i = 0; i < s_renderKeyCount; ++i)
			{
				readRenderKey(s_renderKeys[i], TheWritableGlobalData, s_renderBase[i]);
				if (s_renderKeys[i].text != NULL)
				{
					s_renderBaseText[i] = TheWritableGlobalData->*(s_renderKeys[i].text);
				}
			}
			s_renderBaseTaken = TRUE;
		}
		return TRUE;
	}

	// Steps past the blanks and the equals sign between a key and its value.
	const char *valueText(const char *rest)
	{
		if (rest == NULL)
		{
			return "";
		}
		while (*rest == ' ' || *rest == '\t' || *rest == '=')
		{
			++rest;
		}
		return rest;
	}

	// The value's first word, which is all a file name or None is.
	AsciiString firstWord(const char *text)
	{
		AsciiString word(text);
		word.trim();
		const char *blank = strpbrk(word.str(), " \t");
		if (blank != NULL)
		{
			word.truncateTo(blank - word.str());
		}
		return word;
	}

	Bool parseRenderValue(const RenderKey &k, const char *text, float v[3])
	{
		if (k.color != NULL)
		{
			int r = 0;
			int g = 0;
			int b = 0;
			if (sscanf(text, "R:%d G:%d B:%d", &r, &g, &b) != 3)
			{
				return FALSE;
			}
			v[0] = (float)r;
			v[1] = (float)g;
			v[2] = (float)b;
			return TRUE;
		}
		char *end = NULL;
		v[0] = (float)strtod(text, &end);
		return end != text;
	}

	void readKey(const WaterKey &k, const WaterTransparencySetting *wt, float v[3])
	{
		v[0] = v[1] = v[2] = 0.0f;
		switch (k.kind)
		{
			case WBQT_WATER_FLOAT:
				v[0] = wt->*(k.real);
				break;
			case WBQT_WATER_BOOL:
				v[0] = (wt->*(k.flag)) ? 1.0f : 0.0f;
				break;
			case WBQT_WATER_COLOR:
			{
				const RGBColor &c = wt->*(k.color);
				const Bool unset = c.red < 0.0f || c.green < 0.0f || c.blue < 0.0f;
				v[0] = unset ? -1.0f : c.red * 255.0f;
				v[1] = unset ? -1.0f : c.green * 255.0f;
				v[2] = unset ? -1.0f : c.blue * 255.0f;
				break;
			}
		}
	}

	RGBColor toColor(const float v[3])
	{
		RGBColor c;
		const Bool unset = v[0] < 0.0f || v[1] < 0.0f || v[2] < 0.0f;
		c.red = unset ? -1.0f : v[0] / 255.0f;
		c.green = unset ? -1.0f : v[1] / 255.0f;
		c.blue = unset ? -1.0f : v[2] / 255.0f;
		return c;
	}

	Bool keyDiffers(const WaterKey &k, const WaterTransparencySetting *wt, const float v[3])
	{
		switch (k.kind)
		{
			case WBQT_WATER_FLOAT:
				return wt->*(k.real) != v[0];
			case WBQT_WATER_BOOL:
				return (wt->*(k.flag) != FALSE) != (v[0] >= 0.5f);
			case WBQT_WATER_COLOR:
			{
				const RGBColor c = toColor(v);
				const RGBColor &now = wt->*(k.color);
				return now.red != c.red || now.green != c.green || now.blue != c.blue;
			}
		}
		return FALSE;
	}

	void writeKey(const WaterKey &k, WaterTransparencySetting *wt, const float v[3])
	{
		switch (k.kind)
		{
			case WBQT_WATER_FLOAT:
				wt->*(k.real) = v[0];
				break;
			case WBQT_WATER_BOOL:
				wt->*(k.flag) = v[0] >= 0.5f;
				break;
			case WBQT_WATER_COLOR:
				wt->*(k.color) = toColor(v);
				break;
		}
	}
}

extern "C" int WBQtWaterTuning_Count(void)
{
	return s_keyCount + s_renderKeyCount;
}

// The water keys come first, then the GameData keys.
extern "C" int WBQtWaterTuning_GetDesc(int i, WBQtWaterTuningDesc *out)
{
	if (i < 0 || i >= s_keyCount + s_renderKeyCount || out == NULL)
	{
		return 0;
	}
	if (i >= s_keyCount)
	{
		const RenderKey &r = s_renderKeys[i - s_keyCount];
		out->key = r.key;
		out->block = "GameData";
		out->group = r.group;
		out->help = r.help;
		out->kind = r.kind;
		out->lo = r.lo;
		out->hi = r.hi;
		out->step = r.step;
		out->advanced = 0;
		return 1;
	}
	const WaterKey &k = s_keys[i];
	out->key = k.key;
	out->block = "WaterTransparency";
	out->group = NULL;
	out->help = k.help;
	out->kind = k.kind;
	out->lo = k.lo;
	out->hi = k.hi;
	out->step = k.step;
	out->advanced = k.advanced;
	return 1;
}

extern "C" void WBQtWaterTuning_GetBase(int i, float v[3])
{
	if (v == NULL)
	{
		return;
	}
	v[0] = v[1] = v[2] = 0.0f;
	if (i >= s_keyCount && i < s_keyCount + s_renderKeyCount)
	{
		if (takeRenderBase())
		{
			memcpy(v, s_renderBase[i - s_keyCount], sizeof(s_renderBase[0]));
		}
		return;
	}
	const WaterTransparencySetting *base = TheWaterTransparency.getNonOverloadedPointer();
	if (i < 0 || i >= s_keyCount || base == NULL)
	{
		return;
	}
	readKey(s_keys[i], base, v);
}

extern "C" void WBQtWaterTuning_GetLive(int i, float v[3])
{
	if (v == NULL)
	{
		return;
	}
	v[0] = v[1] = v[2] = 0.0f;
	if (i >= s_keyCount && i < s_keyCount + s_renderKeyCount)
	{
		if (TheGlobalData != NULL)
		{
			readRenderKey(s_renderKeys[i - s_keyCount], TheGlobalData, v);
		}
		return;
	}
	const WaterTransparencySetting *now = TheWaterTransparency;
	if (i >= 0 && i < s_keyCount && now != NULL)
	{
		readKey(s_keys[i], now, v);
	}
}

extern "C" void WBQtWaterTuning_SetLive(int i, const float v[3])
{
	if (i < 0 || i >= s_keyCount + s_renderKeyCount || v == NULL)
	{
		return;
	}
	if (i >= s_keyCount)
	{
		if (!takeRenderBase())
		{
			return;
		}
		writeRenderKey(s_renderKeys[i - s_keyCount], TheWritableGlobalData, v);

		WbView3d *view = CWorldBuilderDoc::GetActive3DView();
		if (view != NULL)
		{
			view->Invalidate(false);
		}
		return;
	}
	const WaterKey &k = s_keys[i];

	// A value the water already has needs no override made for it.
	const WaterTransparencySetting *now = TheWaterTransparency;
	if (now == NULL || !keyDiffers(k, now, v))
	{
		return;
	}
	WaterTransparencySetting *wt = WBMapIni_EnsureWaterOverride();
	if (wt == NULL)
	{
		return;
	}
	writeKey(k, wt, v);

	WbView3d *view = CWorldBuilderDoc::GetActive3DView();
	if (view == NULL)
	{
		return;
	}
	if (k.rebuildsTerrain && TheTerrainRenderObject != NULL)
	{
		IRegion2D range = {0,0,0,0};
		view->updateHeightMapInView(TheTerrainRenderObject->getMap(), false, range);
	}
	view->Invalidate(false);
}

extern "C" int WBQtWaterTuning_GetBaseText(int i, char *buf, int size)
{
	if (buf == NULL || size <= 0)
	{
		return 0;
	}
	buf[0] = 0;
	if (i < s_keyCount || i >= s_keyCount + s_renderKeyCount || s_renderKeys[i - s_keyCount].text == NULL || !takeRenderBase())
	{
		return 0;
	}
	strlcpy(buf, s_renderBaseText[i - s_keyCount].str(), size);
	return 1;
}

extern "C" void WBQtWaterTuning_SetLiveText(int i, const char *text)
{
	if (i < s_keyCount || i >= s_keyCount + s_renderKeyCount || text == NULL || !takeRenderBase())
	{
		return;
	}
	const RenderKey &k = s_renderKeys[i - s_keyCount];
	if (k.text == NULL)
	{
		return;
	}
	TheWritableGlobalData->*(k.text) = firstWord(text);

	WbView3d *view = CWorldBuilderDoc::GetActive3DView();
	if (view != NULL)
	{
		view->Invalidate(false);
	}
}

extern "C" int WBQtWaterTuning_ListTables(char *buf, int size)
{
	if (buf == NULL || size <= 0)
	{
		return 0;
	}
	buf[0] = 0;
	if (TheFileSystem == NULL)
	{
		return 0;
	}

	FilenameList files;
	TheFileSystem->getFileListInDirectory(AsciiString(TGA_DIR_PATH), AsciiString("lut_*.tga"), files, FALSE);

	int count = 0;
	int used = 0;
	for (FilenameList::const_iterator it = files.begin(); it != files.end(); ++it)
	{
		// The list carries the directory, and the key names the file alone.
		const char *name = it->reverseFind('/');
		const char *backslash = it->reverseFind('\\');
		if (backslash != NULL && (name == NULL || backslash > name))
		{
			name = backslash;
		}
		name = (name != NULL) ? name + 1 : it->str();

		const int length = (int)strlen(name);
		if (used + length + 2 > size)
		{
			break;
		}
		if (used > 0)
		{
			buf[used++] = '\n';
		}
		memcpy(buf + used, name, length + 1);
		used += length;
		++count;
	}
	return count;
}

extern "C" void WBQtWaterTuning_NoteSaved(void)
{
	CWorldBuilderDoc *doc = CWorldBuilderDoc::GetActiveDoc();
	if (doc != NULL)
	{
		doc->noteMapIniSaved();
	}
}

extern "C" void WBQtWaterTuning_RestoreGameData(void)
{
	if (!s_renderBaseTaken || TheWritableGlobalData == NULL)
	{
		return;
	}
	for (int i = 0; i < s_renderKeyCount; ++i)
	{
		writeRenderKey(s_renderKeys[i], TheWritableGlobalData, s_renderBase[i]);
		if (s_renderKeys[i].text != NULL)
		{
			TheWritableGlobalData->*(s_renderKeys[i].text) = s_renderBaseText[i];
		}
	}
}

extern "C" void WBQtWaterTuning_ApplyGameData(const char *iniPath)
{
	WBQtWaterTuning_RestoreGameData();
	if (iniPath == NULL || !takeRenderBase())
	{
		return;
	}
	FILE *fp = fopen(iniPath, "rt");
	if (fp == NULL)
	{
		return;
	}

	char line[1024];
	Bool inGameData = FALSE;
	Int depth = 0;
	while (fgets(line, sizeof(line), fp) != NULL)
	{
		char *comment = strchr(line, ';');
		if (comment != NULL)
		{
			*comment = 0;
		}
		const char *key = strtok(line, " \t\r\n=");
		if (key == NULL)
		{
			continue;
		}
		const char *value = valueText(strtok(NULL, "\r\n"));

		if (!inGameData)
		{
			inGameData = strcmp(key, "GameData") == 0;
			depth = 0;
			continue;
		}
		if (strcmp(key, "End") == 0)
		{
			if (depth == 0)
			{
				inGameData = FALSE;
			}
			else
			{
				--depth;
			}
			continue;
		}
		// A key without a value opens a nested block, which has an End of its own.
		if (*value == 0)
		{
			++depth;
			continue;
		}
		if (depth > 0)
		{
			continue;
		}
		for (int i = 0; i < s_renderKeyCount; ++i)
		{
			if (strcmp(key, s_renderKeys[i].key) != 0)
			{
				continue;
			}
			if (s_renderKeys[i].text != NULL)
			{
				TheWritableGlobalData->*(s_renderKeys[i].text) = firstWord(value);
				break;
			}
			float v[3];
			if (parseRenderValue(s_renderKeys[i], value, v))
			{
				writeRenderKey(s_renderKeys[i], TheWritableGlobalData, v);
			}
			break;
		}
	}
	fclose(fp);
}

#endif // RTS_HAS_QT
