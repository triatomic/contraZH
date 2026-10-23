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

// FILE: GasBehavior.cpp ///////////////////////////////////////////////////////////////////////////
// Desc:   Behavior that reacts to GAS damage with damage over time, a slow, disorientation and
//         hallucination
///////////////////////////////////////////////////////////////////////////////////////////////////


// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include "Common/Player.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/GasBehavior.h"
#include "GameLogic/Module/PoisonedBehavior.h"


//-------------------------------------------------------------------------------------------------
static Bool matchesKindOf( const Object *obj, const KindOfMaskType& required, Bool requiresAll, const KindOfMaskType& forbidden )
{
	if( obj->isAnyKindOf( forbidden ) )
	{
		return FALSE;
	}
	if( !required.any() )
	{
		return TRUE;
	}
	if( requiresAll )
	{
		return obj->isKindOfMulti( required, KINDOFMASK_NONE );
	}
	return obj->isAnyKindOf( required );
}

//-------------------------------------------------------------------------------------------------
static Real maxHealthOf( const Object *obj )
{
	const BodyModuleInterface *body = obj->getBodyModule();
	return body != nullptr ? body->getMaxHealth() : 0.0f;
}

//-------------------------------------------------------------------------------------------------
static void resolveUpgradeNames( const std::vector<AsciiString>& names, std::vector<const UpgradeTemplate *>& out )
{
	for( std::vector<AsciiString>::const_iterator it = names.begin(); it != names.end(); ++it )
	{
		const UpgradeTemplate *upgrade = TheUpgradeCenter->findUpgrade( *it );
		DEBUG_ASSERTCRASH( upgrade != nullptr, ("GasBehavior references '%s', which is not an Upgrade", it->str()) );
		if( upgrade != nullptr )
		{
			out.push_back( upgrade );
		}
	}
}

Bool GasBehavior::s_anyInstance = FALSE;

//-------------------------------------------------------------------------------------------------
GasBehaviorModuleData::GasBehaviorModuleData()
{
	m_requiresAllKindOfs = FALSE;
	m_targetsMask = WEAPON_AFFECTS_ENEMIES | WEAPON_AFFECTS_NEUTRALS;
	m_affectAirborne = TRUE;
	m_damageInterval = 0;
	m_duration = 0;
	m_moveSpeedReduction = 0.0f;
	m_moveSpeedRequiresAllKindOfs = FALSE;
	m_disorientKindOf.set( KINDOF_INFANTRY );
	m_disorientRadius = 50.0f;
	m_disorientInterval = 0;
	m_hallucinationInterval = LOGICFRAMES_PER_SECOND;
	m_hallucinationHealRate = 0;
	m_fxListUpgrade = nullptr;
	m_requiresAllTriggers = FALSE;
	m_upgradesResolved = FALSE;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void GasBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{

	static const FieldParse dataFieldParse[] =
	{
		{ "RequiredAffectKindOf", KindOfMaskType::parseFromINI, nullptr, offsetof(GasBehaviorModuleData, m_requiredAffectKindOf) },
		{ "RequiresAllKindOfs", INI::parseBool, nullptr, offsetof(GasBehaviorModuleData, m_requiresAllKindOfs) },
		{ "ForbiddenAffectKindOf", KindOfMaskType::parseFromINI, nullptr, offsetof(GasBehaviorModuleData, m_forbiddenAffectKindOf) },
		{ "AffectsTargets", INI::parseBitString32, TheWeaponAffectsMaskNames, offsetof(GasBehaviorModuleData, m_targetsMask) },
		{ "AffectAirborne", INI::parseBool, nullptr, offsetof(GasBehaviorModuleData, m_affectAirborne) },
		{ "GasDamageInterval", INI::parseDurationUnsignedInt, nullptr, offsetof(GasBehaviorModuleData, m_damageInterval) },
		{ "GasDuration", INI::parseDurationUnsignedInt, nullptr, offsetof(GasBehaviorModuleData, m_duration) },
		{ "MoveSpeedReduction", INI::parsePercentToReal, nullptr, offsetof(GasBehaviorModuleData, m_moveSpeedReduction) },
		{ "MoveSpeedRequiredAffectKindOf", KindOfMaskType::parseFromINI, nullptr, offsetof(GasBehaviorModuleData, m_moveSpeedRequiredAffectKindOf) },
		{ "MoveSpeedRequiresAllKindOfs", INI::parseBool, nullptr, offsetof(GasBehaviorModuleData, m_moveSpeedRequiresAllKindOfs) },
		{ "MoveSpeedForbiddenAffectKindOf", KindOfMaskType::parseFromINI, nullptr, offsetof(GasBehaviorModuleData, m_moveSpeedForbiddenAffectKindOf) },
		{ "DisorientKindOf", KindOfMaskType::parseFromINI, nullptr, offsetof(GasBehaviorModuleData, m_disorientKindOf) },
		{ "DisorientRadius", INI::parseReal, nullptr, offsetof(GasBehaviorModuleData, m_disorientRadius) },
		{ "DisorientInterval", INI::parseDurationUnsignedInt, nullptr, offsetof(GasBehaviorModuleData, m_disorientInterval) },
		{ "HallucinationEffectInterval", INI::parseDurationUnsignedInt, nullptr, offsetof(GasBehaviorModuleData, m_hallucinationInterval) },
		{ "HallucinationDamageCap", SubdualValue::parseFromINI, nullptr, offsetof(GasBehaviorModuleData, m_hallucinationDamageCap) },
		{ "HallucinationHealRate", INI::parseDurationUnsignedInt, nullptr, offsetof(GasBehaviorModuleData, m_hallucinationHealRate) },
		{ "HallucinationHealAmount", SubdualValue::parseFromINI, nullptr, offsetof(GasBehaviorModuleData, m_hallucinationHealAmount) },
		{ "TriggeredBy", INI::parseAsciiStringVector, nullptr, offsetof(GasBehaviorModuleData, m_triggeredBy) },
		{ "ConflictsWith", INI::parseAsciiStringVector, nullptr, offsetof(GasBehaviorModuleData, m_conflictsWith) },
		{ "FXListUpgrade", INI::parseFXList, nullptr, offsetof(GasBehaviorModuleData, m_fxListUpgrade) },
		{ "RequiresAllTriggers", INI::parseBool, nullptr, offsetof(GasBehaviorModuleData, m_requiresAllTriggers) },
		{ nullptr, nullptr, nullptr, 0 }
	};

	UpdateModuleData::buildFieldParse(p);
	p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
void GasBehaviorModuleData::resolveUpgrades() const
{
	if( m_upgradesResolved )
	{
		return;
	}

	resolveUpgradeNames( m_triggeredBy, m_triggerUpgrades );
	resolveUpgradeNames( m_conflictsWith, m_conflictUpgrades );
	m_upgradesResolved = TRUE;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
GasBehavior::GasBehavior( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_gasDamageFrame = 0;
	m_gasStopFrame = 0;
	m_gasDamageAmount = 0.0f;
	m_gasSource = INVALID_ID;
	m_deathType = DEATH_NORMAL;
	m_appliedSpeedScalar = 1.0f;
	m_disorientFrame = 0;
	m_hallucinateFrame = 0;
	m_hallucinationDamage = 0.0f;
	m_hallucinationHealFrame = 0;
	m_hallucinating = FALSE;
	m_issuedOrders = FALSE;
	m_applyingTickDamage = FALSE;
	s_anyInstance = TRUE;
	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
GasBehavior::~GasBehavior()
{
}

//-------------------------------------------------------------------------------------------------
void GasBehavior::onDamage( DamageInfo *damageInfo )
{
	if( m_applyingTickDamage )
	{
		return;
	}

	if( damageInfo->in.m_damageType == DAMAGE_GAS )
	{
		startGasEffects( damageInfo );
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
/*static*/ Bool GasBehavior::blocksPlayerOrders( const Object *obj )
{
	if( !s_anyInstance )
	{
		return FALSE;
	}

	static const NameKeyType key_GasBehavior = NAMEKEY( "GasBehavior" );
	for( BehaviorModule **m = obj->getBehaviorModules(); *m; ++m )
	{
		if( (*m)->getModuleNameKey() == key_GasBehavior && static_cast<const GasBehavior *>( *m )->isBlockingPlayerOrders() )
		{
			return TRUE;
		}
	}
	return FALSE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool GasBehavior::isBlockingPlayerOrders() const
{
	if( getObject()->isContained() )
	{
		return FALSE;
	}
	return m_disorientFrame != 0 || m_hallucinating;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
UpdateSleepTime GasBehavior::update()
{
	const GasBehaviorModuleData* d = getGasBehaviorModuleData();
	UnsignedInt now = TheGameLogic->getFrame();
	Object *me = getObject();

	if( me->isEffectivelyDead() )
	{
		return UPDATE_SLEEP_FOREVER;
	}

	if( m_gasDamageFrame != 0 && now >= m_gasDamageFrame )
	{
		DamageInfo damage;
		damage.in.m_amount = m_gasDamageAmount;
		damage.in.m_sourceID = m_gasSource;
		damage.in.m_damageType = DAMAGE_GAS;
		damage.in.m_damageFXOverride = DAMAGE_GAS;
		damage.in.m_deathType = m_deathType;
		m_applyingTickDamage = TRUE;
		me->attemptDamage( &damage );
		m_applyingTickDamage = FALSE;

		m_gasDamageFrame = now + d->m_damageInterval;

		if( me->isEffectivelyDead() )
		{
			return UPDATE_SLEEP_FOREVER;
		}
	}

	if( m_gasStopFrame != 0 && now >= m_gasStopFrame )
	{
		stopGasEffects();
	}

	if( m_hallucinationHealFrame != 0 && now >= m_hallucinationHealFrame )
	{
		m_hallucinationDamage = max( m_hallucinationDamage - d->m_hallucinationHealAmount.evaluate( maxHealthOf( me ) ), 0.0f );
		m_hallucinationHealFrame = m_hallucinationDamage > 0.0f ? now + d->m_hallucinationHealRate : 0;
		updateHallucinating();
	}

	if( m_hallucinateFrame != 0 && now >= m_hallucinateFrame )
	{
		doHallucination();
		m_hallucinateFrame = d->m_hallucinationInterval != 0 ? now + d->m_hallucinationInterval : 0;
	}

	if( m_disorientFrame != 0 && now >= m_disorientFrame )
	{
		doDisorientation();
		m_disorientFrame = now + d->m_disorientInterval;
	}

	if( !isBlockingPlayerOrders() )
	{
		releaseOrders();
	}

	return calcSleepTime();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
UpdateSleepTime GasBehavior::calcSleepTime() const
{
	UnsignedInt frames[] = { m_gasDamageFrame, m_gasStopFrame, m_disorientFrame, m_hallucinateFrame, m_hallucinationHealFrame };
	UnsignedInt next = 0;
	for( Int i = 0; i < (Int)ARRAY_SIZE(frames); ++i )
	{
		if( frames[i] != 0 && (next == 0 || frames[i] < next) )
		{
			next = frames[i];
		}
	}

	if( next == 0 )
	{
		return UPDATE_SLEEP_FOREVER;
	}

	UnsignedInt now = TheGameLogic->getFrame();
	if( next <= now )
	{
		return UPDATE_SLEEP_NONE;
	}
	return UPDATE_SLEEP( next - now );
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool GasBehavior::isAffectedBy( const Object *attacker, const Player *player ) const
{
	const GasBehaviorModuleData* d = getGasBehaviorModuleData();
	const Object *me = getObject();

	if( !d->m_affectAirborne && me->isAirborneTarget() )
	{
		return FALSE;
	}

	if( !matchesKindOf( me, d->m_requiredAffectKindOf, d->m_requiresAllKindOfs, d->m_forbiddenAffectKindOf ) )
	{
		return FALSE;
	}

	// gas with no known owner counts as hostile
	Relationship r = ENEMIES;
	if( attacker != nullptr )
	{
		r = attacker->getRelationship( me );
	}
	else if( player != nullptr )
	{
		r = player->getRelationship( me->getTeam() );
	}

	Int need = WEAPON_AFFECTS_NEUTRALS;
	if( r == ALLIES )
	{
		need = WEAPON_AFFECTS_ALLIES;
	}
	else if( r == ENEMIES )
	{
		need = WEAPON_AFFECTS_ENEMIES;
	}
	return (d->m_targetsMask & need) != 0;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool GasBehavior::attackerMeetsTriggers( const Object *attacker, const Player *player ) const
{
	const GasBehaviorModuleData* d = getGasBehaviorModuleData();
	d->resolveUpgrades();

	for( std::vector<const UpgradeTemplate *>::const_iterator it = d->m_conflictUpgrades.begin(); it != d->m_conflictUpgrades.end(); ++it )
	{
		if( PoisonedBehavior::attackerHasUpgrade( *it, player, attacker ) )
		{
			return FALSE;
		}
	}

	if( d->m_triggerUpgrades.empty() )
	{
		return TRUE;
	}

	for( std::vector<const UpgradeTemplate *>::const_iterator it = d->m_triggerUpgrades.begin(); it != d->m_triggerUpgrades.end(); ++it )
	{
		Bool has = PoisonedBehavior::attackerHasUpgrade( *it, player, attacker );
		if( d->m_requiresAllTriggers && !has )
		{
			return FALSE;
		}
		if( !d->m_requiresAllTriggers && has )
		{
			return TRUE;
		}
	}
	return d->m_requiresAllTriggers;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GasBehavior::startGasEffects( const DamageInfo *damageInfo )
{
	const GasBehaviorModuleData* d = getGasBehaviorModuleData();
	UnsignedInt now = TheGameLogic->getFrame();
	Object *me = getObject();

	const Object *attacker;
	const Player *player;
	PoisonedBehavior::findAttacker( damageInfo, attacker, player );

	if( !isAffectedBy( attacker, player ) )
	{
		return;
	}

	m_gasDamageAmount = damageInfo->out.m_actualDamageDealt;
	m_gasSource = damageInfo->in.m_sourceID;
	m_deathType = damageInfo->in.m_deathType;
	m_gasStopFrame = now + d->m_duration;

	// a fresh dose never pushes back a tick that is already due sooner
	if( d->m_damageInterval != 0 )
	{
		if( m_gasDamageFrame != 0 )
		{
			m_gasDamageFrame = min( m_gasDamageFrame, now + d->m_damageInterval );
		}
		else
		{
			m_gasDamageFrame = now + d->m_damageInterval;
		}
	}

	AIUpdateInterface *ai = me->getAI();
	if( ai != nullptr )
	{
		if( m_appliedSpeedScalar == 1.0f && d->m_moveSpeedReduction > 0.0f &&
				matchesKindOf( me, d->m_moveSpeedRequiredAffectKindOf, d->m_moveSpeedRequiresAllKindOfs, d->m_moveSpeedForbiddenAffectKindOf ) )
		{
			m_appliedSpeedScalar = max( 1.0f - d->m_moveSpeedReduction, 0.01f );
			ai->applySpeedMultiplier( m_appliedSpeedScalar );
		}

		if( d->m_disorientInterval != 0 && m_disorientFrame == 0 && me->isAnyKindOf( d->m_disorientKindOf ) )
		{
			m_disorientFrame = now + 1;
		}

		if( me->isAbleToAttack() && attackerMeetsTriggers( attacker, player ) )
		{
			addHallucinationDamage( damageInfo->out.m_actualDamageDealt );
		}
	}

	setWakeFrame(me, calcSleepTime());
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GasBehavior::stopGasEffects()
{
	m_gasDamageFrame = 0;
	m_gasStopFrame = 0;
	m_gasDamageAmount = 0.0f;
	m_gasSource = INVALID_ID;
	m_disorientFrame = 0;

	if( m_appliedSpeedScalar != 1.0f )
	{
		AIUpdateInterface *ai = getObject()->getAI();
		if( ai != nullptr )
		{
			ai->applySpeedMultiplier( 1.0f / m_appliedSpeedScalar );
		}
		m_appliedSpeedScalar = 1.0f;
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GasBehavior::releaseOrders()
{
	if( !m_issuedOrders )
	{
		return;
	}
	m_issuedOrders = FALSE;

	Object *me = getObject();
	AIUpdateInterface *ai = me->getAI();
	if( ai != nullptr && !me->isContained() && !me->isEffectivelyDead() )
	{
		ai->aiIdle( CMD_FROM_AI );
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GasBehavior::addHallucinationDamage( Real amount )
{
	const GasBehaviorModuleData* d = getGasBehaviorModuleData();

	// a zero cap means immune, as with subdual
	Real cap = d->m_hallucinationDamageCap.evaluate( maxHealthOf( getObject() ) );
	if( cap <= 0.0f || amount <= 0.0f )
	{
		return;
	}

	m_hallucinationDamage = min( m_hallucinationDamage + amount, cap );

	// as with subdual, every new dose restarts the drain countdown
	m_hallucinationHealFrame = d->m_hallucinationHealRate != 0 ? TheGameLogic->getFrame() + d->m_hallucinationHealRate : 0;
	updateHallucinating();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GasBehavior::updateHallucinating()
{
	Real maxHealth = maxHealthOf( getObject() );
	Bool hallucinating = maxHealth > 0.0f && m_hallucinationDamage >= maxHealth;
	if( hallucinating && !m_hallucinating )
	{
		FXList::doFXObj( getGasBehaviorModuleData()->m_fxListUpgrade, getObject() );
		m_hallucinateFrame = TheGameLogic->getFrame() + 1;
	}
	else if( !hallucinating && m_hallucinating )
	{
		m_hallucinateFrame = 0;
	}
	m_hallucinating = hallucinating;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GasBehavior::doHallucination()
{
	Object *me = getObject();
	AIUpdateInterface *ai = me->getAI();
	if( ai == nullptr || me->isContained() || !me->isAbleToAttack() )
	{
		return;
	}

	PartitionFilterAlive filterAlive;
	PartitionFilterSameMapStatus filterMapStatus( me );
	PartitionFilterRejectByKindOf filterStructures( MAKE_KINDOF_MASK( KINDOF_STRUCTURE ), KINDOFMASK_NONE );
	PartitionFilterPossibleToAttack filterAttack( ATTACK_NEW_TARGET_FORCED, me, CMD_FROM_AI );
	PartitionFilter *filters[] = { &filterAlive, &filterMapStatus, &filterStructures, &filterAttack, nullptr };

	Object *target = ThePartitionManager->getClosestObject( me, me->getLargestWeaponRange(), FROM_BOUNDINGSPHERE_2D, filters );
	if( target == nullptr )
	{
		return;
	}

	// re-ordering the same target would restart the aim
	if( ai->getGoalObject() == target && me->testStatus( OBJECT_STATUS_IS_ATTACKING ) )
	{
		return;
	}

	ai->aiForceAttackObject( target, NO_MAX_SHOTS_LIMIT, CMD_FROM_AI );
	m_issuedOrders = TRUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GasBehavior::doDisorientation()
{
	const GasBehaviorModuleData* d = getGasBehaviorModuleData();
	Object *me = getObject();
	AIUpdateInterface *ai = me->getAI();
	if( ai == nullptr || me->isContained() )
	{
		return;
	}

	// a hallucinating unit keeps shooting instead of wandering off
	if( m_hallucinating && me->testStatus( OBJECT_STATUS_IS_ATTACKING ) )
	{
		return;
	}

	Real angle = GameLogicRandomValueReal( 0.0f, 2.0f * PI );
	Real dist = GameLogicRandomValueReal( 0.5f * d->m_disorientRadius, d->m_disorientRadius );
	Coord3D pos = *me->getPosition();
	pos.x += dist * Cos( angle );
	pos.y += dist * Sin( angle );

	if( !TheAI->pathfinder()->adjustDestination( me, ai->getLocomotorSet(), &pos, nullptr ) )
	{
		return;
	}

	ai->aiMoveToPosition( &pos, CMD_FROM_AI );
	m_issuedOrders = TRUE;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void GasBehavior::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version
	* 2: Hallucination pool replaces the hallucination stop frame
	*/
// ------------------------------------------------------------------------------------------------
void GasBehavior::xfer( Xfer *xfer )
{

	// version
	const XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	xfer->xferUnsignedInt( &m_gasDamageFrame );
	xfer->xferUnsignedInt( &m_gasStopFrame );
	xfer->xferReal( &m_gasDamageAmount );
	xfer->xferObjectID( &m_gasSource );
	xfer->xferUser( &m_deathType, sizeof(m_deathType) );
	xfer->xferReal( &m_appliedSpeedScalar );
	xfer->xferUnsignedInt( &m_disorientFrame );
	xfer->xferUnsignedInt( &m_hallucinateFrame );
	if( version < 2 )
	{
		UnsignedInt legacyStopFrame = 0;
		xfer->xferUnsignedInt( &legacyStopFrame );
		m_hallucinateFrame = 0;
	}
	xfer->xferBool( &m_issuedOrders );
	if( version >= 2 )
	{
		xfer->xferReal( &m_hallucinationDamage );
		xfer->xferUnsignedInt( &m_hallucinationHealFrame );
		xfer->xferBool( &m_hallucinating );
	}
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void GasBehavior::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
