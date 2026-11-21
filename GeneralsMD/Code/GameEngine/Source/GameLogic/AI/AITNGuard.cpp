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

// FILE: AITNGuard.cpp
/*---------------------------------------------------------------------------*/
/* EA Pacific                                                                */
/* Confidential Information	                                                 */
/* Copyright (C) 2001 - All Rights Reserved                                  */
/* DO NOT DISTRIBUTE                                                         */
/*---------------------------------------------------------------------------*/
/* Project:    RTS3                                                          */
/* File name:  AITNGuard.cpp                                                   */
/* Created:    John Ahlquist., 12/20/2002                              */
/* Desc:       // Set up guard tunnel network states for AI                                 */
/* Revision History:                                                         */
/*		12/20/2002 : Initial creation - modified from AIGuard.cpp                                           */
/*---------------------------------------------------------------------------*/
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GlobalData.h"
#include "Common/PerfTimer.h"
#include "Common/Team.h"
#include "Common/Player.h"
#include "Common/Xfer.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/AITNGuard.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/CollideModule.h"
#include "GameLogic/Module/TunnelContain.h"
#include "GameLogic/Weapon.h"
#include "Common/TunnelTracker.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/PolygonTrigger.h"

const Real CLOSE_ENOUGH = (25.0f);


static Real distanceSquared(const Coord3D& a, const Coord3D& b)
{
	const Real dx = a.x - b.x;
	const Real dy = a.y - b.y;
	return dx * dx + dy * dy;
}

static Bool isOperationalTunnel(const Object* tunnel)
{
	return tunnel && !tunnel->isEffectivelyDead() && !tunnel->isDisabled() &&
		!tunnel->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION) &&
		!tunnel->getStatusBits().test(OBJECT_STATUS_SOLD) &&
		tunnel->getContain() && tunnel->getContain()->isTunnelContain();
}

static Object* findReturnTunnel(Object* unit)
{
	Player* player = unit->getControllingPlayer();
	TunnelTracker* network = player ? player->getTunnelSystem() : nullptr;
	if (!network)
	{
		return nullptr;
	}

	Object* best = nullptr;
	Real bestDistance = 0.0f;
	const std::list<ObjectID>& tunnels = *network->getContainerList();
	for (std::list<ObjectID>::const_iterator it = tunnels.begin(); it != tunnels.end(); ++it)
	{
		Object* tunnel = TheGameLogic->findObjectByID(*it);
		if (!isOperationalTunnel(tunnel) || !tunnel->getContain()->isValidContainerFor(unit, true))
		{
			continue;
		}

		Coord3D exit;
		OpenContain* contain = tunnel->getContain()->asOpenContain();
		if (!contain || !contain->getGuardExitPosition(exit))
		{
			continue;
		}

		const Real distance = distanceSquared(exit, *unit->getPosition());
		if ((!best || distance < bestDistance) &&
			TheAI->pathfinder()->clientSafeQuickDoesPathExist(unit->getAI()->getLocomotorSet(),
				unit->getRequiredBridgeHeight(), unit->getPosition(), &exit))
		{
			best = tunnel;
			bestDistance = distance;
		}
	}
	return best;
}

//-- ExitConditions -------------------------------------------------------------------------------
/**
 * This returns true if the conditions specified have been met, false otherwise.
 */
Bool TunnelNetworkExitConditions::shouldExit(const StateMachine* machine) const
{

		if (TheGameLogic->getFrame() >= m_attackGiveUpFrame)
		{
			return true;
		}

	return false;
}


//-- AITNGuardMachine -------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
AITNGuardMachine::AITNGuardMachine( Object *owner ) :
	StateMachine(owner, "AITNGuardMachine"),
	m_nemesisToAttack(INVALID_ID),
	m_guardMode(GUARDMODE_NORMAL),
	m_leashRadius(0.0f),
	m_detectionRadius(0.0f),
	m_outsideUntil(0),
	m_redeployAt(0)
{
	m_positionToGuard.zero();

	// order matters: first state is the default state.
// srj sez: I made "return" the start state, so that if ordered to guard a position
// that isn't the unit's current position, it moves to that position first.
	defineState( AI_TN_GUARD_RETURN,						newInstance(AITNGuardReturnState)( this ), AI_TN_GUARD_IDLE, AI_TN_GUARD_IDLE );
	defineState( AI_TN_GUARD_IDLE,							newInstance(AITNGuardIdleState)( this ), AI_TN_GUARD_INNER, AI_TN_GUARD_RETURN );
	defineState( AI_TN_GUARD_INNER,						newInstance(AITNGuardInnerState)( this ), AI_TN_GUARD_RETURN, AI_TN_GUARD_RETURN);
	defineState( AI_TN_GUARD_OUTER,						newInstance(AITNGuardOuterState)( this ), AI_TN_GUARD_INNER, AI_TN_GUARD_RETURN );
	defineState( AI_TN_GUARD_GET_CRATE,				newInstance(AITNGuardPickUpCrateState)( this ), AI_TN_GUARD_RETURN, AI_TN_GUARD_RETURN );
	defineState( AI_TN_GUARD_ATTACK_AGGRESSOR, newInstance(AITNGuardAttackAggressorState)( this ), AI_TN_GUARD_RETURN, AI_TN_GUARD_RETURN  );
#ifdef STATE_MACHINE_DEBUG
	setDebugOutput(true);
#endif
}

//--------------------------------------------------------------------------------------
AITNGuardMachine::~AITNGuardMachine()
{
}

//--------------------------------------------------------------------------------------
/*static*/ Real AITNGuardMachine::getStdGuardRange(const Object* obj)
{
	Real visionRange = TheAI->getAdjustedVisionRangeForObject(obj,
		AI_VISIONFACTOR_OWNERTYPE | AI_VISIONFACTOR_MOOD | AI_VISIONFACTOR_GUARDINNER);

	return visionRange;
}

//--------------------------------------------------------------------------------------
static Real guardSetting(Real overrideValue, Real fallback)
{
	return overrideValue >= 0.0f ? overrideValue : fallback;
}

static Bool isInGuardZone(const AsciiString& name, const Coord3D& position)
{
	const PolygonTrigger* zone = TheTerrainLogic->getTriggerAreaByName(name);
	ICoord3D point;
	point.x = (Int)position.x;
	point.y = (Int)position.y;
	point.z = (Int)position.z;
	return zone && zone->pointInTrigger(point);
}

static Real getTunnelGuardRadius(Player* player, const Object* tunnel, const TunnelContainModuleData& settings, const TAiData& defaults)
{
	Real zoneRadius = -1.0f;
	const Int start = player->getMpStartIndex();
	AsciiString zoneName;
	zoneName.format("InnerPerimeter%d", start + 1);
	if (start >= 0 && isInGuardZone(zoneName, *tunnel->getPosition()))
	{
		zoneRadius = guardSetting(settings.m_tunnelGuardInnerPerimeterRadius, defaults.m_tunnelGuardInnerPerimeterRadius);
	}
	else
	{
		zoneName.format("OuterPerimeter%d", start + 1);
		if (start >= 0 && isInGuardZone(zoneName, *tunnel->getPosition()))
		{
			zoneRadius = guardSetting(settings.m_tunnelGuardOuterPerimeterRadius, defaults.m_tunnelGuardOuterPerimeterRadius);
		}
		else if (isInGuardZone(AsciiString("CombatZone"), *tunnel->getPosition()))
		{
			zoneRadius = guardSetting(settings.m_tunnelGuardCombatZoneRadius, defaults.m_tunnelGuardCombatZoneRadius);
		}
	}
	return guardSetting(settings.m_tunnelGuardRadius, guardSetting(zoneRadius, defaults.m_tunnelGuardRadius));
}

Bool AITNGuardMachine::isPathWithinLeash(Path* path) const
{
	if (!path || !path->getLastNode())
	{
		return false;
	}

	Real totalDistance = 0.0f;
	const Coord3D* previous = nullptr;
	for (const PathNode* node = path->getFirstNode(); node; node = node->getNext())
	{
		if (distanceSquared(*node->getPosition(), m_positionToGuard) > m_leashRadius * m_leashRadius)
		{
			return false;
		}
		if (previous)
		{
			totalDistance += sqrtf(distanceSquared(*previous, *node->getPosition()));
		}
		previous = node->getPosition();
	}
	return totalDistance <= m_leashRadius;
}

Bool AITNGuardMachine::canFightFrom(const Coord3D& from, Object* target)
{
	Object* owner = getOwner();
	if (!target || target->isEffectivelyDead() || owner->getRelationship(target) != ENEMIES ||
		owner->isDisabled() || owner->getStatusBits().test(OBJECT_STATUS_NO_ATTACK) ||
		target->getShroudedStatus(owner->getControllingPlayer()->getPlayerIndex()) != OBJECTSHROUD_CLEAR ||
		!owner->chooseBestWeaponForTarget(target, PREFER_MOST_DAMAGE, CMD_FROM_AI))
	{
		return false;
	}

	WeaponSlotType slot = PRIMARY_WEAPON;
	const Weapon* weapon = owner->getCurrentWeapon(&slot);
	const AbleToAttackType attackType = owner->getContainedBy() ? ATTACK_TUNNEL_NETWORK_GUARD : ATTACK_NEW_TARGET;
	const CanAttackResult attack = owner->getAbleToAttackSpecificObject(attackType, target, CMD_FROM_AI, slot);
	if (!weapon || (attack != ATTACKRESULT_POSSIBLE && attack != ATTACKRESULT_POSSIBLE_AFTER_MOVING) ||
		distanceSquared(from, m_positionToGuard) > m_leashRadius * m_leashRadius)
	{
		return false;
	}

	Pathfinder* pathfinder = TheAI->pathfinder();
	const LocomotorSet& locomotor = owner->getAI()->getLocomotorSet();
	const Coord3D* targetPos = target->getPosition();
	if (pathfinder->isValidAttackGoal(owner, locomotor, from, target, targetPos, weapon))
	{
		return true;
	}

	Path* path = pathfinder->findAttackPath(owner, locomotor, &from, target, targetPos, weapon);
	const Bool usable = isPathWithinLeash(path) &&
		pathfinder->isValidAttackGoal(owner, locomotor, *path->getLastNode()->getPosition(), target, targetPos, weapon);
	if (path)
	{
		deleteInstance(path);
	}
	return usable;
}

Bool AITNGuardMachine::deploy()
{
	Object* owner = getOwner();
	Player* player = owner->getControllingPlayer();
	TunnelTracker* network = player ? player->getTunnelSystem() : nullptr;
	const UnsignedInt now = TheGameLogic->getFrame();
	if (!network || now < m_redeployAt || !owner->getContainedBy()->getContain() ||
		!owner->getContainedBy()->getContain()->isTunnelContain())
	{
		return false;
	}

	const TAiData& defaults = *TheAI->getAiData();
	PartitionFilterRelationship enemy(owner, PartitionFilterRelationship::ALLOW_ENEMIES);
	PartitionFilterFreeOfFog clearFog(player->getPlayerIndex());
	PartitionFilterSameMapStatus mapStatus(owner);
	PartitionFilter* visibleEnemies[] = { &enemy, &clearFog, &mapStatus, nullptr };

	TunnelContain* best = nullptr;
	ObjectID bestTarget = INVALID_ID;
	Coord3D bestExit;
	Real bestDistance = 0.0f;
	Real bestLeash = 0.0f;
	Real bestRadius = 0.0f;
	const std::list<ObjectID>& tunnels = *network->getContainerList();
	for (std::list<ObjectID>::const_iterator it = tunnels.begin(); it != tunnels.end(); ++it)
	{
		Object* tunnel = TheGameLogic->findObjectByID(*it);
		if (!isOperationalTunnel(tunnel) || network->isAutoExitTunnel(tunnel) || tunnel->getLayer() != owner->getLayer())
		{
			continue;
		}

		TunnelContain* contain = static_cast<TunnelContain*>(tunnel->getContain()->asOpenContain());
		Coord3D exitPosition;
		if (!contain->getGuardExitPosition(exitPosition) || contain->isExitBusy() ||
			!TheAI->pathfinder()->goalBoxFree(owner, &exitPosition))
		{
			continue;
		}

		const TunnelContainModuleData& settings = *contain->getGuardSettings();
		const Real radius = getTunnelGuardRadius(player, tunnel, settings, defaults);
		const Real clearRadius = guardSetting(settings.m_tunnelGuardExitClearRadius, defaults.m_tunnelGuardExitClearRadius);
		if (radius <= 0.0f || guardSetting(settings.m_tunnelGuardMaxOutsideTime, defaults.m_tunnelGuardMaxOutsideTime) < 1.0f ||
			(clearRadius > 0.0f && ThePartitionManager->getClosestObject(&exitPosition, clearRadius, FROM_CENTER_2D, visibleEnemies)))
		{
			continue;
		}

		m_positionToGuard = exitPosition;
		m_leashRadius = guardSetting(settings.m_tunnelGuardLeashRadius, guardSetting(defaults.m_tunnelGuardLeashRadius, radius));
		ObjectIterator* targets = ThePartitionManager->iterateObjectsInRange(&exitPosition, radius,
			FROM_CENTER_2D, visibleEnemies, ITER_SORTED_NEAR_TO_FAR);
		MemoryPoolObjectHolder targetHolder(targets);
		for (Object* target = targets->first(); target; target = targets->next())
		{
			const Real distance = distanceSquared(*target->getPosition(), exitPosition);
			if (best && distance >= bestDistance)
			{
				break;
			}
			if (canFightFrom(exitPosition, target))
			{
				best = contain;
				bestTarget = target->getID();
				bestExit = exitPosition;
				bestDistance = distance;
				bestLeash = m_leashRadius;
				bestRadius = radius;
				break;
			}
		}
	}

	if (!best)
	{
		return false;
	}

	m_nemesisToAttack = bestTarget;
	m_positionToGuard = bestExit;
	m_leashRadius = bestLeash;
	m_detectionRadius = bestRadius;
	const TunnelContainModuleData& settings = *best->getGuardSettings();
	m_outsideUntil = now + (UnsignedInt)guardSetting(settings.m_tunnelGuardMaxOutsideTime, defaults.m_tunnelGuardMaxOutsideTime);
	m_redeployAt = (UnsignedInt)guardSetting(settings.m_tunnelGuardRedeployDelay, defaults.m_tunnelGuardRedeployDelay);
	best->exitObjectInAHurry(owner);
	return true;
}

void AITNGuardMachine::onReturned()
{
	if (m_outsideUntil)
	{
		m_outsideUntil = 0;
		m_redeployAt += TheGameLogic->getFrame();
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void AITNGuardMachine::crc( Xfer *xfer )
{
}

// ------------------------------------------------------------------------------------------------
/** Xfer Method */
// ------------------------------------------------------------------------------------------------
void AITNGuardMachine::xfer( Xfer *xfer )
{
  // version
  XferVersion currentVersion = 3;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

	if (version>=2) {
		StateMachine::xfer(xfer);	// Forgot this in initial implementation.  jba.
	}

	xfer->xferObjectID(&m_nemesisToAttack);
	xfer->xferCoord3D(&m_positionToGuard);
	if (version >= 3)
	{
		xfer->xferReal(&m_leashRadius);
		xfer->xferReal(&m_detectionRadius);
		xfer->xferUnsignedInt(&m_outsideUntil);
		xfer->xferUnsignedInt(&m_redeployAt);
	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void AITNGuardMachine::loadPostProcess()
{
}

//-- AITNGuardInnerState ----------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void AITNGuardInnerState::crc( Xfer *xfer )
{
}

// ------------------------------------------------------------------------------------------------
/** Xfer Method */
// ------------------------------------------------------------------------------------------------
void AITNGuardInnerState::xfer( Xfer *xfer )
{
  // version
  XferVersion currentVersion = 1;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void AITNGuardInnerState::loadPostProcess()
{
	onEnter();
}

// ------------------------------------------------------------------------------------------------
AITNGuardInnerState::~AITNGuardInnerState()
{
	deleteInstance(m_attackState);
}

//--------------------------------------------------------------------------------------
StateReturnType AITNGuardInnerState::onEnter()
{
	Object* nemesis = TheGameLogic->findObjectByID(getGuardMachine()->getNemesisID()) ;
	if (!getGuardMachine()->canFightFrom(*getMachineOwner()->getPosition(), nemesis))
	{
		return STATE_SUCCESS;
	}
	m_exitConditions.m_attackGiveUpFrame = getGuardMachine()->getOutsideUntil();

	m_attackState = newInstance(AIAttackState)(getMachine(), false, true, false, &m_exitConditions);

	m_attackState->getMachine()->setGoalObject(nemesis);

	StateReturnType returnVal = m_attackState->onEnter();
	if (returnVal == STATE_CONTINUE) {
		return STATE_CONTINUE;
	}

	// if we had no one to attack, we were successful, so go to the next state.
	return STATE_SUCCESS;
}

StateReturnType AITNGuardInnerState::update()
{
	AITNGuardMachine* guard = getGuardMachine();
	Object* owner = getMachineOwner();
	Object* target = TheGameLogic->findObjectByID(guard->getNemesisID());
	Path* path = owner->getAI()->getPath();
	if (!m_attackState || !target || target->isEffectivelyDead() ||
		owner->getRelationship(target) != ENEMIES ||
		distanceSquared(*target->getPosition(), *guard->getPositionToGuard()) >
			guard->getDetectionRadius() * guard->getDetectionRadius() ||
		target->getShroudedStatus(owner->getControllingPlayer()->getPlayerIndex()) != OBJECTSHROUD_CLEAR ||
		TheGameLogic->getFrame() >= guard->getOutsideUntil() ||
		distanceSquared(*owner->getPosition(), *guard->getPositionToGuard()) >
			guard->getLeashRadius() * guard->getLeashRadius() ||
		(path && !guard->isPathWithinLeash(path)))
	{
		return STATE_SUCCESS;
	}
	return m_attackState->update();
}

//--------------------------------------------------------------------------------------
void AITNGuardInnerState::onExit( StateExitType status )
{
	if (m_attackState)
	{
		m_attackState->onExit(status);
		deleteInstance(m_attackState);
		m_attackState = nullptr;
	}
}

//-- AITNGuardOuterState ----------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void AITNGuardOuterState::crc( Xfer *xfer )
{
}

// ------------------------------------------------------------------------------------------------
/** Xfer Method */
// ------------------------------------------------------------------------------------------------
void AITNGuardOuterState::xfer( Xfer *xfer )
{
  // version
  XferVersion currentVersion = 1;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void AITNGuardOuterState::loadPostProcess()
{						 AITNGuardOuterState
	onEnter();
}

// ------------------------------------------------------------------------------------------------
AITNGuardOuterState::~AITNGuardOuterState()
{
	deleteInstance(m_attackState);
}

//--------------------------------------------------------------------------------------
StateReturnType AITNGuardOuterState::onEnter()
{
	return STATE_CONTINUE;
}

StateReturnType AITNGuardOuterState::update()
{
	AITNGuardMachine* guard = getGuardMachine();
	Object* owner = getMachineOwner();
	const UnsignedInt now = TheGameLogic->getFrame();
	if (now >= guard->getOutsideUntil() || owner->getContainedBy() ||
		distanceSquared(*owner->getPosition(), *guard->getPositionToGuard()) >
			guard->getLeashRadius() * guard->getLeashRadius())
	{
		return STATE_FAILURE;
	}

	PartitionFilterRelationship enemies(owner, PartitionFilterRelationship::ALLOW_ENEMIES);
	PartitionFilterFreeOfFog visibility(owner->getControllingPlayer()->getPlayerIndex());
	PartitionFilterSameMapStatus mapStatus(owner);
	PartitionFilter* filters[] = { &enemies, &visibility, &mapStatus, nullptr };
	ObjectIterator* targets = ThePartitionManager->iterateObjectsInRange(guard->getPositionToGuard(),
		guard->getDetectionRadius(), FROM_CENTER_2D, filters, ITER_SORTED_NEAR_TO_FAR);
	MemoryPoolObjectHolder holder(targets);
	for (Object* target = targets->first(); target; target = targets->next())
	{
		if (guard->canFightFrom(*owner->getPosition(), target))
		{
			guard->setNemesisID(target->getID());
			return STATE_SUCCESS;
		}
	}

	const UnsignedInt scanRate = TheAI->getAiData()->m_guardEnemyScanRate;
	const UnsignedInt remaining = guard->getOutsideUntil() - now;
	return STATE_SLEEP(scanRate < remaining ? scanRate : remaining);
}

void AITNGuardOuterState::onExit( StateExitType status )
{
}

//-- AITNGuardReturnState ----------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void AITNGuardReturnState::crc( Xfer *xfer )
{
	AIEnterState::crc(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Xfer Method */
// ------------------------------------------------------------------------------------------------
void AITNGuardReturnState::xfer( Xfer *xfer )
{
  // version
  XferVersion currentVersion = 1;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

	AIEnterState::xfer(xfer);

	xfer->xferUnsignedInt(&m_nextReturnScanTime);
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void AITNGuardReturnState::loadPostProcess()
{
	AIEnterState::loadPostProcess();
}

//--------------------------------------------------------------------------------------
StateReturnType AITNGuardReturnState::onEnter()
{
	UnsignedInt now = TheGameLogic->getFrame();
	m_nextReturnScanTime = now + GameLogicRandomValue(0, TheAI->getAiData()->m_guardEnemyReturnScanRate);

	if (getMachineOwner()->getContainedBy()) {
		// we are already inside our tunnel.  Return success. jba [8/24/2003]
		return STATE_SUCCESS;
	}
	Object *bestTunnel = findReturnTunnel(getMachineOwner());
	if (bestTunnel==nullptr) return STATE_FAILURE;

	getMachine()->setGoalObject(bestTunnel);
	getMachineOwner()->getAI()->friend_setGoalObject(bestTunnel);

	return AIEnterState::onEnter();
}

//--------------------------------------------------------------------------------------
StateReturnType AITNGuardReturnState::update()
{
	StateReturnType ret = AIEnterState::update();
	if (ret==STATE_CONTINUE) return STATE_CONTINUE;
	return STATE_SUCCESS;

}

//--------------------------------------------------------------------------------------
void AITNGuardReturnState::onExit( StateExitType status )
{
	AIEnterState::onExit( status );
}

//-- AITNGuardIdleState ----------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void AITNGuardIdleState::crc( Xfer *xfer )
{
}

// ------------------------------------------------------------------------------------------------
/** Xfer Method */
// ------------------------------------------------------------------------------------------------
void AITNGuardIdleState::xfer( Xfer *xfer )
{
  // version
  XferVersion currentVersion = 1;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

	xfer->xferUnsignedInt(&m_nextEnemyScanTime);
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void AITNGuardIdleState::loadPostProcess()
{
}

//--------------------------------------------------------------------------------------
StateReturnType AITNGuardIdleState::onEnter()
{
	// first time thru, use a random amount so that everyone doesn't scan on the same frame,
	// to avoid "spikes".
	UnsignedInt now = TheGameLogic->getFrame();
	m_nextEnemyScanTime = now + GameLogicRandomValue(0, TheAI->getAiData()->m_guardEnemyScanRate);

	getMachineOwner()->getAI()->friend_setGoalObject(nullptr);
	return STATE_CONTINUE;
}

//--------------------------------------------------------------------------------------
StateReturnType AITNGuardIdleState::update()
{
	//DEBUG_LOG(("AITNGuardIdleState frame %d: %08lx",TheGameLogic->getFrame(),getMachineOwner()));

	UnsignedInt now = TheGameLogic->getFrame();
	if (now < m_nextEnemyScanTime)
		return STATE_SLEEP(m_nextEnemyScanTime - now);

	m_nextEnemyScanTime = now + TheAI->getAiData()->m_guardEnemyScanRate;

	getMachineOwner()->getAI()->friend_setGoalObject(nullptr);

#ifdef STATE_MACHINE_DEBUG
	//getMachine()->setDebugOutput(true);
#endif
	Object *owner = getMachineOwner();
	if (owner->getContainedBy())
	{
		getGuardMachine()->onReturned();
		if (getGuardMachine()->deploy())
		{
			return STATE_SLEEP(0);
		}
	}
	else if (getGuardMachine()->getOutsideUntil() > now)
	{
		return STATE_SUCCESS;
	}

	if (!owner->getContainedBy() && findReturnTunnel(owner)) {
		return STATE_FAILURE;	 // go to AITNGuardReturnState, & enter a tunnel.
	}

	return STATE_SLEEP(m_nextEnemyScanTime - now);
}

//--------------------------------------------------------------------------------------
void AITNGuardIdleState::onExit( StateExitType status )
{
}

//-- AITNGuardPickUpCrateState ----------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
AITNGuardPickUpCrateState::AITNGuardPickUpCrateState( StateMachine *machine )	: AIPickUpCrateState(machine)
{
#ifdef STATE_MACHINE_DEBUG
		setName("AITNGuardPickUpCrateState");
#endif
}
//--------------------------------------------------------------------------------------
StateReturnType AITNGuardPickUpCrateState::onEnter()
{
	Object *owner = getMachineOwner();
	AIUpdateInterface *ai = owner->getAIUpdateInterface();

	// Check to see if we have created a crate we need to pick up.
	Object* crate = ai->checkForCrateToPickup();
	if (crate)
	{
		getMachine()->setGoalObject(crate);
		return AIPickUpCrateState::onEnter();
	}

	return STATE_SUCCESS; // no crate, so we're done.
}

//--------------------------------------------------------------------------------------
StateReturnType AITNGuardPickUpCrateState::update()
{
	return AIPickUpCrateState::update();
}

//--------------------------------------------------------------------------------------
void AITNGuardPickUpCrateState::onExit( StateExitType status )
{

}

//-- AITNGuardAttackAggressorState ------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
AITNGuardAttackAggressorState::AITNGuardAttackAggressorState( StateMachine *machine ) :
	State( machine, "AITNGuardAttackAggressorState" )
{
	m_attackState = nullptr;
}

// ------------------------------------------------------------------------------------------------
AITNGuardAttackAggressorState::~AITNGuardAttackAggressorState()
{
	deleteInstance(m_attackState);
}

//-------------------------------------------------------------------------------------------------
StateReturnType AITNGuardAttackAggressorState::onEnter()
{
	Object *obj = getMachineOwner();
	ObjectID nemID = INVALID_ID;

	if (obj->getBodyModule() && obj->getBodyModule()->getLastDamageInfo()->in.m_sourceID) {
		nemID = obj->getBodyModule()->getLastDamageInfo()->in.m_sourceID;
		getGuardMachine()->setNemesisID(nemID);

	}

	Object *nemesis = TheGameLogic->findObjectByID(getGuardMachine()->getNemesisID());
	if (nemesis == nullptr)
	{
		DEBUG_LOG(("Unexpected null nemesis in AITNGuardAttackAggressorState."));
		return STATE_SUCCESS;
	}

	Player *ownerPlayer = getMachineOwner()->getControllingPlayer();
	TunnelTracker *tunnels = nullptr;
	if (ownerPlayer) {
		tunnels = ownerPlayer->getTunnelSystem();
	}
	if (tunnels) tunnels->updateNemesis(nemesis);

	m_exitConditions.m_attackGiveUpFrame = TheGameLogic->getFrame() + TheAI->getAiData()->m_guardChaseUnitFrames;
	m_attackState = newInstance(AIAttackState)(getMachine(), true, true, false, &m_exitConditions);
	m_attackState->getMachine()->setGoalObject(nemesis);

	StateReturnType returnVal = m_attackState->onEnter();
	if (returnVal == STATE_CONTINUE) {
		return STATE_CONTINUE;
	}

	// if we had no one to attack, we were successful, so go to the next state.
	return STATE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
StateReturnType AITNGuardAttackAggressorState::update()
{
	if (m_attackState->getMachine()->getCurrentStateID() == AttackStateMachine::FIRE_WEAPON) {
		Object *nemesis = TheGameLogic->findObjectByID(getGuardMachine()->getNemesisID());
		Player *ownerPlayer = getMachineOwner()->getControllingPlayer();
		TunnelTracker *tunnels = nullptr;
		if (ownerPlayer) {
			tunnels = ownerPlayer->getTunnelSystem();
		}
		if (tunnels) tunnels->updateNemesis(nemesis);
	}
	return m_attackState->update();
}

//-------------------------------------------------------------------------------------------------
void AITNGuardAttackAggressorState::onExit( StateExitType status )
{
	Object *obj = getMachineOwner();
	if (m_attackState)
	{
		m_attackState->onExit(status);
		deleteInstance(m_attackState);
		m_attackState = nullptr;
	}

	if (obj->getTeam())
	{
		obj->getTeam()->setTeamTargetObject(nullptr); // clear the target.
	}
}

//-------------------------------------------------------------------------------------------------
void AITNGuardAttackAggressorState::crc( Xfer *xfer )
{

}

//-------------------------------------------------------------------------------------------------
void AITNGuardAttackAggressorState::xfer( Xfer *xfer )
{
  // version
  XferVersion currentVersion = 1;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

}

//-------------------------------------------------------------------------------------------------
void AITNGuardAttackAggressorState::loadPostProcess()
{
	onEnter();
}

