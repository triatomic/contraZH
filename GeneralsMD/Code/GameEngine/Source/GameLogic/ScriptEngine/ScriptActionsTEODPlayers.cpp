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

// FILE: ScriptActionsTEODPlayers.cpp /////////////////////////////////////////////////////////////
// TEOD script actions that manage players' and teams' units, buildings and attack priorities.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/BuildAssistant.h"
#include "Common/MapObject.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptActions.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/ScriptTEOD.h"
#include "GameLogic/VictoryConditions.h"

#include <map>
#include <vector>

#define REALLY_FAR	(100000 * MAP_XY_FACTOR)

static const Int ANY_KINDOF = -1;

typedef std::vector<Object *> ObjectVector;
typedef std::map<ObjectID, Int> GarrisonPlan;

//-------------------------------------------------------------------------------------------------
// Copy of the file static updateTeamAndPlayerStuff in ScriptActions.cpp.
static void teodUpdateTeamAndPlayerStuff(Object *obj)
{
	obj->updateUpgradeModules();

	Drawable *draw = obj->getDrawable();
	if (draw)
	{
		if (TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT)
		{
			draw->setIndicatorColor(obj->getNightIndicatorColor());
		}
		else
		{
			draw->setIndicatorColor(obj->getIndicatorColor());
		}
	}
}

//-------------------------------------------------------------------------------------------------
static void teodCollectTeamObjects(Team *team, ObjectVector &objs)
{
	for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance())
	{
		Object *obj = iter.cur();
		if (obj && !obj->isEffectivelyDead())
		{
			objs.push_back(obj);
		}
	}
}

//-------------------------------------------------------------------------------------------------
static void teodCollectPlayerObjects(const Player *player, ObjectVector &objs)
{
	const Player::PlayerTeamList *teams = player->getPlayerTeams();
	for (Player::PlayerTeamList::const_iterator it = teams->begin(); it != teams->end(); ++it)
	{
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance())
		{
			Team *team = iter.cur();
			if (team)
			{
				teodCollectTeamObjects(team, objs);
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
static Team *teodFindDestinationTeam(const AsciiString &teamName)
{
	Team *team = TheScriptEngine->getTeamNamed(teamName);
	if (!team)
	{
		team = TheTeamFactory->findTeam(teamName);
	}
	return team;
}

//-------------------------------------------------------------------------------------------------
static Bool teodIsInArea(const Object *obj, PolygonTrigger *area)
{
	const Coord3D *pos = obj->getPosition();
	ICoord3D iPos;
	iPos.x = pos->x;
	iPos.y = pos->y;
	iPos.z = pos->z;
	return area->pointInTrigger(iPos);
}

//-------------------------------------------------------------------------------------------------
// An empty objectType, a null area or ANY_KINDOF skips that test.
static Bool teodPassesFilter(const Object *obj, Int kindOf, const AsciiString &objectType, PolygonTrigger *area)
{
	if (kindOf != ANY_KINDOF && !obj->isKindOf((KindOfType)kindOf))
	{
		return FALSE;
	}
	if (objectType.isNotEmpty() && !teodObjectMatchesType(obj, objectType))
	{
		return FALSE;
	}
	if (area && !teodIsInArea(obj, area))
	{
		return FALSE;
	}
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
static Bool teodIsValidKindOf(Int kindOf)
{
	return kindOf >= KINDOF_FIRST && kindOf < KINDOF_COUNT;
}

//-------------------------------------------------------------------------------------------------
static void teodMoveToTeam(const ObjectVector &objs, Int kindOf, const AsciiString &objectType, Team *dest)
{
	for (size_t i = 0; i < objs.size(); ++i)
	{
		Object *obj = objs[i];
		if (obj->getTeam() == dest || !teodPassesFilter(obj, kindOf, objectType, nullptr))
		{
			continue;
		}
		obj->setTeam(dest);
		teodUpdateTeamAndPlayerStuff(obj);
	}
}

//-------------------------------------------------------------------------------------------------
static void teodPlayerMerge(const AsciiString &playerName, Int kindOf, const AsciiString &objectType, const AsciiString &teamName)
{
	Player *player = TheScriptEngine->getPlayerFromAsciiString(playerName);
	Team *dest = teodFindDestinationTeam(teamName);
	if (!player || !dest)
	{
		return;
	}

	ObjectVector objs;
	teodCollectPlayerObjects(player, objs);
	teodMoveToTeam(objs, kindOf, objectType, dest);
	dest->setActive();
}

//-------------------------------------------------------------------------------------------------
static void teodTeamMerge(const AsciiString &srcName, Int kindOf, const AsciiString &objectType, const AsciiString &destName)
{
	Team *src = TheScriptEngine->getTeamNamed(srcName);
	Team *dest = teodFindDestinationTeam(destName);
	if (!src || !dest || src == dest)
	{
		return;
	}

	ObjectVector objs;
	teodCollectTeamObjects(src, objs);
	teodMoveToTeam(objs, kindOf, objectType, dest);
	dest->setActive();
}

//-------------------------------------------------------------------------------------------------
static void teodPlayerDisband(const AsciiString &playerName, Int kindOf, const AsciiString &objectType)
{
	Player *player = TheScriptEngine->getPlayerFromAsciiString(playerName);
	if (!player || !player->getDefaultTeam())
	{
		return;
	}

	ObjectVector objs;
	teodCollectPlayerObjects(player, objs);
	teodMoveToTeam(objs, kindOf, objectType, player->getDefaultTeam());
}

//-------------------------------------------------------------------------------------------------
static void teodTeamDisband(const AsciiString &teamName, Int kindOf, const AsciiString &objectType)
{
	Team *team = TheScriptEngine->getTeamNamed(teamName);
	if (!team || !team->getControllingPlayer())
	{
		return;
	}
	Team *dest = team->getControllingPlayer()->getDefaultTeam();
	if (!dest || dest == team)
	{
		return;
	}

	ObjectVector objs;
	teodCollectTeamObjects(team, objs);
	teodMoveToTeam(objs, kindOf, objectType, dest);
}

//-------------------------------------------------------------------------------------------------
static void teodTeamlessMerge(const AsciiString &playerName, Int kindOf, const AsciiString &objectType, const AsciiString &teamName)
{
	Player *player = TheScriptEngine->getPlayerFromAsciiString(playerName);
	Team *dest = teodFindDestinationTeam(teamName);
	if (!player || !dest || !player->getDefaultTeam())
	{
		return;
	}

	ObjectVector objs;
	teodCollectTeamObjects(player->getDefaultTeam(), objs);
	teodMoveToTeam(objs, kindOf, objectType, dest);
	dest->setActive();
}

//-------------------------------------------------------------------------------------------------
// maxEach caps the units this action sends to one building; spreadCount splits them over that many buildings.
static void teodGarrisonTeam(Team *team, const AsciiString &buildingType, Int maxEach, Int spreadCount, GarrisonPlan &planned)
{
	ObjectVector units;
	for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance())
	{
		Object *obj = iter.cur();
		if (!obj || obj->isEffectivelyDead() || obj->getContainedBy())
		{
			continue;
		}
		if (!obj->isKindOf(KINDOF_INFANTRY) || obj->isKindOf(KINDOF_NO_GARRISON) || !obj->getAIUpdateInterface())
		{
			continue;
		}
		units.push_back(obj);
	}
	if (units.empty())
	{
		return;
	}

	Object *leader = units[0];

	PartitionFilter *filters[16];
	Int count = 0;

	PartitionFilterAcceptByKindOf f1(MAKE_KINDOF_MASK(KINDOF_FS_INTERNET_CENTER), KINDOFMASK_NONE);
	PartitionFilterGarrisonableByPlayer f2(team->getControllingPlayer(), true, CMD_FROM_SCRIPT);

	if (leader->isKindOf(KINDOF_MONEY_HACKER))
	{
		filters[count++] = &f1;
	}
	else
	{
		filters[count++] = &f2;
	}

	PartitionFilterSameMapStatus filterMapStatus(leader);
	filters[count++] = &filterMapStatus;

	filters[count++] = nullptr;

	ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange(leader, REALLY_FAR, FROM_CENTER_3D, filters, ITER_SORTED_NEAR_TO_FAR);
	MemoryPoolObjectHolder hold(iter);

	size_t next = 0;
	Int buildingsLeft = spreadCount;
	for (Object *building = iter->first(); building && next < units.size(); building = iter->next())
	{
		if (buildingType.isNotEmpty() && !teodObjectMatchesType(building, buildingType))
		{
			continue;
		}
		ContainModuleInterface *cmi = building->getContain();
		if (!cmi)
		{
			continue;
		}

		Int &sent = planned[building->getID()];
		Int slots = cmi->getContainMax() - (Int)cmi->getContainCount() - sent;
		if (maxEach > 0)
		{
			slots = min(slots, maxEach - sent);
		}
		if (spreadCount > 0)
		{
			Int share = max(buildingsLeft, 1);
			Int remaining = (Int)(units.size() - next);
			slots = min(slots, (remaining + share - 1) / share);
		}
		if (slots <= 0)
		{
			continue;
		}

		for (Int i = 0; i < slots && next < units.size(); ++i)
		{
			units[next++]->getAIUpdateInterface()->aiEnter(building, CMD_FROM_SCRIPT);
			++sent;
		}
		--buildingsLeft;
	}
}

//-------------------------------------------------------------------------------------------------
static void teodGarrisonPlayer(const AsciiString &playerName, const AsciiString &buildingType, Int maxEach, Int spreadCount)
{
	Player *player = TheScriptEngine->getPlayerFromAsciiString(playerName);
	if (!player)
	{
		return;
	}

	GarrisonPlan planned;
	const Player::PlayerTeamList *teams = player->getPlayerTeams();
	for (Player::PlayerTeamList::const_iterator it = teams->begin(); it != teams->end(); ++it)
	{
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance())
		{
			Team *team = iter.cur();
			if (team)
			{
				teodGarrisonTeam(team, buildingType, maxEach, spreadCount, planned);
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
static void teodGarrisonNamedTeam(const AsciiString &teamName, const AsciiString &buildingType, Int maxEach, Int spreadCount)
{
	Team *team = TheScriptEngine->getTeamNamed(teamName);
	if (!team)
	{
		return;
	}

	GarrisonPlan planned;
	teodGarrisonTeam(team, buildingType, maxEach, spreadCount, planned);
}

//-------------------------------------------------------------------------------------------------
static PolygonTrigger *teodFindArea(const AsciiString &triggerArea)
{
	return TheScriptEngine->getQualifiedTriggerAreaByName(triggerArea);
}

//-------------------------------------------------------------------------------------------------
// With capturedOnly, sells only buildings being captured, each with a percentage chance.
static void teodSellBuildings(const AsciiString &playerName, const AsciiString &objectType, PolygonTrigger *area, Bool capturedOnly, Int percentage)
{
	Player *player = TheScriptEngine->getPlayerFromAsciiString(playerName);
	if (!player)
	{
		return;
	}

	ObjectVector objs;
	teodCollectPlayerObjects(player, objs);
	for (size_t i = 0; i < objs.size(); ++i)
	{
		Object *obj = objs[i];
		if (obj->getControllingPlayer() != player || !obj->isKindOf(KINDOF_STRUCTURE) || obj->isKindOf(KINDOF_TECH_BUILDING))
		{
			continue;
		}
		if (obj->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) || obj->testStatus(OBJECT_STATUS_SOLD))
		{
			continue;
		}
		if (!teodPassesFilter(obj, ANY_KINDOF, objectType, area))
		{
			continue;
		}
		if (capturedOnly)
		{
			if (!TheScriptTEODState || !TheScriptTEODState->isBeingCaptured(obj))
			{
				continue;
			}
			if (GameLogicRandomValue(1, 100) > percentage)
			{
				continue;
			}
		}
		TheBuildAssistant->sellObject(obj);
	}
}

//-------------------------------------------------------------------------------------------------
// Refunds and kills like GameLogic::onDozerCancelConstruct; the dozer drops the job when the building dies.
static void teodCancelConstructions(const AsciiString &playerName, const AsciiString &objectType, PolygonTrigger *area)
{
	Player *player = TheScriptEngine->getPlayerFromAsciiString(playerName);
	if (!player)
	{
		return;
	}

	ObjectVector objs;
	teodCollectPlayerObjects(player, objs);
	for (size_t i = 0; i < objs.size(); ++i)
	{
		Object *obj = objs[i];
		if (obj->getControllingPlayer() != player || !obj->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		{
			continue;
		}
		if (!teodPassesFilter(obj, ANY_KINDOF, objectType, area))
		{
			continue;
		}
		if (!obj->testStatus(OBJECT_STATUS_RECONSTRUCTING))
		{
			Int amount = obj->getTemplate()->calcCostToBuild(player);
			if (amount > 0)
			{
				player->getMoney()->deposit((UnsignedInt)amount, TRUE, FALSE);
			}
		}
		obj->kill();
	}
}

//-------------------------------------------------------------------------------------------------
static void teodApplyPrioritySet(const ObjectVector &objs, const AttackPriorityInfo *info, const AsciiString &objectType)
{
	for (size_t i = 0; i < objs.size(); ++i)
	{
		Object *obj = objs[i];
		if (!teodPassesFilter(obj, ANY_KINDOF, objectType, nullptr))
		{
			continue;
		}
		AIUpdateInterface *ai = obj->getAIUpdateInterface();
		if (ai)
		{
			ai->setAttackInfo(info);
		}
	}
}

//-------------------------------------------------------------------------------------------------
static void teodHunt(const ObjectVector &objs, const AsciiString &objectType, PolygonTrigger *area)
{
	for (size_t i = 0; i < objs.size(); ++i)
	{
		Object *obj = objs[i];
		if (!teodPassesFilter(obj, ANY_KINDOF, objectType, area))
		{
			continue;
		}
		if (area && (obj->isKindOf(KINDOF_DOZER) || obj->isKindOf(KINDOF_HARVESTER) || obj->isKindOf(KINDOF_IGNORES_SELECT_ALL)))
		{
			continue;
		}
		AIUpdateInterface *ai = obj->getAIUpdateInterface();
		if (!ai)
		{
			continue;
		}
		ai->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
		ai->aiHunt(CMD_FROM_SCRIPT);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerSurrender(const AsciiString& playerName)
{
	Player *player = TheScriptEngine->getPlayerFromAsciiString(playerName);
	if (!player || !player->isPlayerActive())
	{
		return;
	}

	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player *ally = ThePlayerList->getNthPlayer(i);
		if (!ally || ally == player || !ally->isPlayerActive())
		{
			continue;
		}
		if (player->getRelationship(ally->getDefaultTeam()) != ALLIES || ally->getRelationship(player->getDefaultTeam()) != ALLIES)
		{
			continue;
		}
		if (TheVictoryConditions && TheVictoryConditions->hasSinglePlayerBeenDefeated(ally))
		{
			continue;
		}
		ally->transferAssetsFromThat(player);
		break;
	}

	// Matches GameLogic::onSelfDestruct; after a transfer only beacons are left to kill.
	player->killPlayer();
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerMergeKindOf(const AsciiString& playerName, Int kindOf, const AsciiString& teamName)
{
	if (teodIsValidKindOf(kindOf))
	{
		teodPlayerMerge(playerName, kindOf, AsciiString::TheEmptyString, teamName);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doTeamMergeKindOf(const AsciiString& srcName, Int kindOf, const AsciiString& destName)
{
	if (teodIsValidKindOf(kindOf))
	{
		teodTeamMerge(srcName, kindOf, AsciiString::TheEmptyString, destName);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerMergeType(const AsciiString& playerName, const AsciiString& objectType, const AsciiString& teamName)
{
	if (objectType.isNotEmpty())
	{
		teodPlayerMerge(playerName, ANY_KINDOF, objectType, teamName);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doTeamMergeType(const AsciiString& srcName, const AsciiString& objectType, const AsciiString& destName)
{
	if (objectType.isNotEmpty())
	{
		teodTeamMerge(srcName, ANY_KINDOF, objectType, destName);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerDisbandKindOf(const AsciiString& playerName, Int kindOf)
{
	if (teodIsValidKindOf(kindOf))
	{
		teodPlayerDisband(playerName, kindOf, AsciiString::TheEmptyString);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doTeamDisbandKindOf(const AsciiString& teamName, Int kindOf)
{
	if (teodIsValidKindOf(kindOf))
	{
		teodTeamDisband(teamName, kindOf, AsciiString::TheEmptyString);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerDisbandType(const AsciiString& playerName, const AsciiString& objectType)
{
	if (objectType.isNotEmpty())
	{
		teodPlayerDisband(playerName, ANY_KINDOF, objectType);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doTeamDisbandType(const AsciiString& teamName, const AsciiString& objectType)
{
	if (objectType.isNotEmpty())
	{
		teodTeamDisband(teamName, ANY_KINDOF, objectType);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerTeamlessMerge(const AsciiString& playerName, const AsciiString& teamName)
{
	teodTeamlessMerge(playerName, KINDOF_CAN_ATTACK, AsciiString::TheEmptyString, teamName);
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerTeamlessMergeKindOf(const AsciiString& playerName, Int kindOf, const AsciiString& teamName)
{
	if (teodIsValidKindOf(kindOf))
	{
		teodTeamlessMerge(playerName, kindOf, AsciiString::TheEmptyString, teamName);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerTeamlessMergeType(const AsciiString& playerName, const AsciiString& objectType, const AsciiString& teamName)
{
	if (objectType.isNotEmpty())
	{
		teodTeamlessMerge(playerName, ANY_KINDOF, objectType, teamName);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerGarrisonMaxEach(const AsciiString& playerName, Int maxAmount)
{
	if (maxAmount > 0)
	{
		teodGarrisonPlayer(playerName, AsciiString::TheEmptyString, maxAmount, 0);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doTeamGarrisonMaxEach(const AsciiString& teamName, Int maxAmount)
{
	if (maxAmount > 0)
	{
		teodGarrisonNamedTeam(teamName, AsciiString::TheEmptyString, maxAmount, 0);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerGarrisonEqually(const AsciiString& playerName, Int amount)
{
	if (amount > 0)
	{
		teodGarrisonPlayer(playerName, AsciiString::TheEmptyString, 0, amount);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doTeamGarrisonEqually(const AsciiString& teamName, Int amount)
{
	if (amount > 0)
	{
		teodGarrisonNamedTeam(teamName, AsciiString::TheEmptyString, 0, amount);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doTeamGarrisonType(const AsciiString& teamName, const AsciiString& objectType)
{
	if (objectType.isNotEmpty())
	{
		teodGarrisonNamedTeam(teamName, objectType, 0, 0);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerGarrisonMaxEachType(const AsciiString& playerName, const AsciiString& objectType, Int maxAmount)
{
	if (maxAmount > 0 && objectType.isNotEmpty())
	{
		teodGarrisonPlayer(playerName, objectType, maxAmount, 0);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doTeamGarrisonMaxEachType(const AsciiString& teamName, const AsciiString& objectType, Int maxAmount)
{
	if (maxAmount > 0 && objectType.isNotEmpty())
	{
		teodGarrisonNamedTeam(teamName, objectType, maxAmount, 0);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerGarrisonEquallyType(const AsciiString& playerName, Int amount, const AsciiString& objectType)
{
	if (amount > 0 && objectType.isNotEmpty())
	{
		teodGarrisonPlayer(playerName, objectType, 0, amount);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doTeamGarrisonEquallyType(const AsciiString& teamName, Int amount, const AsciiString& objectType)
{
	if (amount > 0 && objectType.isNotEmpty())
	{
		teodGarrisonNamedTeam(teamName, objectType, 0, amount);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerHuntType(const AsciiString& playerName, const AsciiString& objectType)
{
	Player *player = TheScriptEngine->getPlayerFromAsciiString(playerName);
	if (!player || objectType.isEmpty())
	{
		return;
	}

	ObjectVector objs;
	teodCollectPlayerObjects(player, objs);
	teodHunt(objs, objectType, nullptr);
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doTeamHuntType(const AsciiString& teamName, const AsciiString& objectType)
{
	Team *team = TheScriptEngine->getTeamNamed(teamName);
	if (!team || objectType.isEmpty())
	{
		return;
	}

	ObjectVector objs;
	teodCollectTeamObjects(team, objs);
	teodHunt(objs, objectType, nullptr);
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerHuntArea(const AsciiString& playerName, const AsciiString& triggerArea)
{
	Player *player = TheScriptEngine->getPlayerFromAsciiString(playerName);
	PolygonTrigger *area = teodFindArea(triggerArea);
	if (!player || !area)
	{
		return;
	}

	ObjectVector objs;
	teodCollectPlayerObjects(player, objs);
	teodHunt(objs, AsciiString::TheEmptyString, area);
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerSellAllBuildingsType(const AsciiString& playerName, const AsciiString& objectType)
{
	if (objectType.isNotEmpty())
	{
		teodSellBuildings(playerName, objectType, nullptr, FALSE, 0);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerSellAllBuildingsArea(const AsciiString& playerName, const AsciiString& triggerArea)
{
	PolygonTrigger *area = teodFindArea(triggerArea);
	if (area)
	{
		teodSellBuildings(playerName, AsciiString::TheEmptyString, area, FALSE, 0);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerSellAllBuildingsTypeArea(const AsciiString& playerName, const AsciiString& objectType, const AsciiString& triggerArea)
{
	PolygonTrigger *area = teodFindArea(triggerArea);
	if (area && objectType.isNotEmpty())
	{
		teodSellBuildings(playerName, objectType, area, FALSE, 0);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerSellAllBuildingsInCaptureProcessPercentage(const AsciiString& playerName, Int percentage)
{
	teodSellBuildings(playerName, AsciiString::TheEmptyString, nullptr, TRUE, percentage);
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerSellAllBuildingsTypeInCaptureProcessPercentage(const AsciiString& playerName, const AsciiString& objectType, Int percentage)
{
	if (objectType.isNotEmpty())
	{
		teodSellBuildings(playerName, objectType, nullptr, TRUE, percentage);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerSellAllBuildingsAreaInCaptureProcessPercentage(const AsciiString& playerName, const AsciiString& triggerArea, Int percentage)
{
	PolygonTrigger *area = teodFindArea(triggerArea);
	if (area)
	{
		teodSellBuildings(playerName, AsciiString::TheEmptyString, area, TRUE, percentage);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerSellAllBuildingsTypeAreaInCaptureProcessPercentage(const AsciiString& playerName, const AsciiString& objectType, const AsciiString& triggerArea, Int percentage)
{
	PolygonTrigger *area = teodFindArea(triggerArea);
	if (area && objectType.isNotEmpty())
	{
		teodSellBuildings(playerName, objectType, area, TRUE, percentage);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerCancelAllConstructions(const AsciiString& playerName)
{
	teodCancelConstructions(playerName, AsciiString::TheEmptyString, nullptr);
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerCancelAllConstructionsType(const AsciiString& playerName, const AsciiString& objectType)
{
	if (objectType.isNotEmpty())
	{
		teodCancelConstructions(playerName, objectType, nullptr);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerCancelAllConstructionsArea(const AsciiString& playerName, const AsciiString& triggerArea)
{
	PolygonTrigger *area = teodFindArea(triggerArea);
	if (area)
	{
		teodCancelConstructions(playerName, AsciiString::TheEmptyString, area);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerCancelAllConstructionsTypeArea(const AsciiString& playerName, const AsciiString& objectType, const AsciiString& triggerArea)
{
	PolygonTrigger *area = teodFindArea(triggerArea);
	if (area && objectType.isNotEmpty())
	{
		teodCancelConstructions(playerName, objectType, area);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerApplyPrioritySet(const AsciiString& playerName, const AsciiString& attackPrioritySet)
{
	Player *player = TheScriptEngine->getPlayerFromAsciiString(playerName);
	if (!player)
	{
		return;
	}
	const AttackPriorityInfo *info = TheScriptEngine->getAttackInfo(attackPrioritySet);
	if (info->getName().isEmpty())
	{
		return;
	}

	const Player::PlayerTeamList *teams = player->getPlayerTeams();
	for (Player::PlayerTeamList::const_iterator it = teams->begin(); it != teams->end(); ++it)
	{
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance())
		{
			Team *team = iter.cur();
			if (team)
			{
				team->setAttackPriorityName(info->getName());
			}
		}
	}

	ObjectVector objs;
	teodCollectPlayerObjects(player, objs);
	teodApplyPrioritySet(objs, info, AsciiString::TheEmptyString);
}

//-------------------------------------------------------------------------------------------------
// The Type variants set the set per object only; a team's priority name would also reach its other types.
void ScriptActions::doPlayerApplyPrioritySetType(const AsciiString& playerName, const AsciiString& attackPrioritySet, const AsciiString& objectType)
{
	Player *player = TheScriptEngine->getPlayerFromAsciiString(playerName);
	if (!player || objectType.isEmpty())
	{
		return;
	}
	const AttackPriorityInfo *info = TheScriptEngine->getAttackInfo(attackPrioritySet);
	if (info->getName().isEmpty())
	{
		return;
	}

	ObjectVector objs;
	teodCollectPlayerObjects(player, objs);
	teodApplyPrioritySet(objs, info, objectType);
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doTeamApplyPrioritySetType(const AsciiString& teamName, const AsciiString& attackPrioritySet, const AsciiString& objectType)
{
	Team *team = TheScriptEngine->getTeamNamed(teamName);
	if (!team || objectType.isEmpty())
	{
		return;
	}
	const AttackPriorityInfo *info = TheScriptEngine->getAttackInfo(attackPrioritySet);
	if (info->getName().isEmpty())
	{
		return;
	}

	ObjectVector objs;
	teodCollectTeamObjects(team, objs);
	teodApplyPrioritySet(objs, info, objectType);
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doTeamCustomColor(const AsciiString& teamName, Color c)
{
	Team *team = TheScriptEngine->getTeamNamed(teamName);
	if (!team)
	{
		return;
	}

	ObjectVector objs;
	teodCollectTeamObjects(team, objs);
	for (size_t i = 0; i < objs.size(); ++i)
	{
		objs[i]->setCustomIndicatorColor(c);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerCustomColor(const AsciiString& playerName, Color c)
{
	Player *player = TheScriptEngine->getPlayerFromAsciiString(playerName);
	if (!player)
	{
		return;
	}

	ObjectVector objs;
	teodCollectPlayerObjects(player, objs);
	for (size_t i = 0; i < objs.size(); ++i)
	{
		objs[i]->setCustomIndicatorColor(c);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doPlayerSetWillingnessToCrush(const AsciiString& playerName, Bool wantsToCrush)
{
	Player *player = TheScriptEngine->getPlayerFromAsciiString(playerName);
	if (!player || !TheScriptTEODState)
	{
		return;
	}

	// The player setting covers units built later; existing units get their own override like in CLP.
	TheScriptTEODState->setPlayerCrushesInfantry(player->getPlayerIndex(), wantsToCrush);

	ObjectVector objs;
	teodCollectPlayerObjects(player, objs);
	for (size_t i = 0; i < objs.size(); ++i)
	{
		TheScriptTEODState->setObjectCrushesInfantry(objs[i], wantsToCrush);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doTeamSetWillingnessToCrush(const AsciiString& teamName, Bool wantsToCrush)
{
	Team *team = TheScriptEngine->getTeamNamed(teamName);
	if (!team || !TheScriptTEODState)
	{
		return;
	}

	ObjectVector objs;
	teodCollectTeamObjects(team, objs);
	for (size_t i = 0; i < objs.size(); ++i)
	{
		TheScriptTEODState->setObjectCrushesInfantry(objs[i], wantsToCrush);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doUnitSetWillingnessToCrush(const AsciiString& unitName, Bool wantsToCrush)
{
	Object *obj = TheScriptEngine->getUnitNamed(unitName);
	if (!obj || !TheScriptTEODState)
	{
		return;
	}

	TheScriptTEODState->setObjectCrushesInfantry(obj, wantsToCrush);
}

//-------------------------------------------------------------------------------------------------
static Bool teodCanTransferToPlayer(const Object *obj, const Player *dest)
{
	if (obj->isDestroyed() || obj->isEffectivelyDead() || obj->isKindOf(KINDOF_PROJECTILE))
	{
		return FALSE;
	}
	const Player *owner = obj->getControllingPlayer();
	if (!owner || owner == dest || !owner->isPlayableSide())
	{
		return FALSE;
	}
	const ThingTemplate *beacon = TheThingFactory->findTemplate(owner->getPlayerTemplate()->getBeaconTemplate(), FALSE);
	return !beacon || !obj->getTemplate()->isEquivalentTo(beacon);
}

//-------------------------------------------------------------------------------------------------
// Only objects owned by playable sides change hands, so civilian props and neutral buildings stay.
void ScriptActions::doTransferAreaToPlayer(const AsciiString& areaName, const AsciiString& playerName)
{
	PolygonTrigger *area = teodFindArea(areaName);
	Player *player = TheScriptEngine->getPlayerFromAsciiString(playerName);
	if (!area || !player || !player->isPlayerActive())
	{
		return;
	}
	Team *dest = player->getDefaultTeam();
	if (!dest)
	{
		return;
	}

	ObjectVector objs;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->getContainedBy() || !teodCanTransferToPlayer(obj, player) || !teodIsInArea(obj, area))
		{
			continue;
		}
		objs.push_back(obj);

		ContainModuleInterface *contain = obj->getContain();
		const ContainedItemsList *items = contain ? contain->getContainedItemsList() : nullptr;
		if (!items)
		{
			continue;
		}
		for (ContainedItemsList::const_iterator it = items->begin(); it != items->end(); ++it)
		{
			Object *contained = *it;
			if (contained && teodCanTransferToPlayer(contained, player))
			{
				objs.push_back(contained);
			}
		}
	}

	for (size_t i = 0; i < objs.size(); ++i)
	{
		objs[i]->setTeam(dest);
		teodUpdateTeamAndPlayerStuff(objs[i]);
	}
}
