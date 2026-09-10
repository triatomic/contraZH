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

// FILE: PoweredBehavior.h ///////////////////////////////////////////////////////////////////////
// Desc: Per-object reaction to the owning player running out of power. Replaces the blanket
//       DISABLED_UNDERPOWERED that KINDOF_POWERED objects get, so a unit can keep working
//       with a slower move speed, no weapon, or no movement instead of shutting down.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __POWERED_BEHAVIOR_H_
#define __POWERED_BEHAVIOR_H_

#include "GameLogic/Module/UpdateModule.h"

//-------------------------------------------------------------------------------------------------
class PoweredBehaviorModuleData : public UpdateModuleData
{
public:

	PoweredBehaviorModuleData();

	Bool	m_isMobile;				///< No = cannot move while out of power
	Bool	m_disableWeapon;	///< Yes = cannot attack while out of power
	Real	m_movePenalty;		///< share of move speed lost while out of power, 1.0 or more immobilizes
	Real	m_liftPenalty;		///< share of lift lost while out of power, for hovering units
	AsciiString	m_iconName;	///< Animation block shown over the object while out of power

	static void buildFieldParse(MultiIniFieldParse& p);
};

//-------------------------------------------------------------------------------------------------
class PoweredBehavior : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( PoweredBehavior, "PoweredBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( PoweredBehavior, PoweredBehaviorModuleData )

public:

	PoweredBehavior( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	/// applies or removes the configured effects; always takes over from the KINDOF_POWERED disable
	virtual Bool onPowerChange( Bool hasPower ) override;

	virtual UpdateSleepTime update() override;
	virtual void onCapture( Player *oldOwner, Player *newOwner ) override;

	/// the first tick must run even if something else has disabled the object
	virtual DisabledMaskType getDisabledTypesToProcess() const override { return DISABLEDMASK_ALL; }

protected:

	void syncToOwner();

	Bool									m_powered;
	ObjectStatusMaskType	m_appliedStatus;	///< status bits this module set, so only those get cleared

};

#endif
