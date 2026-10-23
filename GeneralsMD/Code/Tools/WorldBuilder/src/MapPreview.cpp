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

// FILE: MapPreview.cpp /////////////////////////////////////////////////
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
//	Filename: 	MapPreview.cpp
//
//	author:		Chris Huybregts
//
//	purpose:	Contains the code used to generate and save the map preview to
//						the map file.  (Original code was taken from the Radar code in
//						game engine).
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// USER INCLUDES //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

#include "StdAfx.h"
#include "resource.h"
#include "WHeightMapEdit.h"
#include "WorldBuilderDoc.h"
#include "MapPreview.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "Common/MapReaderWriterInfo.h"
#include "Common/FileSystem.h"
#include "WWLib/TARGA.h"
#include "Common/DataChunk.h"
#include "GameLogic/PolygonTrigger.h"
#include "wbview3d.h"
#include <float.h>
#include <math.h>
#include <vector>
//-----------------------------------------------------------------------------
// DEFINES ////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
Bool localIsUnderwater( Real x, Real y);
// Marks a tga written by writeHQ, so saving the map leaves it alone.
static const char HQ_TGA_ID[] = "WBHQ";
enum { HQ_TGA_ID_LENGTH = 4 };

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
MapPreview::MapPreview()
	: m_hqSize(HQ_PREVIEW_SIZE)
	, m_hqSuper(HQ_SUPERSAMPLE)
	, m_hqWaterRendered(false)
{
	memset(m_pixelBuffer, 0xffffffff, sizeof(m_pixelBuffer));
}

void MapPreview::save( CString mapName )
{
	CString newStr = mapName;
	newStr.Replace(".map", ".tga");

	// An HQ preview is only replaced by generating it again.
	FILE *fp = fopen(newStr, "rb");
	if (fp != NULL)
	{
		unsigned char header[18 + HQ_TGA_ID_LENGTH];
		const size_t got = fread(header, 1, sizeof(header), fp);
		fclose(fp);
		const Bool tagged = got == sizeof(header) && header[0] == HQ_TGA_ID_LENGTH && memcmp(header + 18, HQ_TGA_ID, HQ_TGA_ID_LENGTH) == 0;
		if (tagged || (got >= 18 && (header[12] | (header[13] << 8)) > MAP_PREVIEW_WIDTH))
		{
			return;
		}
	}
	buildMapPreviewTexture( newStr );

/*
	chunkWriter.openDataChunk("MapPreview", K_MAPPREVIEW_VERSION_1);
	chunkWriter.writeInt(MAP_PREVIEW_WIDTH);
	chunkWriter.writeInt(MAP_PREVIEW_HEIGHT);
	DEBUG_LOG(("BeginMapPreviewInfo"));
	for(Int i = 0; i < MAP_PREVIEW_HEIGHT; ++i)
	{
		for(Int j = 0; j < MAP_PREVIEW_WIDTH; ++j)
		{
			chunkWriter.writeInt(m_pixelBuffer[i][j]);
			DEBUG_LOG(("x:%d, y:%d, %X", j, i, m_pixelBuffer[i][j]));
		}
	}
	chunkWriter.closeDataChunk();
	DEBUG_LOG(("EndMapPreviewInfo"));
*/
}

// Highest water surface over a border-relative world point, or -FLT_MAX when none covers it.
static Real waterLevelAt(Real x, Real y)
{
	ICoord3D iLoc;
	iLoc.x = (Int)floor(x + 0.5f);
	iLoc.y = (Int)floor(y + 0.5f);
	iLoc.z = 0;
	Real level = -FLT_MAX;
	for (PolygonTrigger *pTrig = PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext())
	{
		if (pTrig->isWaterArea() && pTrig->pointInTrigger(iLoc))
		{
			level = max(level, (Real)pTrig->getPoint(0)->z);
		}
	}
	return level;
}

void MapPreview::getDefaultHQParams( HQPreviewParams *params )
{
	params->relief = 0.8f;
	params->elevation = 0.35f;
	params->waterFalloff = 40.0f;
	params->depthTint = true;
	params->shallow[0] = 70;
	params->shallow[1] = 140;
	params->shallow[2] = 160;
	params->deep[0] = 20;
	params->deep[1] = 55;
	params->deep[2] = 95;
}

void MapPreview::getDefaultHQCapture( HQCaptureParams *capture )
{
	capture->objects = true;
	capture->trees = true;
	capture->roads = true;
	capture->colorGrade = false;
	capture->renderedWater = false;
	capture->shaderWater = false;
	capture->clouds = true;
	capture->macroTexture = true;
	capture->stochastic = false;
	capture->shadows = true;
	capture->timeOfDay = TIME_OF_DAY_INVALID;
	capture->area = HQ_AREA_MAP;
	capture->customX0 = 0;
	capture->customY0 = 0;
	capture->customX1 = 0;
	capture->customY1 = 0;
	capture->supersample = HQ_SUPERSAMPLE;
	capture->size = HQ_PREVIEW_SIZE;
}

Bool MapPreview::getHQMapCells( Int *width, Int *height, Int *playableWidth, Int *playableHeight )
{
	*width = 0;
	*height = 0;
	*playableWidth = 0;
	*playableHeight = 0;
	WorldHeightMapEdit *pMap = CWorldBuilderDoc::GetActiveDoc() ? CWorldBuilderDoc::GetActiveDoc()->GetHeightMap() : NULL;
	if (pMap == NULL)
	{
		return false;
	}
	*width = pMap->getXExtent() - 2*pMap->getBorderSize();
	*height = pMap->getYExtent() - 2*pMap->getBorderSize();
	*playableWidth = *width;
	*playableHeight = *height;
	if (pMap->getNumBoundaries() > 0)
	{
		ICoord2D bound;
		pMap->getBoundary(0, &bound);
		if (bound.x > 0 && bound.y > 0)
		{
			*playableWidth = min(bound.x, *width);
			*playableHeight = min(bound.y, *height);
		}
	}
	return true;
}

Bool MapPreview::getHQTopView( const HQCaptureParams &capture, WbView3d::TopViewCapture *view3d )
{
	Int mapW = 0;
	Int mapH = 0;
	Int playableW = 0;
	Int playableH = 0;
	if (!getHQMapCells(&mapW, &mapH, &playableW, &playableH))
	{
		return false;
	}

	// The area in border-relative cells.
	Int cx0 = 0;
	Int cy0 = 0;
	Int cx1 = mapW;
	Int cy1 = mapH;
	if (capture.area == HQ_AREA_PLAYABLE)
	{
		cx1 = playableW;
		cy1 = playableH;
	}
	else if (capture.area == HQ_AREA_CUSTOM)
	{
		cx0 = max(0, min(capture.customX0, capture.customX1));
		cy0 = max(0, min(capture.customY0, capture.customY1));
		cx1 = min(mapW, max(capture.customX0, capture.customX1));
		cy1 = min(mapH, max(capture.customY0, capture.customY1));
	}
	if (cx1 - cx0 < 1 || cy1 - cy0 < 1)
	{
		return false;
	}

	// The lobby draws the tga into a rect at the map's own proportions, so the tga stretches the area to a square.
	view3d->x0 = cx0 * MAP_XY_FACTOR;
	view3d->y0 = cy0 * MAP_XY_FACTOR;
	view3d->x1 = cx1 * MAP_XY_FACTOR;
	view3d->y1 = cy1 * MAP_XY_FACTOR;
	view3d->objects = capture.objects;
	view3d->trees = capture.trees;
	view3d->roads = capture.roads;
	view3d->colorGrade = capture.colorGrade;
	view3d->timeOfDay = capture.timeOfDay;
	view3d->clouds = capture.clouds;
	view3d->macroTexture = capture.macroTexture;
	view3d->stochastic = capture.stochastic;
	view3d->shadows = capture.shadows;
	view3d->water = capture.shaderWater ? WbView3d::TOP_VIEW_WATER_SHADER
		: (capture.renderedWater ? WbView3d::TOP_VIEW_WATER_FLAT : WbView3d::TOP_VIEW_WATER_NONE);
	return true;
}

Bool MapPreview::prepareHQ( WbView3d *view, const HQCaptureParams &capture )
{
	const Int ABOVE_GROUND_DIFF = 24;	// colour change that marks a bridge or object over the ground

	WorldHeightMapEdit *pMap = CWorldBuilderDoc::GetActiveDoc() ? CWorldBuilderDoc::GetActiveDoc()->GetHeightMap() : NULL;
	WbView3d::TopViewCapture view3d;
	if (view == NULL || pMap == NULL || TheTerrainRenderObject == NULL || !getHQTopView(capture, &view3d))
	{
		return false;
	}
	const Int border = pMap->getBorderSize();
	const Int mapW = pMap->getXExtent() - 2*border;
	const Int mapH = pMap->getYExtent() - 2*border;

	m_hqSize = max((Int)HQ_MIN_SIZE, min(capture.size, (Int)HQ_MAX_SIZE));
	m_hqSuper = max(1, min(capture.supersample, HQ_MAX_CAPTURE / m_hqSize));
	const Int big = m_hqSize * m_hqSuper;
	const Int count = big*big;
	m_hqScene.resize(count*4);
	std::vector<UnsignedByte> ground(count*4);
	if (!view->captureTopView(big, view3d, true, &m_hqScene[0]) || !view->captureTopView(big, view3d, false, &ground[0]))
	{
		m_hqScene.clear();
		return false;
	}
	m_hqWaterRendered = view3d.water != WbView3d::TOP_VIEW_WATER_NONE;

	const Real worldX = view3d.x1 - view3d.x0;
	const Real worldY = view3d.y1 - view3d.y0;
	Real minZ = FLT_MAX;
	Real maxZ = -FLT_MAX;
	Real sumZ = 0.0f;
	for (Int j = border; j < pMap->getYExtent() - border; j++)
	{
		for (Int i = border; i < pMap->getXExtent() - border; i++)
		{
			const Real z = pMap->getHeight(i, j) * MAP_HEIGHT_SCALE;
			minZ = min(minZ, z);
			maxZ = max(maxZ, z);
			sumZ += z;
		}
	}
	const Real rangeZ = max(maxZ - minZ, 1.0f);
	const Real meanZ = sumZ / max(mapW*mapH, 1);

	// Light from the north-west, as on most maps' sun.
	Real lx = -1.0f;
	Real ly = 1.0f;
	Real lz = 1.4f;
	const Real lLen = sqrt(lx*lx + ly*ly + lz*lz);
	lx /= lLen;
	ly /= lLen;
	lz /= lLen;

	m_hqLight.resize(count);
	m_hqHeight.resize(count);
	m_hqDepth.resize(count);
	const Real step = MAP_XY_FACTOR;
	for (Int py = 0; py < big; py++)
	{
		const Real y = view3d.y0 + worldY * (1.0f - (py + 0.5f) / big);
		for (Int px = 0; px < big; px++)
		{
			const Real x = view3d.x0 + worldX * (px + 0.5f) / big;
			const Int n = py*big + px;
			const UnsignedByte *s = &m_hqScene[n*4];
			const UnsignedByte *g = &ground[n*4];
			const Bool aboveGround = (abs(s[0] - g[0]) + abs(s[1] - g[1]) + abs(s[2] - g[2])) > ABOVE_GROUND_DIFF;

			const Real z = TheTerrainRenderObject->getHeightMapHeight(x, y, NULL);
			const Real level = waterLevelAt(x, y);
			m_hqDepth[n] = level > z ? level - z : 0.0f;

			// Bridges and objects over water keep their own colour.
			if (aboveGround && level > z)
			{
				m_hqLight[n] = -1.0f;
				continue;
			}
			if (aboveGround)
			{
				m_hqDepth[n] = 0.0f;
			}

			const Real dzdx = (TheTerrainRenderObject->getHeightMapHeight(x + step, y, NULL) - TheTerrainRenderObject->getHeightMapHeight(x - step, y, NULL)) / (2.0f*step);
			const Real dzdy = (TheTerrainRenderObject->getHeightMapHeight(x, y + step, NULL) - TheTerrainRenderObject->getHeightMapHeight(x, y - step, NULL)) / (2.0f*step);
			const Real nLen = sqrt(dzdx*dzdx + dzdy*dzdy + 1.0f);
			m_hqLight[n] = (-dzdx*lx - dzdy*ly + lz) / (nLen*lz);
			// Centred on the average ground, so the elevation shading leaves the map's overall brightness alone.
			m_hqHeight[n] = (z - meanZ) / rangeZ;
		}
	}
	return true;
}

void MapPreview::composeHQ( const HQPreviewParams &params, UnsignedByte *bgra )
{
	const Int big = m_hqSize * m_hqSuper;
	if (m_hqScene.empty())
	{
		memset(bgra, 0, m_hqSize*m_hqSize*4);
		return;
	}
	const Real falloff = max(params.waterFalloff, 1.0f);

	for (Int oy = 0; oy < m_hqSize; oy++)
	{
		for (Int ox = 0; ox < m_hqSize; ox++)
		{
			Real sum[3] = { 0.0f, 0.0f, 0.0f };
			for (Int sy = 0; sy < m_hqSuper; sy++)
			{
				for (Int sx = 0; sx < m_hqSuper; sx++)
				{
					const Int n = (oy*m_hqSuper + sy)*big + ox*m_hqSuper + sx;
					const UnsignedByte *s = &m_hqScene[n*4];
					Real c[3] = { (Real)s[0], (Real)s[1], (Real)s[2] };
					// A rendered water surface is flat, so the ground's relief stays off it.
					const Bool water = m_hqDepth[n] > 0.0f;
					if (m_hqLight[n] >= 0.0f && !(water && m_hqWaterRendered))
					{
						Real shade = 1.0f + params.relief * (m_hqLight[n] - 1.0f);
						shade *= 1.0f + params.elevation * m_hqHeight[n];
						shade = max(0.0f, shade);
						for (Int k = 0; k < 3; k++)
						{
							c[k] *= shade;
						}
					}
					if (m_hqLight[n] >= 0.0f && water && params.depthTint)
					{
						const Real m = 1.0f - exp(-m_hqDepth[n] / falloff);
						const Real alpha = 0.55f + 0.35f*m;
						for (Int k = 0; k < 3; k++)
						{
							// Colours are RGB and the pixels BGR.
							const Real shallow = params.shallow[2 - k];
							const Real tint = shallow + (params.deep[2 - k] - shallow)*m;
							c[k] += (tint - c[k])*alpha;
						}
					}
					for (Int k = 0; k < 3; k++)
					{
						sum[k] += max(0.0f, min(c[k], 255.0f));
					}
				}
			}
			UnsignedByte *o = &bgra[(oy*m_hqSize + ox)*4];
			for (Int k = 0; k < 3; k++)
			{
				o[k] = (UnsignedByte)(sum[k] / (m_hqSuper*m_hqSuper) + 0.5f);
			}
			o[3] = 255;
		}
	}
}

Bool MapPreview::writeHQ( CString mapName, const UnsignedByte *bgra, Int size )
{
	CString tgaName = mapName;
	tgaName.Replace(".map", ".tga");
	FILE *fp = fopen(tgaName, "wb");
	if (fp == NULL)
	{
		return false;
	}
	unsigned char header[18];
	memset(header, 0, sizeof(header));
	header[0] = HQ_TGA_ID_LENGTH;
	header[2] = 2;
	header[12] = size & 0xff;
	header[13] = size >> 8;
	header[14] = size & 0xff;
	header[15] = size >> 8;
	header[16] = 32;
	header[17] = 8;
	Bool written = fwrite(header, 1, sizeof(header), fp) == sizeof(header)
		&& fwrite(HQ_TGA_ID, 1, HQ_TGA_ID_LENGTH, fp) == HQ_TGA_ID_LENGTH;

	// TGA rows run bottom-up, so the south edge is written first.
	for (Int y = size - 1; y >= 0 && written; y--)
	{
		written = fwrite(bgra + y*size*4, 1, size*4, fp) == (size_t)(size*4);
	}
	fclose(fp);
	return written;
}

void MapPreview::interpolateColorForHeight( RGBColor *color,
																					Real height,
																					Real hiZ,
																					Real midZ,
																					Real loZ )
{
	const Real howBright = 0.30f;  // bigger is brighter (0.0 to 1.0)
	const Real howDark   = 0.60f;  // bigger is darker (0.0 to 1.0)

	// sanity on map height (flat maps bomb)
	if (hiZ == midZ)
		hiZ = midZ+0.1f;
	if (midZ == loZ)
		loZ = midZ-0.1f;
	if (hiZ == loZ)
		hiZ = loZ+0.2f;

//	Real heightPercent = height / (hiZ - loZ);
	Real t;
	RGBColor colorTarget;

	// if "over" the middle height, interpolate lighter
	if( height >= midZ )
	{

		// how far are we from the middleZ towards the hi Z
		t = (height - midZ) / (hiZ - midZ);

		// compute what our "lightest" color possible we want to use is
		colorTarget.red = color->red + (1.0f - color->red) * howBright;
		colorTarget.green = color->green + (1.0f - color->green) * howBright;
		colorTarget.blue = color->blue + (1.0f - color->blue) * howBright;

	}
	else  // interpolate darker
	{

		// how far are we from the middleZ towards the low Z
		t = (midZ - height) / (midZ - loZ);

		// compute what the "darkest" color possible we want to use is
		colorTarget.red = color->red + (0.0f - color->red) * howDark;
		colorTarget.green = color->green + (0.0f - color->green) * howDark;
		colorTarget.blue = color->blue + (0.0f - color->blue) * howDark;

	}

	// interpolate toward the target color
	color->red = color->red + (colorTarget.red - color->red) * t;
	color->green = color->green + (colorTarget.green - color->green) * t;
	color->blue = color->blue + (colorTarget.blue - color->blue) * t;

	// keep the color real
	if( color->red < 0.0f )
		color->red = 0.0f;
	if( color->red > 1.0f )
		color->red = 1.0f;
	if( color->green < 0.0f )
		color->green = 0.0f;
	if( color->green > 1.0f )
		color->green = 1.0f;
	if( color->blue < 0.0f )
		color->blue = 0.0f;
	if( color->blue > 1.0f )
		color->blue = 1.0f;

}

Bool MapPreview::mapPreviewToWorld(const ICoord2D *radar, Coord3D *world)
{
	Int x, y;

	// sanity
	if( radar == nullptr || world == nullptr )
		return FALSE;

	// get the coords
	x = radar->x;
	y = radar->y;

	// more sanity
	if( x < 0 )
		x = 0;
	if( x >= MAP_PREVIEW_WIDTH )
		x = MAP_PREVIEW_WIDTH - 1;
	if( y < 0 )
		y = 0;
	if( y >= MAP_PREVIEW_HEIGHT )
		y = MAP_PREVIEW_HEIGHT - 1;
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	Real xSample, ySample;

	xSample = INT_TO_REAL(pMap->getXExtent() - (2 * pMap->getBorderSize())) / MAP_PREVIEW_WIDTH;
	ySample = INT_TO_REAL(pMap->getYExtent() - (2 * pMap->getBorderSize())) / MAP_PREVIEW_HEIGHT;
	// translate to world
	world->x = x * xSample + pMap->getBorderSize();
	world->y = y * ySample + pMap->getBorderSize();

	// find the terrain height here
//	world->z = pMap->gethigh
//	TheTerrainLogic->getGroundHeight( world->x, world->y );

	return TRUE;

}

void MapPreview::buildMapPreviewTexture( CString tgaName )
{
//	SurfaceClass *surface;
	RGBColor waterColor;

	// we will want to reconstruct our new view box now
	//m_reconstructViewBox = TRUE;

	// setup our water color
	waterColor.red = 0.55f;
	waterColor.green = 0.55f;
	waterColor.blue = 1.0f;

	// fill the terrain texture with a representation of the terrain
//	Real mapDepth = m_mapExtent.depth();

	// build the terrain
	RGBColor sampleColor;
	RGBColor color;
	Int i, j, samples;
	Int x, y, z;
	ICoord2D radarPoint, boundary;
	Coord3D worldPoint;
	Real getTerrainAverageZ = 0;
	Real maxHeight = 0;
	Real minHeight = 100.0f;
	Real tempHeight = 0;
	Int terrainCount = 0;
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	pMap->getBoundary(0, &boundary );
	for(i = 0; i < MAP_PREVIEW_HEIGHT; ++i)
	{
		for(j = 0; j < MAP_PREVIEW_WIDTH; ++j)
		{
			radarPoint.x = j;
			radarPoint.y = i;
			mapPreviewToWorld( &radarPoint, &worldPoint );
			tempHeight = pMap->getHeight(worldPoint.x, worldPoint.y);
			getTerrainAverageZ += tempHeight;
			if(tempHeight > maxHeight)
				maxHeight = tempHeight;
			if(tempHeight < minHeight)
				minHeight = tempHeight;
			++terrainCount;
		}
	}
	getTerrainAverageZ = getTerrainAverageZ /terrainCount;



//	Color paddingColor = GameMakeColor( 100, 100, 100, 255 );
	for( y = 0; y < MAP_PREVIEW_HEIGHT; y++ )
	{

		for( x = 0; x < MAP_PREVIEW_WIDTH; x++ )
		{

			// what point are we inspecting
			radarPoint.x = x;
			radarPoint.y = y;
			mapPreviewToWorld( &radarPoint, &worldPoint );


			// get height of the terrain at this sample point
			z = pMap->getHeight(worldPoint.x, worldPoint.y);

			// create a color based on the Z height of the map
			//Real waterZ;

			if( localIsUnderwater( MAP_XY_FACTOR *(worldPoint.x - pMap->getBorderSize()), MAP_XY_FACTOR * ( worldPoint.y - pMap->getBorderSize())) )
			{
				const Int waterSamplesAway = 1;		// how many "tiles" from the center tile we will sample away
																					// to average a color for the tile color

				sampleColor.red = sampleColor.green = sampleColor.blue = 0.0f;
				samples = 0;

				for( j = y - waterSamplesAway; j <= y + waterSamplesAway; j++ )
				{

					if( j >= 0 && j < MAP_PREVIEW_HEIGHT )
					{

						for( i = x - waterSamplesAway; i <= x + waterSamplesAway; i++ )
						{

							if( i >= 0 && i < MAP_PREVIEW_WIDTH )
							{

								// the the world point we are concerned with
								radarPoint.x = i;
								radarPoint.y = j;
								mapPreviewToWorld( &radarPoint, &worldPoint );

								// get Z at this sample height
								Real underwaterZ = pMap->getHeight(worldPoint.x, worldPoint.y);

								// get color for this Z and add to our sample color
								if( localIsUnderwater( MAP_XY_FACTOR *(worldPoint.x - pMap->getBorderSize()), MAP_XY_FACTOR * ( worldPoint.y - pMap->getBorderSize())))
								{

									// this is our "color" for water
									color = waterColor;

									// interpolate the water color for height in the water table
									interpolateColorForHeight( &color, underwaterZ, pMap->getMaxHeightValue(),getTerrainAverageZ,  pMap->getMinHeightValue() );

									// add color to our samples
									sampleColor.red += color.red;
									sampleColor.green += color.green;
									sampleColor.blue += color.blue;
									samples++;

								}

							}

						}

					}

				}

				// prevent divide by zeros
				if( samples == 0 )
					samples = 1;

				// set the color to an average of the colors read
				color.red = sampleColor.red / (Real)samples;
				color.green = sampleColor.green / (Real)samples;
				color.blue = sampleColor.blue / (Real)samples;

			}
			else  // regular terrain ...
			{
				const Int samplesAway = 1;  // how many "tiles" from the center tile we will sample away
																		// to average a color for the tile color

				sampleColor.red = sampleColor.green = sampleColor.blue = 0.0f;
				samples = 0;

				for( j = y - samplesAway; j <= y + samplesAway; j++ )
				{

					if( j >= 0 && j < MAP_PREVIEW_HEIGHT )
					{

						for( i = x - samplesAway; i <= x + samplesAway; i++ )
						{

							if( i >= 0 && i < MAP_PREVIEW_WIDTH )
							{

								// the the world point we are concerned with
								radarPoint.x = i;
								radarPoint.y = j;
//								radarPoint.x = x;
//								radarPoint.y = y;
								mapPreviewToWorld( &radarPoint, &worldPoint );

								// get the color at this point
								pMap->getTerrainColorAt( MAP_XY_FACTOR *(worldPoint.x - pMap->getBorderSize()) ,MAP_XY_FACTOR * ( worldPoint.y - pMap->getBorderSize()), &color );

								// interpolate the color for height
								interpolateColorForHeight( &color, z,
																						maxHeight, getTerrainAverageZ, minHeight);
																					 //pMap->getMaxHeightValue(),getTerrainAverageZ,  pMap->getMinHeightValue() );

								// add color to our sample
								sampleColor.red += color.red;
								sampleColor.green += color.green;
								sampleColor.blue += color.blue;
								samples++;

							}

						}

					}

				}

				// prevent divide by zeros
				if( samples == 0 )
					samples = 1;

				// set the color to an average of the colors read
				color.red = sampleColor.red / (Real)samples;
				color.green = sampleColor.green / (Real)samples;
				color.blue = sampleColor.blue / (Real)samples;

			}

			//
			// draw the pixel for the terrain at this point, note that because of the orientation
			// of our world we draw it with positive y in the "up" direction
			//
			// FYI: I tried making this faster by pulling out all the code inside DrawPixel
			// and locking only once ... but it made absolutely *no* performance difference,
			// the sampling and interpolation algorithm for generating pretty looking terrain
			// and water for the radar is just, well, expensive.
			//
			m_pixelBuffer[y][x]= 255 | (REAL_TO_INT(color.red *255) << 8) | (REAL_TO_INT(color.green *255) << 16) | REAL_TO_INT(color.blue * 255)<< 24;

		}

	}
	{
		Targa tga;
		tga.Header.Width = MAP_PREVIEW_WIDTH;
		tga.Header.Height = MAP_PREVIEW_HEIGHT;
		tga.Header.PixelDepth = 32;
		tga.Header.ImageType = TGA_TRUECOLOR;
		tga.SetImage((char *)m_pixelBuffer);
		tga.Save(tgaName,TGAF_IMAGE, FALSE);
	}

}

void MapPreview::buildMapPreviewTextureAnime(CString tgaName)
{
    RGBColor waterColor;
    waterColor.red = 0.35f;   // slightly more vibrant blue
    waterColor.green = 0.55f;
    waterColor.blue = 1.0f;

    CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
    WorldHeightMapEdit* pMap = pDoc->GetHeightMap();

    Real maxHeight = -10000.0f;
    Real minHeight = 10000.0f;
    Real avgHeight = 0.0f;
    Int count = 0;

    // Precompute min, max, and average heights
    Coord3D worldPoint;
    ICoord2D radarPoint;
    for (Int y = 0; y < MAP_PREVIEW_HEIGHT; ++y)
    {
        for (Int x = 0; x < MAP_PREVIEW_WIDTH; ++x)
        {
            radarPoint.x = x;
            radarPoint.y = y;
            mapPreviewToWorld(&radarPoint, &worldPoint);

            Real h = pMap->getHeight(worldPoint.x, worldPoint.y);
            avgHeight += h;
            if (h > maxHeight) maxHeight = h;
            if (h < minHeight) minHeight = h;
            ++count;
        }
    }
    avgHeight /= count;

    // Anime-style terrain rendering
    for (Int b = 0; b < MAP_PREVIEW_HEIGHT; ++b)
    {
        for (Int x = 0; x < MAP_PREVIEW_WIDTH; ++x)
        {
            radarPoint.x = x;
            radarPoint.y = b;
            mapPreviewToWorld(&radarPoint, &worldPoint);

            Real h = pMap->getHeight(worldPoint.x, worldPoint.y);
            RGBColor color;

            if (localIsUnderwater(MAP_XY_FACTOR * (worldPoint.x - pMap->getBorderSize()), 
                                  MAP_XY_FACTOR * (worldPoint.y - pMap->getBorderSize())))
            {
                // Anime water: more saturated
                color = waterColor;
                color.red = min(color.red + 0.15f, 1.0f);
                color.green = min(color.green + 0.05f, 1.0f);
            }
            else
            {
                // Terrain color
                pMap->getTerrainColorAt(MAP_XY_FACTOR * (worldPoint.x - pMap->getBorderSize()), 
                                        MAP_XY_FACTOR * (worldPoint.y - pMap->getBorderSize()), &color);

                // Exaggerate height differences
                Real heightFactor = (h - avgHeight) / (maxHeight - minHeight + 0.001f);
                
                // Darken cliffs: if steep relative to neighbors
                Int xNext = min(x + 1, MAP_PREVIEW_WIDTH - 1);
                Int yNext = min(b + 1, MAP_PREVIEW_HEIGHT - 1);

                radarPoint.x = xNext; radarPoint.y = b;
                Coord3D worldNextX; mapPreviewToWorld(&radarPoint, &worldNextX);
                Real hNextX = pMap->getHeight(worldNextX.x, worldNextX.y);

                radarPoint.x = x; radarPoint.y = yNext;
                Coord3D worldNextY; mapPreviewToWorld(&radarPoint, &worldNextY);
                Real hNextY = pMap->getHeight(worldNextY.x, worldNextY.y);

                Real slope = max(fabs(h - hNextX), fabs(h - hNextY));

                if (slope > 1.0f) // threshold for cliff
                {
                    color.red *= 0.5f;
                    color.green *= 0.5f;
                    color.blue *= 0.5f;
                }

                // Brighten peaks, darken valleys
                if (heightFactor > 0)
                {
                    color.red = min(color.red + 0.4f * heightFactor, 1.0f);
                    color.green = min(color.green + 0.4f * heightFactor, 1.0f);
                    color.blue = min(color.blue + 0.4f * heightFactor, 1.0f);
                }
                else
                {
                    color.red = max(color.red + 0.3f * heightFactor, 0.0f);
                    color.green = max(color.green + 0.3f * heightFactor, 0.0f);
                    color.blue = max(color.blue + 0.3f * heightFactor, 0.0f);
                }
            }

            // Write to pixel buffer
            m_pixelBuffer[b][x] = 255 | 
                (REAL_TO_INT(color.red * 255) << 8) | 
                (REAL_TO_INT(color.green * 255) << 16) | 
                (REAL_TO_INT(color.blue * 255) << 24);
        }
    }

    // Save TGA
    Targa tga;
    tga.Header.Width = MAP_PREVIEW_WIDTH;
    tga.Header.Height = MAP_PREVIEW_HEIGHT;
    tga.Header.PixelDepth = 32;
    tga.Header.ImageType = TGA_TRUECOLOR;
    tga.SetImage((char*)m_pixelBuffer);
    tga.Save(tgaName, TGAF_IMAGE, FALSE);
}



//-----------------------------------------------------------------------------
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

