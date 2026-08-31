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
// Desc:   Attaches to an existing superweapon and switches the world time of day when it fires.
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

	setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
}

//-------------------------------------------------------------------------------------------------
TimeOfDayOverrideUpdate::~TimeOfDayOverrideUpdate()
{
}

//-------------------------------------------------------------------------------------------------
Bool TimeOfDayOverrideUpdate::initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions )
{
	const TimeOfDayOverrideUpdateModuleData* d = getTimeOfDayOverrideUpdateModuleData();

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
		// The map is already where we would take it, so there is nothing for us to do. The power
		// itself still fires, we just have no time of day change to make.
		return;
	}

	// Where we go back to when the override ends, which is simply wherever the map was.
	m_originalTimeOfDay = currentTOD;

	if( TheGameClient->switchTimeOfDay( targetTOD ) == FALSE )
	{
		return;
	}

	m_activeTimeOfDay = targetTOD;

	if( d->m_durationFrames > 0 )
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
void TimeOfDayOverrideUpdate::revertOverride()
{
	const TimeOfDayOverrideUpdateModuleData* d = getTimeOfDayOverrideUpdateModuleData();

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
	if( m_revertFrame != 0 && TheGameLogic->getFrame() >= m_revertFrame )
	{
		revertOverride();
	}

	return UPDATE_SLEEP_FOREVER;
}

//-------------------------------------------------------------------------------------------------
void TimeOfDayOverrideUpdate::onDelete()
{
	// A timed override must not become permanent just because we died holding it.
	if( m_revertFrame != 0 )
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
