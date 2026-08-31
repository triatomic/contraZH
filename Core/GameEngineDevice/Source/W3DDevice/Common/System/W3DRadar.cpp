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

// FILE: W3DRadar.cpp /////////////////////////////////////////////////////////////////////////////
// Author: Colin Day, January 2002
// Desc:   W3D radar implementation, this has the necessary device dependent drawing
//				 necessary for the radar
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/AudioEventRTS.h"
#include "Common/Debug.h"
#include "Common/GlobalData.h"
#include "Common/OptionPreferences.h"
#include "Common/GameUtility.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"

#include "GameLogic/TerrainLogic.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

#include "GameClient/Color.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Display.h"
#include "GameClient/GameClient.h"
#include "GameClient/GameWindow.h"
#include "GameClient/Image.h"
#include "GameClient/Line2D.h"
#include "GameClient/TerrainVisual.h"
#include "GameClient/Water.h"
#include "W3DDevice/Common/W3DRadar.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/W3DShroud.h"
#include "WW3D2/texture.h"
#include "WW3D2/dx8caps.h"
#include "WWMath/vector2i.h"

#include <vector>



// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
enum { OVERLAY_REFRESH_RATE = 6 };  ///< over updates once this many frames

//-------------------------------------------------------------------------------------------------
/** Is the point legal, that is, inside the resolution of the radar cells */
//-------------------------------------------------------------------------------------------------
Bool W3DRadar::legalRadarPoint( Int px, Int py ) const
{

	if( px < 0 || py < 0 || px >= getCellWidth() || py >= getCellHeight() )
		return FALSE;

	return TRUE;

}

//-------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
static WW3DFormat findFormat(const WW3DFormat formats[])
{
	for( Int i = 0; formats[ i ] != WW3D_FORMAT_UNKNOWN; i++ )
	{

		if( DX8Wrapper::Get_Current_Caps()->Support_Texture_Format( formats[ i ] ) )
		{

			return formats[ i ];

		}

	}
	DEBUG_CRASH(("WW3DRadar: No appropriate texture format") );
	return WW3D_FORMAT_UNKNOWN;
}

//-------------------------------------------------------------------------------------------------
/** Find the texture format we're going to use for the radar.  The texture format must
	* be supported by the hardware.  The "more preferred" formats appear at the top of
	* the format tables in order from most preferred to least preferred */
//-------------------------------------------------------------------------------------------------
void W3DRadar::initializeTextureFormats()
{
	const WW3DFormat terrainFormats[] =
	{
		WW3D_FORMAT_R8G8B8,
		WW3D_FORMAT_X8R8G8B8,
		WW3D_FORMAT_R5G6B5,
		WW3D_FORMAT_X1R5G5B5,
		WW3D_FORMAT_UNKNOWN				// keep this one last
	};
	const WW3DFormat overlayFormats[] =
	{
		WW3D_FORMAT_A8R8G8B8,
		WW3D_FORMAT_A4R4G4B4,
		WW3D_FORMAT_UNKNOWN				// keep this one last
	};
	const WW3DFormat shroudFormats[] =
	{
		WW3D_FORMAT_A8R8G8B8,
		WW3D_FORMAT_A4R4G4B4,
		WW3D_FORMAT_UNKNOWN				// keep this one last
	};

	// find a format for the terrain texture
	m_terrainTextureFormat = findFormat(terrainFormats);

	// find a format for the overlay texture
	m_overlayTextureFormat = findFormat(overlayFormats);

	// find a format for the shroud texture
	m_shroudTextureFormat = findFormat(shroudFormats);

}

//-------------------------------------------------------------------------------------------------
/** Delete resources used specifically in this W3D radar implementation */
//-------------------------------------------------------------------------------------------------
void W3DRadar::deleteResources()
{

	//
	// delete terrain resources used
	//
	if( m_terrainTexture )
		m_terrainTexture->Release_Ref();
	m_terrainTexture = nullptr;

	deleteInstance(m_terrainImage);
	m_terrainImage = nullptr;

	//
	// delete overlay resources used
	//
	if( m_overlayTexture )
		m_overlayTexture->Release_Ref();
	m_overlayTexture = nullptr;

	deleteInstance(m_overlayImage);
	m_overlayImage = nullptr;

	//
	// delete shroud resources used
	//
	if( m_shroudTexture )
		m_shroudTexture->Release_Ref();
	m_shroudTexture = nullptr;

	deleteInstance(m_shroudImage);
	m_shroudImage = nullptr;

	DEBUG_ASSERTCRASH(m_shroudSurface == nullptr, ("W3DRadar::deleteResources: m_shroudSurface is expected null"));
	DEBUG_ASSERTCRASH(m_shroudSurfaceBits == nullptr, ("W3DRadar::deleteResources: m_shroudSurfaceBits is expected null"));

}

//-------------------------------------------------------------------------------------------------
/** Reconstruct the view box given the current camera settings */
//-------------------------------------------------------------------------------------------------
void W3DRadar::reconstructViewBox()
{
	m_reconstructViewBox = FALSE;

	Coord3D world[ 4 ];
	ICoord2D radar[ 4 ];
	Int i;

	// Get the 4 points of the view corners in the 3D world at the average Z height in the map
	//
	//  1-------2
	//   \     /
	//    4---3
	if( TheTacticalView->getScreenCornerWorldPointsAtZ(&world[0], &world[1], &world[2], &world[3], getTerrainAverageZ()) == PlaneClass::NO_INTERSECTION )
		return;

	// convert each of the 4 points in the world to radar cell positions
	for( i = 0; i < 4; i++ )
	{

		// first convert to radar cells
 		radar[ i ].x = world[ i ].x / (m_mapExtent.width() / getCellWidth());
 		radar[ i ].y = world[ i ].y / (m_mapExtent.height() / getCellHeight());

		//
		// store these points in the view box array which contains a first position
		// of (0,0) and then offsets for each additional entry point
		//
		if( i == 0 )
		{

			m_viewBox[ i ].x = 0;
			m_viewBox[ i ].y = 0;

		}
		else
		{

			m_viewBox[ i ].x = radar[ i ].x - radar[ i - 1 ].x;
			m_viewBox[ i ].y = radar[ i ].y - radar[ i - 1 ].y;

		}

	}

}

//-------------------------------------------------------------------------------------------------
/** Convert radar position to actual pixel coord */
//-------------------------------------------------------------------------------------------------
void W3DRadar::radarToPixel( const ICoord2D *radar, ICoord2D *pixel,
														 Int radarUpperLeftX, Int radarUpperLeftY,
														 Int radarWidth, Int radarHeight )
{

	// sanity
	if( radar == nullptr || pixel == nullptr )
		return;

	pixel->x = (radar->x * radarWidth / getCellWidth()) + radarUpperLeftX;
	// note the "inverted" y here to orient the way our world looks with +x=right and -y=down
	pixel->y = ((getCellHeight() - 1 - radar->y) * radarHeight / getCellHeight()) + radarUpperLeftY;

}


//-------------------------------------------------------------------------------------------------
/** Draw a hero icon at a position, given radar box upper left location and dimensions.  */
//-------------------------------------------------------------------------------------------------
void W3DRadar::drawHeroIcon( Int pixelX, Int pixelY, Int width, Int height, const Coord3D *pos )
{
	// get the hero icon image
	static const Image *image = (Image *)TheMappedImageCollection->findImageByName("HeroReticle");
	if (image != nullptr)
	{
		// convert world to radar coords
		ICoord2D ulRadar;
		ulRadar.x = pos->x / (m_mapExtent.width() / getCellWidth());
		ulRadar.y = pos->y / (m_mapExtent.height() / getCellHeight());

		// convert radar to screen coords
		ICoord2D offsetScreen;
		radarToPixel( &ulRadar, &offsetScreen, pixelX, pixelY, width, height );

		// shift from an upper left to a center focus for the icon
		int iconWidth = image->getImageWidth();
		int iconHeight = image->getImageHeight();
		offsetScreen.x -= (iconWidth / 2) - 1;
		offsetScreen.y -= iconHeight / 2;

		// draw the icon
		TheDisplay->drawImage( image, offsetScreen.x , offsetScreen.y, offsetScreen.x + iconWidth, offsetScreen.y + iconHeight );
	}
}

//-------------------------------------------------------------------------------------------------
/** Draw a "box" into the texture passed in that represents the viewable area for
	* the tactical display into the game world */
//-------------------------------------------------------------------------------------------------
void W3DRadar::drawViewBox( Int pixelX, Int pixelY, Int width, Int height )
{
	ICoord2D ulScreen;
	ICoord2D ulRadar;
	Coord3D ulWorld;
	ICoord2D ulPixel;
	ICoord2D pixelStart, pixelEnd;
	ICoord2D clipStart, clipEnd;
	Real lineWidth = 1.0f;
	Color topColor = GameMakeColor( 225, 225, 0, 255 );
	Color bottomColor = GameMakeColor( 158, 158, 0, 255 );

	//
	// setup the clipping region ... note that this clipping region is not over just the
	// radar image area ... it's in the WHOLE window available for the radar
	//
	IRegion2D clipRegion;
	ICoord2D radarWindowSize, radarWindowScreenPos;
	m_radarWindow->winGetSize( &radarWindowSize.x, &radarWindowSize.y );
	m_radarWindow->winGetScreenPosition( &radarWindowScreenPos.x, &radarWindowScreenPos.y );
	clipRegion.lo.x = radarWindowScreenPos.x;
	clipRegion.lo.y = radarWindowScreenPos.y;
	clipRegion.hi.x = radarWindowScreenPos.x + radarWindowSize.x;
	clipRegion.hi.y = radarWindowScreenPos.y + radarWindowSize.y;

	// convert top left of screen into world position
	TheTacticalView->getOrigin( &ulScreen.x, &ulScreen.y );
	if( TheTacticalView->screenToWorldAtZ( &ulScreen, &ulWorld, getTerrainAverageZ() ) == PlaneClass::NO_INTERSECTION )
		return;

	// convert world to radar coords
 	ulRadar.x = ulWorld.x / (m_mapExtent.width() / getCellWidth());
 	ulRadar.y = ulWorld.y / (m_mapExtent.height() / getCellHeight());

	//
	// convert radar point to actual pixel coords on the screen, shifted
	// into position on the radar for where the radar is drawn and the size of the
	// area that the radar is drawn in
	//
	radarToPixel( &ulRadar, &ulPixel, pixelX, pixelY, width, height );

	//
	// using our view box offset array, convert each of those radar cell offset points
	// into screen pixels and draw the box.  The view box array is setup with the
	// first index containing (0,0) (the point we just converted in theory), with cell
	// offsets to each of the other corners in the following order
	// (upper left, upper right, lower right, lower left)
	//
	ICoord2D radar;

	// top line
	pixelStart = ulPixel;
	radar.x = ulRadar.x + m_viewBox[ 1 ].x;
	radar.y = ulRadar.y + m_viewBox[ 1 ].y;
	radarToPixel( &radar, &pixelEnd, pixelX, pixelY, width, height );
	if( ClipLine2D( &pixelStart, &pixelEnd, &clipStart, &clipEnd, &clipRegion ) )
		TheDisplay->drawLine( clipStart.x, clipStart.y, clipEnd.x, clipEnd.y,
													lineWidth, topColor );

  // right line
	pixelStart = pixelEnd;
	radar.x += m_viewBox[ 2 ].x;
	radar.y += m_viewBox[ 2 ].y;
	radarToPixel( &radar, &pixelEnd, pixelX, pixelY, width, height );
	if( ClipLine2D( &pixelStart, &pixelEnd, &clipStart, &clipEnd, &clipRegion ) )
		TheDisplay->drawLine( clipStart.x, clipStart.y, clipEnd.x, clipEnd.y,
													lineWidth, topColor, bottomColor );

  // bottom line
	pixelStart = pixelEnd;
	radar.x += m_viewBox[ 3 ].x;
	radar.y += m_viewBox[ 3 ].y;
	radarToPixel( &radar, &pixelEnd, pixelX, pixelY, width, height );
	if( ClipLine2D( &pixelStart, &pixelEnd, &clipStart, &clipEnd, &clipRegion ) )
		TheDisplay->drawLine( clipStart.x, clipStart.y, clipEnd.x, clipEnd.y,
													lineWidth, bottomColor );

  // left line
	pixelStart = pixelEnd;
	pixelEnd = ulPixel;
	if( ClipLine2D( &pixelStart, &pixelEnd, &clipStart, &clipEnd, &clipRegion ) )
		TheDisplay->drawLine( clipStart.x, clipStart.y, clipEnd.x, clipEnd.y,
													lineWidth, bottomColor, topColor );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void W3DRadar::drawSingleBeaconEvent( Int pixelX, Int pixelY, Int width, Int height, Int index )
{
	RadarEvent *event = &(m_event[index]);
	ICoord2D tri[ 3 ];
	ICoord2D start, end;
	Real angle, addAngle;
	Color startColor, endColor;
	Real lineWidth = 1.0f;
	UnsignedInt currentFrame = TheGameLogic->getFrame();
	UnsignedInt frameDiff;							// frames the event has been alive for
	Real maxEventSize = width / 10.0f;   // max size of the event marker
	Int minEventSize = 6;     // min size of the event marker
	Int eventSize;									 // current size of a marker to draw
	const Real TIME_FROM_FULL_SIZE_TO_SMALL_SIZE = LOGICFRAMES_PER_SECOND * 1.5;
	Real totalAnglesToSpin = 2.0f * PI;  ///< spin around this many angles going from big to small
	UnsignedByte r, g, b, a;

	// setup screen clipping region
	IRegion2D clipRegion;
	clipRegion.lo.x = pixelX;
	clipRegion.lo.y = pixelY;
	clipRegion.hi.x = pixelX + width;
	clipRegion.hi.y = pixelY + height;

	// get the difference in frame from the current frame to the frame we were created on
	frameDiff = currentFrame - event->createFrame;

	// compute the size of the event marker, it is largest when it starts and smallest at the end
	eventSize = REAL_TO_INT( maxEventSize * ( 1.0f - frameDiff / TIME_FROM_FULL_SIZE_TO_SMALL_SIZE) );

	// we never let the event size get too small
	if( eventSize < minEventSize )
		eventSize = minEventSize;

	// compute how much "angle" we will add to each point to make it rotate as it's getting small
	addAngle = -totalAnglesToSpin * (frameDiff / TIME_FROM_FULL_SIZE_TO_SMALL_SIZE);

	// create a triangle around the event
	angle = 0.0f - addAngle;
	tri[ 0 ].x = REAL_TO_INT( (DOUBLE_TO_REAL( Cos( angle ) ) * eventSize) + event->radarLoc.x );
	tri[ 0 ].y = REAL_TO_INT( (DOUBLE_TO_REAL( Sin( angle ) ) * eventSize) + event->radarLoc.y );

	angle = 2.0f * PI / 3.0f - addAngle;
	tri[ 1 ].x = REAL_TO_INT( (DOUBLE_TO_REAL( Cos( angle ) ) * eventSize) + event->radarLoc.x );
	tri[ 1 ].y = REAL_TO_INT( (DOUBLE_TO_REAL( Sin( angle ) ) * eventSize) + event->radarLoc.y );

	angle = -2.0f * PI / 3.0f - addAngle;
	tri[ 2 ].x = REAL_TO_INT( (DOUBLE_TO_REAL( Cos( angle ) ) * eventSize) + event->radarLoc.x );
	tri[ 2 ].y = REAL_TO_INT( (DOUBLE_TO_REAL( Sin( angle ) ) * eventSize) + event->radarLoc.y );

	// translate radar coords to screen coords
	radarToPixel( &tri[ 0 ], &tri[ 0 ], pixelX, pixelY, width, height );
	radarToPixel( &tri[ 1 ], &tri[ 1 ], pixelX, pixelY, width, height );
	radarToPixel( &tri[ 2 ], &tri[ 2 ], pixelX, pixelY, width, height );

	//
	// make the colors we're going to use, when we're at our smallest size we will start to
	// fade the alpha away to transparent so that at our lifetime frame we are completely gone
	//

	// color 1 ------------------
	r = event->color1.red;
	g = event->color1.green;
	b = event->color1.blue;
	a = event->color1.alpha;
	if( currentFrame > event->fadeFrame )
	{

		a = REAL_TO_UNSIGNEDBYTE( (Real)a * (1.0f - (Real)(currentFrame - event->fadeFrame) /
																								(Real)(event->dieFrame - event->fadeFrame) ) );

	}
	startColor = GameMakeColor( r, g, b, a );

	// color 2 ------------------
	r = event->color2.red;
	g = event->color2.green;
	b = event->color2.blue;
	a = event->color2.alpha;
	if( currentFrame > event->fadeFrame )
	{

		a = REAL_TO_UNSIGNEDBYTE( (Real)a * (1.0f - (Real)(currentFrame - event->fadeFrame) /
																								(Real)(event->dieFrame - event->fadeFrame) ) );

	}
	endColor = GameMakeColor( r, g, b, a );

	// draw the lines
	if( ClipLine2D( &tri[ 0 ], &tri[ 1 ], &start, &end, &clipRegion ) )
		TheDisplay->drawLine( start.x, start.y, end.x, end.y, lineWidth, startColor, endColor );
	if( ClipLine2D( &tri[ 1 ], &tri[ 2 ], &start, &end, &clipRegion ) )
		TheDisplay->drawLine( start.x, start.y, end.x, end.y, lineWidth, startColor, endColor );
	if( ClipLine2D( &tri[ 2 ], &tri[ 0 ], &start, &end, &clipRegion ) )
		TheDisplay->drawLine( start.x, start.y, end.x, end.y, lineWidth, startColor, endColor );
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void W3DRadar::drawSingleGenericEvent( Int pixelX, Int pixelY, Int width, Int height, Int index )
{
	RadarEvent *event = &(m_event[index]);
	ICoord2D tri[ 3 ];
	ICoord2D start, end;
	Real angle, addAngle;
	Color startColor, endColor;
	Real lineWidth = 1.0f;
	UnsignedInt currentFrame = TheGameLogic->getFrame();
	UnsignedInt frameDiff;							// frames the event has been alive for
	Real maxEventSize = width / 2.0f;   // max size of the event marker
	Int minEventSize = 6;     // min size of the event marker
	Int eventSize;									 // current size of a marker to draw
	const Real TIME_FROM_FULL_SIZE_TO_SMALL_SIZE = LOGICFRAMES_PER_SECOND * 1.5;
	Real totalAnglesToSpin = 2.0f * PI;  ///< spin around this many angles going from big to small
	UnsignedByte r, g, b, a;

	// setup screen clipping region
	IRegion2D clipRegion;
	clipRegion.lo.x = pixelX;
	clipRegion.lo.y = pixelY;
	clipRegion.hi.x = pixelX + width;
	clipRegion.hi.y = pixelY + height;

	// get the difference in frame from the current frame to the frame we were created on
	frameDiff = currentFrame - event->createFrame;

	// compute the size of the event marker, it is largest when it starts and smallest at the end
	eventSize = REAL_TO_INT( maxEventSize * ( 1.0f - frameDiff / TIME_FROM_FULL_SIZE_TO_SMALL_SIZE) );

	// we never let the event size get too small
	if( eventSize < minEventSize )
		eventSize = minEventSize;

	// compute how much "angle" we will add to each point to make it rotate as it's getting small
	addAngle = totalAnglesToSpin * (frameDiff / TIME_FROM_FULL_SIZE_TO_SMALL_SIZE);

	// create a triangle around the event
	angle = 0.0f - addAngle;
	tri[ 0 ].x = REAL_TO_INT( (DOUBLE_TO_REAL( Cos( angle ) ) * eventSize) + event->radarLoc.x );
	tri[ 0 ].y = REAL_TO_INT( (DOUBLE_TO_REAL( Sin( angle ) ) * eventSize) + event->radarLoc.y );

	angle = 2.0f * PI / 3.0f - addAngle;
	tri[ 1 ].x = REAL_TO_INT( (DOUBLE_TO_REAL( Cos( angle ) ) * eventSize) + event->radarLoc.x );
	tri[ 1 ].y = REAL_TO_INT( (DOUBLE_TO_REAL( Sin( angle ) ) * eventSize) + event->radarLoc.y );

	angle = -2.0f * PI / 3.0f - addAngle;
	tri[ 2 ].x = REAL_TO_INT( (DOUBLE_TO_REAL( Cos( angle ) ) * eventSize) + event->radarLoc.x );
	tri[ 2 ].y = REAL_TO_INT( (DOUBLE_TO_REAL( Sin( angle ) ) * eventSize) + event->radarLoc.y );

	// translate radar coords to screen coords
	radarToPixel( &tri[ 0 ], &tri[ 0 ], pixelX, pixelY, width, height );
	radarToPixel( &tri[ 1 ], &tri[ 1 ], pixelX, pixelY, width, height );
	radarToPixel( &tri[ 2 ], &tri[ 2 ], pixelX, pixelY, width, height );

	//
	// make the colors we're going to use, when we're at our smallest size we will start to
	// fade the alpha away to transparent so that at our lifetime frame we are completely gone
	//

	// color 1 ------------------
	r = event->color1.red;
	g = event->color1.green;
	b = event->color1.blue;
	a = event->color1.alpha;
	if( currentFrame > event->fadeFrame )
	{

		a = REAL_TO_UNSIGNEDBYTE( (Real)a * (1.0f - (Real)(currentFrame - event->fadeFrame) /
																								(Real)(event->dieFrame - event->fadeFrame) ) );

	}
	startColor = GameMakeColor( r, g, b, a );

	// color 2 ------------------
	r = event->color2.red;
	g = event->color2.green;
	b = event->color2.blue;
	a = event->color2.alpha;
	if( currentFrame > event->fadeFrame )
	{

		a = REAL_TO_UNSIGNEDBYTE( (Real)a * (1.0f - (Real)(currentFrame - event->fadeFrame) /
																								(Real)(event->dieFrame - event->fadeFrame) ) );

	}
	endColor = GameMakeColor( r, g, b, a );

	// draw the lines
	if( ClipLine2D( &tri[ 0 ], &tri[ 1 ], &start, &end, &clipRegion ) )
		TheDisplay->drawLine( start.x, start.y, end.x, end.y, lineWidth, startColor, endColor );
	if( ClipLine2D( &tri[ 1 ], &tri[ 2 ], &start, &end, &clipRegion ) )
		TheDisplay->drawLine( start.x, start.y, end.x, end.y, lineWidth, startColor, endColor );
	if( ClipLine2D( &tri[ 2 ], &tri[ 0 ], &start, &end, &clipRegion ) )
		TheDisplay->drawLine( start.x, start.y, end.x, end.y, lineWidth, startColor, endColor );
}

//-------------------------------------------------------------------------------------------------
/** Draw all the radar events */
//-------------------------------------------------------------------------------------------------
void W3DRadar::drawEvents( Int pixelX, Int pixelY, Int width, Int height )
{
	Int i;

	for( i = 0;  i < MAX_RADAR_EVENTS; i++ )
	{

		// only 'active' events actually have something to draw
		if( m_event[ i ].active == TRUE && m_event[ i ].type != RADAR_EVENT_FAKE )
		{

			// if we haven't played the sound for this event, do it now that we can see it
			if( m_event[ i ].soundPlayed == FALSE && m_event[i].type != RADAR_EVENT_BEACON_PULSE )
			{
				static AudioEventRTS eventSound("RadarEvent");
				TheAudio->addAudioEvent( &eventSound );

			}

			m_event[ i ].soundPlayed = TRUE;

			if ( m_event[ i ].type == RADAR_EVENT_BEACON_PULSE )
				drawSingleBeaconEvent( pixelX, pixelY, width, height, i );
			else
				drawSingleGenericEvent( pixelX, pixelY, width, height, i );

		}

	}

}


//-------------------------------------------------------------------------------------------------
/** Draw all the radar icons */
//-------------------------------------------------------------------------------------------------
void W3DRadar::drawIcons( Int pixelX, Int pixelY, Int width, Int height )
{
	Player *player = rts::getObservedOrLocalPlayer();
	for (RadarObject *heroObj = m_localObjectList; heroObj; heroObj = heroObj->friend_getNext())
	{
		const Object *obj = heroObj->friend_getObject();

		if (!obj->isHero())
			continue;

		if (!canRenderObject(heroObj, player))
			continue;

		drawHeroIcon(pixelX, pixelY, width, height, obj->getPosition());
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DRadar::updateObjectTexture(TextureClass *texture)
{
	// reset the overlay texture
	SurfaceClass *surface = texture->Get_Surface_Level();
	surface->Clear();
	REF_PTR_RELEASE(surface);

	// rebuild the object overlay
	renderObjectList( m_objectList, texture );
	renderObjectList( m_localObjectList, texture );
}

//-------------------------------------------------------------------------------------------------
/** The alpha a stealthed object blinks at this frame. Both the blip core and the outline around
	* it take this, so that a stealthed unit fades as one thing rather than leaving a solid ring
	* around a vanishing middle. */
//-------------------------------------------------------------------------------------------------
UnsignedByte W3DRadar::stealthBlinkAlpha()
{
	const UnsignedInt framesForTransition = LOGICFRAMES_PER_SECOND;
	const UnsignedByte minAlpha = 32;

	Real alphaScale = INT_TO_REAL(TheGameLogic->getFrame() % framesForTransition) / (framesForTransition / 2.0f);
	if( alphaScale > 0.0f )
		return REAL_TO_UNSIGNEDBYTE( ((alphaScale - 1.0f) * (255.0f - minAlpha)) + minAlpha );

	return REAL_TO_UNSIGNEDBYTE( (alphaScale * (255.0f - minAlpha)) + minAlpha );

}
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bool W3DRadar::canRenderObject( const RadarObject *rObj, const Player *localPlayer )
{
	if (rObj->isTemporarilyHidden())
	{
		return false;
	}

	const Int playerIndex = localPlayer->getPlayerIndex();
	const Object *obj = rObj->friend_getObject();

	//
	// check for shrouded status
	// if object is fogged or shrouded, don't render it
	//
	if (obj->getShroudedStatus(playerIndex) > OBJECTSHROUD_PARTIAL_CLEAR)
	{
		return false;
	}

	//
	// objects with a local only unit priority will only appear on the radar if they
	// are controlled by the local player, or if the local player is an observer (cause
	// they are godlike and can see everything)
	//
	if (obj->getRadarPriority() == RADAR_PRIORITY_LOCAL_UNIT_ONLY &&
		obj->getControllingPlayer() != localPlayer &&
		localPlayer->isPlayerActive() )
	{
		return false;
	}

	//
	// ML-- What the heck is this? local-only and neutral-observer-viewed units are stealthy?? Since when?
	// Now it twinkles for any stealthed object, whether locally controlled or neutral-observer-viewed
	//
	if (TheControlBar->getCurrentlyViewedPlayerRelationship(obj->getTeam()) == ENEMIES &&
		obj->testStatus( OBJECT_STATUS_STEALTHED ) &&
		!obj->testStatus( OBJECT_STATUS_DETECTED ) &&
		!obj->testStatus( OBJECT_STATUS_DISGUISED ) )
	{
		return false;
	}

	return true;
}

//-------------------------------------------------------------------------------------------------
/** Draw one list of radar blips straight onto the screen, rather than into the overlay texture.
	*
	* The overlay texture is built at the radar cell resolution and then scaled down into whatever
	* the radar window happens to be, so a blip drawn into it is either smeared by the filtering on
	* the way down or, with the filtering off, shimmers as the texel that wins under each screen
	* pixel changes while the unit moves. Drawing at screen resolution sidesteps both: the blip is
	* the size it is meant to be, its edges land on pixel boundaries, and it moves smoothly.
	*
	* Called once per pass; see RadarBlipPass. */
//-------------------------------------------------------------------------------------------------
void W3DRadar::drawObjectListBlips( const RadarObject *listHead, RadarBlipPass pass,
																 Int pixelX, Int pixelY, Int width, Int height )
{

	// sanity
	if( listHead == nullptr )
		return;

	Player *player = rts::getObservedOrLocalPlayer();

	//
	// how big a blip is on screen. The radar window is a good deal smaller than the cell grid, so
	// these are in screen pixels and do not follow the grid at all.
	//
	const UnsignedByte outlineAlpha = 200;

	// the world to radar cell divisors do not change between objects
	const Real xSample = m_mapExtent.width() / getCellWidth();
	const Real ySample = m_mapExtent.height() / getCellHeight();

	// keep the blips inside the radar image, the same region the terrain is drawn into
	const Int clipLoX = pixelX;
	const Int clipLoY = pixelY;
	const Int clipHiX = pixelX + width;
	const Int clipHiY = pixelY + height;

	for( const RadarObject *rObj = listHead; rObj; rObj = rObj->friend_getNext() )
	{
		if (!canRenderObject(rObj, player))
			continue;

		// get object position
		const Object *obj = rObj->friend_getObject();
		const Coord3D *pos = obj->getPosition();

		// world to radar cell, then radar cell to a pixel inside the radar image
		ICoord2D radarPoint;
		radarPoint.x = pos->x / xSample;
		radarPoint.y = pos->y / ySample;

		if( legalRadarPoint( radarPoint.x, radarPoint.y ) == FALSE )
			continue;

		ICoord2D screenPoint;
		radarToPixel( &radarPoint, &screenPoint, pixelX, pixelY, width, height );

		// get the color we are going to draw in
		UnsignedByte r, g, b, a;
		GameGetColorComponents( rObj->getColor(), &r, &g, &b, &a );

		// adjust the alpha for stealth units so they "fade/blink" on the radar for the controller
		if( obj->testStatus( OBJECT_STATUS_STEALTHED ) )
		{
			a = stealthBlinkAlpha();
		}

		//
		// structures draw bigger than units. They are large and they do not move, so reading a base
		// apart from the army standing in it is worth the extra pixels.
		//
		const Bool isStructure = (obj->getRadarPriority() == RADAR_PRIORITY_STRUCTURE);
		const Int coreSize = isStructure ? m_structureBlipSize : m_unitBlipSize;

		// centre the blip on the object rather than hanging it off to one side
		Int drawSize = (pass == RADAR_BLIP_PASS_OUTLINE) ? coreSize + 2 : coreSize;
		Int drawX = screenPoint.x - drawSize / 2;
		Int drawY = screenPoint.y - drawSize / 2;

		// clip to the radar image
		if( drawX < clipLoX || drawY < clipLoY ||
				drawX + drawSize > clipHiX || drawY + drawSize > clipHiY )
			continue;

		if( pass == RADAR_BLIP_PASS_OUTLINE )
		{

			//
			// fade the outline in step with the core, otherwise a stealthed unit sits inside a
			// solid ring while the middle of it blinks away
			//
			const Color outlineColor = GameMakeColor( 0, 0, 0, (UnsignedByte)((outlineAlpha * a) / 255) );
			TheDisplay->drawFillRect( drawX, drawY, drawSize, drawSize, outlineColor );

		}
		else
		{

			TheDisplay->drawFillRect( drawX, drawY, drawSize, drawSize, GameMakeColor( r, g, b, a ) );

		}

	}

}
//-------------------------------------------------------------------------------------------------
/** Render an object list into the texture passed in */
//-------------------------------------------------------------------------------------------------
void W3DRadar::renderObjectList( const RadarObject *listHead, TextureClass *texture )
{

	// sanity
	if( listHead == nullptr || texture == nullptr )
		return;

	// get surface for texture to render into
	SurfaceClass *surface = texture->Get_Surface_Level();

	// loop through all objects and draw
	ICoord2D radarPoint;

	Player *player = rts::getObservedOrLocalPlayer();

	SurfaceClass::SurfaceDescription surfaceDesc;
	surface->Get_Description(surfaceDesc);
	int pitch;
	void *pBits = surface->Lock(&pitch);
	const unsigned int bytesPerPixel = Get_Bytes_Per_Pixel(surfaceDesc.Format);

	for( const RadarObject *rObj = listHead; rObj; rObj = rObj->friend_getNext() )
	{
		if (!canRenderObject(rObj, player))
			continue;

		// get object position
		const Object *obj = rObj->friend_getObject();
		const Coord3D *pos = obj->getPosition();

		// compute object position as a radar blip
		radarPoint.x = pos->x / (m_mapExtent.width() / getCellWidth());
		radarPoint.y = pos->y / (m_mapExtent.height() / getCellHeight());

		// get the color we're going to draw in
		Color argbColor = rObj->getColor();

		// adjust the alpha for stealth units so they "fade/blink" on the radar for the controller
		// if( obj->getRadarPriority() == RADAR_PRIORITY_LOCAL_UNIT_ONLY )
		// ML-- What the heck is this? local-only and neutral-observer-viewed units are stealthy?? Since when?
		// Now it twinkles for any stealthed object, whether locally controlled or neutral-observer-viewed
		if( obj->testStatus( OBJECT_STATUS_STEALTHED ) )
		{
			UnsignedByte r, g, b, a;
			GameGetColorComponents( argbColor, &r, &g, &b, &a );

			a = stealthBlinkAlpha();
			argbColor = GameMakeColor( r, g, b, a );

		}

		const unsigned int pixelColor = ARGB_Color_To_WW3D_Color(surfaceDesc.Format, argbColor);

		// draw the blip, but make sure the points are legal
		if( legalRadarPoint( radarPoint.x, radarPoint.y ) )
			surface->Draw_Pixel( radarPoint.x, radarPoint.y, pixelColor, bytesPerPixel, pBits, pitch );

		radarPoint.y++;
		if( legalRadarPoint( radarPoint.x, radarPoint.y ) )
			surface->Draw_Pixel( radarPoint.x, radarPoint.y, pixelColor, bytesPerPixel, pBits, pitch );

		radarPoint.x++;
		if( legalRadarPoint( radarPoint.x, radarPoint.y ) )
			surface->Draw_Pixel( radarPoint.x, radarPoint.y, pixelColor, bytesPerPixel, pBits, pitch );

		radarPoint.y--;
		if( legalRadarPoint( radarPoint.x, radarPoint.y ) )
			surface->Draw_Pixel( radarPoint.x, radarPoint.y, pixelColor, bytesPerPixel, pBits, pitch );

	}

	surface->Unlock();
	REF_PTR_RELEASE(surface);

}

//-------------------------------------------------------------------------------------------------
/** Does this terrain sample read as a man made surface rather than natural ground?
	*
	* The terrain sampler only hands back the averaged color of the texture under the point, so there
	* is nothing in it that says "this is concrete" -- the material has to be guessed from the color.
	* Natural ground keeps a colour cast to it, greens and browns and sand, while rock, concrete and
	* snow all come back close to neutral, so how far the sample sits from grey sorts the two apart.
	*
	* Brightness deliberately does not come into it. Calling anything bright enough man made looks
	* like it would catch pale concrete, but it catches desert sand first: sand is bright and it is
	* everywhere, and a desert then draws as though the whole map were paved. The cast on its own
	* already tells the two apart, sand being warm where concrete is neutral. */
//-------------------------------------------------------------------------------------------------
Bool W3DRadar::isManMadeTerrainColor( const RGBColor *color ) const
{
	const Real greyThreshold = 0.10f;		// below this much colour cast the sample is called man made

	//
	// how far from neutral the colour sits. The spread between the strongest and weakest channel
	// is the cheap stand in for saturation, and it does not need to be more clever than that.
	//
	const Real channelHi = max( color->red, max( color->green, color->blue ) );
	const Real channelLo = min( color->red, min( color->green, color->blue ) );
	const Real colorCast = channelHi - channelLo;

	return (colorCast < greyThreshold);

}

//-------------------------------------------------------------------------------------------------
/** The tone the new radar draws ground in: a pale grey for rock and the man made surfaces sitting
	* on it, and an olive for everything natural.
	*
	* Mixed by how much of the cell read as man made rather than picked outright, because picking
	* leaves every boundary a hard step between the two tones and the ground stops looking like
	* ground. Ground that is all one thing still comes out as one flat tone, which is the look this
	* is after; it is only the cells along an edge that land somewhere in between. */
//-------------------------------------------------------------------------------------------------
void W3DRadar::getTerrainTone( RGBColor *color, Real manMadeFraction ) const
{
	const RGBColor groundTone = { 0.45f, 0.42f, 0.26f };		///< olive for natural ground
	const RGBColor structureTone = { 0.72f, 0.73f, 0.72f };	///< pale grey for rock and concrete

	manMadeFraction = clamp( 0.0f, manMadeFraction, 1.0f );

	color->red = groundTone.red + (structureTone.red - groundTone.red) * manMadeFraction;
	color->green = groundTone.green + (structureTone.green - groundTone.green) * manMadeFraction;
	color->blue = groundTone.blue + (structureTone.blue - groundTone.blue) * manMadeFraction;

}
//-------------------------------------------------------------------------------------------------
/** Shade the color passed in using the height parameter to lighten and darken it.  Colors
	* will be interpolated using the value "height" across the range from loZ to hiZ.  The
	* midZ is the "middle" point, height values above it will be lightened, while
	* lower ones are darkened. */
//-------------------------------------------------------------------------------------------------
void W3DRadar::interpolateColorForHeight( RGBColor *color,
																					Real height,
																					Real hiZ,
																					Real midZ,
																					Real loZ )
{
	const Real howBright = 0.95f;  // bigger is brighter (0.0 to 1.0)
	const Real howDark   = 0.60f;  // bigger is darker (0.0 to 1.0)

	// sanity on map height (flat maps bomb)
	if (hiZ == midZ)
		hiZ = midZ+0.1f;
	if (midZ == loZ)
		loZ = midZ-0.1f;
	if (hiZ == loZ)
		hiZ = loZ+0.2f;

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

///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS /////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DRadar::W3DRadar()
{

	m_terrainTextureFormat = WW3D_FORMAT_UNKNOWN;
	m_terrainImage = nullptr;
	m_terrainTexture = nullptr;

	m_overlayTextureFormat = WW3D_FORMAT_UNKNOWN;
	m_overlayImage = nullptr;
	m_overlayTexture = nullptr;

	m_shroudTextureFormat = WW3D_FORMAT_UNKNOWN;
	m_shroudImage = nullptr;
	m_shroudTexture = nullptr;
	m_shroudSurface = nullptr;
	m_shroudSurfaceBits = nullptr;
	m_shroudSurfacePitch = 0;
	m_shroudSurfaceFormat = WW3D_FORMAT_UNKNOWN;
	m_shroudSurfacePixelSize = 0;

	m_textureWidth = getCellWidth();
	m_textureHeight = getCellHeight();

	m_newRadar = (TheGlobalData && TheGlobalData->m_newRadar);

	//
	// resolve the blip size option into screen pixels here, so the draw loop just reads a number.
	// Both sizes are odd, because a blip is centred on its object by halving its size and an even
	// one would land half a pixel off.
	//
	if( TheGlobalData && TheGlobalData->m_radarBlipSize == RadarBlipSize_Small )
	{
		m_unitBlipSize = 3;
		m_structureBlipSize = 5;
	}
	else
	{
		m_unitBlipSize = 5;
		m_structureBlipSize = 7;
	}

	m_reconstructViewBox = TRUE;

	for( Int i = 0; i < 4; i++ )
	{
		m_viewBox[ i ].zero();
	}

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DRadar::~W3DRadar()
{

	// delete resources used for the W3D radar
	deleteResources();

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DRadar::xfer( Xfer *xfer )
{
	Radar::xfer(xfer);
}

//-------------------------------------------------------------------------------------------------
/** Radar initialization */
//-------------------------------------------------------------------------------------------------
void W3DRadar::init()
{
	ICoord2D size;
	Region2D uv;

	// extending functionality
	Radar::init();

	// gather specific texture format information
	initializeTextureFormats();

	// allocate our terrain texture
	// poolify
	m_terrainTexture = MSGNEW("TextureClass") TextureClass( m_textureWidth, m_textureHeight,
																			 m_terrainTextureFormat, MIP_LEVELS_1 );
	DEBUG_ASSERTCRASH( m_terrainTexture, ("W3DRadar: Unable to allocate terrain texture") );

	// allocate our overlay texture
	m_overlayTexture = MSGNEW("TextureClass") TextureClass( m_textureWidth, m_textureHeight,
																			 m_overlayTextureFormat, MIP_LEVELS_1 );
	DEBUG_ASSERTCRASH( m_overlayTexture, ("W3DRadar: Unable to allocate overlay texture") );

	// set filter type for the overlay texture, try it and see if you like it, I don't ;)
//	m_overlayTexture->Set_Min_Filter( TextureFilterClass::FILTER_TYPE_NONE );
//	m_overlayTexture->Set_Mag_Filter( TextureFilterClass::FILTER_TYPE_NONE );

	// allocate our shroud texture
	m_shroudTexture = MSGNEW("TextureClass") TextureClass( m_textureWidth, m_textureHeight,
																			 m_shroudTextureFormat, MIP_LEVELS_1 );
	DEBUG_ASSERTCRASH( m_shroudTexture, ("W3DRadar: Unable to allocate shroud texture") );
	m_shroudTexture->Get_Filter().Set_Min_Filter( TextureFilterClass::FILTER_TYPE_DEFAULT );
	m_shroudTexture->Get_Filter().Set_Mag_Filter( TextureFilterClass::FILTER_TYPE_DEFAULT );

	//
	// create images used for rendering and set them up with the textures
	//

	//
	// the terrain image, note the UV coords change it from (0,0) in the upper left
	// to (0,0) in the lower left cause that's how we are initially oriented in the
	// world (positive X to the right and positive Y up)
	//
	m_terrainImage = newInstance(Image);
	uv.lo.x = 0.0f;
	uv.lo.y = 1.0f;
	uv.hi.x = 1.0f;
	uv.hi.y = 0.0f;
	m_terrainImage->setStatus( IMAGE_STATUS_RAW_TEXTURE );
	m_terrainImage->setRawTextureData( m_terrainTexture );
	m_terrainImage->setUV( &uv );
	m_terrainImage->setTextureWidth( m_textureWidth );
	m_terrainImage->setTextureHeight( m_textureHeight );
	size.x = m_textureWidth;
	size.y = m_textureHeight;
	m_terrainImage->setImageSize( &size );

	// the overlay image
	m_overlayImage = newInstance(Image);
	uv.lo.x = 0.0f;
	uv.lo.y = 1.0f;
	uv.hi.x = 1.0f;
	uv.hi.y = 0.0f;
	m_overlayImage->setStatus( IMAGE_STATUS_RAW_TEXTURE );
	m_overlayImage->setRawTextureData( m_overlayTexture );
	m_overlayImage->setUV( &uv );
	m_overlayImage->setTextureWidth( m_textureWidth );
	m_overlayImage->setTextureHeight( m_textureHeight );
	size.x = m_textureWidth;
	size.y = m_textureHeight;
	m_overlayImage->setImageSize( &size );

	// the shroud image
	m_shroudImage = newInstance(Image);
	uv.lo.x = 0.0f;
	uv.lo.y = 1.0f;
	uv.hi.x = 1.0f;
	uv.hi.y = 0.0f;
	m_shroudImage->setStatus( IMAGE_STATUS_RAW_TEXTURE );
	m_shroudImage->setRawTextureData( m_shroudTexture );
	m_shroudImage->setUV( &uv );
	m_shroudImage->setTextureWidth( m_textureWidth );
	m_shroudImage->setTextureHeight( m_textureHeight );
	size.x = m_textureWidth;
	size.y = m_textureHeight;
	m_shroudImage->setImageSize( &size );

}

//-------------------------------------------------------------------------------------------------
/** Reset the radar to the initial empty state ready for new data */
//-------------------------------------------------------------------------------------------------
void W3DRadar::reset()
{

	// extending functionality, call base class
	Radar::reset();

	// clear our texture data, but do not delete the resources
	SurfaceClass *surface;

	surface = m_terrainTexture->Get_Surface_Level();
	if( surface )
	{
		surface->Clear();
		REF_PTR_RELEASE(surface);
	}

	surface = m_overlayTexture->Get_Surface_Level();
	if( surface )
	{
		surface->Clear();
		REF_PTR_RELEASE(surface);
	}

	// don't call Clear(); that wips to transparent. do this instead.
	//gs Dude, it's called CLEARshroud.  It needs to clear the shroud.
	clearShroud();

}

//-------------------------------------------------------------------------------------------------
/** Update */
//-------------------------------------------------------------------------------------------------
void W3DRadar::update()
{

	// extend base class
	Radar::update();

}

//-------------------------------------------------------------------------------------------------
/** Reset the radar for the new map data being given to it */
//-------------------------------------------------------------------------------------------------
void W3DRadar::newMap( TerrainLogic *terrain )
{

	//
	// extending functionality, call the base class ... this will cause a reset of the
	// system which will clear out our textures but not free them
	//
	Radar::newMap( terrain );

	// sanity
	if( terrain == nullptr )
		return;

	// build terrain texture
	buildTerrainTexture( terrain );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void W3DRadar::buildTerrainTexture( TerrainLogic *terrain )
{
	SurfaceClass *surface;
	RGBColor waterColor;

	// we will want to reconstruct our new view box now
	m_reconstructViewBox = TRUE;

	// setup our water color
	waterColor.red = TheWaterTransparency->m_radarColor.red;
	waterColor.green = TheWaterTransparency->m_radarColor.green;
	waterColor.blue = TheWaterTransparency->m_radarColor.blue;

	// get the terrain surface to draw in
	surface = m_terrainTexture->Get_Surface_Level();
	DEBUG_ASSERTCRASH( surface, ("W3DRadar: Can't get surface for terrain texture") );

	// build the terrain
	RGBColor sampleColor;
	RGBColor color;
	Int i, j, samples;
	Int x, y;
	ICoord2D radarPoint;
	Coord3D worldPoint;
	Bridge *bridge;

	SurfaceClass::SurfaceDescription surfaceDesc;
	surface->Get_Description(surfaceDesc);
	int pitch;
	void *pBits = surface->Lock(&pitch);
	const unsigned int bytesPerPixel = Get_Bytes_Per_Pixel(surfaceDesc.Format);

	//
	// TheSuperHackers @feature the new radar draws a contour where the water meets the land. Note
	// down which cells are water, and what colour each one came out, so that a second pass can
	// darken just the waterline. Nothing is allocated for the retail radar.
	//
	std::vector<UnsignedByte> waterFlags;
	std::vector<RGBColor> cellColors;
	if( m_newRadar )
	{
		waterFlags.resize( m_textureWidth * m_textureHeight, 0 );
		cellColors.resize( m_textureWidth * m_textureHeight );
	}

	//
	// water is a smooth depth gradient rather than tiled texture, so unlike the terrain it does
	// want a wider sample window at the finer grid, and it has no hard edges to soften.
	//
	Int waterSampleRadius = getCellWidth() / RADAR_CELL_WIDTH;
	if( waterSampleRadius > 2 )
	{
		waterSampleRadius = 2;
	}

	for( y = 0; y < m_textureHeight; y++ )
	{

		for( x = 0; x < m_textureWidth; x++ )
		{

			// what point are we inspecting
			radarPoint.x = x;
			radarPoint.y = y;
			radarToWorld2D( &radarPoint, &worldPoint );

			// check to see if this point is part of a working bridge
			Bool workingBridge = FALSE;
			bridge = TheTerrainLogic->findBridgeAt( &worldPoint );
			if( bridge != nullptr )
			{
				Object *obj = TheGameLogic->findObjectByID( bridge->peekBridgeInfo()->bridgeObjectID );

				if( obj )
				{
					BodyModuleInterface *body = obj->getBodyModule();

					if( body->getDamageState() != BODY_RUBBLE )
						workingBridge = TRUE;

				}

			}

			// create a color based on the Z height of the map
			Real waterZ;
			const Bool isWater = terrain->isUnderwater( worldPoint.x, worldPoint.y, &waterZ );
			if( m_newRadar )
			{
				waterFlags[ y * m_textureWidth + x ] = isWater ? 1 : 0;
			}

			if( workingBridge == FALSE && isWater )
			{
				const Int waterSamplesAway = waterSampleRadius;

				sampleColor.red = sampleColor.green = sampleColor.blue = 0.0f;
				samples = 0;

				for( j = y - waterSamplesAway; j <= y + waterSamplesAway; j++ )
				{

					if( j >= 0 && j < m_textureHeight )
					{

						for( i = x - waterSamplesAway; i <= x + waterSamplesAway; i++ )
						{

							if( i >= 0 && i < m_textureWidth )
							{

								// the the world point we are concerned with
								radarPoint.x = i;
								radarPoint.y = j;
								radarToWorld2D( &radarPoint, &worldPoint );

								// get color for this Z and add to our sample color
								Real underwaterZ;
								if( terrain->isUnderwater( worldPoint.x, worldPoint.y, nullptr, &underwaterZ ) )
								{
									// this is our "color" for water
									color = waterColor;

									// interpolate the water color for height in the water table
									interpolateColorForHeight( &color, underwaterZ, waterZ,
																						 waterZ,
																						 m_mapExtent.lo.z );

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
				//
				// how many "tiles" from the center tile we will sample away to average a color for the
				// tile color. TheSuperHackers @feature a cell at the doubled grid covers half the world
				// a retail cell did, so a fixed window would average a quarter of the ground it used to
				// and every patch of texture would speckle through. The two tone mix already flattens
				// the texture out far better than averaging did, so this is no longer about noise: it
				// sets how finely the mix between the two tones can be graded. A wider window gives more
				// steps between all ground and all pavement, which is what keeps an edge from reading as
				// a seam, and it costs no sharpness now that the tones themselves are flat.
				//
				const Int samplesAway = 2;

				sampleColor.red = sampleColor.green = sampleColor.blue = 0.0f;
				samples = 0;
				Int manMadeSamples = 0;
				Real sampleHeight = 0.0f;

				for( j = y - samplesAway; j <= y + samplesAway; j++ )
				{

					if( j >= 0 && j < m_textureHeight )
					{

						for( i = x - samplesAway; i <= x + samplesAway; i++ )
						{

							if( i >= 0 && i < m_textureWidth )
							{

								// the the world point we are concerned with
								radarPoint.x = i;
								radarPoint.y = j;
								radarToWorld( &radarPoint, &worldPoint );

								// get the color we're going to use here
								if( workingBridge )
								{
									AsciiString bridgeTName = bridge->getBridgeTemplateName();
									TerrainRoadType *bridgeTemplate = TheTerrainRoads->findBridge( bridgeTName );

									// sanity
									DEBUG_ASSERTCRASH( bridgeTemplate, ("W3DRadar::buildTerrainTexture - Can't find bridge template for '%s'", bridgeTName.str()) );

									// use bridge color
									if ( bridgeTemplate )
										color = bridgeTemplate->getRadarColor();
									else
										color.setFromInt(0xffffffff);
									//
									// we won't use the height of the terrain at this sample point, we will
									// instead use the height for the entire bridge
									//
									Real bridgeHeight = (bridge->peekBridgeInfo()->fromLeft.z +
																			 bridge->peekBridgeInfo()->fromRight.z +
																			 bridge->peekBridgeInfo()->toLeft.z +
																			 bridge->peekBridgeInfo()->toRight.z) / 4.0f;

									// interpolate the color, but use the bridge height, not the terrain height
									interpolateColorForHeight( &color, bridgeHeight,
																						 getTerrainAverageZ(),
																						 m_mapExtent.hi.z, m_mapExtent.lo.z );

								}
								else
								{

									// get the color at this point
									TheTerrainVisual->getTerrainColorAt( worldPoint.x, worldPoint.y, &color );

									//
									// just take the vote here. Which tone the cell ends up in is settled once, after
									// all its samples are in, because a cell over the edge of a car park or a road
									// through a park genuinely has both kinds of ground under it, and snapping each
									// sample on its own leaves those cells dithering between the two tones.
									//
									if( m_newRadar && isManMadeTerrainColor( &color ) )
									{
										manMadeSamples++;
									}

									//
									// gather the height over the same samples the colour uses. Shading the cell
									// from a single probe instead would let one bump or hollow darken exactly one
									// cell, which shows up as dots on the sample grid rather than as terrain.
									//
									sampleHeight += worldPoint.z;

									//
									// shade the sample only if its colour is going to be used. The two tone path
									// replaces the averaged colour outright further down and shades once from the
									// averaged height, so doing it per sample here would be thrown away.
									//
									if( m_newRadar == FALSE )
									{
										// interpolate the color for height
										interpolateColorForHeight( &color, worldPoint.z, getTerrainAverageZ(),
																							 m_mapExtent.hi.z, m_mapExtent.lo.z );
									}

								}

								// add color to our samples
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

				//
				// take the tone from how much of the cell read as man made, rather than from which kind
				// won it. Ground that is all one thing still comes out flat, a cell over a kerb no longer
				// dithers, and an edge between the two gets a step of shading across it instead of a
				// hard seam. The averaged colour above is dropped; only its height shading is re-applied.
				//
				if( m_newRadar && workingBridge == FALSE )
				{
					getTerrainTone( &color, (Real)manMadeSamples / (Real)samples );
					interpolateColorForHeight( &color, sampleHeight / (Real)samples, getTerrainAverageZ(),
																	 m_mapExtent.hi.z, m_mapExtent.lo.z );
				}

			}

			//
			// only the water cells are ever read back, by the shoreline pass below, so there is no
			// reason to keep the colour of every cell on the map
			//
			if( m_newRadar && isWater )
			{
				cellColors[ y * m_textureWidth + x ] = color;
			}

			// draw the pixel for the terrain at this point, note that because of the orientation
			// of our world we draw it with positive y in the "up" direction
			const Color argbColor = GameMakeColor( color.red * 255, color.green * 255, color.blue * 255, 255 );
			const unsigned int pixelColor = ARGB_Color_To_WW3D_Color(surfaceDesc.Format, argbColor);
			surface->Draw_Pixel( x, y, pixelColor, bytesPerPixel, pBits, pitch );

		}

	}

	//
	// TheSuperHackers @feature draw the shoreline. A water cell that touches land on any of its
	// four sides gets darkened, which puts a one cell rim on the water side of the waterline and
	// leaves the land reading as it did. Marking both sides would double the thickness, and going
	// eight ways would fatten it again on every diagonal stretch of coast.
	//
	if( m_newRadar )
	{
		//
		// the water is already dark, so darkening it again would just smudge the edge. Bias the
		// waterline toward a fixed near black instead, which keeps the contour a definite line
		// whatever colour the map gave its water.
		//
		const Real shoreDarken = 0.30f;		// what is left of the cell colour along the waterline
		const Real shoreFloor = 0.04f;		// the near black the waterline is pulled toward

		for( y = 0; y < m_textureHeight; y++ )
		{

			for( x = 0; x < m_textureWidth; x++ )
			{

				if( waterFlags[ y * m_textureWidth + x ] == 0 )
				{
					continue;
				}

				//
				// a missing neighbour off the edge of the map counts as more of the same, so that a
				// map that runs into water at its border does not get a dark frame drawn round it
				//
				Bool touchesLand = FALSE;
				if( x > 0 && waterFlags[ y * m_textureWidth + (x - 1) ] == 0 )
				{
					touchesLand = TRUE;
				}
				if( x < m_textureWidth - 1 && waterFlags[ y * m_textureWidth + (x + 1) ] == 0 )
				{
					touchesLand = TRUE;
				}
				if( y > 0 && waterFlags[ (y - 1) * m_textureWidth + x ] == 0 )
				{
					touchesLand = TRUE;
				}
				if( y < m_textureHeight - 1 && waterFlags[ (y + 1) * m_textureWidth + x ] == 0 )
				{
					touchesLand = TRUE;
				}

				if( touchesLand == FALSE )
				{
					continue;
				}

				//
				// darken what is already there instead of writing a flat colour, so the depth shading
				// survives and a map that overrides its radar water colour still looks right
				//
				const RGBColor &cell = cellColors[ y * m_textureWidth + x ];
				const Color shoreArgb = GameMakeColor( (shoreFloor + cell.red * shoreDarken) * 255,
																							 (shoreFloor + cell.green * shoreDarken) * 255,
																							 (shoreFloor + cell.blue * shoreDarken) * 255, 255 );
				const unsigned int shoreColor = ARGB_Color_To_WW3D_Color(surfaceDesc.Format, shoreArgb);
				surface->Draw_Pixel( x, y, shoreColor, bytesPerPixel, pBits, pitch );

			}

		}

	}

	// all done with the surface
	surface->Unlock();
	REF_PTR_RELEASE(surface);

}

// ------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DRadar::clearShroud()
{
#if ENABLE_CONFIGURABLE_SHROUD
	if (!TheGlobalData->m_shroudOn)
		return;
#endif

	SurfaceClass *surface = m_shroudTexture->Get_Surface_Level();

	// fill to clear, shroud will make black.  Don't want to make something black that logic can't clear

	int pitch;
	void *pBits = surface->Lock(&pitch);
	const unsigned int bytesPerPixel = surface->Get_Bytes_Per_Pixel();
	const Color color = GameMakeColor( 0, 0, 0, 0 );

	for( Int y = 0; y < m_textureHeight; y++ )
	{
		surface->Draw_H_Line(y, 0, m_textureWidth-1, color, bytesPerPixel, pBits, pitch);
	}

	surface->Unlock();
	REF_PTR_RELEASE(surface);
}

// ------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DRadar::setShroudLevel(Int shroudX, Int shroudY, CellShroudStatus setting)
{
#if ENABLE_CONFIGURABLE_SHROUD
	if (!TheGlobalData->m_shroudOn)
		return;
#endif

	W3DShroud* shroud = TheTerrainRenderObject ? TheTerrainRenderObject->getShroud() : nullptr;
	if (!shroud)
		return;

	Int mapMinX = shroudX * shroud->getCellWidth();
	Int mapMinY = shroudY * shroud->getCellHeight();
	Int mapMaxX = (shroudX+1) * shroud->getCellWidth();
	Int mapMaxY = (shroudY+1) * shroud->getCellHeight();

	ICoord2D radarPoint;
	Coord3D worldPoint;

	worldPoint.x = mapMinX;
	worldPoint.y = mapMinY;
	worldToRadar( &worldPoint, &radarPoint );
	const Int radarMinX = radarPoint.x;
	const Int radarMinY = radarPoint.y;

	worldPoint.x = mapMaxX;
	worldPoint.y = mapMaxY;
	worldToRadar( &worldPoint, &radarPoint );
	const Int radarMaxX = radarPoint.x;
	const Int radarMaxY = radarPoint.y;

	// Int radarMinX = REAL_TO_INT_FLOOR(mapMinX / getXSample());
	// Int radarMinY = REAL_TO_INT_FLOOR(mapMinY / getYSample());
	// Int radarMaxX = REAL_TO_INT_CEIL(mapMaxX / getXSample());
	// Int radarMaxY = REAL_TO_INT_CEIL(mapMaxY / getYSample());

	/// @todo srj -- this really needs to smooth the display!

	//Logic is saying shroud.  We can add alpha levels here in client if needed.
	// W3DShroud is a 0-255 alpha byte.  Logic shroud is a double reference count.
	Int alpha;
	if( setting == CELLSHROUD_SHROUDED )
		alpha = 255;
	else if( setting == CELLSHROUD_FOGGED )
		alpha = 127;///< @todo placeholder to get feedback on logic work while graphic side being decided
	else
		alpha = 0;

	if (m_shroudSurface == nullptr)
	{
		// This is expensive.
		SurfaceClass* surface = m_shroudTexture->Get_Surface_Level();
		DEBUG_ASSERTCRASH( surface, ("W3DRadar: Can't get surface for Shroud texture") );
		SurfaceClass::SurfaceDescription surfaceDesc;
		surface->Get_Description(surfaceDesc);
		int pitch;
		void *pBits = surface->Lock(&pitch);
		const unsigned int bytesPerPixel = Get_Bytes_Per_Pixel(surfaceDesc.Format);
		const Color argbColor = GameMakeColor( 0, 0, 0, alpha );
		const unsigned int pixelColor = ARGB_Color_To_WW3D_Color(surfaceDesc.Format, argbColor);

		//
		// TheSuperHackers @performance fill by the row rather than the pixel. Draw_H_Line works out
		// the start of the row once instead of once per pixel, and this runs for every shroud cell
		// that changed, every frame, so it is worth not paying that per pixel.
		//
		for( Int y = radarMinY; y <= radarMaxY; ++y )
		{
			surface->Draw_H_Line( y, radarMinX, radarMaxX, pixelColor, bytesPerPixel, pBits, pitch );
		}

		surface->Unlock();
		REF_PTR_RELEASE(surface);
	}
	else
	{
		// This is cheap.
		DEBUG_ASSERTCRASH(m_shroudSurfaceBits != nullptr, ("W3DRadar::setShroudLevel: m_shroudSurfaceBits is not expected null"));
		DEBUG_ASSERTCRASH(m_shroudSurfaceFormat != WW3D_FORMAT_UNKNOWN, ("W3DRadar::setShroudLevel: m_shroudSurfaceFormat is not expected UNKNOWN"));
		DEBUG_ASSERTCRASH(m_shroudSurfacePixelSize != 0, ("W3DRadar::setShroudLevel: m_shroudSurfacePixelSize is not expected 0"));
		const Color argbColor = GameMakeColor( 0, 0, 0, alpha );
		const unsigned int pixelColor = ARGB_Color_To_WW3D_Color(m_shroudSurfaceFormat, argbColor);

		for( Int y = radarMinY; y <= radarMaxY; ++y )
		{
			m_shroudSurface->Draw_H_Line( y, radarMinX, radarMaxX, pixelColor, m_shroudSurfacePixelSize, m_shroudSurfaceBits, m_shroudSurfacePitch );
		}
	}
}

void W3DRadar::beginSetShroudLevel()
{
	DEBUG_ASSERTCRASH( m_shroudSurface == nullptr, ("W3DRadar::beginSetShroudLevel: m_shroudSurface is expected null") );
	m_shroudSurface = m_shroudTexture->Get_Surface_Level();
	DEBUG_ASSERTCRASH( m_shroudSurface != nullptr, ("W3DRadar::beginSetShroudLevel: Can't get surface for Shroud texture") );

	SurfaceClass::SurfaceDescription surfaceDesc;
	m_shroudSurface->Get_Description(surfaceDesc);
	m_shroudSurfaceBits = m_shroudSurface->Lock(&m_shroudSurfacePitch);
	m_shroudSurfaceFormat = surfaceDesc.Format;
	m_shroudSurfacePixelSize = Get_Bytes_Per_Pixel(surfaceDesc.Format);
}

void W3DRadar::endSetShroudLevel()
{
	DEBUG_ASSERTCRASH( m_shroudSurface != nullptr, ("W3DRadar::endSetShroudLevel: m_shroudSurface is not expected null") );
	if (m_shroudSurfaceBits != nullptr)
	{
		m_shroudSurface->Unlock();
		m_shroudSurfaceBits = nullptr;
		m_shroudSurfacePitch = 0;
		m_shroudSurfaceFormat = WW3D_FORMAT_UNKNOWN;
		m_shroudSurfacePixelSize = 0;
	}
	REF_PTR_RELEASE(m_shroudSurface);
}

//-------------------------------------------------------------------------------------------------
/** Actually draw the radar at the screen coordinates provided
	* NOTE about how drawing works: The radar images are computed at samples across the
	* map and are built into a "square" texture area.  At the time of drawing and computing
	* radar<->world coords we consider the "ratio" of width to height of the map dimensions
	* so that when we draw we preserve the aspect ratio of the map and don't squish it in
	* any direction that would cause the map to be distorted.  Extra blank space is drawn
	* around the radar images to keep the whole radar area covered when the map displayed
	* is "long" or "tall" */
//-------------------------------------------------------------------------------------------------
void W3DRadar::draw( Int pixelX, Int pixelY, Int width, Int height )
{
	// if the local player does not have a radar then we can't draw anything
	if( !rts::localPlayerHasRadar() )
		return;

	//
	// given a upper left corner at pixelX|Y and a width and height to draw into, figure out
	// where we should start and end the image so that the final drawn image has the
	// same ratio as the map and isn't stretched or distorted
	//
	ICoord2D ul, lr;
	findDrawPositions( pixelX, pixelY, width, height, &ul, &lr );

	Int scaledWidth = lr.x - ul.x;
	Int scaledHeight = lr.y - ul.y;

	// draw black border areas where we need map
	Color fillColor = GameMakeColor( 0, 0, 0, 255 );
	Color lineColor = GameMakeColor( 50, 50, 50, 255 );
	if( m_mapExtent.width()/width >= m_mapExtent.height()/height )
	{

		// draw horizontal bars at top and bottom
		TheDisplay->drawFillRect( pixelX, pixelY, width, ul.y - pixelY - 1, fillColor );
		TheDisplay->drawFillRect( pixelX, lr.y + 1, width, pixelY + height - lr.y - 1, fillColor);
		TheDisplay->drawLine(pixelX, ul.y, pixelX + width, ul.y, 1, lineColor);
		TheDisplay->drawLine(pixelX, lr.y + 1, pixelX + width, lr.y + 1, 1, lineColor);

	}
	else
	{

		// draw vertical bars to the left and right
		TheDisplay->drawFillRect( pixelX, pixelY, ul.x - pixelX - 1, height, fillColor );
		TheDisplay->drawFillRect( lr.x + 1, pixelY, width - (lr.x - pixelX) - 1, height, fillColor );
		TheDisplay->drawLine(ul.x, pixelY, ul.x, pixelY + height, 1, lineColor);
		TheDisplay->drawLine(lr.x + 1, pixelY, lr.x + 1, pixelY + height, 1, lineColor);

	}

	// draw the terrain texture
	TheDisplay->drawImage( m_terrainImage, ul.x, ul.y, lr.x, lr.y );

	//
	// the new radar draws its blips onto the screen further down instead of into this texture,
	// so there is nothing to rebuild or to lay over the terrain here
	//
	if( m_newRadar == FALSE )
	{

		// refresh the overlay texture once every so many frames
		if( TheGameClient->getFrame() % OVERLAY_REFRESH_RATE == 0 )
		{
			updateObjectTexture(m_overlayTexture);
		}

		// draw the overlay image
		TheDisplay->drawImage( m_overlayImage, ul.x, ul.y, lr.x, lr.y );

	}

	// draw the shroud image
#if ENABLE_CONFIGURABLE_SHROUD
	if( TheGlobalData->m_shroudOn )
#else
	if (true)
#endif
	{
		TheDisplay->drawImage( m_shroudImage, ul.x, ul.y, lr.x, lr.y );
	}

	//
	// draw the blips at screen resolution. Every outline goes down before any core does, across
	// both lists, so that no blip can have its core eaten by the outline of the one beside it,
	// and the local list goes last within each pass so friendly blips win an overlap.
	//
	if( m_newRadar )
	{
		//
		// batch the rects. Every blip is its own drawFillRect, and without a batch open each one of
		// those flushes the 2D renderer on its own, so a big army would cost a draw call per blip
		// per pass. Inside a batch they all accumulate and go down together.
		//
		TheDisplay->beginBatch();

		drawObjectListBlips( m_objectList, RADAR_BLIP_PASS_OUTLINE, ul.x, ul.y, scaledWidth, scaledHeight );
		drawObjectListBlips( m_localObjectList, RADAR_BLIP_PASS_OUTLINE, ul.x, ul.y, scaledWidth, scaledHeight );
		drawObjectListBlips( m_objectList, RADAR_BLIP_PASS_CORE, ul.x, ul.y, scaledWidth, scaledHeight );
		drawObjectListBlips( m_localObjectList, RADAR_BLIP_PASS_CORE, ul.x, ul.y, scaledWidth, scaledHeight );

		TheDisplay->endBatch();
	}

	// draw any icons
	drawIcons( ul.x, ul.y, scaledWidth, scaledHeight );

	// draw any radar events
	drawEvents( ul.x, ul.y, scaledWidth, scaledHeight );

	if( m_reconstructViewBox )
	{
		reconstructViewBox();
	}

	// draw the view region on top of the radar reconstructing if necessary
	drawViewBox( ul.x, ul.y, scaledWidth, scaledHeight );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void W3DRadar::refreshTerrain( TerrainLogic *terrain )
{

	// extend base class
	Radar::refreshTerrain( terrain );

	// rebuild the entire terrain texture
	buildTerrainTexture( terrain );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void W3DRadar::refreshObjects()
{
	if constexpr (OVERLAY_REFRESH_RATE > 1)
	{
		if (m_overlayTexture != nullptr)
		{
			updateObjectTexture(m_overlayTexture);
		}
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void W3DRadar::notifyViewChanged()
{
	m_reconstructViewBox = TRUE;
}


///The following is an "archive" of an attempt to foil the mapshroud hack... saved for later, since it is too close to release to try it


/*
 *
	void W3DRadar::renderObjectList( const RadarObject *listHead, TextureClass *texture )
{

	// sanity
	if( listHead == nullptr || texture == nullptr )
		return;

	// get surface for texture to render into
	SurfaceClass *surface = texture->Get_Surface_Level();

	// loop through all objects and draw
	ICoord2D radarPoint;

	Player *player = rts::getObservedOrLocalPlayer();
	const Int playerIndex = player->getPlayerIndex();

	UnsignedByte minAlpha = 8;

	int pitch;
	void *pBits = surface->Lock(&pitch);
	const unsigned int bytesPerPixel = surface->Get_Bytes_Per_Pixel();

	for( const RadarObject *rObj = listHead; rObj; rObj = rObj->friend_getNext() )
	{
    UnsignedByte h = (UnsignedByte)(rObj->isTemporarilyHidden());
    if ( h )
			continue;

    UnsignedByte a = 0;

		// get object
		const Object *obj = rObj->friend_getObject();
		UnsignedByte r = 1;   // all decoys

		// get the color we're going to draw in
		UnsignedInt c = 0xfe000000;// this is a decoy
    c |= (UnsignedInt)( obj->testStatus( OBJECT_STATUS_STEALTHED ) );//so is this

		// check for shrouded status
		UnsignedByte k =  (UnsignedByte)(obj->getShroudedStatus(playerIndex) > OBJECTSHROUD_PARTIAL_CLEAR);
    if ( k || a)
			continue;	//object is fogged or shrouded, don't render it.

 		//
 		// objects with a local only unit priority will only appear on the radar if they
 		// are controlled by the local player, or if the local player is an observer (cause
		// they are godlike and can see everything)
 		//
 		if( obj->getRadarPriority() == RADAR_PRIORITY_LOCAL_UNIT_ONLY &&
				obj->getControllingPlayer() != player &&
				player->isPlayerActive() )
 			continue;

    UnsignedByte g = c|a;
    UnsignedByte b = h|a;
		// get object position
		const Coord3D *pos = obj->getPosition();

		// compute object position as a radar blip
		radarPoint.x = pos->x / (m_mapExtent.width() / RADAR_CELL_WIDTH);
		radarPoint.y = pos->y / (m_mapExtent.height() / RADAR_CELL_HEIGHT);


		const UnsignedInt framesForTransition = LOGICFRAMES_PER_SECOND;



		// adjust the alpha for stealth units so they "fade/blink" on the radar for the controller
		// if( obj->getRadarPriority() == RADAR_PRIORITY_LOCAL_UNIT_ONLY )
		// ML-- What the heck is this? local-only and neutral-observier-viewed units are stealthy?? Since when?
		// Now it twinkles for any stealthed object, whether locally controlled or neutral-observier-viewed
    c = rObj->getColor();

		if( g & r )
		{
		  Real alphaScale = INT_TO_REAL(TheGameLogic->getFrame() % framesForTransition) / (framesForTransition * 0.5f);
      minAlpha <<= 2; // decoy

 			if ( ( obj->isLocallyControlled() == (Bool)a ) // another decoy, comparing the return of this non-inline with a local
        && !obj->testStatus( OBJECT_STATUS_DISGUISED )
        && !obj->testStatus( OBJECT_STATUS_DETECTED )
        && ++a != 0 // The trick is that this increment does not occur unless all three above conditions are true
        && minAlpha == 32  // tricksy hobbit decoy
        && c != 0 )        // ditto
      {
        g = (UnsignedByte)(rObj->getColor());
        continue;
      }

      a |= k | b;
			GameGetColorComponentsWithCheatSpy( c, &r, &g, &b, &a );//this function does not touch the low order bit in 'a'


			if( alphaScale > 0.0f )
				a = REAL_TO_UNSIGNEDBYTE( ((alphaScale - 1.0f) * (255.0f - minAlpha)) + minAlpha );
			else
				a = REAL_TO_UNSIGNEDBYTE( (alphaScale * (255.0f - minAlpha)) + minAlpha );
			c = GameMakeColor( r, g, b, a );

		}




		// draw the blip, but make sure the points are legal
		if( legalRadarPoint( radarPoint.x, radarPoint.y ) )
			surface->Draw_Pixel( radarPoint.x, radarPoint.y, c, bytesPerPixel, pBits, pitch );

		radarPoint.x++;
		if( legalRadarPoint( radarPoint.x, radarPoint.y ) )
			surface->Draw_Pixel( radarPoint.x, radarPoint.y, c, bytesPerPixel, pBits, pitch );

		radarPoint.y++;
		if( legalRadarPoint( radarPoint.x, radarPoint.y ) )
			surface->Draw_Pixel( radarPoint.x, radarPoint.y, c, bytesPerPixel, pBits, pitch );

		radarPoint.x--;
		if( legalRadarPoint( radarPoint.x, radarPoint.y ) )
			surface->Draw_Pixel( radarPoint.x, radarPoint.y, c, bytesPerPixel, pBits, pitch );




	}

	surface->Unlock();
	REF_PTR_RELEASE(surface);

}


 *
 */
