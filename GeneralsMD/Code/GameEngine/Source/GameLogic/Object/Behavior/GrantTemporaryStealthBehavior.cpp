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

// FILE: GrantTemporaryStealthBehavior.cpp ////////////////////////////////////////////////////////
// Desc:   Grants temporary stealth, either when the special power it is attached to fires or,
//         with ActivateOnCreate, when the object carrying it is created.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/SpecialPower.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/GrantTemporaryStealthBehavior.h"
#include "GameLogic/Module/StealthUpdate.h"

//-------------------------------------------------------------------------------------------------
GrantTemporaryStealthBehavior::GrantTemporaryStealthBehavior( Thing *thing, const ModuleData* moduleData ) : SpecialPowerUpdateModule( thing, moduleData )
{
	const GrantTemporaryStealthBehaviorModuleData *d = getGrantTemporaryStealthBehaviorModuleData();

	DEBUG_ASSERTCRASH( d->m_durationFrames > 0, ("GrantTemporaryStealthBehavior on '%s' has no Duration", getObject()->getTemplate()->getName().str()) );

	if( d->m_activateOnCreate )
	{
		setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
	}
	else
	{
		setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
	}
}

//-------------------------------------------------------------------------------------------------
GrantTemporaryStealthBehavior::~GrantTemporaryStealthBehavior()
{
}

//-------------------------------------------------------------------------------------------------
Bool GrantTemporaryStealthBehavior::initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions )
{
	const GrantTemporaryStealthBehaviorModuleData *d = getGrantTemporaryStealthBehaviorModuleData();

	if( d->m_activateOnCreate )
	{
		return FALSE;
	}

	if( d->m_specialPowerTemplate != nullptr && d->m_specialPowerTemplate != specialPowerTemplate )
	{
		return FALSE;
	}

	if( targetObj )
	{
		grantStealth( targetObj, targetObj->getPosition() );
	}
	else if( targetPos )
	{
		grantStealth( nullptr, targetPos );
	}
	else
	{
		grantStealth( getObject(), getObject()->getPosition() );
	}

	// Never consume the intent, the power's own update module still needs to see it.
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
void GrantTemporaryStealthBehavior::grantStealth( const Object *centerObj, const Coord3D *centerPos )
{
	const GrantTemporaryStealthBehaviorModuleData *d = getGrantTemporaryStealthBehaviorModuleData();

	if( d->m_radius <= 0.0f )
	{
		if( centerObj )
		{
			grantStealthToObject( centerObj );
		}
		return;
	}

	Object *self = getObject();
	PartitionFilterRelationship relationship( self, PartitionFilterRelationship::ALLOW_ALLIES );
	PartitionFilterSameMapStatus filterMapStatus( self );
	PartitionFilterAlive filterAlive;
	PartitionFilter *filters[] = { &relationship, &filterAlive, &filterMapStatus, nullptr };

	ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( centerPos, d->m_radius, FROM_CENTER_2D, filters );
	MemoryPoolObjectHolder hold( iter );
	for( Object *obj = iter->first(); obj; obj = iter->next() )
	{
		grantStealthToObject( obj );
	}
}

//-------------------------------------------------------------------------------------------------
void GrantTemporaryStealthBehavior::grantStealthToObject( const Object *obj )
{
	const GrantTemporaryStealthBehaviorModuleData *d = getGrantTemporaryStealthBehaviorModuleData();

	if( obj->isEffectivelyDead() )
	{
		return;
	}

	if( getObject()->getRelationship( obj ) != ALLIES )
	{
		return;
	}

	if( !obj->isAnyKindOf( d->m_kindOf ) || obj->isAnyKindOf( d->m_forbiddenKindOf ) )
	{
		return;
	}

	StealthUpdate *stealth = obj->getStealth();
	if( stealth )
	{
		stealth->receiveTemporaryGrant( d->m_durationFrames );
	}
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime GrantTemporaryStealthBehavior::update()
{
	// Only ActivateOnCreate ever wakes us, and only for this one grant.
	grantStealth( getObject(), getObject()->getPosition() );

	return UPDATE_SLEEP_FOREVER;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void GrantTemporaryStealthBehavior::crc( Xfer *xfer )
{

	// extend base class
	SpecialPowerUpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void GrantTemporaryStealthBehavior::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	SpecialPowerUpdateModule::xfer( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void GrantTemporaryStealthBehavior::loadPostProcess()
{

	// extend base class
	SpecialPowerUpdateModule::loadPostProcess();

}
