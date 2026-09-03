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

// FILE: TornadoUpdate.cpp ///////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_DEATH_NAMES

#include "Common/Xfer.h"
#include "Common/GlobalData.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/TornadoUpdate.h"

#include <algorithm>

// Inside this fraction of the radius the pull fades out, so a victim at the eye does not jitter.
static const Real TORNADO_CORE_FRACTION = 0.1f;

// How hard a victim is steered toward the climb rate the tornado wants for it.
static const Real TORNADO_HOVER_DAMPING = 0.35f;

// Frames spent easing into the ceiling, so victims settle instead of slamming into it.
static const Real TORNADO_APPROACH_FRAMES = 12.0f;

// How far a victim is lifted clear of the ground when it is first grabbed.
static const Real TORNADO_LIFTOFF_HEIGHT = 2.0f;

// How long a tornado with no timer, no lifetime and no controller runs before ending itself.
static const UnsignedInt TORNADO_ORPHAN_FRAMES = 30 * LOGICFRAMES_PER_SECOND;

//-------------------------------------------------------------------------------------------------
TornadoUpdateModuleData::TornadoUpdateModuleData()
{
	m_radius = 0.0f;
	m_pullForce = 0.0f;
	m_liftForce = 0.0f;
	m_spinForce = 0.0f;
	m_yawRate = 0.0f;
	m_maxLiftHeight = 0.0f;
	m_maxVictimSpeed = 0.0f;
	m_massReference = 0.0f;
	m_releaseSpeed = 0.0f;
	m_requiredKindOf.clear();
	m_forbiddenKindOf.clear();
	m_targetsMask = WEAPON_AFFECTS_ALLIES | WEAPON_AFFECTS_ENEMIES | WEAPON_AFFECTS_NEUTRALS;
	m_affectAirborne = FALSE;
	m_rampUpFrames = 0;
	m_fullStrengthFrames = 0;
	m_rampDownFrames = 0;
	m_damagePerSecond = 0.0f;
	m_damageRadius = 0.0f;
	m_damagePulseFrames = 0;
	m_damageType = DAMAGE_EXPLOSION;
	m_deathType = DEATH_EXPLODED;
	m_killObjectWhenDone = FALSE;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void TornadoUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse( p );

	static const FieldParse dataFieldParse[] =
	{
		{ "Radius",				INI::parseReal,							nullptr,					offsetof( TornadoUpdateModuleData, m_radius ) },
		{ "PullForce",			INI::parseReal,							nullptr,					offsetof( TornadoUpdateModuleData, m_pullForce ) },
		{ "LiftForce",			INI::parseReal,							nullptr,					offsetof( TornadoUpdateModuleData, m_liftForce ) },
		{ "SpinForce",			INI::parseReal,							nullptr,					offsetof( TornadoUpdateModuleData, m_spinForce ) },
		{ "YawRate",			INI::parseAngularVelocityReal,			nullptr,					offsetof( TornadoUpdateModuleData, m_yawRate ) },
		{ "MaxLiftHeight",		INI::parseReal,							nullptr,					offsetof( TornadoUpdateModuleData, m_maxLiftHeight ) },
		{ "MaxVictimSpeed",		INI::parseReal,							nullptr,					offsetof( TornadoUpdateModuleData, m_maxVictimSpeed ) },
		{ "MassReference",		INI::parseReal,							nullptr,					offsetof( TornadoUpdateModuleData, m_massReference ) },
		{ "ReleaseSpeed",		INI::parseReal,							nullptr,					offsetof( TornadoUpdateModuleData, m_releaseSpeed ) },
		{ "RequiredKindOf",		KindOfMaskType::parseFromINI,			nullptr,					offsetof( TornadoUpdateModuleData, m_requiredKindOf ) },
		{ "ForbiddenKindOf",	KindOfMaskType::parseFromINI,			nullptr,					offsetof( TornadoUpdateModuleData, m_forbiddenKindOf ) },
		{ "AffectsTargets",		INI::parseBitString32,					TheWeaponAffectsMaskNames,	offsetof( TornadoUpdateModuleData, m_targetsMask ) },
		{ "AffectAirborne",		INI::parseBool,							nullptr,					offsetof( TornadoUpdateModuleData, m_affectAirborne ) },
		{ "RampUpTime",			INI::parseDurationUnsignedInt,			nullptr,					offsetof( TornadoUpdateModuleData, m_rampUpFrames ) },
		{ "FullStrengthTime",	INI::parseDurationUnsignedInt,			nullptr,					offsetof( TornadoUpdateModuleData, m_fullStrengthFrames ) },
		{ "RampDownTime",		INI::parseDurationUnsignedInt,			nullptr,					offsetof( TornadoUpdateModuleData, m_rampDownFrames ) },
		{ "DamagePerSecond",	INI::parseReal,							nullptr,					offsetof( TornadoUpdateModuleData, m_damagePerSecond ) },
		{ "DamageRadius",		INI::parseReal,							nullptr,					offsetof( TornadoUpdateModuleData, m_damageRadius ) },
		{ "DamagePulseDelay",	INI::parseDurationUnsignedInt,			nullptr,					offsetof( TornadoUpdateModuleData, m_damagePulseFrames ) },
		{ "DamageType",			DamageTypeFlags::parseSingleBitFromINI,	nullptr,					offsetof( TornadoUpdateModuleData, m_damageType ) },
		{ "DeathType",			INI::parseIndexList,					TheDeathNames,				offsetof( TornadoUpdateModuleData, m_deathType ) },
		{ "KillObjectWhenDone",	INI::parseBool,							nullptr,					offsetof( TornadoUpdateModuleData, m_killObjectWhenDone ) },
		{ 0, 0, 0, 0 }
	};
	p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
TornadoUpdate::TornadoUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_victims.clear();
	m_startFrame = 0;
	m_rampDownStartFrame = 0;
	m_rampDownStartStrength = 0.0f;
	m_nextDamagePulseFrame = 0;
	m_started = FALSE;
	m_rampingDown = FALSE;
	m_done = FALSE;

	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
TornadoUpdate::~TornadoUpdate( void )
{
	// The engine may already be tearing down, in which case there is nothing left to release.
	if( TheGameLogic != nullptr )
	{
		releaseAll();
	}
}

//-------------------------------------------------------------------------------------------------
void TornadoUpdate::beginRampDown( void )
{
	if( m_rampingDown )
	{
		return;
	}

	UnsignedInt now = TheGameLogic->getFrame();

	// A controller can end us before our first update, when there is no start frame to measure
	// from yet. Treat that as a tornado that never got going rather than one at full strength.
	m_rampDownStartStrength = m_started ? computeStrength( now ) : 0.0f;
	m_rampDownStartFrame = now;
	m_rampingDown = TRUE;
}

//-------------------------------------------------------------------------------------------------
/** The weak to strong to weak envelope, scaling forces, spin and damage. */
//-------------------------------------------------------------------------------------------------
Real TornadoUpdate::computeStrength( UnsignedInt now ) const
{
	const TornadoUpdateModuleData *data = getTornadoUpdateModuleData();

	if( m_rampingDown )
	{
		if( data->m_rampDownFrames == 0 )
		{
			return 0.0f;
		}
		Real fade = 1.0f - (Real)(now - m_rampDownStartFrame) / (Real)data->m_rampDownFrames;
		if( fade <= 0.0f )
		{
			return 0.0f;
		}
		return m_rampDownStartStrength * fade;
	}

	if( data->m_rampUpFrames == 0 )
	{
		return 1.0f;
	}

	UnsignedInt elapsed = now - m_startFrame;
	if( elapsed >= data->m_rampUpFrames )
	{
		return 1.0f;
	}
	return (Real)elapsed / (Real)data->m_rampUpFrames;
}

//-------------------------------------------------------------------------------------------------
Int TornadoUpdate::buildRelationshipFlags( void ) const
{
	const TornadoUpdateModuleData *data = getTornadoUpdateModuleData();

	Int targetFlags = 0;
	if( data->m_targetsMask & WEAPON_AFFECTS_ALLIES )
	{
		targetFlags |= PartitionFilterRelationship::ALLOW_ALLIES;
	}
	if( data->m_targetsMask & WEAPON_AFFECTS_ENEMIES )
	{
		targetFlags |= PartitionFilterRelationship::ALLOW_ENEMIES;
	}
	if( data->m_targetsMask & WEAPON_AFFECTS_NEUTRALS )
	{
		targetFlags |= PartitionFilterRelationship::ALLOW_NEUTRAL;
	}
	return targetFlags;
}

//-------------------------------------------------------------------------------------------------
/** The KindOf test that both the pull and the damage share. */
//-------------------------------------------------------------------------------------------------
Bool TornadoUpdate::canAffect( const Object *obj ) const
{
	const TornadoUpdateModuleData *data = getTornadoUpdateModuleData();

	if( obj == getObject() )
	{
		return FALSE;
	}
	if( data->m_requiredKindOf.any() && !obj->isAnyKindOf( data->m_requiredKindOf ) )
	{
		return FALSE;
	}
	if( obj->isAnyKindOf( data->m_forbiddenKindOf ) )
	{
		return FALSE;
	}
	if( !data->m_affectAirborne && obj->isAirborneTarget() )
	{
		return FALSE;
	}
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Is something other than this module going to end the object? A controller ramps us down by
	* hand, and a lifetime module kills the object outright; either way we need no fallback. */
//-------------------------------------------------------------------------------------------------
Bool TornadoUpdate::hasExternalLifetime( void ) const
{
	static NameKeyType key_LifetimeUpdate = NAMEKEY( "LifetimeUpdate" );
	return getObject()->findUpdateModule( key_LifetimeUpdate ) != nullptr;
}

//-------------------------------------------------------------------------------------------------
/** The rest of the victim test: what the tornado can physically take hold of. Damage uses this
	* too, so that nothing takes tornado damage without being a candidate for the pull. */
//-------------------------------------------------------------------------------------------------
Bool TornadoUpdate::canGrab( const Object *obj ) const
{
	if( obj->getPhysics() == nullptr )
	{
		return FALSE;
	}
	if( obj->isKindOf( KINDOF_IMMOBILE ) || obj->isKindOf( KINDOF_STRUCTURE ) || obj->isKindOf( KINDOF_PROJECTILE ) )
	{
		return FALSE;
	}
	if( obj->getContainedBy() != nullptr )
	{
		return FALSE;
	}
	if( obj->isDisabledByType( DISABLED_HELD ) )
	{
		return FALSE;
	}
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Break a victim loose from whatever it was doing, once, as it is grabbed. */
//-------------------------------------------------------------------------------------------------
void TornadoUpdate::captureVictim( Object *obj )
{
	AIUpdateInterface *ai = obj->getAIUpdateInterface();
	if( ai )
	{
		ai->aiIdle( CMD_FROM_AI );
	}

	PhysicsBehavior *physics = obj->getPhysics();
	if( physics == nullptr )
	{
		return;
	}

	physics->clearAcceleration();
	physics->setAllowBouncing( TRUE );

	// Physics runs before this module and kills upward velocity while an object is on the
	// ground, so lift alone never gets a victim airborne. Break it loose by hand, once, but
	// only into space the victim already occupies - setPosition does no collision test.
	physics->setStickToGround( FALSE );
	Coord3D liftOff = *obj->getPosition();
	Real headroom = obj->getGeometryInfo().getMaxHeightAbovePosition();
	Real step = __min( TORNADO_LIFTOFF_HEIGHT, headroom );
	if( step > 0.0f )
	{
		liftOff.z += step;
		obj->setPosition( &liftOff );

		// A ground unit owns pathfind cells; leave them behind or others path around bare ground.
		TheAI->pathfinder()->updatePos( obj, &liftOff );
	}

	// A braking object does not integrate its horizontal velocity, so it could never be dragged in.
	obj->clearStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_BRAKING ) );

	obj->setModelConditionState( MODELCONDITION_STUNNED_FLAILING );
}

//-------------------------------------------------------------------------------------------------
/** Spin and drag one victim for a frame. Physics undoes most of this every frame, so it is all
	* re-applied rather than set once. */
//-------------------------------------------------------------------------------------------------
void TornadoUpdate::holdVictim( Object *obj, const Coord3D *center, Real groundZ, Real strength )
{
	const TornadoUpdateModuleData *data = getTornadoUpdateModuleData();
	PhysicsBehavior *physics = obj->getPhysics();
	if( physics == nullptr )
	{
		return;
	}

	// Keeps the locomotor from fighting us, and lets the victim leave the ground. The locomotor
	// re-arms stickToGround every time it runs, so clear it again every frame.
	physics->setStunned( TRUE );
	physics->setAllowToFall( TRUE );
	physics->setStickToGround( FALSE );

	const Coord3D *pos = obj->getPosition();
	Coord3D toCenter;
	toCenter.x = center->x - pos->x;
	toCenter.y = center->y - pos->y;
	toCenter.z = 0.0f;

	Real distance = toCenter.length();
	Coord3D radial;
	Real pullScale = 1.0f;
	if( distance > 0.01f )
	{
		radial.x = toCenter.x / distance;
		radial.y = toCenter.y / distance;
	}
	else
	{
		// Dead on the axis there is no radial direction to derive a tangent from, and a zero
		// tangent means the controller would brake the victim to a halt and pin it at the eye.
		// Any direction will do, so pick one off the victim's own facing and keep it orbiting.
		const Coord3D *dir = obj->getUnitDirectionVector2D();
		radial.x = dir->x;
		radial.y = dir->y;
	}
	radial.z = 0.0f;

	// Inside the core the pull reverses and pushes back out, so victims settle into a ring and
	// orbit there instead of all collapsing onto the axis and stacking up in one spot. The
	// tangential term is left at full strength throughout so the orbit never stalls.
	Real core = data->m_radius * TORNADO_CORE_FRACTION;
	if( core > 0.0f && distance < core )
	{
		pullScale = ( distance / core ) * 2.0f - 1.0f;
	}

	// Lift is flown as a target climb rate, not a raw push, else the victim accelerates for as
	// long as it is under the ceiling and shoots through it. Every term is multiplied by mass
	// because applyMotiveForce divides it straight back out, which is what lets a heavy tank
	// ride at the same height as an infantryman.
	Real mass = physics->getMass();
	Real ceiling = groundZ + data->m_maxLiftHeight * strength;
	Real wantedRise = 0.0f;
	if( pos->z < ceiling )
	{
		wantedRise = data->m_liftForce * strength;
		if( data->m_maxVictimSpeed > 0.0f && wantedRise > data->m_maxVictimSpeed )
		{
			wantedRise = data->m_maxVictimSpeed;
		}

		// Ease off over the last stretch so the victim arrives already slow.
		Real remaining = ceiling - pos->z;
		if( remaining < wantedRise * TORNADO_APPROACH_FRAMES )
		{
			wantedRise = remaining / TORNADO_APPROACH_FRAMES;
		}
	}

	// A victim that overshoots the ceiling needs a downward correction, so the damping term is
	// allowed to go negative. Zero is the floor only for a victim still climbing; above the
	// ceiling we stop short of cancelling gravity so it can actually sink back.
	Real hold = -TheGlobalData->m_gravity * mass;
	Real lift = hold + ( wantedRise - physics->getVelocity()->z ) * mass * TORNADO_HOVER_DAMPING;
	if( lift < 0.0f )
	{
		lift = 0.0f;
	}

	// A unit built to shrug off shockwaves shrugs off the tornado by the same amount.
	Real resistance = 1.0f - __min( 1.0f, __max( 0.0f, physics->getShockResistance() ) );
	Real scale = strength * resistance;

	// The orbit is flown as a target velocity too. Once a victim is off the ground the engine
	// switches it to air drag, which grows with speed and would otherwise cancel a fixed push
	// and leave the victim hanging still under the ceiling.
	Coord3D wantedVel;
	wantedVel.x = ( data->m_pullForce * pullScale * radial.x - data->m_spinForce * radial.y ) * scale;
	wantedVel.y = ( data->m_pullForce * pullScale * radial.y + data->m_spinForce * radial.x ) * scale;

	// Limit what we ask for, rather than clamping the victim after the fact.
	if( data->m_maxVictimSpeed > 0.0f )
	{
		Real wantedSpeed = sqrtf( sqr( wantedVel.x ) + sqr( wantedVel.y ) );
		if( wantedSpeed > data->m_maxVictimSpeed )
		{
			Real trim = data->m_maxVictimSpeed / wantedSpeed;
			wantedVel.x *= trim;
			wantedVel.y *= trim;
		}
	}

	const Coord3D *vel = physics->getVelocity();
	Coord3D force;
	force.x = ( wantedVel.x - vel->x ) * mass * TORNADO_HOVER_DAMPING;
	force.y = ( wantedVel.y - vel->y ) * mass * TORNADO_HOVER_DAMPING;
	force.z = lift * scale;

	// applyForce would keep only the sideways part of the pull on anything that is driving.
	physics->applyMotiveForce( &force );

	// Held victims spin at the full rate whatever they weigh; MassReference only slows the
	// spin-up of heavy things while the tornado is still building.
	Real massScale = 1.0f;
	if( data->m_massReference > 0.0f && strength < 1.0f )
	{
		massScale = __min( 1.0f, data->m_massReference / mass );
		massScale += ( 1.0f - massScale ) * strength;
	}
	physics->setYawRate( data->m_yawRate * massScale );

	// No velocity scrubbing here on purpose. The orbit and climb are already flown as target
	// speeds, so a scrub would chop the velocity back every frame right after the force built
	// it up, and the victim would jerk around the tornado instead of gliding. MaxVictimSpeed is
	// applied to the target above, which is where a limit belongs.

	// Physics drops the stun again the moment a victim is slow or low, and a cleared stun hands
	// the unit back to its locomotor, which lands it.
	physics->setStunned( TRUE );
}

//-------------------------------------------------------------------------------------------------
/** Let go. Physics clears the stun and fall flags by itself on landing and awards the falling
	* damage, which only counts a steep enough descent - hence scrubbing the horizontal speed. */
//-------------------------------------------------------------------------------------------------
void TornadoUpdate::releaseVictim( Object *obj )
{
	PhysicsBehavior *physics = obj->getPhysics();
	if( physics == nullptr )
	{
		return;
	}

	physics->setYawRate( 0.0f );
	physics->scrubVelocity2D( getTornadoUpdateModuleData()->m_releaseSpeed );

	// We cleared this to lift the victim; an idle unit never runs the locomotor that re-arms it.
	physics->setStickToGround( TRUE );
}

//-------------------------------------------------------------------------------------------------
void TornadoUpdate::releaseAll( void )
{
	for( ObjectIDVectorIterator it = m_victims.begin(); it != m_victims.end(); ++it )
	{
		Object *obj = TheGameLogic->findObjectByID( *it );
		if( obj && !obj->isEffectivelyDead() )
		{
			releaseVictim( obj );
		}
	}
	m_victims.clear();
}

//-------------------------------------------------------------------------------------------------
void TornadoUpdate::doDamagePulse( const Coord3D *center, Real strength )
{
	const TornadoUpdateModuleData *data = getTornadoUpdateModuleData();
	Object *me = getObject();

	Real pulseSeconds = (Real)data->m_damagePulseFrames / LOGICFRAMES_PER_SECONDS_REAL;

	DamageInfo damageInfo;
	damageInfo.in.m_amount = data->m_damagePerSecond * strength * pulseSeconds;
	damageInfo.in.m_sourceID = me->getID();
	damageInfo.in.m_damageType = data->m_damageType;
	damageInfo.in.m_deathType = data->m_deathType;

	Real radius = ( data->m_damageRadius > 0.0f ) ? data->m_damageRadius : data->m_radius;

	PartitionFilterRelationship relationship( me, buildRelationshipFlags() );
	PartitionFilterSameMapStatus filterMapStatus( me );
	PartitionFilterAlive filterAlive;
	PartitionFilter *filters[] = { &relationship, &filterAlive, &filterMapStatus, nullptr };

	// Gather first, since damage can kill, and killing while walking the partition is not safe.
	ObjectIDVector targets;
	{
		ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( center, radius, FROM_CENTER_2D, filters );
		MemoryPoolObjectHolder hold( iter );
		for( Object *obj = iter->first(); obj; obj = iter->next() )
		{
			if( canAffect( obj ) && canGrab( obj ) )
			{
				targets.push_back( obj->getID() );
			}
		}
	}

	for( ObjectIDVectorIterator it = targets.begin(); it != targets.end(); ++it )
	{
		Object *obj = TheGameLogic->findObjectByID( *it );
		if( obj == nullptr )
		{
			continue;
		}
		BodyModuleInterface *body = obj->getBodyModule();
		if( body )
		{
			body->attemptDamage( &damageInfo );
		}
	}
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime TornadoUpdate::update( void )
{
	if( m_done )
	{
		return UPDATE_SLEEP_FOREVER;
	}

	const TornadoUpdateModuleData *data = getTornadoUpdateModuleData();
	Object *me = getObject();
	UnsignedInt now = TheGameLogic->getFrame();

	if( !m_started )
	{
		m_started = TRUE;
		m_startFrame = now;
		m_nextDamagePulseFrame = now;
	}

	if( me->isEffectivelyDead() )
	{
		releaseAll();
		m_done = TRUE;
		return UPDATE_SLEEP_FOREVER;
	}

	if( data->m_fullStrengthFrames > 0 && now >= m_startFrame + data->m_rampUpFrames + data->m_fullStrengthFrames )
	{
		beginRampDown();
	}
	else if( data->m_fullStrengthFrames == 0 && !m_rampingDown && !hasExternalLifetime() )
	{
		// FullStrengthTime 0 means "until something else ends us". If nothing can - no lifetime
		// module and no controller driving us - fall back to the ramp up time so we still stop.
		UnsignedInt endless = m_startFrame + data->m_rampUpFrames + TORNADO_ORPHAN_FRAMES;
		if( now >= endless )
		{
			beginRampDown();
		}
	}

	Real strength = computeStrength( now );
	if( m_rampingDown && strength <= 0.0f )
	{
		releaseAll();
		m_done = TRUE;
		if( data->m_killObjectWhenDone )
		{
			TheGameLogic->destroyObject( me );
		}
		return UPDATE_SLEEP_FOREVER;
	}

	Coord3D center = *me->getPosition();
	Real groundZ = TheTerrainLogic->getGroundHeight( center.x, center.y );

	// Gather first, so that touching a victim cannot disturb the query we are walking.
	ObjectIDVector inRange;
	{
		PartitionFilterRelationship relationship( me, buildRelationshipFlags() );
		PartitionFilterSameMapStatus filterMapStatus( me );
		PartitionFilterAlive filterAlive;
		PartitionFilter *filters[] = { &relationship, &filterAlive, &filterMapStatus, nullptr };

		ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( &center, data->m_radius, FROM_CENTER_2D, filters );
		MemoryPoolObjectHolder hold( iter );
		for( Object *obj = iter->first(); obj; obj = iter->next() )
		{
			if( !canAffect( obj ) || !canGrab( obj ) )
			{
				continue;
			}
			inRange.push_back( obj->getID() );
		}
	}

	// Sorted, so that comparing against last frame does not depend on the partition order.
	std::sort( inRange.begin(), inRange.end() );

	for( ObjectIDVectorIterator it = m_victims.begin(); it != m_victims.end(); ++it )
	{
		if( std::binary_search( inRange.begin(), inRange.end(), *it ) )
		{
			continue;
		}
		Object *obj = TheGameLogic->findObjectByID( *it );
		if( obj && !obj->isEffectivelyDead() )
		{
			releaseVictim( obj );
		}
	}

	for( ObjectIDVectorIterator it = inRange.begin(); it != inRange.end(); ++it )
	{
		Object *obj = TheGameLogic->findObjectByID( *it );
		if( obj == nullptr )
		{
			continue;
		}
		if( !std::binary_search( m_victims.begin(), m_victims.end(), *it ) )
		{
			captureVictim( obj );
		}
		holdVictim( obj, &center, groundZ, strength );
	}

	m_victims.swap( inRange );

	if( data->m_damagePulseFrames > 0 && data->m_damagePerSecond > 0.0f && m_nextDamagePulseFrame <= now )
	{
		doDamagePulse( &center, strength );
		m_nextDamagePulseFrame = now + data->m_damagePulseFrames;
	}

	return UPDATE_SLEEP_NONE;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void TornadoUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}  // end crc

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void TornadoUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// victims
	Int vectorSize = m_victims.size();
	xfer->xferInt( &vectorSize );
	m_victims.resize( vectorSize );
	for( Int vectorIndex = 0; vectorIndex < vectorSize; ++vectorIndex )
	{
		xfer->xferObjectID( &m_victims[vectorIndex] );
	}

	xfer->xferUnsignedInt( &m_startFrame );
	xfer->xferUnsignedInt( &m_rampDownStartFrame );
	xfer->xferReal( &m_rampDownStartStrength );
	xfer->xferUnsignedInt( &m_nextDamagePulseFrame );
	xfer->xferBool( &m_started );
	xfer->xferBool( &m_rampingDown );
	xfer->xferBool( &m_done );

}  // end xfer

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void TornadoUpdate::loadPostProcess( void )
{

	// extend base class
	UpdateModule::loadPostProcess();

}  // end loadPostProcess
