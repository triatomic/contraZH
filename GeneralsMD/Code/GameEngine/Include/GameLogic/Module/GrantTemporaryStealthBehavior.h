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

// FILE: GrantTemporaryStealthBehavior.h //////////////////////////////////////////////////////////
// Desc:   Grants temporary stealth, either when the special power it is attached to fires or,
//         with ActivateOnCreate, when the object carrying it is created. The stealth runs on its
//         own timer in StealthUpdate, so it never touches a permanent grant.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/INI.h"
#include "Common/KindOf.h"
#include "GameLogic/Module/SpecialPowerUpdateModule.h"

class SpecialPowerTemplate;

//-------------------------------------------------------------------------------------------------
class GrantTemporaryStealthBehaviorModuleData : public UpdateModuleData
{
public:
	SpecialPowerTemplate *m_specialPowerTemplate;		///< only react to this power, or any power when nullptr
	UnsignedInt m_durationFrames;										///< how long the stealth lasts
	Real m_radius;																	///< 0 grants to the center object alone
	KindOfMaskType m_kindOf;												///< receiver needs any of these
	KindOfMaskType m_forbiddenKindOf;								///< receiver may have none of these
	Bool m_activateOnCreate;												///< grant around ourselves once we exist

	GrantTemporaryStealthBehaviorModuleData()
	{
		m_specialPowerTemplate = nullptr;
		m_durationFrames = 0;
		m_radius = 0.0f;
		SET_ALL_KINDOFMASK_BITS( m_kindOf );
		m_activateOnCreate = FALSE;
	}

	static void buildFieldParse(MultiIniFieldParse& p)
	{
		UpdateModuleData::buildFieldParse(p);

		static const FieldParse dataFieldParse[] =
		{
			{ "SpecialPowerTemplate",	INI::parseSpecialPowerTemplate,	nullptr,	offsetof( GrantTemporaryStealthBehaviorModuleData, m_specialPowerTemplate ) },
			{ "Duration",							INI::parseDurationUnsignedInt,	nullptr,	offsetof( GrantTemporaryStealthBehaviorModuleData, m_durationFrames ) },
			{ "Radius",								INI::parseReal,									nullptr,	offsetof( GrantTemporaryStealthBehaviorModuleData, m_radius ) },
			{ "KindOf",								KindOfMaskType::parseFromINI,		nullptr,	offsetof( GrantTemporaryStealthBehaviorModuleData, m_kindOf ) },
			{ "ForbiddenKindOf",			KindOfMaskType::parseFromINI,		nullptr,	offsetof( GrantTemporaryStealthBehaviorModuleData, m_forbiddenKindOf ) },
			{ "ActivateOnCreate",			INI::parseBool,									nullptr,	offsetof( GrantTemporaryStealthBehaviorModuleData, m_activateOnCreate ) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add(dataFieldParse);
	}
};

//-------------------------------------------------------------------------------------------------
class GrantTemporaryStealthBehavior : public SpecialPowerUpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( GrantTemporaryStealthBehavior, "GrantTemporaryStealthBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( GrantTemporaryStealthBehavior, GrantTemporaryStealthBehaviorModuleData )

public:

	GrantTemporaryStealthBehavior( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	//SpecialPowerUpdateInterface pure virtual implementations
	// We never consume the intent, so the power's own update module still gets it.
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

private:

	void grantStealth( const Object *centerObj, const Coord3D *centerPos );
	void grantStealthToObject( const Object *obj );
};
