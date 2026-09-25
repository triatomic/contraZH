/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: Water.h //////////////////////////////////////////////////////////////////////////////////
// Author: Colin Day, December 2001
// Desc:   Water settings
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INLCLUDES //////////////////////////////////////////////////////////////////////////////////////
#include "Common/GameType.h"
#include "Common/Overridable.h"
#include "Common/Override.h"

//-------------------------------------------------------------------------------------------------
struct FieldParse;

//-------------------------------------------------------------------------------------------------
/** This structures keeps the settings for how our water will look */
//-------------------------------------------------------------------------------------------------
class WaterSetting
{

public:

	WaterSetting();
	virtual ~WaterSetting();

	/// Get the INI parsing table for loading
	const FieldParse *getFieldParse() { return m_waterSettingFieldParseTable; }

	static const FieldParse m_waterSettingFieldParseTable[];		///< the parse table for INI definition
	AsciiString m_skyTextureFile;
	AsciiString m_waterTextureFile;
	Int m_waterRepeatCount;
	Real m_skyTexelsPerUnit;	//texel density of sky plane (higher value repeats texture more).
	RGBAColorInt m_vertex00Diffuse;
	RGBAColorInt m_vertex10Diffuse;
	RGBAColorInt m_vertex11Diffuse;
	RGBAColorInt m_vertex01Diffuse;
	RGBAColorInt m_waterDiffuseColor;
	RGBAColorInt m_transparentWaterDiffuse;
	Real m_uScrollPerMs;
	Real m_vScrollPerMs;

};

//-------------------------------------------------------------------------------------------------
/** This structure keeps the transparency and vertex settings, which are the same regardless of the
		time of day. They can be overridden on a per-map basis. */
//-------------------------------------------------------------------------------------------------
class WaterTransparencySetting : public Overridable
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( WaterTransparencySetting, "WaterTransparencySetting"  )

	public:
		Real m_transparentWaterDepth;
		Real m_minWaterOpacity;
		RGBColor m_standingWaterColor;
		RGBColor m_radarColor;
		Bool m_additiveBlend;
		AsciiString m_standingWaterTexture;

		AsciiString m_skyboxTextureN;
		AsciiString m_skyboxTextureE;
		AsciiString m_skyboxTextureS;
		AsciiString m_skyboxTextureW;
		AsciiString m_skyboxTextureT;

		// Shader water (D3D9 with Smooth Water on)
		Real m_shaderWaterReflection;	///< scales the Fresnel sky reflection
		Real m_shaderWaterSpecular;		///< scales the sun glint
		Real m_shaderWaterSpecularSpread;	///< widens the sun glint, so it shows at more view angles
		Bool m_shaderWaterVirtualSun;	///< glints off a sun ahead of the camera instead of the map's sun
		Real m_shaderWaterRefraction;	///< how far the waves bend the seabed, in screen fractions
		Real m_shaderWaterWaveScale;	///< world units one wave tile covers
		Real m_shaderWaterWaveStrength;	///< steepness of the waves
		Real m_shaderWaterFoamDepth;	///< depth where shore foam fades out
		Real m_shaderWaterFoamReach;	///< world units from land within which water foams as if at the shore
		Real m_shaderWaterFoamScale;	///< world units one foam pattern covers
		Real m_shaderWaterFoamStrength;	///< scales the foam's brightness
		Real m_shaderWaterClarity;		///< scales TransparentWaterDepth, higher sees deeper
		Real m_shaderWaterOpacity;		///< opacity of deep water, 0 takes TransparentWaterMinOpacity
		Real m_shaderWaterSwellHeight;	///< height of the vertex waves, 0 turns them off
		Real m_shaderWaterSwellScale;	///< world units one swell tile covers
		Real m_shaderWaterSwellSpeed;	///< world units a second the swell drifts
		Real m_shaderWaterPlanarDistortion;	///< how far the waves bend the mirrored scene, in screen fractions
		Real m_shaderWaterPlanarFade;	///< height difference over which other water fades from the mirror to the sky
		Real m_shaderWaterPlanarStrength;	///< reflection the mirrored scene adds on top of the Fresnel term
		Real m_shaderWaterStochasticSize;	///< world units between the centres of hex cells with random texture offsets, 0 turns them off
		Real m_shaderWaterStochasticSharpness;	///< narrows the blend between hex cells, below 1 widens it
		Real m_shaderWaterStochasticRandom;	///< how far each hex cell shifts the textures, 0 to 1
		Real m_shaderWaterStochasticRotation;	///< how far each hex cell turns the water texture, 0 to 1
		Bool m_isWater;					///< FALSE draws the old water without shaders, for lava and the like
		Int m_waterAnimationFps;		///< most steps a second the water moves in, 30 to 60, 0 moves it every frame

	public:
		WaterTransparencySetting()
		{
			m_transparentWaterDepth = 3.0f;
			m_minWaterOpacity = 1.0f;
			m_standingWaterColor.red = 1.0f;
			m_standingWaterColor.green = 1.0f;
			m_standingWaterColor.blue = 1.0f;
			m_radarColor.red = 0.55f;
			m_radarColor.green = 0.55f;
			m_radarColor.blue = 1.0f;
			m_standingWaterTexture = "TWWater01.tga";
			m_additiveBlend = FALSE;

			m_skyboxTextureN = "TSMorningN.tga";
			m_skyboxTextureE = "TSMorningE.tga";
			m_skyboxTextureS = "TSMorningS.tga";
			m_skyboxTextureW = "TSMorningW.tga";
			m_skyboxTextureT = "TSMorningT.tga";

			m_shaderWaterReflection = 3.0f;
			m_shaderWaterSpecular = 1.0f;
			m_shaderWaterSpecularSpread = 1.0f;
			m_shaderWaterVirtualSun = FALSE;
			m_shaderWaterRefraction = 0.015f;
			m_shaderWaterWaveScale = 160.0f;
			m_shaderWaterWaveStrength = 0.3f;
			m_shaderWaterFoamDepth = 6.0f;
			m_shaderWaterFoamReach = 15.0f;
			m_shaderWaterFoamScale = 150.0f;
			m_shaderWaterFoamStrength = 0.5f;
			m_shaderWaterClarity = 1.0f;
			m_shaderWaterOpacity = 0.95f;
			m_shaderWaterSwellHeight = 3.0f;
			m_shaderWaterSwellScale = 700.0f;
			m_shaderWaterSwellSpeed = 30.0f;
			m_shaderWaterPlanarDistortion = 0.02f;
			m_shaderWaterPlanarFade = 4.0f;
			m_shaderWaterPlanarStrength = 0.3f;
			m_shaderWaterStochasticSize = 100.0f;
			m_shaderWaterStochasticSharpness = 3.0f;
			m_shaderWaterStochasticRandom = 1.0f;
			m_shaderWaterStochasticRotation = 1.0f;
			m_isWater = TRUE;
			m_waterAnimationFps = 0;
		}

		static const FieldParse m_waterTransparencySettingFieldParseTable[];		///< the parse table for INI definition

		/// Get the INI parsing table for loading
		const FieldParse *getFieldParse() const { return m_waterTransparencySettingFieldParseTable; }
};

EMPTY_DTOR(WaterTransparencySetting)

// EXTERNAL ///////////////////////////////////////////////////////////////////////////////////////
extern WaterSetting WaterSettings[ TIME_OF_DAY_COUNT ];

extern OVERRIDE<WaterTransparencySetting> TheWaterTransparency;

/// Parses a water INI file again into the water settings and every map override of them. Throws on bad data.
void reloadWaterINI( const AsciiString& filename );
