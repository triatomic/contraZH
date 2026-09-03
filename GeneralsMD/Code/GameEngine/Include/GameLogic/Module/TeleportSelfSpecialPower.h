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

// FILE: TeleportSelfSpecialPower.h /////////////////////////////////////////////////////////////////
// Desc:   Special power update module that teleports its own object to a clicked position. This is
//         the activated-ability counterpart to TeleporterAIUpdate, which instead replaces all
//         normal movement with teleporting.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "GameClient/TintStatus.h"
#include "Common/ModelState.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/SpecialPowerUpdateModule.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class SpecialPowerModuleInterface;
class FXList;

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class TeleportSelfSpecialPowerModuleData : public ModuleData
{
public:
	SpecialPowerTemplate *m_specialPowerTemplate;

	Real				m_maxRange;					///< furthest the object may teleport, 0 = unlimited
	UnsignedInt			m_teleportDelayFrames;		///< warm-up before the teleport happens
	UnsignedInt			m_recoverDurationFrames;	///< lockdown after landing, 0 = no recovery at all

	const FXList*		m_sourceFX;					///< FX at the position we left
	const FXList*		m_targetFX;					///< FX at the position we arrived at
	const FXList*		m_recoverEndFX;				///< FX when the recovery finishes

	AudioEventRTS		m_recoverSoundLoop;			///< ambient sound played while recovering

	TintStatus			m_tintStatus;				///< tint color to apply while recovering
	ModelConditionFlagType	m_recoverCondition;		///< model condition to set while recovering

	Real				m_opacityStart;
	Real				m_opacityEnd;

	TeleportSelfSpecialPowerModuleData();

	static void buildFieldParse(MultiIniFieldParse& p);

private:

};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class TeleportSelfSpecialPower : public SpecialPowerUpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( TeleportSelfSpecialPower, "TeleportSelfSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( TeleportSelfSpecialPower, TeleportSelfSpecialPowerModuleData );

public:

	TeleportSelfSpecialPower( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	// SpecialPowerUpdateInterface
	virtual Bool initiateIntentToDoSpecialPower(const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions );

	// TheSuperHackers @info Object::findSpecialAbilityUpdate casts anything answering true here to
	// SpecialAbilityUpdate, so a unit ability must still report itself as a special power.
	virtual Bool isSpecialAbility() const { return false; }
	virtual Bool isSpecialPower() const { return true; }
	virtual Bool isActive() const { return m_active || m_isRecovering; }
	virtual SpecialPowerUpdateInterface* getSpecialPowerUpdateInterface() { return this; }
	virtual CommandOption getCommandOption() const { return (CommandOption)0; }
	virtual Bool isPowerCurrentlyInUse( const CommandButton *command = nullptr ) const { return m_active || m_isRecovering; }

	// A single click delivers the destination straight to initiateIntentToDoSpecialPower, so the
	// re-aim channel stays unused.
	virtual Bool doesSpecialPowerHaveOverridableDestinationActive() const { return false; }
	virtual Bool doesSpecialPowerHaveOverridableDestination() const { return false; }
	virtual void setSpecialPowerOverridableDestination( const Coord3D *loc ) {}

	virtual void onObjectCreated();
	virtual UpdateSleepTime update();

	/// keep updating while disabled, otherwise our own recovery would stop us cleaning it up
	virtual DisabledMaskType getDisabledTypesToProcess() const { return DISABLEDMASK_ALL; }

protected:

	Bool validateDestination( Coord3D *destination );	///< snap to a legal spot, FALSE if there is none

	void doTeleport();

	Bool usesOpacityRamp() const;					///< TRUE when either end of the ramp is not fully opaque
	Real getRecoverOpacity( UnsignedInt now ) const;	///< opacity for a point in the recovery ramp

	void applyRecoverEffects( UnsignedInt now );
	void removeRecoverEffects();

	SpecialPowerModuleInterface*	m_specialPowerModule;	///< cached paired power module (recharge/cost/timer)

	Coord3D			m_destLocation;			///< validated position we teleport to
	UnsignedInt		m_teleportFrame;		///< logic frame at which the teleport fires
	UnsignedInt		m_recoverUntilFrame;	///< frame the recovery ends

	AudioEventRTS	m_recoverSoundLoop;		///< live handle of the ambient recovery sound

	Bool			m_active;				///< TRUE while a teleport is pending (armed, not yet fired)
	Bool			m_isRecovering;			///< TRUE while recovering from a teleport
};
