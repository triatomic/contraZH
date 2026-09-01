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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: PoisonedBehavior.cpp /////////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood, July 2002
// Desc:   Behavior that reacts to poison Damage by continuously damaging us further in an Update
///////////////////////////////////////////////////////////////////////////////////////////////////


// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/PoisonedBehavior.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"


// tinting is all handled in drawable, now, Graham look near the bottom of Drawable::UpdateDrawable()
//static const RGBColor poisonedTint = {0.0f, 1.0f, 0.0f};

//-------------------------------------------------------------------------------------------------
PoisonedBehaviorModuleData::PoisonedBehaviorModuleData()
{
	m_poisonDamageIntervalData = 0; // How often I retake poison damage dealt me
	m_poisonDurationData = 0;				// And how long after the last poison dose I am poisoned
	m_poisonBetaDamageIntervalData = 0;
	m_poisonBetaDurationData = 0;
	m_poisonBetaDamageBonus = 1.0f;
	m_poisonGammaDamageIntervalData = 0;
	m_poisonGammaDurationData = 0;
	m_poisonGammaDamageBonus = 1.0f;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void PoisonedBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{

	static const FieldParse dataFieldParse[] =
	{
		{ "PoisonDamageInterval", INI::parseDurationUnsignedInt, nullptr, offsetof(PoisonedBehaviorModuleData, m_poisonDamageIntervalData) },
		{ "PoisonDuration", INI::parseDurationUnsignedInt, nullptr, offsetof(PoisonedBehaviorModuleData, m_poisonDurationData) },
		{ "PoisonBetaDamageInterval", INI::parseDurationUnsignedInt, nullptr, offsetof(PoisonedBehaviorModuleData, m_poisonBetaDamageIntervalData) },
		{ "PoisonBetaDuration", INI::parseDurationUnsignedInt, nullptr, offsetof(PoisonedBehaviorModuleData, m_poisonBetaDurationData) },
		{ "PoisonBetaDamageBonus", INI::parseReal, nullptr, offsetof(PoisonedBehaviorModuleData, m_poisonBetaDamageBonus) },
		{ "PoisonBetaTriggeredBy", INI::parseAsciiString, nullptr, offsetof(PoisonedBehaviorModuleData, m_poisonBetaTriggeredBy) },
		{ "PoisonGammaDamageInterval", INI::parseDurationUnsignedInt, nullptr, offsetof(PoisonedBehaviorModuleData, m_poisonGammaDamageIntervalData) },
		{ "PoisonGammaDuration", INI::parseDurationUnsignedInt, nullptr, offsetof(PoisonedBehaviorModuleData, m_poisonGammaDurationData) },
		{ "PoisonGammaDamageBonus", INI::parseReal, nullptr, offsetof(PoisonedBehaviorModuleData, m_poisonGammaDamageBonus) },
		{ "PoisonGammaTriggeredBy", INI::parseAsciiString, nullptr, offsetof(PoisonedBehaviorModuleData, m_poisonGammaTriggeredBy) },
		{ nullptr, nullptr, nullptr, 0 }
	};

  UpdateModuleData::buildFieldParse(p);
  p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
UnsignedInt PoisonedBehaviorModuleData::getDamageInterval( PoisonTier tier ) const
{
	if( tier == POISON_TIER_GAMMA && m_poisonGammaDamageIntervalData != 0 )
	{
		return m_poisonGammaDamageIntervalData;
	}
	if( tier == POISON_TIER_BETA && m_poisonBetaDamageIntervalData != 0 )
	{
		return m_poisonBetaDamageIntervalData;
	}
	return m_poisonDamageIntervalData;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt PoisonedBehaviorModuleData::getDuration( PoisonTier tier ) const
{
	if( tier == POISON_TIER_GAMMA && m_poisonGammaDurationData != 0 )
	{
		return m_poisonGammaDurationData;
	}
	if( tier == POISON_TIER_BETA && m_poisonBetaDurationData != 0 )
	{
		return m_poisonBetaDurationData;
	}
	return m_poisonDurationData;
}

//-------------------------------------------------------------------------------------------------
Real PoisonedBehaviorModuleData::getDamageBonus( PoisonTier tier ) const
{
	if( tier == POISON_TIER_GAMMA )
	{
		return m_poisonGammaDamageBonus;
	}
	if( tier == POISON_TIER_BETA )
	{
		return m_poisonBetaDamageBonus;
	}
	return 1.0f;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
PoisonedBehavior::PoisonedBehavior( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_poisonDamageFrame = 0;
	m_poisonOverallStopFrame = 0;
	m_poisonDamageAmount = 0.0f;
	m_poisonSource = INVALID_ID;
	m_deathType = DEATH_POISONED;
	m_activeTier = POISON_TIER_PLAIN;
	m_applyingTickDamage = FALSE;
	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
PoisonedBehavior::~PoisonedBehavior()
{
}

//-------------------------------------------------------------------------------------------------
/** Damage has been dealt, this is an opportunity to react to that damage */
//-------------------------------------------------------------------------------------------------
void PoisonedBehavior::onDamage( DamageInfo *damageInfo )
{
	if( m_applyingTickDamage )
	{
		return;
	}

	// @bugfix hanfield 01/08/2025 Check m_sourceID to see if we are causing damage. If we are - ignore.
	if( damageInfo->in.m_damageType == DAMAGE_POISON && 
		damageInfo->in.m_sourceID != INVALID_ID) 
		startPoisonedEffects( damageInfo );      
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void PoisonedBehavior::onHealing( DamageInfo *damageInfo )
{
	stopPoisonedEffects();

	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
UpdateSleepTime PoisonedBehavior::update()
{
	const PoisonedBehaviorModuleData* d = getPoisonedBehaviorModuleData();
	UnsignedInt now = TheGameLogic->getFrame();

	if( m_poisonOverallStopFrame == 0 )
	{
		DEBUG_CRASH(("hmm, this should not happen"));
		return UPDATE_SLEEP_FOREVER;
		//we aren't poisoned, so nevermind
	}

	if (m_poisonDamageFrame != 0 && now >= m_poisonDamageFrame)
	{
		// If it is time to do damage, then do it and reset the damage timer
		DamageInfo damage;
		damage.in.m_amount = m_poisonDamageAmount;
		damage.in.m_sourceID = m_poisonSource;
		damage.in.m_damageType = DAMAGE_POISON; // @bugfix hanfield 01/08/2025 Since we now check for sourceID, this damage will not cause an infinite poison loop
		damage.in.m_damageFXOverride = DAMAGE_POISON; // Not necessary anymore, but can help to make sure proper FX are used, if template is wonky
		damage.in.m_deathType = m_deathType;
		m_applyingTickDamage = TRUE;
		getObject()->attemptDamage( &damage );
		m_applyingTickDamage = FALSE;

		m_poisonDamageFrame = now + d->getDamageInterval( m_activeTier );
	}

	// If we are now at zero we need to turn off our special effects...
	// unless the poison killed us, then we continue to be a pulsating toxic pus ball
	if( m_poisonOverallStopFrame != 0 &&
			now >= m_poisonOverallStopFrame &&
			!getObject()->isEffectivelyDead())
	{
		stopPoisonedEffects();
	}

	return calcSleepTime();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
UpdateSleepTime PoisonedBehavior::calcSleepTime()
{
	// UPDATE_SLEEP requires a count-of-frames, not an absolute-frame, so subtract 'now'
	UnsignedInt now = TheGameLogic->getFrame();
	if (m_poisonOverallStopFrame == 0 || m_poisonOverallStopFrame == now)
		return UPDATE_SLEEP_FOREVER;
	return frameToSleepTime(m_poisonDamageFrame, m_poisonOverallStopFrame);
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
/*static*/ Bool PoisonedBehavior::attackerHasUpgrade( const AsciiString& upgradeName, const Player *player, const Object *attacker )
{
	if( upgradeName.isEmpty() )
	{
		return FALSE;
	}

	const UpgradeTemplate *upgradeTemplate = TheUpgradeCenter->findUpgrade( upgradeName );
	if( upgradeTemplate == nullptr )
	{
		return FALSE;
	}

	// the upgrade may be owned by the player or by the attacking object alone
	if( player != nullptr && player->hasUpgradeComplete( upgradeTemplate ) )
	{
		return TRUE;
	}
	if( attacker != nullptr && attacker->hasUpgrade( upgradeTemplate ) )
	{
		return TRUE;
	}
	return FALSE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
PoisonTier PoisonedBehavior::resolveTier( const DamageInfo *damageInfo ) const
{
	const PoisonedBehaviorModuleData* d = getPoisonedBehaviorModuleData();
	if( d->m_poisonBetaTriggeredBy.isEmpty() && d->m_poisonGammaTriggeredBy.isEmpty() )
	{
		return POISON_TIER_PLAIN;
	}

	// an attacker that no longer exists leaves us with the player mask alone, or with nothing
	const Object *attacker = TheGameLogic->findObjectByID( damageInfo->in.m_sourceID );
	const Player *player = nullptr;
	if( damageInfo->in.m_sourcePlayerMask != 0 )
	{
		player = ThePlayerList->getPlayerFromMask( damageInfo->in.m_sourcePlayerMask );
	}
	if( player == nullptr && attacker != nullptr )
	{
		player = attacker->getControllingPlayer();
	}

	if( attackerHasUpgrade( d->m_poisonGammaTriggeredBy, player, attacker ) )
	{
		return POISON_TIER_GAMMA;
	}
	if( attackerHasUpgrade( d->m_poisonBetaTriggeredBy, player, attacker ) )
	{
		return POISON_TIER_BETA;
	}
	return POISON_TIER_PLAIN;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void PoisonedBehavior::startPoisonedEffects( const DamageInfo *damageInfo )
{
	const PoisonedBehaviorModuleData* d = getPoisonedBehaviorModuleData();
	UnsignedInt now = TheGameLogic->getFrame();

	PoisonTier tier = resolveTier( damageInfo );
	// a weaker dose never dilutes or extends a stronger one that is still running
	if( m_poisonOverallStopFrame != 0 && tier < m_activeTier )
	{
		return;
	}
	m_activeTier = tier;

	// We are going to take the damage dealt by the original poisoner every so often for a while.
	m_poisonDamageAmount = damageInfo->out.m_actualDamageDealt * d->getDamageBonus( tier );
#if !(RETAIL_COMPATIBLE_CRC || PRESERVE_NO_XP_FROM_POISON_KILLS)
	// TheSuperHackers @bugfix Stubbjax 03/09/2025 Allow poison damage to award xp to the poison source.
	m_poisonSource = damageInfo->in.m_sourceID;
#endif

	m_poisonOverallStopFrame = now + d->getDuration( tier );

	UnsignedInt interval = d->getDamageInterval( tier );
	// If we are getting re-poisoned, don't reset the damage counter if running, but do set it if unset
	if( m_poisonDamageFrame != 0 )
		m_poisonDamageFrame = min( m_poisonDamageFrame, now + interval );
	else
		m_poisonDamageFrame = now + interval;

	m_deathType = damageInfo->in.m_deathType;
	// let a tier weapon that only asks for plain poison still die the way its tier looks
	if( m_deathType == DEATH_POISONED && tier == POISON_TIER_BETA )
	{
		m_deathType = DEATH_POISONED_BETA;
	}
	else if( m_deathType == DEATH_POISONED && tier == POISON_TIER_GAMMA )
	{
		m_deathType = DEATH_POISONED_GAMMA;
	}

	Drawable *myDrawable = getObject()->getDrawable();
	if( myDrawable )
		myDrawable->setTintStatus( TINT_STATUS_POISONED );// Graham, It has changed, see UpdateDrawable()

	setWakeFrame(getObject(), calcSleepTime());
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void PoisonedBehavior::stopPoisonedEffects()
{
	m_poisonDamageFrame = 0;
	m_poisonOverallStopFrame = 0;
	m_poisonDamageAmount = 0.0f;
	m_poisonSource = INVALID_ID;
	m_activeTier = POISON_TIER_PLAIN;

	Drawable *myDrawable = getObject()->getDrawable();
	if( myDrawable )
		myDrawable->clearTintStatus( TINT_STATUS_POISONED );// Graham, It has changed, see UpdateDrawable()
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void PoisonedBehavior::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version
	* 2: Serialize death type
	* 3: TheSuperHackers @tweak Serialize poison source
	* 4: Serialize the active poison tier
	*/
// ------------------------------------------------------------------------------------------------
void PoisonedBehavior::xfer( Xfer *xfer )
{

	// version
#if RETAIL_COMPATIBLE_XFER_SAVE
	const XferVersion currentVersion = 2;
#else
	const XferVersion currentVersion = 4;
#endif
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// poisoned damage frame
	xfer->xferUnsignedInt( &m_poisonDamageFrame );

	// poison overall stop frame
	xfer->xferUnsignedInt( &m_poisonOverallStopFrame );

	// poison damage amount
	xfer->xferReal( &m_poisonDamageAmount );

	if (version >= 2)
	{
		xfer->xferUser(&m_deathType, sizeof(m_deathType));
	}

	if (version >= 3)
	{
		xfer->xferObjectID(&m_poisonSource);
	}

	if (version >= 4)
	{
		xfer->xferUser(&m_activeTier, sizeof(m_activeTier));
	}
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void PoisonedBehavior::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
