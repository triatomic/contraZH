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

// FILE: TimeOfDayOverrideUpdate.h ////////////////////////////////////////////////////////////////
// Desc:   Switches the world time of day, either when the superweapon it is attached to fires or,
//         with ActivateOnCreate, for as long as the object carrying it lives. The switch is global
//         presentation for every player and observer, and replays reproduce it because the logic
//         re-executes. Several objects can hold the world at night at once, and it stays there
//         until the last of them lets go.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/GameType.h"
#include "Common/INI.h"
#include "GameLogic/Module/SpecialPowerUpdateModule.h"

class SpecialPowerTemplate;

//-------------------------------------------------------------------------------------------------
class TimeOfDayOverrideUpdateModuleData : public UpdateModuleData
{
public:
	SpecialPowerTemplate *m_specialPowerTemplate;		///< only react to this power, or any power when nullptr
	Int m_targetTimeOfDay;													///< time of day we switch the world to
	Int m_fallbackTimeOfDay;												///< where we revert to when the map itself sits at the target
	UnsignedInt m_durationFrames;										///< in frames, 0 means the switch is permanent
	Bool m_activateOnCreate;												///< switch as soon as we exist and hold it until we die

	TimeOfDayOverrideUpdateModuleData()
	{
		m_specialPowerTemplate = nullptr;
		m_targetTimeOfDay = TIME_OF_DAY_NIGHT;
		m_fallbackTimeOfDay = TIME_OF_DAY_AFTERNOON;
		m_durationFrames = 0;
		m_activateOnCreate = FALSE;
	}

	static void buildFieldParse(MultiIniFieldParse& p)
	{
		UpdateModuleData::buildFieldParse(p);

		static const FieldParse dataFieldParse[] =
		{
			{ "SpecialPowerTemplate",	INI::parseSpecialPowerTemplate,	nullptr,				offsetof( TimeOfDayOverrideUpdateModuleData, m_specialPowerTemplate ) },
			{ "TimeOfDay",						INI::parseIndexList,						TimeOfDayNames,	offsetof( TimeOfDayOverrideUpdateModuleData, m_targetTimeOfDay ) },
			{ "FallbackTimeOfDay",		INI::parseIndexList,						TimeOfDayNames,	offsetof( TimeOfDayOverrideUpdateModuleData, m_fallbackTimeOfDay ) },
			{ "Duration",							INI::parseDurationUnsignedInt,	nullptr,				offsetof( TimeOfDayOverrideUpdateModuleData, m_durationFrames ) },
			{ "ActivateOnCreate",			INI::parseBool,									nullptr,				offsetof( TimeOfDayOverrideUpdateModuleData, m_activateOnCreate ) },
			{ 0, 0, 0, 0 }
		};
		p.add(dataFieldParse);
	}
};

//-------------------------------------------------------------------------------------------------
class TimeOfDayOverrideUpdate : public SpecialPowerUpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( TimeOfDayOverrideUpdate, "TimeOfDayOverrideUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( TimeOfDayOverrideUpdate, TimeOfDayOverrideUpdateModuleData )

public:

	TimeOfDayOverrideUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	//SpecialPowerUpdateInterface pure virtual implementations
	// We never consume the intent, so the host superweapon's own update module still gets it.
	virtual Bool initiateIntentToDoSpecialPower(const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions ) override;
	virtual Bool isSpecialAbility() const override { return false; }
	virtual Bool isSpecialPower() const override { return true; }
	virtual Bool isActive() const override { return false; }
	virtual Bool doesSpecialPowerHaveOverridableDestinationActive() const override { return false; }
	virtual Bool doesSpecialPowerHaveOverridableDestination() const override { return false; }
	virtual void setSpecialPowerOverridableDestination( const Coord3D *loc ) override {}
	virtual Bool isPowerCurrentlyInUse( const CommandButton *command = nullptr ) const override { return false; }

	virtual SpecialPowerUpdateInterface* getSpecialPowerUpdateInterface() override { return this; }
	virtual CommandOption getCommandOption() const override { return (CommandOption)0; }

	virtual UpdateSleepTime update() override;
	virtual void onDelete() override;

private:

	void activateOverride();
	void revertOverride();

	static TimeOfDayOverrideUpdate *findOtherHolder( const TimeOfDayOverrideUpdate *exclude, TimeOfDay heldTimeOfDay, Int *count );

	Int m_activeTimeOfDay;					///< time of day we forced on the world, TIME_OF_DAY_INVALID when we have no override running
	Int m_originalTimeOfDay;				///< time of day we go back to
	UnsignedInt m_revertFrame;			///< frame we revert on, 0 when no timed revert is pending
};
