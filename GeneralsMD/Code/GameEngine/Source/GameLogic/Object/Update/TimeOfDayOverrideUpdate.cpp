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

// FILE: TimeOfDayOverrideUpdate.cpp //////////////////////////////////////////////////////////////
// Desc:   Switches the world time of day, either when the superweapon it is attached to fires or,
//         with ActivateOnCreate, for as long as the object carrying it lives.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GlobalData.h"
#include "Common/SpecialPower.h"
#include "Common/Xfer.h"
#include "GameClient/GameClient.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/TimeOfDayOverrideUpdate.h"

//-------------------------------------------------------------------------------------------------
TimeOfDayOverrideUpdate::TimeOfDayOverrideUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialPowerUpdateModule( thing, moduleData )
{
	m_activeTimeOfDay = TIME_OF_DAY_INVALID;
	m_originalTimeOfDay = TIME_OF_DAY_INVALID;
	m_revertFrame = 0;

	// An object that brings the night with it has to wake once to do the switch.
	if( getTimeOfDayOverrideUpdateModuleData()->m_activateOnCreate )
	{
		setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
	}
	else
	{
		setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
	}
}

//-------------------------------------------------------------------------------------------------
TimeOfDayOverrideUpdate::~TimeOfDayOverrideUpdate()
{
}

//-------------------------------------------------------------------------------------------------
Bool TimeOfDayOverrideUpdate::initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions )
{
	const TimeOfDayOverrideUpdateModuleData* d = getTimeOfDayOverrideUpdateModuleData();

	if( d->m_activateOnCreate )
	{
		// We run off our own lifetime, so a power firing is none of our business.
		return FALSE;
	}

	if( d->m_specialPowerTemplate != nullptr && d->m_specialPowerTemplate != specialPowerTemplate )
	{
		return FALSE;
	}

	activateOverride();

	// Never consume the intent, the superweapon this module rides on still needs to see it.
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
void TimeOfDayOverrideUpdate::activateOverride()
{
	const TimeOfDayOverrideUpdateModuleData* d = getTimeOfDayOverrideUpdateModuleData();

	TimeOfDay currentTOD = TheGlobalData->m_timeOfDay;
	TimeOfDay targetTOD = (TimeOfDay)d->m_targetTimeOfDay;

	if( m_activeTimeOfDay != TIME_OF_DAY_INVALID )
	{
		// We are already holding the world at the target, so firing again puts it back.
		revertOverride();
		setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
		return;
	}

	if( currentTOD == targetTOD )
	{
		// Somebody else already took the world where we would have taken it, so we join them rather
		// than doing nothing. Otherwise the world would snap back the moment they let go, even
		// though we are still here wanting the night.
		TimeOfDayOverrideUpdate *holder = findOtherHolder( this, targetTOD, nullptr );

		if( holder == nullptr )
		{
			// Nobody is holding it, so the map itself sits at our target and there is nothing for us
			// to do. The power still fires, we just have no time of day change to make.
			return;
		}

		// Go back to wherever the first of us found the world, not to the night we are all holding.
		m_originalTimeOfDay = holder->m_originalTimeOfDay;
		m_activeTimeOfDay = targetTOD;
	}
	else
	{
		// Where we go back to when the override ends, which is simply wherever the map was.
		m_originalTimeOfDay = currentTOD;

		if( TheGameClient->switchTimeOfDay( targetTOD ) == FALSE )
		{
			return;
		}

		m_activeTimeOfDay = targetTOD;
	}

	if( d->m_activateOnCreate )
	{
		// We hold the night for as long as we live, so there is no timer to run.
		m_revertFrame = 0;
		setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
	}
	else if( d->m_durationFrames > 0 )
	{
		m_revertFrame = TheGameLogic->getFrame() + d->m_durationFrames;
		setWakeFrame( getObject(), UPDATE_SLEEP( d->m_durationFrames ) );
	}
	else
	{
		// Permanent until the power is fired again.
		m_revertFrame = 0;
		setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
	}
}

//-------------------------------------------------------------------------------------------------
/** Find another module that is holding the world at the given time of day right now, and count how
	* many there are. Only holders of that same time of day count: one holding the world somewhere
	* else neither tells us where to go back to nor has any say in when we let go. We walk the
	* objects rather than keeping a counter, so a save and load or a replay rebuilds the same answer
	* without any extra state to get out of step. */
//-------------------------------------------------------------------------------------------------
TimeOfDayOverrideUpdate *TimeOfDayOverrideUpdate::findOtherHolder( const TimeOfDayOverrideUpdate *exclude, TimeOfDay heldTimeOfDay, Int *count )
{
	static const NameKeyType key = NAMEKEY( "TimeOfDayOverrideUpdate" );

	TimeOfDayOverrideUpdate *found = nullptr;

	if( count != nullptr )
	{
		*count = 0;
	}

	for( Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject() )
	{
		for( BehaviorModule **b = obj->getBehaviorModules(); *b; ++b )
		{
			if( (*b)->getModuleNameKey() != key )
			{
				continue;
			}

			TimeOfDayOverrideUpdate *other = (TimeOfDayOverrideUpdate*)(*b);

			if( other == exclude || other->m_activeTimeOfDay != (Int)heldTimeOfDay )
			{
				continue;
			}

			if( found == nullptr )
			{
				found = other;
			}

			if( count == nullptr )
			{
				return found;
			}

			++(*count);
		}
	}

	return found;
}

//-------------------------------------------------------------------------------------------------
void TimeOfDayOverrideUpdate::revertOverride()
{
	const TimeOfDayOverrideUpdateModuleData* d = getTimeOfDayOverrideUpdateModuleData();

	// Somebody else still wants the world where we are holding it, so just let go of our own claim
	// and leave the last one out to put it back.
	Int otherHolders = 0;
	findOtherHolder( this, (TimeOfDay)m_activeTimeOfDay, &otherHolders );

	if( otherHolders > 0 )
	{
		m_activeTimeOfDay = TIME_OF_DAY_INVALID;
		m_revertFrame = 0;
		return;
	}

	TimeOfDay returnTOD = (TimeOfDay)m_originalTimeOfDay;

	if( returnTOD == (TimeOfDay)d->m_targetTimeOfDay )
	{
		// The map itself sits at the time of day we force, so going back to it would leave the
		// world where the override put it. The fallback gives us somewhere to return to.
		returnTOD = (TimeOfDay)d->m_fallbackTimeOfDay;
	}

	TheGameClient->switchTimeOfDay( returnTOD );
	m_activeTimeOfDay = TIME_OF_DAY_INVALID;
	m_revertFrame = 0;
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime TimeOfDayOverrideUpdate::update()
{
	const TimeOfDayOverrideUpdateModuleData* d = getTimeOfDayOverrideUpdateModuleData();

	if( d->m_activateOnCreate )
	{
		// First tick after we came into the world. We hold the night for as long as we live, so
		// there is nothing to do afterwards and onDelete does the putting back.
		if( m_activeTimeOfDay == TIME_OF_DAY_INVALID )
		{
			activateOverride();
		}

		return UPDATE_SLEEP_FOREVER;
	}

	if( m_revertFrame != 0 && TheGameLogic->getFrame() >= m_revertFrame )
	{
		revertOverride();
	}

	return UPDATE_SLEEP_FOREVER;
}

//-------------------------------------------------------------------------------------------------
void TimeOfDayOverrideUpdate::onDelete()
{
	// Whether we were holding the night for our own lifetime or running a timed switch, dying with
	// it still in place must not leave the world stuck there.
	if( m_activeTimeOfDay != TIME_OF_DAY_INVALID )
	{
		revertOverride();
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void TimeOfDayOverrideUpdate::crc( Xfer *xfer )
{

	// extend base class
	SpecialPowerUpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void TimeOfDayOverrideUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	SpecialPowerUpdateModule::xfer( xfer );

	// active time of day
	xfer->xferInt( &m_activeTimeOfDay );

	// original time of day
	xfer->xferInt( &m_originalTimeOfDay );

	// revert frame
	xfer->xferUnsignedInt( &m_revertFrame );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void TimeOfDayOverrideUpdate::loadPostProcess()
{

	// extend base class
	SpecialPowerUpdateModule::loadPostProcess();

	// Loading a game reloads the map, which puts the world back on the map's own time of day.
	// Nothing saves the global time of day, so we have to put our override back ourselves.
	if( m_activeTimeOfDay != TIME_OF_DAY_INVALID )
	{
		TheGameClient->switchTimeOfDay( (TimeOfDay)m_activeTimeOfDay );
	}

}
