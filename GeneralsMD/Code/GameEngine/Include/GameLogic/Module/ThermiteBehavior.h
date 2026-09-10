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

// FILE: ThermiteBehavior.h //////////////////////////////////////////////////////////////////////
// Desc: Turns a detonated projectile into a timed burn. The projectile object stays alive, sticks
//       to the object it hit or to the ground, fires Weapon on its own cadence, and dies when the
//       lifetime runs out.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __THERMITE_BEHAVIOR_H_
#define __THERMITE_BEHAVIOR_H_

#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/UpgradeModule.h"
#include "GameLogic/Weapon.h"

//-------------------------------------------------------------------------------------------------
class ThermiteBehaviorModuleData : public UpdateModuleData
{
public:

	ThermiteBehaviorModuleData();

	const WeaponTemplate*	m_weaponTemplate;	///< fired at the victim or the ground on every pulse
	UnsignedInt						m_minFrames;			///< shortest burn
	UnsignedInt						m_maxFrames;			///< longest burn
	UpgradeMuxData				m_upgradeMuxData;	///< TriggeredBy / ConflictsWith / RequiresAllTriggers

	static void buildFieldParse(MultiIniFieldParse& p);
};

//-------------------------------------------------------------------------------------------------
class ThermiteBehavior : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ThermiteBehavior, "ThermiteBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ThermiteBehavior, ThermiteBehaviorModuleData )

public:

	ThermiteBehavior( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	/// Projectile modules call this from detonate(). True means the thermite now owns the object.
	static Bool tryIgnite( Object *projectile, Object *victim );

	virtual UpdateSleepTime update() override;

	/// the burn holds its object with DISABLED_HELD, and must keep ticking through any disable
	virtual DisabledMaskType getDisabledTypesToProcess() const override { return DISABLEDMASK_ALL; }

protected:

	Bool ignite( Object *victim );
	Bool isTriggered() const;
	void dropToGround();

	Weapon*				m_weapon;
	ObjectID			m_victimID;
	UnsignedInt		m_endFrame;		///< 0 until ignited

};

#endif
