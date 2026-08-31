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

// FILE: W3DRadar.h ///////////////////////////////////////////////////////////////////////////////
// Author: Colin Day, January 2002
// Desc:   W3D radar implementation, this has the necessary device dependent drawing
//				 necessary for the radar

///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/Radar.h"
#include "WW3D2/ww3dformat.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class TextureClass;
class SurfaceClass;
class TerrainLogic;

// PROTOTYPES /////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
/** Which half of an outlined blip to draw. All the outlines are drawn before any of the cores so
	* that a blip packed in tight against its neighbours cannot have its core chewed away by the
	* outline of the blip next to it. */
//-------------------------------------------------------------------------------------------------
enum RadarBlipPass CPP_11(: Int)
{
	RADAR_BLIP_PASS_OUTLINE,
	RADAR_BLIP_PASS_CORE
};

//-------------------------------------------------------------------------------------------------
/** W3D radar class.  This has the device specific implementation of the radar such as
	* the drawing routines */
//-------------------------------------------------------------------------------------------------
class W3DRadar : public Radar
{

public:

	W3DRadar();
	virtual ~W3DRadar() override;

	virtual void xfer( Xfer *xfer ) override;

	virtual void init() override;																		///< subsystem init
	virtual void update() override;																	///< subsystem update
	virtual void reset() override;																		///< subsystem reset

	virtual void newMap( TerrainLogic *terrain ) override;				///< reset radar for new map

	virtual void draw( Int pixelX, Int pixelY, Int width, Int height ) override;		///< draw the radar

	virtual void clearShroud() override;
	virtual void setShroudLevel(Int x, Int y, CellShroudStatus setting) override; ///< set the shroud level at shroud cell x,y
	virtual void beginSetShroudLevel() override; ///< call this once before multiple calls to setShroudLevel for better performance
	virtual void endSetShroudLevel() override; ///< call this once after beginSetShroudLevel and setShroudLevel

	virtual void refreshTerrain( TerrainLogic *terrain ) override;
	virtual void refreshObjects() override;

	virtual void notifyViewChanged() override; ///< signals that the camera view has changed

protected:

	void drawSingleBeaconEvent( Int pixelX, Int pixelY, Int width, Int height, Int index );
	void drawSingleGenericEvent( Int pixelX, Int pixelY, Int width, Int height, Int index );

	Bool legalRadarPoint( Int px, Int py ) const;	///< is the point inside the radar cell grid
	void initializeTextureFormats();				///< find format to use for the radar texture
	void deleteResources();									///< delete resources used
	void drawEvents( Int pixelX, Int pixelY, Int width, Int height);		///< draw all of the radar events
	void drawHeroIcon( Int pixelX, Int pixelY, Int width, Int height, const Coord3D *pos );	//< draw a hero icon
	void drawViewBox( Int pixelX, Int pixelY, Int width, Int height );  ///< draw view box
	void buildTerrainTexture( TerrainLogic *terrain );	 ///< create the terrain texture of the radar
	void drawIcons( Int pixelX, Int pixelY, Int width, Int height );	///< draw all of the radar icons
	void updateObjectTexture(TextureClass *texture);
	static Bool canRenderObject( const RadarObject *rObj, const Player *localPlayer );
	void renderObjectList( const RadarObject *listHead, TextureClass *texture );
	void drawObjectListBlips( const RadarObject *listHead, RadarBlipPass pass,
												Int pixelX, Int pixelY, Int width, Int height );
	Bool isManMadeTerrainColor( const RGBColor *color ) const;	///< does this sample read as man made
	void getTerrainTone( RGBColor *color, Real manMadeFraction ) const;	///< mix of the two tones
	void interpolateColorForHeight( RGBColor *color,
																	Real height,
																	Real hiZ,
																	Real midZ,
																	Real loZ );		///< "shade" color according to height value
	void reconstructViewBox();							///< remake the view box
	void radarToPixel( const ICoord2D *radar, ICoord2D *pixel,
										 Int radarUpperLeftX, Int radarUpperLeftY,
										 Int radarWidth, Int radarHeight );  ///< convert radar coord to pixel location

	WW3DFormat m_terrainTextureFormat;						///< format to use for terrain texture
	Image *m_terrainImage;												///< terrain image abstraction for drawing
	TextureClass *m_terrainTexture;								///< terrain background texture

	WW3DFormat m_overlayTextureFormat;						///< format to use for overlay texture
	Image *m_overlayImage;												///< overlay image abstraction for drawing
	TextureClass *m_overlayTexture;								///< overlay texture

	WW3DFormat m_shroudTextureFormat;							///< format to use for shroud texture
	Image *m_shroudImage;													///< shroud image abstraction for drawing
	TextureClass *m_shroudTexture;								///< shroud texture
	SurfaceClass *m_shroudSurface;								///< surface to shroud texture
	void *m_shroudSurfaceBits;										///< shroud surface bits
	int m_shroudSurfacePitch;											///< shroud surface pitch
	WW3DFormat m_shroudSurfaceFormat;							///< shroud surface format
	UnsignedInt m_shroudSurfacePixelSize;					///< shroud surface pixel size

	Int m_textureWidth;														///< width for all radar textures
	Int m_textureHeight;													///< height for all radar textures

	// TheSuperHackers @feature cached Options.ini NewRadar, so the drawing code does not have
	// to reach through TheGlobalData for every blip and every terrain cell.
	Bool m_newRadar;															///< draw outlined blips and a shoreline contour

	// TheSuperHackers @feature Options.ini BlipSize, resolved to screen pixels once at startup.
	Int m_unitBlipSize;												///< screen size of a unit blip core
	Int m_structureBlipSize;									///< screen size of a structure blip core


	//
	// We want to keep a flag that tells us when to reconstruct the view box.
	// We want to avoid making the view box every frame because the 4 points
	// visible on the edge of the screen will "jitter" unevenly as we translate
	// real world coordinates to integer radar positions.
	//
	Bool m_reconstructViewBox;										///< true when we need to reconstruct the box
	ICoord2D m_viewBox[ 4 ];											///< radar cell points for the 4 corners of view box
};
