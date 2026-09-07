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

// W3DParticleSys.cpp
// W3D Particle System implementation
// Author: Michael S. Booth, November 2001

#include "Common/GlobalData.h"
// TheSuperHackers @feature for ground morphing particle quads
#include "GameLogic/TerrainLogic.h"
#include "GameClient/Color.h"
#include "W3DDevice/GameClient/W3DParticleSys.h"
#include "W3DDevice/GameClient/W3DTerrainParticle.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/W3DSmudge.h"
#include "W3DDevice/GameClient/W3DSnow.h"
#include "WW3D2/camera.h"
#include "WW3D2/ww3d.h"
#include <algorithm>


//------------------------------------------------------------------------------ Performance Timers
//#include "Common/PerfMetrics.h"
//#include "Common/PerfTimer.h"

//-------------------------------------------------------------------------------------------------


// TheSuperHackers @feature Terrain height source handed to the point group renderer, which sits
// below the game and has no way to ask about terrain itself. Returns 0 before the map exists, which
// only happens outside a game, where there are no ground aligned particles to morph anyway.
static float getParticleGroundHeight(float x, float y)
{
	if (TheTerrainLogic == nullptr)
		return 0.0f;

	return TheTerrainLogic->getGroundHeight(x, y);
}

// one preset per blend mode, shared by every renderer a system can go through
static const ShaderClass &shaderForType(ParticleSystemInfo::ParticleShaderType type)
{
	switch (type)
	{
		case ParticleSystemInfo::ALPHA:
			return ShaderClass::_PresetAlphaSpriteShader;
		case ParticleSystemInfo::ALPHA_TEST:
			return ShaderClass::_PresetATestSpriteShader;
		case ParticleSystemInfo::MULTIPLY:
			return ShaderClass::_PresetMultiplicativeSpriteShader;
		default:
			return ShaderClass::_PresetAdditiveSpriteShader;
	}
}

W3DParticleSystemManager::W3DParticleSystemManager()
{
	PointGroupClass::Set_Ground_Height_Func(getParticleGroundHeight);

	m_batchBillboard = true;
	m_batchShaderType = ParticleSystemInfo::INVALID_SHADER;

	m_pointGroup = nullptr;
	m_terrainParticles = nullptr;
	m_streakLine = nullptr;
	m_posBuffer = nullptr;
	m_RGBABuffer = nullptr;
	m_sizeBuffer = nullptr;
	m_angleBuffer = nullptr;
	m_readyToRender = false;

	m_onScreenParticleCount = 0;

	m_pointGroup = NEW PointGroupClass();
	m_terrainParticles = NEW W3DTerrainParticle();
	//m_streakLine = nullptr;
	m_streakLine = NEW StreakLineClass();

	m_posBuffer = NEW_REF( ShareBufferClass<Vector3>, (MAX_POINTS_PER_GROUP, "W3DParticleSystemManager::m_posBuffer") );
	m_RGBABuffer = NEW_REF( ShareBufferClass<Vector4>, (MAX_POINTS_PER_GROUP, "W3DParticleSystemManager::m_RGBABuffer") );
	m_sizeBuffer = NEW_REF( ShareBufferClass<float>, (MAX_POINTS_PER_GROUP, "W3DParticleSystemManager::m_sizeBuffer") );
	m_angleBuffer = NEW_REF( ShareBufferClass<uint8>, (MAX_POINTS_PER_GROUP, "W3DParticleSystemManager::m_angleBuffer") );
}

W3DParticleSystemManager::~W3DParticleSystemManager()
{
	delete m_terrainParticles;
	m_terrainParticles = nullptr;

	delete m_pointGroup;

//	W3DDisplay::m_3DScene->Remove_Render_Object( m_streakLine );

	if (m_streakLine)
	{
		REF_PTR_RELEASE(m_streakLine);
	}

	REF_PTR_RELEASE(m_posBuffer);
	REF_PTR_RELEASE(m_RGBABuffer);
	REF_PTR_RELEASE(m_sizeBuffer);
	REF_PTR_RELEASE(m_angleBuffer);
}

/**
 * Hack because DoParticles is called from Flush(), which is called
 * multiple times per frame.  We only want to render once.
 * @todo Clean up the flag/Flush hack.
 */
void W3DParticleSystemManager::queueParticleRender()
{
	m_readyToRender = true;
}

/**
 * Nasty hack to render particles last. Called directly by WW3D::Flush()
 */
void DoParticles( RenderInfoClass &rinfo )
{
	if (TheParticleSystemManager)
		TheParticleSystemManager->doParticles(rinfo);
}

void W3DParticleSystemManager::doParticles(RenderInfoClass &rinfo)
{

	if (m_readyToRender == false)
		return;

	// external mechanism must tell us when it's OK to render again...
	m_readyToRender = false;

	//reset each frame
	/// @todo lorenzen sez: this should be debug only:
	m_onScreenParticleCount = 0;

 	const FrustumClass & frustum = rinfo.Camera.Get_Frustum();
	AABoxClass bbox;

	//Get a bounding box around our visible universe.  Bounded by terrain and the sky
	//so much tighter fitting volume than what's actually visible.  This will cull
	//particles falling under the ground.

 	TheTerrainRenderObject->getMaximumVisibleBox(frustum, &bbox, TRUE);

	//@todo lorenzen sez: put these in registers for sure
	Real bcX = bbox.Center.X;
	Real bcY = bbox.Center.Y;
	Real bcZ = bbox.Center.Z;
	Real beX = bbox.Extent.X;
	Real beY = bbox.Extent.Y;
	Real beZ = bbox.Extent.Z;

	unsigned int personalities[MAX_POINTS_PER_GROUP];


	m_fieldParticleCount = 0;

	const Bool drawSmudge = TheSmudgeManager && TheSmudgeManager->getHardwareSupport() && TheGlobalData->m_useHeatEffects;

	if (drawSmudge)
	{
		TheSmudgeManager->resetDraw();
	}

	// Number of particles/points being rendered.
	UnsignedInt pointCount = 0;

	const Bool batchParticles = TheGlobalData->m_batchParticles;
	// without the triangle sorter, draw order is the only depth cue, so order whole systems far to near
	const Bool backToFront = TheGlobalData->m_backToFront && !WW3D::Is_Sorting_Enabled();

	m_drawOrder.clear();

	ParticleSystemManager::ParticleSystemList &particleSysList = TheParticleSystemManager->getAllParticleSystems();
	for( ParticleSystemManager::ParticleSystemListIt it = particleSysList.begin(); it != particleSysList.end(); ++it)
	{
		ParticleSystem *sys = (*it);
		if (!sys) {
			continue;
		}

		// only look at particle/point style systems
		if (sys->isUsingDrawables())
			continue;

		// TheSuperHackers @performance Mauller 16/08/2026 Skip processing particle system if no particles are in view.
		UnsignedInt particleCount = 0;
		Vector3 visibleSum(0.0f, 0.0f, 0.0f);
		for (Particle* vp = sys->getFirstParticle(); vp; vp = vp->m_systemNext)
		{
			const Coord3D* pos = vp->getPosition();
			const Real psize = vp->getSize();

			//Test if particle is at the screen or terrain edges.
			if (WWMath::Fabs(pos->x - bcX) > (beX + psize) ||
				WWMath::Fabs(pos->y - bcY) > (beY + psize) ||
				WWMath::Fabs(pos->z - bcZ) > (beZ + psize))
			{
				vp->setIsCulled(true);
				continue;
			}

			vp->setIsCulled(false);
			particleCount++;
			visibleSum.X += pos->x;
			visibleSum.Y += pos->y;
			visibleSum.Z += pos->z;
		}

		// Particle system has no particles on screen
		if (particleCount == 0)
			continue;

		DrawEntry entry;
		entry.sys = sys;
		entry.depth = 0.0f;
		if (backToFront)
		{
			// only the view Z row matters, and the row's translation is the same for every system
			const Vector4 &viewZ = rinfo.Camera.Get_View_Matrix()[2];
			entry.depth = (viewZ.X * visibleSum.X + viewZ.Y * visibleSum.Y + viewZ.Z * visibleSum.Z) / particleCount;
		}
		m_drawOrder.push_back(entry);
	}

	if (backToFront)
	{
		std::stable_sort(m_drawOrder.begin(), m_drawOrder.end(), isFarther);
	}

	for (std::vector<DrawEntry>::iterator entry = m_drawOrder.begin(); entry != m_drawOrder.end(); ++entry)
	{
		ParticleSystem *sys = entry->sys;

		// Handle smudge type particles
		if (sys->isUsingSmudge())
		{
			if (!drawSmudge)
				continue;

			for (Particle *p = sys->getFirstParticle(); p; p = p->m_systemNext)
			{
				if (p->isCulled())
					continue;

				if (Smudge *smudge = TheSmudgeManager->findSmudge(p))
				{
					// The particle is in view. Draw the smudge!
					smudge->m_draw = true;
				}
			}
			continue;
		}

		// TheSuperHackers @performance Ronin/Mauller 09/08/2026 Implement batched rendering for similar particles.
		// Particles with the same properties will now be batched onto a single texture surface before being drawn.
		// If a different particle type appears before the batch is filled, the previous batch will be drawn first.
		RefCountPtr<TextureClass> texture;
		texture.Assign_No_Add_Ref(W3DDisplay::m_assetManager->Get_Texture(sys->getParticleTypeName().str()));

		// TheSuperHackers @feature A ground aligned particle drawn as a quad can only ever be a
		// flat plane, so a wide one cuts through a hillside no matter how its corners are placed.
		// This renderer instead builds a mesh from the terrain's own heightmap cells, the same way
		// projected decals do, so the particle inherits the ground geometry exactly.
		const Bool useTerrainConformingParticles = !sys->shouldBillboard() &&
				sys->getVolumeParticleDepth() == 0 &&
				sys->shouldConformToTerrain();

		const Bool canBatch = batchParticles && sys->isUsingParticles() && !useTerrainConformingParticles;
		if (!canBatch || finishedBatch(*sys, texture))
		{
			flushParticleBatch(rinfo, pointCount);
		}

		// the batch state always describes the system being filled, batched or not
		if (m_batchTexture == nullptr)
		{
			initializeBatch(*sys, texture);
		}

		UnsignedInt startCount = pointCount;

		// build W3D particle buffer
		Vector3 *posArray = m_posBuffer->Get_Array();
		Real *sizeArray = m_sizeBuffer->Get_Array();
		Vector4 *RGBAArray = m_RGBABuffer->Get_Array();
		uint8 *angleArray = m_angleBuffer->Get_Array();
		const Coord3D *pos;
		const RGBColor *color;
		Real psize;



		//set-up all the per-particle
		for (Particle *p = sys->getFirstParticle(); p; p = p->m_systemNext)
		{
			if (p->isCulled())
				continue;

			pos = p->getPosition();
			psize = p->getSize();

			m_fieldParticleCount += ( sys->getPriority() == AREA_EFFECT && sys->m_isGroundAligned != FALSE );

			//@todo lorenzen sez: use pointer arithmetic for these arrays
			personalities[pointCount] = p->getPersonality();

			posArray[pointCount].X = pos->x;
			posArray[pointCount].Y = pos->y;
			posArray[pointCount].Z = pos->z;

			sizeArray[pointCount] = psize;

			color = p->getColor();
			RGBAArray[pointCount].X = color->red;
			RGBAArray[pointCount].Y = color->green;
			RGBAArray[pointCount].Z = color->blue;
			RGBAArray[pointCount].W = p->getAlpha();

			angleArray[pointCount] = (uint8)(p->getAngle() * 255.0f / (2.0f * PI));

			if (++pointCount == MAX_POINTS_PER_GROUP)
			{
				if (!canBatch)
				{
					break;
				}

				// TheSuperHackers @info The Buffer is full mid-system so draw what we have and carry on with the SAME system.
				// This prevents particles being dropped. Bank the stats first as the flush resets count to 0.
				m_onScreenParticleCount += (pointCount - startCount);
				flushParticleBatch(rinfo, pointCount);
				initializeBatch(*sys, texture);
				startCount = 0;
			}
		}

		if (pointCount == startCount)
		{
			continue;	//this system has no particles to render
		}

		// Handle drawing streak type particles.
		if ( sys->isUsingStreak() && (pointCount >= 2) )
		{
			m_streakLine->Reset_Line();

			m_streakLine->Set_Texture( texture.Peek() );
			m_streakLine->Set_Shader( shaderForType( sys->getShaderType() ) );

			//UPDATE THE STREAK'S ARRAYS
			m_streakLine->Set_LocsWidthsColors(
				pointCount,
				m_posBuffer->Get_Array(),
				m_sizeBuffer->Get_Array(),
				m_RGBABuffer->Get_Array(),
				&personalities[0]
				);

			//WWASSERT( m_streakLine->Get_Num_Points() == pointCount );

			// This is the happy place for this!
			RGBAArray[0].X = 0;//eliminates the scissor edge on the trailing edge of the streak
			RGBAArray[0].Y = 0;
			RGBAArray[0].Z = 0;
			RGBAArray[0].W = 0;


			//RENDER STREAK!
			m_streakLine->Render( rinfo );
			m_onScreenParticleCount += (pointCount - startCount);
			pointCount = startCount;
		}

		if (useTerrainConformingParticles)
		{
			m_terrainParticles->Set_Texture( texture.Peek() );
			m_terrainParticles->Set_Shader( shaderForType( sys->getShaderType() ) );
			m_terrainParticles->Set_Arrays( m_posBuffer, m_RGBABuffer, m_sizeBuffer, m_angleBuffer, pointCount );

			m_terrainParticles->Render();
			m_onScreenParticleCount += (pointCount - startCount);
			pointCount = startCount;
		}

		// Handle volumetric type particle systems.
		const UnsignedInt volumeParticleDepth = sys->getVolumeParticleDepth();
		if( sys->isUsingVolumeParticles() && volumeParticleDepth > DEFAULT_VOLUME_PARTICLE_DEPTH )
		{
			m_pointGroup->Set_Texture( texture.Peek() );
			m_pointGroup->Set_Flag( PointGroupClass::TRANSFORM, true );	// transform to screen space
			m_pointGroup->Set_Shader( shaderForType( sys->getShaderType() ) );

			/// @todo Use both QUADS and TRIS for particles
			m_pointGroup->Set_Point_Mode( PointGroupClass::QUADS );
			m_pointGroup->Set_Arrays( m_posBuffer, m_RGBABuffer, nullptr, m_sizeBuffer, m_angleBuffer, nullptr, pointCount );
			m_pointGroup->Set_Billboard(sys->shouldBillboard());

			/// @todo Support animated texture particles
			/// @todo lorenzen sez: unimplemented code wastes cpu cycles
			m_pointGroup->Set_Point_Frame( 0 );

			m_pointGroup->RenderVolumeParticle( rinfo, volumeParticleDepth);
			m_onScreenParticleCount += (pointCount - startCount);
			pointCount = startCount;
		}

		// an unbatched system draws on its own right away; a batched one waits for the flush
		if (!canBatch)
		{
			m_onScreenParticleCount += (pointCount - startCount);
			flushParticleBatch(rinfo, pointCount);
			continue;
		}


		/// @todo lorenzen sez: this should be debug only:
		//add particle count to total
		m_onScreenParticleCount += (pointCount - startCount);

	/*
		// draw the wind vector for this particle system on the screen
		UnsignedInt width = TheDisplay->getWidth();
		UnsignedInt height = TheDisplay->getHeight();
		Coord3D worldStart, worldEnd;
		ICoord2D pixelStart, pixelEnd;
		sys->getPosition( &worldStart );
		worldEnd.x = Cos( sys->getWindAngle() ) * 50.0f + worldStart.x;
		worldEnd.y = Sin( sys->getWindAngle() ) * 50.0f + worldStart.y;
		worldEnd.z = worldStart.z;
		TheTacticalView->worldToScreen( &worldStart, &pixelStart );
		TheTacticalView->worldToScreen( &worldEnd, &pixelEnd );
		Color colorStart = GameMakeColor( 255, 255, 255, 255 );
		Color colorEnd = GameMakeColor( 255, 128, 128, 255 );
		TheDisplay->drawLine( pixelStart.x, pixelStart.y, pixelEnd.x, pixelEnd.y, 1.0f, colorStart, colorEnd );
	*/


	}

	// TheSuperHackers @info Flush the last batch if one is pending.
	flushParticleBatch(rinfo, pointCount);

		/// @todo lorenzen sez: this should be debug only:
	TheParticleSystemManager->setOnScreenParticleCount(m_onScreenParticleCount);

	//Draw any particles belonging to weather effects
	if (TheSnowManager)
		((W3DSnowManager *)TheSnowManager)->render(rinfo);

	//Now process screen smudges which are particles that distort the background behind them.
	if(TheSmudgeManager)
	{
		((W3DSmudgeManager *)TheSmudgeManager)->render(rinfo);
	}
}

// the camera looks down -Z, so the smallest view Z is the farthest system
Bool W3DParticleSystemManager::isFarther(const DrawEntry &a, const DrawEntry &b)
{
	return a.depth < b.depth;
}

Bool W3DParticleSystemManager::finishedBatch(const ParticleSystem& system, const RefCountPtr<TextureClass>& texture)
{
	return texture.Peek() != m_batchTexture.Peek() ||
		system.getShaderType() != m_batchShaderType ||
		system.shouldBillboard() != m_batchBillboard;
}

void W3DParticleSystemManager::initializeBatch(const ParticleSystem& system, const RefCountPtr<TextureClass>& texture)
{
	m_batchTexture = texture;
	m_batchShaderType = system.getShaderType();
	m_batchBillboard = system.shouldBillboard();
}

void W3DParticleSystemManager::flushParticleBatch(RenderInfoClass& rinfo, UnsignedInt& pointCount)
{
	if (pointCount > 0)
	{
		m_pointGroup->Set_Texture(m_batchTexture.Peek());
		m_pointGroup->Set_Shader(shaderForType(m_batchShaderType));
		m_pointGroup->Set_Flag(PointGroupClass::TRANSFORM, true);
		m_pointGroup->Set_Point_Mode(PointGroupClass::QUADS);
		m_pointGroup->Set_Arrays(m_posBuffer, m_RGBABuffer, nullptr, m_sizeBuffer, m_angleBuffer, nullptr, pointCount);
		m_pointGroup->Set_Billboard(m_batchBillboard);
		m_pointGroup->Set_Point_Frame(0);
		m_pointGroup->Render(rinfo);

		pointCount = 0;
	}

	m_batchTexture.Clear();
	m_batchBillboard = false;
	m_batchShaderType = ParticleSystemInfo::INVALID_SHADER;
}
