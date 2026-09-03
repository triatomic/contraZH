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
#include "GameLogic/AI.h"
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
	releaseAll();
}

//-------------------------------------------------------------------------------------------------
void TornadoUpdate::beginRampDown( void )
{
	if( m_rampingDown )
	{
		return;
	}

	UnsignedInt now = TheGameLogic->getFrame();
	m_rampDownStartStrength = computeStrength( now );
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

	// Keeps the locomotor from fighting us, and lets the victim leave the ground.
	physics->setStunned( TRUE );
	physics->setAllowToFall( TRUE );

	const Coord3D *pos = obj->getPosition();
	Coord3D toCenter;
	toCenter.x = center->x - pos->x;
	toCenter.y = center->y - pos->y;
	toCenter.z = 0.0f;

	Real distance = toCenter.length();
	Coord3D radial;
	radial.zero();
	Real pullScale = 1.0f;
	if( distance > 0.01f )
	{
		radial.x = toCenter.x / distance;
		radial.y = toCenter.y / distance;

		// Fade the inward pull near the axis, else a victim at the eye jitters across it.
		Real core = data->m_radius * TORNADO_CORE_FRACTION;
		if( core > 0.0f && distance < core )
		{
			pullScale = distance / core;
		}
	}

	Real lift = 0.0f;
	if( pos->z < groundZ + data->m_maxLiftHeight * strength )
	{
		lift = data->m_liftForce;
	}

	// A unit built to shrug off shockwaves shrugs off the tornado by the same amount.
	Real resistance = 1.0f - __min( 1.0f, __max( 0.0f, physics->getShockResistance() ) );
	Real scale = strength * resistance;

	Coord3D force;
	force.x = ( data->m_pullForce * pullScale * radial.x - data->m_spinForce * radial.y ) * scale;
	force.y = ( data->m_pullForce * pullScale * radial.y + data->m_spinForce * radial.x ) * scale;
	force.z = lift * scale;

	// applyForce would keep only the sideways part of the pull on anything that is driving.
	physics->applyMotiveForce( &force );

	Real massScale = 1.0f;
	if( data->m_massReference > 0.0f )
	{
		massScale = __min( 1.0f, data->m_massReference / physics->getMass() );
	}
	physics->setYawRate( data->m_yawRate * strength * massScale );

	if( data->m_maxVictimSpeed > 0.0f )
	{
		// Both scrub calls only ever reduce speed, and scrubVelocityZ is signed, so cap each way.
		physics->scrubVelocity2D( data->m_maxVictimSpeed );
		physics->scrubVelocityZ( data->m_maxVictimSpeed );
		physics->scrubVelocityZ( -data->m_maxVictimSpeed );
	}
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
			if( canAffect( obj ) )
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
			if( !canAffect( obj ) )
			{
				continue;
			}
			if( obj->getPhysics() == nullptr )
			{
				continue;
			}
			if( obj->isKindOf( KINDOF_IMMOBILE ) || obj->isKindOf( KINDOF_STRUCTURE ) || obj->isKindOf( KINDOF_PROJECTILE ) )
			{
				continue;
			}
			if( obj->getContainedBy() != nullptr )
			{
				continue;
			}
			if( obj->isDisabledByType( DISABLED_HELD ) )
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
