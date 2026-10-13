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

// FILE: ScriptConditionsTEODTeams.cpp ////////////////////////////////////////////////////////////
// TEOD script conditions about teams, units, ratios and captures.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectTypes.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptConditions.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/ScriptTEOD.h"
#include "GameLogic/Scripts.h"

//-------------------------------------------------------------------------------------------------
/** Accepts objects the player can see right now. */
//-------------------------------------------------------------------------------------------------
class PartitionFilterTEODSeenBy : public PartitionFilter
{
public:
	PartitionFilterTEODSeenBy(const Player *player) : m_player(player) { }
	virtual Bool allow(Object *other) override
	{
		return teodIsSeenByPlayer(other, m_player);
	}
#if defined(RTS_DEBUG)
	virtual const char* debugGetName() override { return "PartitionFilterTEODSeenBy"; }
#endif
private:
	const Player *m_player;
};

//-------------------------------------------------------------------------------------------------
/** True for units, meaning objects that are not structures, inert objects or projectiles. */
//-------------------------------------------------------------------------------------------------
static Bool teodIsUnit(const Object *obj)
{
	return !obj->isKindOf(KINDOF_STRUCTURE) && !obj->isKindOf(KINDOF_INERT) && !obj->isKindOf(KINDOF_PROJECTILE);
}

class PartitionFilterTEODUnits : public PartitionFilter
{
public:
	PartitionFilterTEODUnits() { }
	virtual Bool allow(Object *other) override
	{
		return teodIsUnit(other);
	}
#if defined(RTS_DEBUG)
	virtual const char* debugGetName() override { return "PartitionFilterTEODUnits"; }
#endif
};

//-------------------------------------------------------------------------------------------------
/** Rejects contained objects, members of the team and the object itself. */
//-------------------------------------------------------------------------------------------------
class PartitionFilterTEODRejectSelf : public PartitionFilter
{
public:
	PartitionFilterTEODRejectSelf(const Team *team, const Object *obj) : m_team(team), m_obj(obj) { }
	virtual Bool allow(Object *other) override
	{
		return !other->isContained() && other != m_obj && (!m_team || other->getTeam() != m_team);
	}
#if defined(RTS_DEBUG)
	virtual const char* debugGetName() override { return "PartitionFilterTEODRejectSelf"; }
#endif
private:
	const Team *m_team;
	const Object *m_obj;
};

//-------------------------------------------------------------------------------------------------
static Bool teodCompare(Int comparison, Real value, Real target, Real tolerance)
{
	switch (comparison)
	{
		case Parameter::LESS_THAN:			return value < target;
		case Parameter::LESS_EQUAL:			return value <= target;
		case Parameter::EQUAL:					return fabs(value - target) < tolerance;
		case Parameter::GREATER_EQUAL:	return value >= target;
		case Parameter::GREATER:				return value > target;
		case Parameter::NOT_EQUAL:			return fabs(value - target) >= tolerance;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
static Bool teodIsKnownType(const AsciiString &objectType)
{
	return TheScriptEngine->getObjectTypes(objectType) || TheThingFactory->findTemplate(objectType, FALSE);
}

//-------------------------------------------------------------------------------------------------
static Bool teodTemplateMatchesType(const ThingTemplate *tmpl, const AsciiString &objectType)
{
	const ObjectTypes *types = TheScriptEngine->getObjectTypes(objectType);
	if (types)
	{
		return types->isInSet(tmpl);
	}
	const ThingTemplate *thing = TheThingFactory->findTemplate(objectType, FALSE);
	return thing && thing->isEquivalentTo(tmpl);
}

//-------------------------------------------------------------------------------------------------
static Object *teodFirstLiveMember(const Team *team)
{
	for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance())
	{
		if (!iter.cur()->isEffectivelyDead())
		{
			return iter.cur();
		}
	}
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/** Counts live objects near pos with a relation to observer that match objectType, or are units
	when objectType is null. With mustBeSeen, only objects the observer's player sees count. */
//-------------------------------------------------------------------------------------------------
static Int teodCountInRange(const Object *observer, const Coord3D *pos, Real range, Int relationFlags, Bool mustBeSeen, const AsciiString *objectType)
{
	if (range <= 0.0f)
	{
		return 0;
	}
	PartitionFilterRelationship filterRelation(observer, relationFlags);
	PartitionFilterAlive filterAlive;
	PartitionFilterSameMapStatus filterMapStatus(observer);
	PartitionFilterTEODSeenBy filterSeen(observer->getControllingPlayer());
	PartitionFilter *filters[] = { &filterRelation, &filterAlive, &filterMapStatus, nullptr, nullptr };
	if (mustBeSeen)
	{
		filters[3] = &filterSeen;
	}

	SimpleObjectIterator *iter = ThePartitionManager->iterateObjectsInRange(pos, range, FROM_CENTER_2D, filters);
	MemoryPoolObjectHolder hold(iter);
	Int count = 0;
	for (Object *obj = iter->first(); obj; obj = iter->next())
	{
		if (objectType ? teodObjectMatchesType(obj, *objectType) : teodIsUnit(obj))
		{
			++count;
		}
	}
	return count;
}

//-------------------------------------------------------------------------------------------------
/** True if one of the player's structures is being captured. A null objectType or trigger
	does not filter. */
//-------------------------------------------------------------------------------------------------
static Bool teodBuildingBeingCaptured(Player *player, const AsciiString *objectType, const PolygonTrigger *trigger)
{
	Player::PlayerTeamList::const_iterator it;
	for (it = player->getPlayerTeams()->begin(); it != player->getPlayerTeams()->end(); ++it)
	{
		for (DLINK_ITERATOR<Team> teamIter = (*it)->iterate_TeamInstanceList(); !teamIter.done(); teamIter.advance())
		{
			for (DLINK_ITERATOR<Object> iter = teamIter.cur()->iterate_TeamMemberList(); !iter.done(); iter.advance())
			{
				Object *obj = iter.cur();
				if (!obj->isKindOf(KINDOF_STRUCTURE) || !TheScriptTEODState->isBeingCaptured(obj))
				{
					continue;
				}
				if (objectType && !teodObjectMatchesType(obj, *objectType))
				{
					continue;
				}
				if (trigger && !obj->isInside(trigger))
				{
					continue;
				}
				return TRUE;
			}
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** Compares the 2D distance from pos to the closest live object of objectType that has relation
	to ref, ignoring contained objects, members of skipTeam and ref itself. */
//-------------------------------------------------------------------------------------------------
static Bool teodClosestRelationType(const Object *ref, const Team *skipTeam, const Coord3D *pos, Int relation, const AsciiString &objectType, Int comparison, Real value)
{
	if (!ref || !pos || relation < ENEMIES || relation > ALLIES)
	{
		return FALSE;
	}
	PartitionFilterRelationship filterRelation(ref, 1 << relation);
	PartitionFilterSameMapStatus filterMapStatus(ref);
	PartitionFilterTEODRejectSelf filterSelf(skipTeam, ref);
	PartitionFilter *filters[] = { &filterRelation, &filterMapStatus, &filterSelf, nullptr };
	Object *closest = teodSelectObject(pos, filters, objectType, Parameter::CLOSEST);
	if (!closest)
	{
		return FALSE;
	}
	Real dx = closest->getPosition()->x - pos->x;
	Real dy = closest->getPosition()->y - pos->y;
	return teodCompare(comparison, sqrtf(dx * dx + dy * dy), value, 0.01f);
}

//-------------------------------------------------------------------------------------------------
/** Finds the team member closest to the closest seen enemy, then compares the enemies it sees to
	the allies within radius of it times ratio. Null types count units. */
//-------------------------------------------------------------------------------------------------
static Bool teodClosestMemberSightedRatio(Team *team, Int comparison, Real ratio, const AsciiString *allyType, const AsciiString *enemyType, Real radius)
{
	Object *ref = teodFirstLiveMember(team);
	const Coord3D *teamPos = team->getEstimateTeamPosition();
	if (!ref || !teamPos)
	{
		return FALSE;
	}

	PartitionFilterSameMapStatus filterMapStatus(ref);
	PartitionFilterRelationship filterEnemies(ref, PartitionFilterRelationship::ALLOW_ENEMIES);
	PartitionFilterTEODSeenBy filterSeen(team->getControllingPlayer());
	PartitionFilterTEODUnits filterUnits;
	PartitionFilter *enemyFilters[] = { &filterMapStatus, &filterEnemies, &filterSeen, nullptr, nullptr };
	if (!enemyType)
	{
		enemyFilters[3] = &filterUnits;
	}
	Object *enemy = teodSelectObject(teamPos, enemyFilters, enemyType ? *enemyType : AsciiString::TheEmptyString, Parameter::CLOSEST);
	if (!enemy)
	{
		return FALSE;
	}

	PartitionFilterAcceptOnTeam filterTeam(team);
	PartitionFilter *memberFilters[] = { &filterMapStatus, &filterTeam, nullptr };
	Object *member = teodSelectObject(enemy->getPosition(), memberFilters, AsciiString::TheEmptyString, Parameter::CLOSEST);
	if (!member)
	{
		return FALSE;
	}

	Real countEnemies = (Real)teodCountInRange(member, member->getPosition(), member->getShroudClearingRange(), PartitionFilterRelationship::ALLOW_ENEMIES, TRUE, enemyType);
	if (countEnemies < 1.0f)
	{
		return FALSE;
	}
	Real countAllies = (Real)teodCountInRange(member, member->getPosition(), radius, PartitionFilterRelationship::ALLOW_ALLIES, FALSE, allyType);
	return teodCompare(comparison, countEnemies, countAllies * ratio, 0.1f);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamSightedRelationType(Parameter* pTeam, Int relationType, Parameter* pObjectType)
{
	Team *team = TheScriptEngine->getTeamNamed(pTeam->getString());
	if (!team || relationType < ENEMIES || relationType > ALLIES)
	{
		return FALSE;
	}

	PartitionFilterAlive filterAlive;
	PartitionFilterRejectByObjectStatus filterStealth(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_STEALTHED),
		MAKE_OBJECT_STATUS_MASK2(OBJECT_STATUS_DETECTED, OBJECT_STATUS_DISGUISED));

	for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance())
	{
		Object *member = iter.cur();
		if (member->isEffectivelyDead())
		{
			continue;
		}
		Real visionRange = member->getVisionRange();
		if (visionRange <= 0.0f)
		{
			continue;
		}

		PartitionFilterRelationship filterRelation(member, 1 << relationType);
		PartitionFilterSameMapStatus filterMapStatus(member);
		PartitionFilterTEODRejectSelf filterSelf(team, member);
		PartitionFilter *filters[] = { &filterRelation, &filterAlive, &filterStealth, &filterMapStatus, &filterSelf, nullptr };

		SimpleObjectIterator *them = ThePartitionManager->iterateObjectsInRange(member, visionRange, FROM_CENTER_2D, filters);
		MemoryPoolObjectHolder hold(them);
		for (Object *other = them->first(); other; other = them->next())
		{
			if (teodObjectMatchesType(other, pObjectType->getString()))
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamLostType(Condition* pCondition, Parameter* pTeamParm, Parameter* objectType)
{
	UnsignedInt since = TheScriptTEODState->readEvents(pCondition);
	Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!team)
	{
		return FALSE;
	}
	const ScriptTEODState::EventList &losses = TheScriptTEODState->getTeamLosses(team->getID());
	for (size_t i = 0; i < losses.size(); ++i)
	{
		if (losses[i].seq > since && teodTemplateMatchesType(losses[i].thing, objectType->getString()))
		{
			return TRUE;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamLostUnit(Condition* pCondition, Parameter* pTeamParm)
{
	UnsignedInt since = TheScriptTEODState->readEvents(pCondition);
	Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!team)
	{
		return FALSE;
	}
	const ScriptTEODState::EventList &losses = TheScriptTEODState->getTeamLosses(team->getID());
	return !losses.empty() && losses.back().seq > since;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerBuildingBeingCaptured(Parameter* pPlayerParm)
{
	Player *player = playerFromParam(pPlayerParm);
	if (!player)
	{
		return FALSE;
	}
	return teodBuildingBeingCaptured(player, nullptr, nullptr);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerBuildingBeingCapturedType(Parameter* pPlayerParm, Parameter* objectType)
{
	Player *player = playerFromParam(pPlayerParm);
	if (!player)
	{
		return FALSE;
	}
	return teodBuildingBeingCaptured(player, &objectType->getString(), nullptr);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerBuildingBeingCapturedArea(Parameter* pPlayerParm, Parameter* pTriggerParm)
{
	Player *player = playerFromParam(pPlayerParm);
	PolygonTrigger *trigger = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!player || !trigger)
	{
		return FALSE;
	}
	return teodBuildingBeingCaptured(player, nullptr, trigger);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerBuildingBeingCapturedTypeArea(Parameter* pPlayerParm, Parameter* objectType, Parameter* pTriggerParm)
{
	Player *player = playerFromParam(pPlayerParm);
	PolygonTrigger *trigger = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!player || !trigger)
	{
		return FALSE;
	}
	return teodBuildingBeingCaptured(player, &objectType->getString(), trigger);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamIdle(Parameter* pTeamParm)
{
	Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!team)
	{
		return FALSE;
	}
	Int idleFrames = TheScriptTEODState->getTeamIdleFrames(team);
	return idleFrames > 0 && team->hasAnyObjects();
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamIdleFrames(Parameter* pTeam, Int value)
{
	Team *team = TheScriptEngine->getTeamNamed(pTeam->getString());
	if (!team)
	{
		return FALSE;
	}
	Int idleFrames = TheScriptTEODState->getTeamIdleFrames(team);
	return idleFrames >= value && team->hasAnyObjects();
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamSeen(Parameter* pTeamParm)
{
	Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!team)
	{
		return FALSE;
	}

	std::vector<const Player *> enemies;
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		const Player *player = ThePlayerList->getNthPlayer(i);
		if (player && player->getRelationship(team) == ENEMIES)
		{
			enemies.push_back(player);
		}
	}

	for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance())
	{
		const Object *member = iter.cur();
		if (member->isEffectivelyDead())
		{
			continue;
		}
		for (size_t i = 0; i < enemies.size(); ++i)
		{
			if (teodIsSeenByPlayer(member, enemies[i]))
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamContainsType(Parameter* pTeamParm, Parameter* objectType)
{
	Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!team)
	{
		return FALSE;
	}
	for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance())
	{
		if (!iter.cur()->isEffectivelyDead() && teodObjectMatchesType(iter.cur(), objectType->getString()))
		{
			return TRUE;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamContainsComparisonType(Parameter* pTeamParm, Parameter* pComparisonParm, Int value, Parameter* objectType)
{
	Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!team)
	{
		return FALSE;
	}
	Int count = 0;
	for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance())
	{
		if (!iter.cur()->isEffectivelyDead() && teodObjectMatchesType(iter.cur(), objectType->getString()))
		{
			++count;
		}
	}
	return teodCompare(pComparisonParm->getInt(), (Real)count, (Real)value, 0.5f);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamSingleBelowHealth(Parameter* pTeamParm, Real value)
{
	Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!team)
	{
		return FALSE;
	}
	for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance())
	{
		const Object *member = iter.cur();
		const BodyModuleInterface *body = member->getBodyModule();
		if (member->isEffectivelyDead() || !body)
		{
			continue;
		}
		if (body->getMaxHealth() * (value / 100.0f) > body->getHealth())
		{
			return TRUE;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamBelowHealth(Parameter* pTeamParm, Real value)
{
	Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!team)
	{
		return FALSE;
	}
	Real healthPercent = TheScriptTEODState->getTeamHealthPercent(team);
	return healthPercent < value && team->hasAnyObjects();
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamHasComparisonRatioSighted(Parameter* pTeamParm, Parameter* pComparisonParm, Real ratio)
{
	Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!team)
	{
		return FALSE;
	}

	Object *observer = nullptr;
	Real count = 0.0f;
	Real maxVision = 0.0f;
	for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance())
	{
		Object *member = iter.cur();
		if (member->isEffectivelyDead() || (member->isKindOf(KINDOF_INERT) && !member->isKindOf(KINDOF_CRATE)))
		{
			continue;
		}
		if (!observer)
		{
			observer = member;
		}
		maxVision = max(maxVision, member->getShroudClearingRange());
		count += 1.0f;
	}

	const Coord3D *teamPos = team->getEstimateTeamPosition();
	if (!observer || !teamPos)
	{
		return FALSE;
	}
	Real countOther = (Real)teodCountInRange(observer, teamPos, maxVision, PartitionFilterRelationship::ALLOW_ENEMIES, TRUE, nullptr);
	return teodCompare(pComparisonParm->getInt(), count, countOther * ratio, 0.1f);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamHasComparisonRatioTypeSighted(Parameter* pTeamParm, Parameter* pComparisonParm, Real ratio, Parameter* objectType, Parameter* otherObjectType)
{
	Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!team)
	{
		return FALSE;
	}

	Object *observer = nullptr;
	Real count = 0.0f;
	Real maxVision = 0.0f;
	for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance())
	{
		Object *member = iter.cur();
		if (member->isEffectivelyDead() || (member->isKindOf(KINDOF_INERT) && !member->isKindOf(KINDOF_CRATE)))
		{
			continue;
		}
		if (!observer)
		{
			observer = member;
		}
		maxVision = max(maxVision, member->getShroudClearingRange());
		if (teodObjectMatchesType(member, objectType->getString()))
		{
			count += 1.0f;
		}
	}

	const Coord3D *teamPos = team->getEstimateTeamPosition();
	if (!observer || !teamPos)
	{
		return FALSE;
	}
	Real countOther = (Real)teodCountInRange(observer, teamPos, maxVision, PartitionFilterRelationship::ALLOW_ENEMIES, TRUE, &otherObjectType->getString());
	return teodCompare(pComparisonParm->getInt(), count, countOther * ratio, 0.1f);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamClosestRelationType(Parameter* pTeamParm, Int pRelation, Parameter* objectType, Parameter* pComparisonParm, Real value)
{
	Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!team)
	{
		return FALSE;
	}
	return teodClosestRelationType(teodFirstLiveMember(team), team, team->getEstimateTeamPosition(), pRelation, objectType->getString(), pComparisonParm->getInt(), value);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateUnitClosestRelationType(Parameter* pUnitParm, Int pRelation, Parameter* objectType, Parameter* pComparisonParm, Real value)
{
	Object *unit = TheScriptEngine->getUnitNamed(pUnitParm->getString());
	if (!unit || unit->isEffectivelyDead())
	{
		return FALSE;
	}
	return teodClosestRelationType(unit, nullptr, unit->getPosition(), pRelation, objectType->getString(), pComparisonParm->getInt(), value);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamAllClear(Parameter* pTeamParm)
{
	Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!team)
	{
		return FALSE;
	}
	return TheScriptTEODState->isTeamAllClear(team);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamApart(Parameter* pTeamParm, Parameter* pComparisonParm, Real value)
{
	Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!team)
	{
		return FALSE;
	}
	const Coord3D *teamPos = team->getEstimateTeamPosition();
	if (!teamPos)
	{
		return FALSE;
	}

	Real totalDistSqr = 0.0f;
	Int memberCount = 0;
	for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance())
	{
		const Object *member = iter.cur();
		if (member->isEffectivelyDead())
		{
			continue;
		}
		Real dx = member->getPosition()->x - teamPos->x;
		Real dy = member->getPosition()->y - teamPos->y;
		totalDistSqr += dx * dx + dy * dy;
		++memberCount;
	}
	if (memberCount == 0)
	{
		return FALSE;
	}
	return teodCompare(pComparisonParm->getInt(), totalDistSqr / memberCount, value * value, 0.01f);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamCanLoadTransports(Parameter* pTeamParm)
{
	Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!team)
	{
		return FALSE;
	}

	std::vector<const Object *> looseInfantry;
	std::vector<const ContainModuleInterface *> transports;
	for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance())
	{
		const Object *member = iter.cur();
		if (member->isEffectivelyDead())
		{
			continue;
		}
		if (member->isKindOf(KINDOF_INFANTRY) && !member->isContained())
		{
			looseInfantry.push_back(member);
		}
		const ContainModuleInterface *contain = member->getContain();
		if (contain && member->isKindOf(KINDOF_TRANSPORT))
		{
			transports.push_back(contain);
		}
	}

	for (size_t i = 0; i < looseInfantry.size(); ++i)
	{
		for (size_t j = 0; j < transports.size(); ++j)
		{
			if (transports[j]->isValidContainerFor(looseInfantry[i], TRUE))
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateClosestTeamMemberToEnemyHasSightedComparisonRatioRadius(Parameter* pTeamParm, Parameter* pComparisonParm, Real ratio, Real radius)
{
	Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!team)
	{
		return FALSE;
	}
	return teodClosestMemberSightedRatio(team, pComparisonParm->getInt(), ratio, nullptr, nullptr, radius);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateClosestTeamMemberToEnemyHasSightedComparisonTypeRatioRadius(Parameter* pTeamParm, Parameter* pComparisonParm, Real ratio, Parameter* objectType, Parameter* otherObjectType, Real radius)
{
	Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!team || !teodIsKnownType(objectType->getString()) || !teodIsKnownType(otherObjectType->getString()))
	{
		return FALSE;
	}
	return teodClosestMemberSightedRatio(team, pComparisonParm->getInt(), ratio, &objectType->getString(), &otherObjectType->getString(), radius);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateUnitIsReloaded(Parameter* pUnitParm, Bool partially)
{
	Object *unit = TheScriptEngine->getUnitNamed(pUnitParm->getString());
	if (!unit || unit->isEffectivelyDead())
	{
		return FALSE;
	}
	Int totalShots = 0;
	Int curShots = 0;
	if (!unit->getAmmoPipShowingInfo(totalShots, curShots))
	{
		return FALSE;
	}
	return partially ? curShots > 0 : curShots >= totalShots;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamIsReloaded(Parameter* pTeamParm, Bool partially)
{
	Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!team)
	{
		return FALSE;
	}

	// Members without ammo pips are ignored, so a team with none of them is never reloaded.
	Bool anyWithAmmo = FALSE;
	for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance())
	{
		const Object *member = iter.cur();
		Int totalShots = 0;
		Int curShots = 0;
		if (member->isEffectivelyDead() || !member->getAmmoPipShowingInfo(totalShots, curShots))
		{
			continue;
		}
		anyWithAmmo = TRUE;
		if (partially && curShots > 0)
		{
			return TRUE;
		}
		if (!partially && curShots < totalShots)
		{
			return FALSE;
		}
	}
	return anyWithAmmo && !partially;
}
