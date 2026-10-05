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

// FILE: MapPreview.h /////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
//	created:	Oct 2002
//
//	Filename: 	MapPreview.h
//
//	author:		Chris Huybregts
//
//	purpose:
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

//-----------------------------------------------------------------------------
// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// USER INCLUDES //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// FORWARD REFERENCES /////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// TYPE DEFINES ///////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
enum
{
	MAP_PREVIEW_HEIGHT = 128,//256,
	MAP_PREVIEW_WIDTH = 128,//256,
	HQ_PREVIEW_SIZE = 256,
	HQ_SUPERSAMPLE = 4,
	HQ_MIN_SIZE = 128,
	HQ_MAX_SIZE = 512,
	HQ_MAX_CAPTURE = 2048,	///< largest supersampled render, which bounds memory in the 32-bit editor
};

enum HQPreviewArea
{
	HQ_AREA_MAP = 0,	///< the whole map without its border, which the lobby places start positions on
	HQ_AREA_PLAYABLE,
	HQ_AREA_CUSTOM,
};

#include "wbview3d.h"

/// Shading for the HQ preview. Colours are RGB, 0 to 255.
struct HQPreviewParams
{
	Real relief;		///< hillshade strength
	Real elevation;		///< brightness spread from the lowest to the highest ground
	Real waterFalloff;	///< depth in world units at which water reaches most of its deep colour
	Bool depthTint;		///< colour water by its depth over the render
	Int shallow[3];
	Int deep[3];
};

/// What the HQ preview renders. Changing any of it needs a new render.
struct HQCaptureParams
{
	Bool objects;
	Bool trees;
	Bool roads;			///< roads and bridges
	Bool colorGrade;	///< the map.ini colour grade
	Bool renderedWater;	///< the editor's flat water surface
	Bool shaderWater;	///< the shader water, which wins over renderedWater
	Bool clouds;		///< the cloud shadows
	Bool macroTexture;	///< the map's macro texture
	Bool stochastic;	///< stochastic filtering over all the ground, which breaks up the textures' repeat
	Bool shadows;		///< object, tree and building shadows
	Int timeOfDay;		///< TIME_OF_DAY_INVALID keeps the current one
	Int area;			///< HQPreviewArea
	Int customX0;		///< HQ_AREA_CUSTOM corners in border-relative cells
	Int customY0;
	Int customX1;
	Int customY1;
	Int supersample;	///< reduced so size * supersample stays within HQ_MAX_CAPTURE
	Int size;			///< output pixels per side
};

class MapPreview
{
public:
	MapPreview();
	void save( CString mapName );

	static void getDefaultHQParams( HQPreviewParams *params );
	static void getDefaultHQCapture( HQCaptureParams *capture );
	/// The map without its border, and its playable boundary within that, in cells.
	static Bool getHQMapCells( Int *width, Int *height, Int *playableWidth, Int *playableHeight );
	/// The top view an HQ preview renders, stretched over the square tga.
	static Bool getHQTopView( const HQCaptureParams &capture, WbView3d::TopViewCapture *view3d );
	/// Renders the map from above and caches what composeHQ needs.
	Bool prepareHQ( WbView3d *view, const HQCaptureParams &capture );
	Int getHQSize() const { return m_hqSize; }
	Int getHQSupersample() const { return m_hqSuper; }
	/// Shades the cached render into getHQSize()^2 BGRA pixels, top row north.
	void composeHQ( const HQPreviewParams &params, UnsignedByte *bgra );
	/// Writes the pixels as <map>.tga, tagged so saving the map keeps them.
	static Bool writeHQ( CString mapName, const UnsignedByte *bgra, Int size );
private:
	void interpolateColorForHeight( RGBColor *color, Real height, Real hiZ, Real midZ, Real loZ );
	Bool mapPreviewToWorld(const ICoord2D *radar, Coord3D *world);
	void buildMapPreviewTexture( CString tgaName );
	void buildMapPreviewTextureAnime( CString tgaName );
	
	UnsignedInt m_pixelBuffer[MAP_PREVIEW_HEIGHT][MAP_PREVIEW_WIDTH];

	// Per supersampled pixel. A light of -1 marks a bridge or object over water, left unshaded.
	std::vector<UnsignedByte> m_hqScene;
	std::vector<Real> m_hqLight;
	std::vector<Real> m_hqHeight;
	std::vector<Real> m_hqDepth;
	Int m_hqSize;
	Int m_hqSuper;
	Bool m_hqWaterRendered;


};
//-----------------------------------------------------------------------------
// INLINING ///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// EXTERNALS //////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
