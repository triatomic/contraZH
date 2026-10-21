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

// FILE: GasBehavior.h /////////////////////////////////////////////////////////////////////////////
// Desc:   Behavior that reacts to GAS damage with damage over time, a slow, disorientation and
//         hallucination
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/GlobalData.h"
#include "Common/KindOf.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/DamageModule.h"
#include "GameLogic/Module/UpdateModule.h"

class FXList;
class Player;
class UpgradeTemplate;

//-------------------------------------------------------------------------------------------------
class GasBehaviorModuleData : public UpdateModuleData
{
public:
	KindOfMaskType m_requiredAffectKindOf;
	KindOfMaskType m_forbiddenAffectKindOf;
	Bool m_requiresAllKindOfs;
	Int m_targetsMask;
	Bool m_affectAirborne;

	UnsignedInt m_damageInterval;
	UnsignedInt m_duration;

	Real m_moveSpeedReduction;
	KindOfMaskType m_moveSpeedRequiredAffectKindOf;
	KindOfMaskType m_moveSpeedForbiddenAffectKindOf;
	Bool m_moveSpeedRequiresAllKindOfs;

	KindOfMaskType m_disorientKindOf;
	Real m_disorientRadius;
	UnsignedInt m_disorientInterval;

	UnsignedInt m_hallucinationInterval;
	SubdualValue m_hallucinationDamageCap;
	SubdualValue m_hallucinationHealRate;
	SubdualValue m_hallucinationHealAmount;
	std::vector<AsciiString> m_triggeredBy;
	std::vector<AsciiString> m_conflictsWith;
	const FXList *m_fxListUpgrade;
	Bool m_requiresAllTriggers;

	mutable std::vector<const UpgradeTemplate *> m_triggerUpgrades;
	mutable std::vector<const UpgradeTemplate *> m_conflictUpgrades;
	mutable Bool m_upgradesResolved;

	GasBehaviorModuleData();

	static void buildFieldParse(MultiIniFieldParse& p);

	// Upgrade.ini is not parsed yet when this module data is, so resolve on first use
	void resolveUpgrades() const;
};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class GasBehavior : public UpdateModule,
										public DamageModuleInterface
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( GasBehavior, "GasBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( GasBehavior, GasBehaviorModuleData )

public:

	GasBehavior( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	static Int getInterfaceMask() { return UpdateModule::getInterfaceMask() | (MODULEINTERFACE_DAMAGE); }

	// BehaviorModule
	virtual DamageModuleInterface* getDamage() override { return this; }

	// DamageModuleInterface
	virtual void onDamage( DamageInfo *damageInfo ) override;
	virtual void onHealing( DamageInfo *damageInfo ) override { }
	virtual void onBodyDamageStateChange(const DamageInfo* damageInfo, BodyDamageType oldState, BodyDamageType newState) override { }

	// UpdateInterface
	virtual UpdateSleepTime update() override;
	virtual DisabledMaskType getDisabledTypesToProcess() const override { return DISABLEDMASK_ALL; }

	// AIUpdate and ActionManager drop player orders and abilities while this is true
	static Bool blocksPlayerOrders( const Object *obj );

protected:

	Bool isBlockingPlayerOrders() const;

	Bool isAffectedBy( const Object *attacker, const Player *player ) const;
	Bool attackerMeetsTriggers( const Object *attacker, const Player *player ) const;
	void startGasEffects( const DamageInfo *damageInfo );
	void stopGasEffects();
	void releaseOrders();
	void addHallucinationDamage( Real amount );
	void updateHallucinating();
	void doHallucination();
	void doDisorientation();
	UpdateSleepTime calcSleepTime() const;

private:
	UnsignedInt		m_gasDamageFrame;
	UnsignedInt		m_gasStopFrame;
	Real					m_gasDamageAmount;
	ObjectID			m_gasSource;
	DeathType			m_deathType;
	Real					m_appliedSpeedScalar;	// 1 while no slow is applied
	UnsignedInt		m_disorientFrame;
	UnsignedInt		m_hallucinateFrame;
	Real					m_hallucinationDamage;	// subdual-style pool; hallucinating while it reaches max health
	UnsignedInt		m_hallucinationHealFrame;
	Bool					m_hallucinating;
	Bool					m_issuedOrders;
	Bool					m_applyingTickDamage;	// guards against our own gas ticks retriggering us

	static Bool		s_anyInstance;	// lets games without gas skip the module scan in blocksPlayerOrders

};
