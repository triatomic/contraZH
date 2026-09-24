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

// FILE: W3DLaserDraw.cpp /////////////////////////////////////////////////////////////////////////
// Author: Colin Day, May 2001
// Desc:   W3DLaserDraw
// Updated: Kris Morness July 2002 -- made it data driven and added new features to make it flexible.
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include <stdlib.h>

#include "Common/GlobalData.h"
#include "Common/Thing.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Color.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/RayEffect.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/LaserUpdate.h"
#include "W3DDevice/GameClient/Module/W3DLaserDraw.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/W3DDynamicLight.h"
#include "W3DDevice/GameClient/W3DScene.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WW3D2/segline.h"
#include "WWMath/vector3.h"
#include "WW3D2/assetmgr.h"
#include "WW3D2/surfaceclass.h"
#include "WW3D2/texture.h"

#include <map>

enum { GLOW_SAMPLE_SIZE = 8 };

// lasers spawn constantly, so each texture is sampled once and remembered by name
static RGBColor getAverageTextureColor( const AsciiString &name, TextureClass *texture )
{
	static std::map<AsciiString, RGBColor> cache;

	std::map<AsciiString, RGBColor>::const_iterator it = cache.find( name );
	if (it != cache.end())
	{
		return it->second;
	}

	RGBColor average;
	average.red = average.green = average.blue = 1.0f;

	SurfaceClass *source = (texture && name.isNotEmpty()) ? texture->Get_Surface_Level( 0 ) : nullptr;
	if (source)
	{
		SurfaceClass::SurfaceDescription desc;
		source->Get_Description( desc );

		// the stretch copy decompresses DXT and filters the whole image down to a few readable texels
		SurfaceClass *sample = NEW_REF( SurfaceClass, ( GLOW_SAMPLE_SIZE, GLOW_SAMPLE_SIZE, WW3D_FORMAT_A8R8G8B8 ) );
		sample->Stretch_Copy( 0, 0, GLOW_SAMPLE_SIZE, GLOW_SAMPLE_SIZE, 0, 0, desc.Width, desc.Height, source );

		int pitch = 0;
		SurfaceClass::LockedSurfacePtr bits = sample->Lock( &pitch );
		if (bits)
		{
			Vector3 sum( 0.0f, 0.0f, 0.0f );
			for( Int y = 0; y < GLOW_SAMPLE_SIZE; y++ )
			{
				for( Int x = 0; x < GLOW_SAMPLE_SIZE; x++ )
				{
					Vector3 texel;
					sample->Get_Pixel( texel, x, y, bits, pitch );
					sum += texel;
				}
			}
			sample->Unlock();

			// a black texture has no hue to give
			if (sum.X + sum.Y + sum.Z > 0.0f)
			{
				average.red = sum.X;
				average.green = sum.Y;
				average.blue = sum.Z;
			}
		}

		REF_PTR_RELEASE( sample );
		REF_PTR_RELEASE( source );
	}

	cache[ name ] = average;
	return average;
}


// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DLaserDrawModuleData::W3DLaserDrawModuleData()
{
	m_innerBeamWidth = 0.0f;         //The total width of beam
	m_outerBeamWidth = 1.0f;         //The total width of beam
  m_numBeams = 1;                 //Number of overlapping cylinders that make the beam. 1 beam will just use inner data.
  m_maxIntensityFrames = 0;				//Laser stays at max intensity for specified time in ms.
  m_fadeFrames = 0;               //Laser will fade and delete.
	m_scrollRate = 0.0f;
	m_tile = false;
	m_segments = 1;
	m_arcHeight = 0.0f;
	m_segmentOverlapRatio = 0.0f;
	m_tilingScalar = 1.0f;
	m_gridColumnsTotal = 1;
	m_gridColumns = 1;
	m_useHouseColorOuter = FALSE;
	m_useHouseColorInner = FALSE;
	m_groundGlowColor = 0;
	m_groundGlowRadius = 0.0f;
	m_groundGlowIntensity = 0.0f;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DLaserDrawModuleData::~W3DLaserDrawModuleData()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DLaserDrawModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  ModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "NumBeams",							INI::parseUnsignedInt,					nullptr, offsetof( W3DLaserDrawModuleData, m_numBeams ) },
		{ "InnerBeamWidth",				INI::parseReal,									nullptr, offsetof( W3DLaserDrawModuleData, m_innerBeamWidth ) },
		{ "OuterBeamWidth",				INI::parseReal,									nullptr, offsetof( W3DLaserDrawModuleData, m_outerBeamWidth ) },
		{ "InnerColor",						INI::parseColorInt,							nullptr, offsetof( W3DLaserDrawModuleData, m_innerColor ) },
		{ "OuterColor",						INI::parseColorInt,							nullptr, offsetof( W3DLaserDrawModuleData, m_outerColor ) },
		{ "MaxIntensityLifetime",	INI::parseDurationUnsignedInt,	nullptr, offsetof( W3DLaserDrawModuleData, m_maxIntensityFrames ) },
		{ "FadeLifetime",					INI::parseDurationUnsignedInt,	nullptr, offsetof( W3DLaserDrawModuleData, m_fadeFrames ) },
		{ "Texture",							INI::parseAsciiString,					nullptr, offsetof( W3DLaserDrawModuleData, m_textureName ) },
		{ "ScrollRate",						INI::parseReal,									nullptr, offsetof( W3DLaserDrawModuleData, m_scrollRate ) },
		{ "Tile",									INI::parseBool,									nullptr, offsetof( W3DLaserDrawModuleData, m_tile ) },
		{ "Segments",							INI::parseUnsignedInt,					nullptr, offsetof( W3DLaserDrawModuleData, m_segments ) },
    { "ArcHeight",						INI::parseReal,									nullptr, offsetof( W3DLaserDrawModuleData, m_arcHeight ) },
		{ "SegmentOverlapRatio",	INI::parseReal,									nullptr, offsetof( W3DLaserDrawModuleData, m_segmentOverlapRatio ) },
		{ "TilingScalar",					INI::parseReal,									nullptr, offsetof( W3DLaserDrawModuleData, m_tilingScalar ) },
		{ "TextureGridTotalColumns",		INI::parseUnsignedInt,							NULL, offsetof(W3DLaserDrawModuleData, m_gridColumnsTotal) },
		{ "TextureGridColumns",				INI::parseUnsignedInt,							NULL, offsetof(W3DLaserDrawModuleData, m_gridColumns) },
		{ "UseHouseColorOuter",				INI::parseBool,							NULL, offsetof(W3DLaserDrawModuleData, m_useHouseColorOuter) },
		{ "UseHouseColorInner",				INI::parseBool,							NULL, offsetof(W3DLaserDrawModuleData, m_useHouseColorInner) },
		{ "GroundGlowColor",					INI::parseColorInt,							nullptr, offsetof(W3DLaserDrawModuleData, m_groundGlowColor) },
		{ "GroundGlowRadius",					INI::parseReal,									nullptr, offsetof(W3DLaserDrawModuleData, m_groundGlowRadius) },
		{ "GroundGlowIntensity",			INI::parsePercentToReal,				nullptr, offsetof(W3DLaserDrawModuleData, m_groundGlowIntensity) },
		{ nullptr, nullptr, nullptr, 0 }
	};
  p.add(dataFieldParse);
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DLaserDraw::W3DLaserDraw( Thing *thing, const ModuleData* moduleData ) :
	DrawModule( thing, moduleData ),
	m_line3D(nullptr),
	m_texture(nullptr),
	m_textureAspectRatio(1.0f),
	m_selfDirty(TRUE),
	m_hexColor(0),
	m_numGroundLights(0)
{
	Vector3 dummyPos1(0.0f, 0.0f, 0.0f);
	Vector3 dummyPos2(1.0f, 1.0f, 1.0f);
	Int i;

	const W3DLaserDrawModuleData* data = getW3DLaserDrawModuleData();

	m_texture = WW3DAssetManager::Get_Instance()->Get_Texture(data->m_textureName.str());
	if (m_texture)
	{
		if (!m_texture->Is_Initialized())
			m_texture->Init();	//make sure texture is actually loaded before accessing surface.

		SurfaceClass::SurfaceDescription surfaceDesc;
		m_texture->Get_Level_Description(surfaceDesc);
		m_textureAspectRatio = (Real)surfaceDesc.Width / (Real)surfaceDesc.Height;
	}
	m_textureColor = getAverageTextureColor( data->m_textureName, m_texture );

	//Get the color components for calculation purposes.
	Real innerRed, innerGreen, innerBlue, innerAlpha, outerRed, outerGreen, outerBlue, outerAlpha;
	GameGetColorComponentsReal(data->m_innerColor, &innerRed, &innerGreen, &innerBlue, &innerAlpha);
	GameGetColorComponentsReal(data->m_outerColor, &outerRed, &outerGreen, &outerBlue, &outerAlpha);

	//DEBUG_LOG(("LaserDraw (Constructor): INIT 1\n"));

	////Get the updatemodule that drives it...
	//Drawable* draw = getDrawable();
	//if (draw) {
	//	static NameKeyType key_LaserUpdate = NAMEKEY("LaserUpdate");
	//	LaserUpdate* update = (LaserUpdate*)draw->findClientUpdateModule(key_LaserUpdate);
	//	if (update) {

	//		if (m_hexColor <= 0) {
	//			m_hexColor = update->getPlayerColor();
	//		}

	//		// handleHouseColor(&innerRed, &innerGreen, &innerBlue, &outerRed, &outerGreen, &outerBlue);
	//		handleHouseColor(innerRed, innerGreen, innerBlue, outerRed, outerGreen, outerBlue);

	//		DEBUG_LOG(("LaserDraw (Constructor): AppliedHousecolor: Inner RGB = %f, %f, %f -- Outer RGB = %f, %f, %f\n", innerRed, innerGreen, innerBlue, outerRed, outerGreen, outerBlue));

	//	}
	//	else {
	//		// DEBUG_ASSERTCRASH(0, ("W3DLaserDraw::doDrawModule() expects its owner drawable %s to have a ClientUpdate = LaserUpdate module.", draw->getTemplate()->getName().str()));
	//		DEBUG_LOG(("W3DLaserDraw::(Constructor) expects its owner drawable %s to have a ClientUpdate = LaserUpdate module.", draw->getTemplate()->getName().str()));
	//		return;
	//	}
	//}
	//else {
	//	DEBUG_LOG(("W3DLaserDraw::(Constructor) Draw is null\n"));
	//}

	//Make sure our beams range between 1 and the maximum cap.
#ifdef I_WANT_TO_BE_FIRED
// srj sez: this data is const for a reason. casting away the constness because we don't like the values
// isn't an acceptable solution. if you need to constrain the values, do so at parsing time, when
// it's still legal to modify these values. (In point of fact, there's not even really any reason to limit
// the numBeams or segments anymore.)
	data->m_numBeams =		 __min( __max( 1, data->m_numBeams ), MAX_LASER_LINES );
	data->m_segments =		 __min( __max( 1, data->m_segments ), MAX_SEGMENTS );
	data->m_tilingScalar = __max( 0.01f, data->m_tilingScalar );
#endif

	//Allocate an array of lines equal to the number of beams * segments
	m_line3D = NEW SegmentedLineClass *[ data->m_numBeams * data->m_segments ];

	for( UnsignedInt segment = 0; segment < data->m_segments; segment++ )
	{
		//We don't care about segment positioning yet until we actually set the position

		// create all the lines we need at the right transparency level
		for( i = data->m_numBeams - 1; i >= 0; i-- )
		{
			int index = segment * data->m_numBeams + i;

			Real red, green, blue, alpha, width;

			if( data->m_numBeams == 1 )
			{
				width = data->m_innerBeamWidth;
				alpha = innerAlpha;
				red = innerRed * innerAlpha;
				green = innerGreen * innerAlpha;
				blue = innerBlue * innerAlpha;
			}
			else
			{
				//Calculate the scale between min and max values
				//0 means use min value, 1 means use max value
				//0.2 means min value + 20% of the diff between min and max
				Real scale = i / ( data->m_numBeams - 1.0f);

				width		= data->m_innerBeamWidth	+ scale * (data->m_outerBeamWidth - data->m_innerBeamWidth);
				alpha		= innerAlpha							+ scale * (outerAlpha - innerAlpha);
				red			= innerRed								+ scale * (outerRed - innerRed) * innerAlpha;
				green		= innerGreen							+ scale * (outerGreen - innerGreen) * innerAlpha;
				blue		= innerBlue								+ scale * (outerBlue - innerBlue) * innerAlpha;
			}

			m_line3D[ index ] = NEW SegmentedLineClass;

			SegmentedLineClass *line = m_line3D[ index ];
			if( line )
			{
				line->Set_Texture( m_texture );
				line->Set_Shader( ShaderClass::_PresetAdditiveShader );	//pick the alpha blending mode you want - see shader.h for others.
				line->Set_Width( width );
				line->Set_Color( Vector3( red, green, blue ) );
				line->Set_UV_Offset_Rate( Vector2(0.0f, data->m_scrollRate) );	//amount to scroll texture on each draw
				if( m_texture )
				{
					if (data->m_gridColumnsTotal > 1) {
						line->Set_Texture_Mapping_Mode(SegLineRendererClass::GRID_TILED_TEXTURE_MAP);	//allows animated U coordinates
						line->Set_U_Scale(1.0f / (Real)(data->m_gridColumnsTotal));
					}
					else {
						line->Set_Texture_Mapping_Mode(SegLineRendererClass::TILED_TEXTURE_MAP);	//this tiles the texture across the line
					}
				}

				// add to scene
				if (W3DDisplay::m_3DScene != nullptr)
					W3DDisplay::m_3DScene->Add_Render_Object( line );	//add it to our scene so it gets rendered with other objects.

				// hide the render object until the first time we come to draw it and
				// set the correct position
				line->Set_Visible( 0 );
			}


		}

	}

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DLaserDraw::~W3DLaserDraw()
{
	const W3DLaserDrawModuleData *data = getW3DLaserDrawModuleData();

	releaseGroundLights();

	for( UnsignedInt i = 0; i < data->m_numBeams * data->m_segments; i++ )
	{

		// remove line from scene
		if (W3DDisplay::m_3DScene != nullptr)
			W3DDisplay::m_3DScene->Remove_Render_Object( m_line3D[ i ] );

		// delete line
		REF_PTR_RELEASE( m_line3D[ i ] );

	}

	delete [] m_line3D;
	// TheSuperHackers @fix Mauller 11/03/2025 Free reference counted material
	REF_PTR_RELEASE(m_texture);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Real W3DLaserDraw::getLaserTemplateWidth() const
{
	const W3DLaserDrawModuleData *data = getW3DLaserDrawModuleData();
	return data->m_outerBeamWidth * 0.5f;
}

// a module value above zero wins, then a GameData value above zero, else the beam-derived default
static Real pickGlowReal( Real moduleValue, Real globalValue, Real derivedValue )
{
	if (moduleValue > 0.0f)
	{
		return moduleValue;
	}
	if (globalValue > 0.0f)
	{
		return globalValue;
	}
	return derivedValue;
}

// a parsed color always carries alpha, so black means unset only once alpha is masked off
static Color pickGlowColor( Color moduleColor, Color globalColor )
{
	moduleColor &= 0x00FFFFFF;
	if (moduleColor != 0)
	{
		return moduleColor;
	}
	return globalColor & 0x00FFFFFF;
}

static const Real MIN_GROUND_LIGHT_RADIUS = 15.0f;

// widens every light past its sharp radius, which stretches the falloff over more vertices
static const Real GROUND_LIGHT_BLUR = 1.25f;

// the most extra falloff the blur may add
static const Real MAX_GROUND_LIGHT_BLUR = 6.0f;

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DLaserDraw::getGroundGlowColor( Real &red, Real &green, Real &blue ) const
{
	const W3DLaserDrawModuleData *data = getW3DLaserDrawModuleData();

	Color glowColor = pickGlowColor( data->m_groundGlowColor, TheGlobalData->m_laserGlowColor );
	if (glowColor != 0)
	{
		Real alpha;
		GameGetColorComponentsReal( glowColor, &red, &green, &blue, &alpha );
	}
	else
	{
		Real innerRed, innerGreen, innerBlue, innerAlpha;
		GameGetColorComponentsReal( data->m_innerColor, &innerRed, &innerGreen, &innerBlue, &innerAlpha );

		// the beam lines are additive, so the light they give off is every line's color weighed by its width
		red = green = blue = 0.0f;
		for( UnsignedInt i = 0; i < data->m_numBeams; i++ )
		{
			Real scale = data->m_numBeams > 1 ? i / ( data->m_numBeams - 1.0f ) : 0.0f;
			Real width = data->m_innerBeamWidth + scale * (data->m_outerBeamWidth - data->m_innerBeamWidth);
			red += width * (m_tintedInner.red + scale * (m_tintedOuter.red - m_tintedInner.red) * innerAlpha);
			green += width * (m_tintedInner.green + scale * (m_tintedOuter.green - m_tintedInner.green) * innerAlpha);
			blue += width * (m_tintedInner.blue + scale * (m_tintedOuter.blue - m_tintedInner.blue) * innerAlpha);
		}

		// textured beams often leave the ini colors white and carry the hue in the texture
		red *= m_textureColor.red;
		green *= m_textureColor.green;
		blue *= m_textureColor.blue;
	}

	// normalize so a dim color still lights at the same strength as a bright one
	Real brightest = MAX( red, MAX( green, blue ) );
	if (brightest > 0.0f)
	{
		red /= brightest;
		green /= brightest;
		blue /= brightest;
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DLaserDraw::setFullyObscuredByShroud( Bool fullyObscured )
{
	// the drawable stops drawing under shroud, so nothing else would let the lights go
	if (fullyObscured)
	{
		releaseGroundLights();
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DLaserDraw::acquireGroundLights( Int count )
{
	releaseGroundLights();

	if (W3DDisplay::m_3DScene == nullptr)
	{
		return;
	}

	// the pool hands out enabled lights with no decay, so they stay lit until we release them
	for( Int i = 0; i < count; i++ )
	{
		W3DDynamicLight *light = W3DDisplay::m_3DScene->getADynamicLight();
		light->setOwner( this );
		light->setTerrainOnly( true );
		light->Set_Ambient( Vector3( 0.0f, 0.0f, 0.0f ) );
		light->Set_Flag( LightClass::FAR_ATTENUATION, true );
		m_groundLights[ i ] = light;
	}
	m_numGroundLights = count;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DLaserDraw::releaseGroundLights()
{
	// disabling returns a light to the scene pool, which owns it; an expired light may already serve someone else
	for( Int i = 0; i < m_numGroundLights; i++ )
	{
		if (m_groundLights[ i ]->isOwnedBy( this ))
		{
			m_groundLights[ i ]->setEnabled( false );
		}
	}
	m_numGroundLights = 0;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DLaserDraw::updateGroundLights( LaserUpdate *update, Bool beamChanged )
{
	// the ground is lit from the first frame and goes dark the moment the beam starts to fade or decay
	if (!TheGlobalData->m_laserRef || !TheGlobalData->m_useDynamicLights || update->isEnding() || update->getAlphaScale() <= 0.0f || update->getWidthScale() <= 0.0f)
	{
		releaseGroundLights();
		return;
	}

	Bool stillOurs = m_numGroundLights > 0;
	for( Int i = 0; stillOurs && i < m_numGroundLights; i++ )
	{
		stillOurs = m_groundLights[ i ]->isOwnedBy( this );
	}

	// an unchanged beam only has to keep its lights alive
	if (!beamChanged && stillOurs)
	{
		for( Int i = 0; i < m_numGroundLights; i++ )
		{
			m_groundLights[ i ]->setFrameFade( 0, 3 );
		}
		return;
	}

	const W3DLaserDrawModuleData *data = getW3DLaserDrawModuleData();
	const Coord3D *beamStart = update->getStartPos();
	const Coord3D *beamEnd = update->getEndPos();
	Real dx = beamEnd->x - beamStart->x;
	Real dy = beamEnd->y - beamStart->y;
	Real beamLength = sqrt( dx * dx + dy * dy );

	// terrain lighting is per vertex on a 10 unit grid, so a radius under 1.5 cells lights scattered vertices
	Real wanted = data->m_groundGlowRadius > 0.0f ? data->m_groundGlowRadius : data->m_outerBeamWidth;
	Real spacing = MAX( wanted, MIN_GROUND_LIGHT_RADIUS );

	// centers one spacing apart so the linear falloffs sum to a level strip; the unscaled spacing keeps the count steady while the beam widens
	Int count = (Int)ceil( beamLength / spacing );
	count = MIN( MAX( count, 1 ), (Int)MAX_LASER_GROUND_LIGHTS );
	if (count != m_numGroundLights || !stillOurs)
	{
		acquireGroundLights( count );
	}

	// a beam longer than its lights can cover widens them until they meet again
	Real wantedRadius = spacing * update->getWidthScale();
	Real lightSpacing = beamLength / count;
	Real sharpRadius = MAX( MAX( wantedRadius, MIN_GROUND_LIGHT_RADIUS ), lightSpacing );
	Real radius = sharpRadius + MIN( sharpRadius * (GROUND_LIGHT_BLUR - 1.0f), MAX_GROUND_LIGHT_BLUR );

	{
		Real glowRed, glowGreen, glowBlue;
		getGroundGlowColor( glowRed, glowGreen, glowBlue );

		// diffuse only, so the hue survives being added onto sunlit ground
		Real intensity = pickGlowReal( data->m_groundGlowIntensity, TheGlobalData->m_laserGlowIntensity, 0.7f );
		// a light forced wider than wanted dims by as much, so the strip gives off the same light in total
		intensity *= wantedRadius / sharpRadius;
		// blurred lights overlap their neighbors, so each gives less to keep the middle of the strip level
		if (count > 1)
		{
			intensity *= MIN( lightSpacing / radius, 1.0f );
		}
		Vector3 lightColor( glowRed * intensity, glowGreen * intensity, glowBlue * intensity );
		// halfway up its radius the light still reaches the ground and meets nearby vertices at a steep angle
		Real lightHeight = radius * 0.5f;

		for( Int i = 0; i < m_numGroundLights; i++ )
		{
			Real t = (i + 0.5f) / m_numGroundLights;
			Real x = beamStart->x + dx * t;
			Real y = beamStart->y + dy * t;
			// a fixed height above the ground keeps the strip the same width whatever the beam height
			Real z = TheTerrainLogic->getGroundHeight( x, y ) + lightHeight;
			m_groundLights[ i ]->Set_Diffuse( lightColor );
			m_groundLights[ i ]->Set_Position( Vector3( x, y, z ) );
			m_groundLights[ i ]->Set_Far_Attenuation_Range( 0.5f, radius );
			// expires on its own a few frames after the last draw, so a beam that stops drawing takes its glow along
			m_groundLights[ i ]->setFrameFade( 0, 3 );
		}
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DLaserDraw::doDrawModule(const Matrix3D* transformMtx)
{
	//UnsignedInt currentFrame = TheGameClient->getFrame();
	const W3DLaserDrawModuleData *data = getW3DLaserDrawModuleData();

	//Get the updatemodule that drives it...
	Drawable *draw = getDrawable();
	static NameKeyType key_LaserUpdate = NAMEKEY( "LaserUpdate" );
	LaserUpdate *update = (LaserUpdate*)draw->findClientUpdateModule( key_LaserUpdate );
	if( !update )
	{
		DEBUG_CRASH( ("W3DLaserDraw::doDrawModule() expects its owner drawable %s to have a ClientUpdate = LaserUpdate module.", draw->getTemplate()->getName().str() ));
		return;
	}

	//If the update has moved the laser, it requires a reset of the laser.
	Bool beamChanged = update->isDirty() || m_selfDirty;
	if (beamChanged)
	{
		update->setDirty(false);

		m_hexColor = update->getPlayerColor();
		// DEBUG_LOG(("LaserDraw (doDrawModule 2): m_hexColor = %d\n", m_hexColor));


		// Texture animation
		float u_offset = 0;
		if (data->m_gridColumnsTotal > 1) {
			float prog = update->getLifeTimeProgress();
			int currentFrame = (int)(data->m_gridColumns * prog);
			if (currentFrame == data->m_gridColumns)
				currentFrame = data->m_gridColumns - 1;

			u_offset = (1.0f / (float)data->m_gridColumnsTotal) * (float)currentFrame;

			// We stay dirty if we want to animate the beam
			m_selfDirty = true;
		}
		else {
			m_selfDirty = false;
		}

		Vector3 laserPoints[ 2 ];


		//Get the color components for calculation purposes.
		Real innerRed, innerGreen, innerBlue, innerAlpha, outerRed, outerGreen, outerBlue, outerAlpha;
		GameGetColorComponentsReal(data->m_innerColor, &innerRed, &innerGreen, &innerBlue, &innerAlpha);
		GameGetColorComponentsReal(data->m_outerColor, &outerRed, &outerGreen, &outerBlue, &outerAlpha);

		bool updateColor = false;
		RGBColor houseColor;
		if ((data->m_useHouseColorInner || data->m_useHouseColorOuter)) {
			houseColor.setFromInt(m_hexColor);

			if (data->m_useHouseColorInner) {
				innerRed *= houseColor.red;
				innerGreen *= houseColor.green;
				innerBlue *= houseColor.blue;
			}
			if (data->m_useHouseColorOuter) {
				outerRed *= houseColor.red;
				outerGreen *= houseColor.green;
				outerBlue *= houseColor.blue;
			}

			updateColor = true;
		}
		m_tintedInner.red = innerRed;
		m_tintedInner.green = innerGreen;
		m_tintedInner.blue = innerBlue;
		m_tintedOuter.red = outerRed;
		m_tintedOuter.green = outerGreen;
		m_tintedOuter.blue = outerBlue;

		// DEBUG_LOG(("LaserDraw (doDrawModule): AppliedHousecolor: Inner RGB = %f, %f, %f -- Outer RGB = %f, %f, %f\n", innerRed, innerGreen, innerBlue, outerRed, outerGreen, outerBlue));


		for( UnsignedInt segment = 0; segment < data->m_segments; segment++ )
		{
			if( data->m_arcHeight > 0.0f && data->m_segments > 1 )
			{
				//CALCULATE A CURVED LINE BASED ON TOTAL LENGTH AND DESIRED HEIGHT INCREASE
				//To do this we will use a portion of the cos wave ranging between -0.25PI
				//and +0.25PI. 0PI is 1.0 and 0.25PI is 0.70 -- resulting in a somewhat
				//gentle curve depending on the line height and length. We also have to make
				//the line *level* for this phase of the calculations.

				//Get the desired direct line
				Coord3D lineStart, lineEnd, lineVector;
				lineStart.set( *update->getStartPos() );
				lineEnd.set( *update->getEndPos() );
				//This is critical -- in the case we have sloped lines (at the end, we'll fix it)
//				lineEnd.z = lineStart.z;

				//Get the length of the line
				lineVector.set( lineEnd );
				lineVector.sub( lineStart );
				Real lineLength = lineVector.length();

				//Get the middle point (we'll use this to determine how far we are from
				//that to calculate our height -- middle point is the highest).
				Coord3D lineMiddle;
				lineMiddle.set( lineStart );
				lineMiddle.add( lineEnd );
				lineMiddle.scale( 0.5 );

				//The half length is used to scale with the distance from middle to
				//get our cos( 0 to 0.25 PI) cos value
				Real halfLength = lineLength * 0.5f;

				//Now calculate which segment we will use.
				Real startSegmentRatio = segment / ((Real)data->m_segments);
				Real endSegmentRatio = (segment + 1.0f) / ((Real)data->m_segments);

				//Offset the segment ever-so-slightly to minimize overlap -- only apply
				//to segments that are not the start/end point
				if( segment > 0 )
				{
					startSegmentRatio -= data->m_segmentOverlapRatio;
				}
				if( segment < data->m_segments - 1 )
				{
					endSegmentRatio += data->m_segmentOverlapRatio;
				}

				//Calculate our start segment position on the *ground*.
				Coord3D segmentStart, segmentEnd, vector;
				vector.set( lineVector );
				vector.scale( startSegmentRatio );
				segmentStart.set( lineStart );
				segmentStart.add( vector );

				//Calculate our end segment position on the *ground*.
				vector.set( lineVector );
				vector.scale( endSegmentRatio );
				segmentEnd.set( lineStart );
				segmentEnd.add( vector );

				//--------------------------------------------------------------------------------
				//Now at this point, we have our segment line in the level positions that we want.
				//Calculate the raised height for the start/end segment positions using cosine.
				//--------------------------------------------------------------------------------

				//Calculate the distance from midpoint for the start positions.
				vector.set( lineMiddle );
				vector.sub( segmentStart );
				Real dist = vector.length();
				Real scaledRadians = dist / halfLength * PI * 0.5f;
				Real height = cos( scaledRadians );
				height *= data->m_arcHeight;
				segmentStart.z += height;

				//Now do the same thing for the end position.
				vector.set( lineMiddle );
				vector.sub( segmentEnd );
				dist = vector.length();
				scaledRadians = dist / halfLength * PI * 0.5f;
				height = cos( scaledRadians );
				height *= data->m_arcHeight;
				segmentEnd.z += height;

				//This makes the laser skim the ground rather than penetrate it!
				laserPoints[ 0 ].Set( segmentStart.x, segmentStart.y,
					MAX( segmentStart.z, 2.0f + TheTerrainLogic->getGroundHeight(segmentStart.x, segmentStart.y) ) );
				laserPoints[ 1 ].Set( segmentEnd.x, segmentEnd.y,
					MAX( segmentEnd.z, 2.0f + TheTerrainLogic->getGroundHeight(segmentEnd.x, segmentEnd.y) ) );

			}
			else
			{
				//No arc -- way simpler!
				laserPoints[ 0 ].Set( update->getStartPos()->x, update->getStartPos()->y, update->getStartPos()->z );
				laserPoints[ 1 ].Set( update->getEndPos()->x, update->getEndPos()->y, update->getEndPos()->z );
			}

			//Get the color components for calculation purposes.
			//Real innerRed, innerGreen, innerBlue, innerAlpha, outerRed, outerGreen, outerBlue, outerAlpha;
			//GameGetColorComponentsReal(data->m_innerColor, &innerRed, &innerGreen, &innerBlue, &innerAlpha);
			//GameGetColorComponentsReal(data->m_outerColor, &outerRed, &outerGreen, &outerBlue, &outerAlpha);

			for( Int i = data->m_numBeams - 1; i >= 0; i-- )
			{

				Real red, green, blue, alpha, width;
				int index = segment * data->m_numBeams + i;

				if( data->m_numBeams == 1 )
				{
					width = data->m_innerBeamWidth * update->getWidthScale();
					alpha = innerAlpha * update->getAlphaScale();

					if (updateColor) {
						red = innerRed * innerAlpha;
						green = innerGreen * innerAlpha;
						blue = innerBlue * innerAlpha;
					}
				}
				else
				{
					//Calculate the scale between min and max values
					//0 means use min value, 1 means use max value
					//0.2 means min value + 20% of the diff between min and max
					Real scale = i / ( data->m_numBeams - 1.0f);
					Real ultimateScale = update->getWidthScale();
					Real ultimateAlpha = update->getAlphaScale();
					width		= (data->m_innerBeamWidth	+ scale * (data->m_outerBeamWidth - data->m_innerBeamWidth));
					width *= ultimateScale;
					alpha		= innerAlpha							+ scale * (outerAlpha - innerAlpha);
					alpha *= ultimateAlpha;

					if (updateColor) {
						red = innerRed + scale * (outerRed - innerRed) * innerAlpha;
						green = innerGreen + scale * (outerGreen - innerGreen) * innerAlpha;
						blue = innerBlue + scale * (outerBlue - innerBlue) * innerAlpha;
					}
				}


				//Calculate the number of times to tile the line based on the height of the texture used.
				if( m_texture && data->m_tile )
				{
					//Calculate the length of the line.
					Vector3 lineVector;
					Vector3::Subtract( laserPoints[1], laserPoints[0], &lineVector );
					Real length = lineVector.Length();

					//Adjust tile factor so texture is NOT stretched but tiled equally in both width and length.
					Real tileFactor = length/width*m_textureAspectRatio*data->m_tilingScalar;

					//Set the tile factor
					m_line3D[ index ]->Set_Texture_Tile_Factor( tileFactor );	//number of times to tile texture across each segment
				}

				if (u_offset > 0) {
					Vector2 uvoffset = m_line3D[index]->Get_Current_UV_Offset();
					uvoffset.U = u_offset;
					m_line3D[index]->Set_Current_UV_Offset(uvoffset);
				}
				
				if (updateColor) {
					m_line3D[index]->Set_Color(Vector3(red, green, blue));
				}

				m_line3D[ index ]->Set_Width( width );
				m_line3D[ index ]->Set_Points( 2, &laserPoints[0] );


				//--
				//if ((data->m_useHouseColorInner || data->m_useHouseColorOuter)) { //&& m_hexColor > 0) {

				//	RGBColor myHouseColor;
				//	myHouseColor.setFromInt(m_hexColor);

				//	Vector3 color;
				//	m_line3D[index]->Get_Color(color);
				//	color.X *= myHouseColor.red;
				//	color.Y *= myHouseColor.green;
				//	color.Z *= myHouseColor.blue;

				//	m_line3D[index]->Set_Color(color);
				//}
			}
		}
	}

	// runs every draw, dirty or not, so the lights keep getting renewed while the beam is on screen
	updateGroundLights( update, beamChanged );
}


// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void W3DLaserDraw::crc( Xfer *xfer )
{

	// extend base class
	DrawModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void W3DLaserDraw::xfer( Xfer *xfer )
{

	// version
	const XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	DrawModule::xfer( xfer );

	// Kris says there is no data to save for these, go ask him.
	// m_selfDirty is not saved, is runtime only

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void W3DLaserDraw::loadPostProcess()
{

	// extend base class
	DrawModule::loadPostProcess();

	m_selfDirty = true;	// so we update the first time after reload

}
