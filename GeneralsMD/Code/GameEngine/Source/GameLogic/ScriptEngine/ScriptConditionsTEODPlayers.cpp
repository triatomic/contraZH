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

// FILE: ScriptConditionsTEODPlayers.cpp //////////////////////////////////////////////////////////
// TEOD script conditions about players, start spots, map control and kills.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GlobalData.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectTypes.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptConditions.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/ScriptTEOD.h"
#include "GameLogic/TerrainLogic.h"

#include "GameNetwork/GameInfo.h"

#include <float.h>
#include <vector>

typedef std::vector<const ThingTemplate *> TeodTemplateList;

static const Real TEOD_MAP_CONTROL_GRID = 200.0f;
static const Real TEOD_MAP_CONTROL_RADIUS = 300.0f;
static const Real TEOD_RATIO_TOLERANCE = 0.1f;

//-------------------------------------------------------------------------------------------------
static Bool teodCompareInt(Int comparison, Int value, Int against)
{
	switch (comparison)
	{
		case Parameter::LESS_THAN:			return value < against;
		case Parameter::LESS_EQUAL:			return value <= against;
		case Parameter::EQUAL:					return value == against;
		case Parameter::GREATER_EQUAL:	return value >= against;
		case Parameter::GREATER:				return value > against;
		case Parameter::NOT_EQUAL:			return value != against;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
static Bool teodCompareReal(Int comparison, Real value, Real against)
{
	switch (comparison)
	{
		case Parameter::LESS_THAN:			return value < against;
		case Parameter::LESS_EQUAL:			return value <= against;
		case Parameter::EQUAL:					return value == against;
		case Parameter::GREATER_EQUAL:	return value >= against;
		case Parameter::GREATER:				return value > against;
		case Parameter::NOT_EQUAL:			return value != against;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** Compares count with ratio times countOther. EQUAL and NOT_EQUAL allow TEOD_RATIO_TOLERANCE. */
//-------------------------------------------------------------------------------------------------
static Bool teodCompareRatio(Int comparison, Int count, Int countOther, Real ratio)
{
	Real scaled = (Real)countOther * ratio;
	Real diff = (Real)count - scaled;
	switch (comparison)
	{
		case Parameter::LESS_THAN:			return (Real)count < scaled;
		case Parameter::LESS_EQUAL:			return (Real)count <= scaled;
		case Parameter::EQUAL:					return fabs(diff) < TEOD_RATIO_TOLERANCE;
		case Parameter::GREATER_EQUAL:	return (Real)count >= scaled;
		case Parameter::GREATER:				return (Real)count > scaled;
		case Parameter::NOT_EQUAL:			return fabs(diff) >= TEOD_RATIO_TOLERANCE;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** Resolves a ThingTemplate or ObjectTypes list name into templates. An empty name gives none. */
//-------------------------------------------------------------------------------------------------
static void teodTemplatesFromType(const AsciiString &objectType, TeodTemplateList &out)
{
	out.clear();
	if (objectType.isEmpty())
	{
		return;
	}
	const ObjectTypes *types = TheScriptEngine->getObjectTypes(objectType);
	if (types)
	{
		for (size_t i = 0; i < types->getListSize(); ++i)
		{
			const ThingTemplate *thing = TheThingFactory->findTemplate(types->getNthInList(i), FALSE);
			if (thing)
			{
				out.push_back(thing);
			}
		}
		return;
	}
	const ThingTemplate *thing = TheThingFactory->findTemplate(objectType, FALSE);
	if (thing)
	{
		out.push_back(thing);
	}
}

//-------------------------------------------------------------------------------------------------
static Bool teodTemplateInList(const ThingTemplate *thing, const TeodTemplateList &list)
{
	if (!thing)
	{
		return FALSE;
	}
	for (size_t i = 0; i < list.size(); ++i)
	{
		if (thing->isEquivalentTo(list[i]))
		{
			return TRUE;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** True for a player that takes a start spot: a playable side that is not an observer. */
//-------------------------------------------------------------------------------------------------
static Bool teodIsMatchPlayer(const Player *player)
{
	return player && player->isPlayableSide() && !player->isPlayerObserver();
}

//-------------------------------------------------------------------------------------------------
/** True if other belongs to subject's forces of relationType. ALLIES includes subject itself. */
//-------------------------------------------------------------------------------------------------
static Bool teodIsForcePlayer(Player *subject, Player *other, Int relationType)
{
	if (!other || other->isPlayerObserver())
	{
		return FALSE;
	}
	if (other == subject)
	{
		return relationType == ALLIES;
	}
	const Team *team = other->getDefaultTeam();
	return team && subject->getRelationship(team) == relationType;
}

//-------------------------------------------------------------------------------------------------
/** True if other is a match player of relationType to subject, never subject itself. */
//-------------------------------------------------------------------------------------------------
static Bool teodIsCoPlayer(Player *subject, Player *other, Int relationType)
{
	if (other == subject || !teodIsMatchPlayer(other))
	{
		return FALSE;
	}
	const Team *team = other->getDefaultTeam();
	return team && subject->getRelationship(team) == relationType;
}

//-------------------------------------------------------------------------------------------------
static Int teodRelationTo(const Player *player, const Object *obj)
{
	if (obj->getControllingPlayer() == player)
	{
		return ALLIES;
	}
	return player->getRelationship(obj->getTeam());
}

//-------------------------------------------------------------------------------------------------
static Real teodDistance2D(const Coord3D *a, const Coord3D *b)
{
	Real dx = a->x - b->x;
	Real dy = a->y - b->y;
	return sqrtf(dx * dx + dy * dy);
}

//-------------------------------------------------------------------------------------------------
static Waypoint *teodSpotWaypoint(Int spot)
{
	AsciiString name;
	name.format("Player_%d_Start", spot);
	return TheTerrainLogic->getWaypointByName(name);
}

//-------------------------------------------------------------------------------------------------
static Waypoint *teodPlayerSpotWaypoint(Player *player)
{
	if (!player || player->getMpStartIndex() < 0)
	{
		return nullptr;
	}
	return teodSpotWaypoint(player->getMpStartIndex() + 1);
}

//-------------------------------------------------------------------------------------------------
/** Returns how far another start spot may be to neighbour spot, or a negative value if there is no other spot. */
//-------------------------------------------------------------------------------------------------
static Real teodNeighbourRadius(const Waypoint *spot)
{
	Real minDist = FLT_MAX;
	for (Waypoint *way = TheTerrainLogic->getFirstWaypoint(); way; way = way->getNext())
	{
		if (way == spot)
		{
			continue;
		}
		const AsciiString name = way->getName();
		if (!name.startsWith("Player_") || !name.endsWith("_Start"))
		{
			continue;
		}
		Real dist = teodDistance2D(spot->getLocation(), way->getLocation());
		if (dist < minDist)
		{
			minDist = dist;
		}
	}
	if (minDist == FLT_MAX)
	{
		return -1.0f;
	}
	return minDist * 1.5f;
}

//-------------------------------------------------------------------------------------------------
static Bool teodIsNeighbouring(const Waypoint *from, const Waypoint *to, Real radius)
{
	return from && to && from != to && teodDistance2D(from->getLocation(), to->getLocation()) <= radius;
}

//-------------------------------------------------------------------------------------------------
/** Counts Player_N_Start waypoints from N = 1 up to the first missing one. */
//-------------------------------------------------------------------------------------------------
static Int teodMapSpotCount()
{
	Int count = 0;
	for (Int spot = 1; spot <= MAX_PLAYER_COUNT; ++spot)
	{
		if (!teodSpotWaypoint(spot))
		{
			break;
		}
		count++;
	}
	return count;
}

//-------------------------------------------------------------------------------------------------
static Bool teodIsSpotOccupied(Int spot)
{
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player *player = ThePlayerList->getNthPlayer(i);
		if (teodIsMatchPlayer(player) && player->getMpStartIndex() + 1 == spot)
		{
			return player->isPlayerActive();
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
static Int teodActiveMatchPlayerCount()
{
	Int count = 0;
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player *player = ThePlayerList->getNthPlayer(i);
		if (teodIsMatchPlayer(player) && player->isPlayerActive())
		{
			count++;
		}
	}
	return count;
}

//-------------------------------------------------------------------------------------------------
struct TeodCountData
{
	const TeodTemplateList *types;		///< null counts units that are not structures
	const PolygonTrigger *area;				///< null counts the whole map
	Int count;
};

//-------------------------------------------------------------------------------------------------
static void teodCountObject(Object *obj, void *userData)
{
	TeodCountData *data = (TeodCountData *)userData;
	if (obj->isEffectivelyDead())
	{
		return;
	}
	if (data->types)
	{
		if (obj->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) || !teodTemplateInList(obj->getTemplate(), *data->types))
		{
			return;
		}
	}
	else
	{
		if (obj->isKindOf(KINDOF_STRUCTURE) || (obj->isKindOf(KINDOF_INERT) && !obj->isKindOf(KINDOF_CRATE)))
		{
			return;
		}
	}
	if (data->area && !obj->isInside(data->area))
	{
		return;
	}
	data->count++;
}

//-------------------------------------------------------------------------------------------------
/** Counts the player's live objects of types, or its units when types is null, optionally inside area. */
//-------------------------------------------------------------------------------------------------
static Int teodCountObjects(const Player *player, const TeodTemplateList *types, const PolygonTrigger *area)
{
	if (types && types->empty())
	{
		return 0;
	}
	TeodCountData data;
	data.types = types;
	data.area = area;
	data.count = 0;
	player->iterateObjects(teodCountObject, &data);
	return data.count;
}

//-------------------------------------------------------------------------------------------------
static Int teodCountForceObjects(Player *subject, Int relationType, const TeodTemplateList *types, const PolygonTrigger *area)
{
	Int count = 0;
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player *player = ThePlayerList->getNthPlayer(i);
		if (teodIsForcePlayer(subject, player, relationType))
		{
			count += teodCountObjects(player, types, area);
		}
	}
	return count;
}

//-------------------------------------------------------------------------------------------------
/** Grid of map points, each marked once a player's structure, vehicle or infantry is within reach. */
//-------------------------------------------------------------------------------------------------
struct TeodControlGrid
{
	Real loX;
	Real loY;
	Int cols;
	Int rows;
	std::vector<Bool> marks;
};

//-------------------------------------------------------------------------------------------------
static void teodInitControlGrid(TeodControlGrid &grid)
{
	Region3D extent;
	TheTerrainLogic->getExtent(&extent);
	grid.loX = extent.lo.x;
	grid.loY = extent.lo.y;
	grid.cols = MAX(0, (Int)ceilf((extent.hi.x - extent.lo.x) / TEOD_MAP_CONTROL_GRID));
	grid.rows = MAX(0, (Int)ceilf((extent.hi.y - extent.lo.y) / TEOD_MAP_CONTROL_GRID));
	grid.marks.assign(grid.cols * grid.rows, FALSE);
}

//-------------------------------------------------------------------------------------------------
static void teodMarkControl(Object *obj, void *userData)
{
	TeodControlGrid *grid = (TeodControlGrid *)userData;
	if (obj->isEffectivelyDead())
	{
		return;
	}
	if (!obj->isKindOf(KINDOF_STRUCTURE) && !obj->isKindOf(KINDOF_VEHICLE) && !obj->isKindOf(KINDOF_INFANTRY))
	{
		return;
	}
	const Coord3D *pos = obj->getPosition();
	const Real radius = TEOD_MAP_CONTROL_RADIUS;
	Int minCol = MAX(0, (Int)ceilf((pos->x - radius - grid->loX) / TEOD_MAP_CONTROL_GRID));
	Int maxCol = MIN(grid->cols - 1, (Int)floorf((pos->x + radius - grid->loX) / TEOD_MAP_CONTROL_GRID));
	Int minRow = MAX(0, (Int)ceilf((pos->y - radius - grid->loY) / TEOD_MAP_CONTROL_GRID));
	Int maxRow = MIN(grid->rows - 1, (Int)floorf((pos->y + radius - grid->loY) / TEOD_MAP_CONTROL_GRID));
	for (Int col = minCol; col <= maxCol; ++col)
	{
		Real dx = grid->loX + col * TEOD_MAP_CONTROL_GRID - pos->x;
		for (Int row = minRow; row <= maxRow; ++row)
		{
			Real dy = grid->loY + row * TEOD_MAP_CONTROL_GRID - pos->y;
			if (dx * dx + dy * dy <= radius * radius)
			{
				grid->marks[row * grid->cols + col] = TRUE;
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Compares the controlled share of grid points, inside area if given, with percent. */
//-------------------------------------------------------------------------------------------------
static Bool teodCompareControl(const TeodControlGrid &grid, PolygonTrigger *area, Int comparison, Real percent)
{
	Int controlled = 0;
	Int total = 0;
	for (Int row = 0; row < grid.rows; ++row)
	{
		for (Int col = 0; col < grid.cols; ++col)
		{
			if (area)
			{
				ICoord3D point;
				point.x = REAL_TO_INT_FLOOR(grid.loX + col * TEOD_MAP_CONTROL_GRID);
				point.y = REAL_TO_INT_FLOOR(grid.loY + row * TEOD_MAP_CONTROL_GRID);
				point.z = 0;
				if (!area->pointInTrigger(point))
				{
					continue;
				}
			}
			total++;
			if (grid.marks[row * grid.cols + col])
			{
				controlled++;
			}
		}
	}
	if (total == 0)
	{
		return FALSE;
	}
	return teodCompareReal(comparison, (Real)controlled / (Real)total, percent / 100.0f);
}

//-------------------------------------------------------------------------------------------------
static Bool teodRelationMapControl(Player *subject, Int relationType, Int comparison, Real percent, PolygonTrigger *area)
{
	TeodControlGrid grid;
	teodInitControlGrid(grid);
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player *player = ThePlayerList->getNthPlayer(i);
		if (teodIsForcePlayer(subject, player, relationType))
		{
			player->iterateObjects(teodMarkControl, &grid);
		}
	}
	return teodCompareControl(grid, area, comparison, percent);
}

//-------------------------------------------------------------------------------------------------
/** True if observer currently sees a live object of relationType to it, of types and inside area when given. */
//-------------------------------------------------------------------------------------------------
static Bool teodPlayerSighted(const Player *observer, Int relationType, const TeodTemplateList *types, const PolygonTrigger *area)
{
	if (types && types->empty())
	{
		return FALSE;
	}
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->isEffectivelyDead() || obj->isOffMap() || obj->getContainedBy() || obj->isKindOf(KINDOF_INERT))
		{
			continue;
		}
		if (types && !teodTemplateInList(obj->getTemplate(), *types))
		{
			continue;
		}
		if (area && !obj->isInside(area))
		{
			continue;
		}
		if (teodRelationTo(observer, obj) != relationType)
		{
			continue;
		}
		if (teodIsSeenByPlayer(obj, observer))
		{
			return TRUE;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
static Bool teodForceSighted(Player *subject, Int playerRelationType, Int relationType, const TeodTemplateList *types, const PolygonTrigger *area)
{
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player *player = ThePlayerList->getNthPlayer(i);
		if (teodIsForcePlayer(subject, player, playerRelationType) && teodPlayerSighted(player, relationType, types, area))
		{
			return TRUE;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerRelation(const AsciiString& playerSrcName, Int relationType, const AsciiString& playerDstName)
{
	Player *src = TheScriptEngine->getPlayerFromAsciiString(playerSrcName);
	Player *dst = TheScriptEngine->getPlayerFromAsciiString(playerDstName);
	if (!src || !dst)
	{
		return FALSE;
	}
	if (src == dst)
	{
		return relationType == ALLIES;
	}
	const Team *team = dst->getDefaultTeam();
	return team && src->getRelationship(team) == relationType;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishAnyRelationFaction(Parameter* pPlayerParm, Int relationType, Parameter* pFactionParm)
{
	Player *subject = playerFromParam(pPlayerParm);
	if (!subject)
	{
		return FALSE;
	}
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player *player = ThePlayerList->getNthPlayer(i);
		if (teodIsCoPlayer(subject, player, relationType) && player->getSide() == pFactionParm->getString())
		{
			return TRUE;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateStartingCash(Parameter* pComparisonParm, Int pAmount)
{
	// Player::init deposits this amount when the player template gives no money.
	UnsignedInt startingCash = TheGameInfo ? TheGameInfo->getStartingCash().countMoney() : TheGlobalData->m_defaultStartingCash.countMoney();
	return teodCompareInt(pComparisonParm->getInt(), (Int)startingCash, pAmount);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateMapSize(Parameter* pComparisonParm, Int pMapSize)
{
	return teodCompareInt(pComparisonParm->getInt(), teodMapSpotCount(), pMapSize);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateActivePlayerCount(Parameter* pComparisonParm, Int pPlayerCount)
{
	return teodCompareInt(pComparisonParm->getInt(), teodActiveMatchPlayerCount(), pPlayerCount);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateEmptySpotCount(Parameter* pComparisonParm, Int pEmptySpotCount)
{
	Int spotCount = teodMapSpotCount();
	Int emptyCount = 0;
	for (Int spot = 1; spot <= spotCount; ++spot)
	{
		if (!teodIsSpotOccupied(spot))
		{
			emptyCount++;
		}
	}
	return teodCompareInt(pComparisonParm->getInt(), emptyCount, pEmptySpotCount);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateEmptySpot(Parameter* pStartNdx, Bool isEmpty)
{
	Int spot = pStartNdx->getInt();
	if (spot < 1)
	{
		return !isEmpty;
	}
	return teodIsSpotOccupied(spot) ? !isEmpty : isEmpty;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNeighbouringSpot(Parameter* pPlayerParm, Parameter* pStartNdx)
{
	Player *player = playerFromParam(pPlayerParm);
	Waypoint *home = teodPlayerSpotWaypoint(player);
	if (!home)
	{
		return FALSE;
	}
	Real radius = teodNeighbourRadius(home);
	if (radius < 0.0f || pStartNdx->getInt() < 1)
	{
		return FALSE;
	}
	return teodIsNeighbouring(home, teodSpotWaypoint(pStartNdx->getInt()), radius);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNeighbouringSpotsEmpty(Parameter* pPlayerParm, Parameter* pComparisonParm, Int pCount)
{
	Player *player = playerFromParam(pPlayerParm);
	Waypoint *home = teodPlayerSpotWaypoint(player);
	if (!home)
	{
		return FALSE;
	}
	Real radius = teodNeighbourRadius(home);
	if (radius < 0.0f)
	{
		return FALSE;
	}
	Int count = 0;
	for (Int spot = 1; spot <= MAX_PLAYER_COUNT; ++spot)
	{
		if (teodIsNeighbouring(home, teodSpotWaypoint(spot), radius) && !teodIsSpotOccupied(spot))
		{
			count++;
		}
	}
	return teodCompareInt(pComparisonParm->getInt(), count, pCount);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNeighbouringSpotsRelation(Parameter* pPlayerParm, Parameter* pComparisonParm, Int pCount, Int relationType)
{
	Player *subject = playerFromParam(pPlayerParm);
	Waypoint *home = teodPlayerSpotWaypoint(subject);
	if (!home)
	{
		return FALSE;
	}
	Real radius = teodNeighbourRadius(home);
	if (radius < 0.0f)
	{
		return FALSE;
	}
	Int count = 0;
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player *player = ThePlayerList->getNthPlayer(i);
		if (teodIsCoPlayer(subject, player, relationType) && teodIsNeighbouring(home, teodPlayerSpotWaypoint(player), radius))
		{
			count++;
		}
	}
	return teodCompareInt(pComparisonParm->getInt(), count, pCount);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSpotNeighbouringRelation(Parameter* pPlayerParm, Int relationType, Int pStartNdx)
{
	Player *subject = playerFromParam(pPlayerParm);
	if (!subject)
	{
		return FALSE;
	}
	Waypoint *spot = teodSpotWaypoint(pStartNdx);
	if (!spot)
	{
		return FALSE;
	}
	Real radius = teodNeighbourRadius(spot);
	if (radius < 0.0f)
	{
		return FALSE;
	}
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player *player = ThePlayerList->getNthPlayer(i);
		if (teodIsCoPlayer(subject, player, relationType) && teodIsNeighbouring(spot, teodPlayerSpotWaypoint(player), radius))
		{
			return TRUE;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNoTeams(Bool isFFA)
{
	Int activeCount = 0;
	Bool anyAlliance = FALSE;
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player *player = ThePlayerList->getNthPlayer(i);
		if (!teodIsMatchPlayer(player) || !player->isPlayerActive())
		{
			continue;
		}
		activeCount++;
		for (Int j = 0; j < ThePlayerList->getPlayerCount(); ++j)
		{
			Player *other = ThePlayerList->getNthPlayer(j);
			if (other == player || !teodIsMatchPlayer(other) || !other->isPlayerActive())
			{
				continue;
			}
			const Team *team = other->getDefaultTeam();
			if (team && player->getRelationship(team) == ALLIES)
			{
				anyAlliance = TRUE;
			}
		}
	}
	// A 1v1 is never a free for all.
	Bool freeForAll = activeCount >= 3 && !anyAlliance;
	return isFFA ? freeForAll : !freeForAll;
}

//-------------------------------------------------------------------------------------------------
struct TeodClosestData
{
	Coord3D pos;
	Real minDistSqr;
};

//-------------------------------------------------------------------------------------------------
static void teodFindClosest(Object *obj, void *userData)
{
	TeodClosestData *data = (TeodClosestData *)userData;
	if (obj->isEffectivelyDead())
	{
		return;
	}
	Real dx = obj->getPosition()->x - data->pos.x;
	Real dy = obj->getPosition()->y - data->pos.y;
	Real distSqr = dx * dx + dy * dy;
	if (distSqr < data->minDistSqr)
	{
		data->minDistSqr = distSqr;
	}
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateClosestRelationUnitToMySpawn(Int relationType, Parameter* pPlayerParm, Parameter* pComparisonParm, Real pDist)
{
	Player *subject = playerFromParam(pPlayerParm);
	Waypoint *home = teodPlayerSpotWaypoint(subject);
	if (!home)
	{
		return FALSE;
	}
	TeodClosestData data;
	data.pos = *home->getLocation();
	data.minDistSqr = FLT_MAX;
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player *player = ThePlayerList->getNthPlayer(i);
		if (teodIsCoPlayer(subject, player, relationType))
		{
			player->iterateObjects(teodFindClosest, &data);
		}
	}
	if (data.minDistSqr == FLT_MAX)
	{
		return FALSE;
	}
	return teodCompareReal(pComparisonParm->getInt(), sqrtf(data.minDistSqr), pDist);
}

//-------------------------------------------------------------------------------------------------
static void teodFindDozerSource(Object *obj, void *userData)
{
	Bool *found = (Bool *)userData;
	if (*found || obj->isEffectivelyDead())
	{
		return;
	}
	if (obj->isKindOf(KINDOF_DOZER))
	{
		*found = TRUE;
		return;
	}
	if (obj->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
	{
		return;
	}
	if (obj->isKindOf(KINDOF_COMMANDCENTER) || (obj->isKindOf(KINDOF_STRUCTURE) && obj->isKindOf(KINDOF_FS_SUPPLY_CENTER)))
	{
		*found = TRUE;
	}
}

//-------------------------------------------------------------------------------------------------
/** True if the player has no dozer or worker and no finished command center or supply center. */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateHunted(Parameter* pPlayerParm)
{
	Player *player = playerFromParam(pPlayerParm);
	if (!player)
	{
		return FALSE;
	}
	Bool found = FALSE;
	player->iterateObjects(teodFindDozerSource, &found);
	return !found;
}

//-------------------------------------------------------------------------------------------------
struct TeodAttackedData
{
	Player *player;
	const PolygonTrigger *area;
	UnsignedInt frame;
	Bool attacked;
};

//-------------------------------------------------------------------------------------------------
static void teodFindAttacked(Object *obj, void *userData)
{
	TeodAttackedData *data = (TeodAttackedData *)userData;
	if (data->attacked || obj->isEffectivelyDead() || obj->isKindOf(KINDOF_INERT) || obj->isKindOf(KINDOF_CRATE))
	{
		return;
	}
	const BodyModuleInterface *body = obj->getBodyModule();
	if (!body)
	{
		return;
	}
	const UnsignedInt *timestamp = body->getLastDamageTimestamp();
	if (!timestamp || *timestamp > data->frame || data->frame - *timestamp >= LOGICFRAMES_PER_SECOND)
	{
		return;
	}
	const DamageInfo *info = body->getLastDamageInfo();
	if (!info || !obj->isInside(data->area))
	{
		return;
	}
	Player *attacker = nullptr;
	Object *source = TheGameLogic->findObjectByID(info->in.m_sourceID);
	if (source)
	{
		attacker = source->getControllingPlayer();
	}
	else if (info->in.m_sourcePlayerMask)
	{
		attacker = ThePlayerList->getPlayerFromMask(info->in.m_sourcePlayerMask);
	}
	if (!attacker || attacker == data->player)
	{
		return;
	}
	const Team *team = attacker->getDefaultTeam();
	if (team && data->player->getRelationship(team) == ENEMIES)
	{
		data->attacked = TRUE;
	}
}

//-------------------------------------------------------------------------------------------------
/** True if an enemy damaged one of the player's objects inside the area within the last second. */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerAttackedInArea(Parameter* pPlayerParm, Parameter* pTriggerParm)
{
	Player *player = playerFromParam(pPlayerParm);
	if (!player)
	{
		return FALSE;
	}
	PolygonTrigger *area = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!area)
	{
		return FALSE;
	}
	TeodAttackedData data;
	data.player = player;
	data.area = area;
	data.frame = TheGameLogic->getFrame();
	data.attacked = FALSE;
	player->iterateObjects(teodFindAttacked, &data);
	return data.attacked;
}

//-------------------------------------------------------------------------------------------------
struct TeodValueData
{
	const PolygonTrigger *area;
	Int value;
};

//-------------------------------------------------------------------------------------------------
static void teodSumValue(Object *obj, void *userData)
{
	TeodValueData *data = (TeodValueData *)userData;
	if (obj->isEffectivelyDead() || obj->isKindOf(KINDOF_INERT) || !obj->isInside(data->area))
	{
		return;
	}
	data->value += obj->getTemplate()->friend_getBuildCost();
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerValueArea(Condition* pCondition, Parameter* pPlayerParm, Int relationType, Parameter* pComparisonParm, Int value, Parameter* pTriggerParm)
{
	Player *subject = playerFromParam(pPlayerParm);
	if (!subject)
	{
		return FALSE;
	}
	PolygonTrigger *area = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!area)
	{
		return FALSE;
	}
	TeodValueData data;
	data.area = area;
	data.value = 0;
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player *player = ThePlayerList->getNthPlayer(i);
		if (teodIsForcePlayer(subject, player, relationType))
		{
			player->iterateObjects(teodSumValue, &data);
		}
	}
	return teodCompareInt(pComparisonParm->getInt(), data.value, value);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerOwnsComparisonType(Condition* pCondition, Parameter* pPlayerParm, Int relationType, Parameter* pComparisonParm, Int value, Parameter* objectType)
{
	Player *subject = playerFromParam(pPlayerParm);
	if (!subject)
	{
		return FALSE;
	}
	TeodTemplateList types;
	teodTemplatesFromType(objectType->getString(), types);
	Int count = teodCountForceObjects(subject, relationType, &types, nullptr);
	return teodCompareInt(pComparisonParm->getInt(), count, value);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerComparisonTypeArea(Condition* pCondition, Parameter* pPlayerParm, Int relationType, Parameter* pComparisonParm, Int value, Parameter* objectType, Parameter* pTriggerParm)
{
	Player *subject = playerFromParam(pPlayerParm);
	if (!subject)
	{
		return FALSE;
	}
	PolygonTrigger *area = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!area)
	{
		return FALSE;
	}
	TeodTemplateList types;
	teodTemplatesFromType(objectType->getString(), types);
	Int count = teodCountForceObjects(subject, relationType, &types, area);
	return teodCompareInt(pComparisonParm->getInt(), count, value);
}

//-------------------------------------------------------------------------------------------------
/** True if an event newer than since matches the types (null accepts any) and lies in the area (null accepts any). */
//-------------------------------------------------------------------------------------------------
static Bool teodHasNewEvent(const ScriptTEODState::EventList &events, UnsignedInt since, const TeodTemplateList *types, PolygonTrigger *area)
{
	for (size_t i = 0; i < events.size(); ++i)
	{
		const ScriptTEODState::Event &event = events[i];
		if (event.seq <= since)
		{
			continue;
		}
		if (types && !teodTemplateInList(event.thing, *types))
		{
			continue;
		}
		if (area)
		{
			ICoord3D pos;
			pos.x = REAL_TO_INT(event.pos.x);
			pos.y = REAL_TO_INT(event.pos.y);
			pos.z = REAL_TO_INT(event.pos.z);
			if (!area->pointInTrigger(pos))
			{
				continue;
			}
		}
		return TRUE;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerLostTypeInArea(Condition* pCondition, Parameter* pPlayerParm, Parameter* pObjectType, Parameter* pArea)
{
	UnsignedInt since = TheScriptTEODState->readEvents(pCondition);
	Player *player = playerFromParam(pPlayerParm);
	if (!player)
	{
		return FALSE;
	}
	PolygonTrigger *area = TheScriptEngine->getQualifiedTriggerAreaByName(pArea->getString());
	if (!area)
	{
		return FALSE;
	}
	TeodTemplateList types;
	teodTemplatesFromType(pObjectType->getString(), types);
	if (types.empty())
	{
		return FALSE;
	}
	return teodHasNewEvent(TheScriptTEODState->getPlayerLosses(player->getPlayerIndex()), since, &types, area);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerLostUnit(Condition* pCondition, Parameter* pPlayerParm)
{
	UnsignedInt since = TheScriptTEODState->readEvents(pCondition);
	Player *player = playerFromParam(pPlayerParm);
	if (!player)
	{
		return FALSE;
	}
	return teodHasNewEvent(TheScriptTEODState->getPlayerLosses(player->getPlayerIndex()), since, nullptr, nullptr);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerDestroyedEnemyType(Condition* pCondition, Parameter* pPlayerParm, Parameter* pObjectType)
{
	UnsignedInt since = TheScriptTEODState->readEvents(pCondition);
	Player *player = playerFromParam(pPlayerParm);
	if (!player)
	{
		return FALSE;
	}
	TeodTemplateList types;
	teodTemplatesFromType(pObjectType->getString(), types);
	if (types.empty())
	{
		return FALSE;
	}
	return teodHasNewEvent(TheScriptTEODState->getPlayerKills(player->getPlayerIndex()), since, &types, nullptr);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerDestroyedEnemyUnit(Condition* pCondition, Parameter* pPlayerParm)
{
	UnsignedInt since = TheScriptTEODState->readEvents(pCondition);
	Player *player = playerFromParam(pPlayerParm);
	if (!player)
	{
		return FALSE;
	}
	return teodHasNewEvent(TheScriptTEODState->getPlayerKills(player->getPlayerIndex()), since, nullptr, nullptr);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateMapControl(Parameter* pPlayerParm, Parameter* pComparisonParm, Real pValue)
{
	Player *player = playerFromParam(pPlayerParm);
	if (!player)
	{
		return FALSE;
	}
	TeodControlGrid grid;
	teodInitControlGrid(grid);
	player->iterateObjects(teodMarkControl, &grid);
	return teodCompareControl(grid, nullptr, pComparisonParm->getInt(), pValue);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationMapControl(Parameter* pPlayerParm, Int relationType, Parameter* pComparisonParm, Real pValue)
{
	Player *subject = playerFromParam(pPlayerParm);
	if (!subject)
	{
		return FALSE;
	}
	return teodRelationMapControl(subject, relationType, pComparisonParm->getInt(), pValue, nullptr);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateMapControlArea(Parameter* pPlayerParm, Parameter* pComparisonParm, Real pValue, Parameter* pTriggerParm)
{
	Player *player = playerFromParam(pPlayerParm);
	if (!player)
	{
		return FALSE;
	}
	PolygonTrigger *area = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!area)
	{
		return FALSE;
	}
	TeodControlGrid grid;
	teodInitControlGrid(grid);
	player->iterateObjects(teodMarkControl, &grid);
	return teodCompareControl(grid, area, pComparisonParm->getInt(), pValue);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationMapControlArea(Parameter* pPlayerParm, Int relationType, Parameter* pComparisonParm, Real pValue, Parameter* pTriggerParm)
{
	Player *subject = playerFromParam(pPlayerParm);
	if (!subject)
	{
		return FALSE;
	}
	PolygonTrigger *area = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!area)
	{
		return FALSE;
	}
	return teodRelationMapControl(subject, relationType, pComparisonParm->getInt(), pValue, area);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerSightedRelationType(Parameter* pPlayerParm, Int relationType, Parameter* pObjectType)
{
	Player *player = playerFromParam(pPlayerParm);
	if (!player)
	{
		return FALSE;
	}
	TeodTemplateList types;
	teodTemplatesFromType(pObjectType->getString(), types);
	return teodPlayerSighted(player, relationType, &types, nullptr);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerSightedRelationType(Parameter* pPlayerParm, Int playerRelationType, Int relationType, Parameter* pObjectType)
{
	Player *subject = playerFromParam(pPlayerParm);
	if (!subject)
	{
		return FALSE;
	}
	TeodTemplateList types;
	teodTemplatesFromType(pObjectType->getString(), types);
	return teodForceSighted(subject, playerRelationType, relationType, &types, nullptr);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerSightedRelationArea(Parameter* pPlayerParm, Int playerRelationType, Int relationType, Parameter* pTriggerParm)
{
	Player *subject = playerFromParam(pPlayerParm);
	if (!subject)
	{
		return FALSE;
	}
	PolygonTrigger *area = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!area)
	{
		return FALSE;
	}
	return teodForceSighted(subject, playerRelationType, relationType, nullptr, area);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerSightedRelationTypeArea(Parameter* pPlayerParm, Int playerRelationType, Int relationType, Parameter* pObjectType, Parameter* pTriggerParm)
{
	Player *subject = playerFromParam(pPlayerParm);
	if (!subject)
	{
		return FALSE;
	}
	PolygonTrigger *area = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!area)
	{
		return FALSE;
	}
	TeodTemplateList types;
	teodTemplatesFromType(pObjectType->getString(), types);
	return teodForceSighted(subject, playerRelationType, relationType, &types, area);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerHasComparisonRatioOther(Condition* pCondition, Parameter* pPlayerParm, Parameter* pComparisonParm, Real ratio, Parameter* pOther)
{
	Player *player = playerFromParam(pPlayerParm);
	Player *other = playerFromParam(pOther);
	if (!player || !other)
	{
		return FALSE;
	}
	Int count = teodCountObjects(player, nullptr, nullptr);
	Int countOther = teodCountObjects(other, nullptr, nullptr);
	return teodCompareRatio(pComparisonParm->getInt(), count, countOther, ratio);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerHasComparisonRatioTypeOther(Condition* pCondition, Parameter* pPlayerParm, Parameter* pComparisonParm, Real ratio, Parameter* objectType, Parameter* pOther, Parameter* otherObjectType)
{
	Player *player = playerFromParam(pPlayerParm);
	Player *other = playerFromParam(pOther);
	if (!player || !other)
	{
		return FALSE;
	}
	TeodTemplateList types;
	TeodTemplateList typesOther;
	teodTemplatesFromType(objectType->getString(), types);
	teodTemplatesFromType(otherObjectType->getString(), typesOther);
	Int count = teodCountObjects(player, &types, nullptr);
	Int countOther = teodCountObjects(other, &typesOther, nullptr);
	return teodCompareRatio(pComparisonParm->getInt(), count, countOther, ratio);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerHasComparisonRatioAreaOther(Condition* pCondition, Parameter* pPlayerParm, Parameter* pComparisonParm, Real ratio, Parameter* pTriggerParm, Parameter* pOther)
{
	Player *player = playerFromParam(pPlayerParm);
	Player *other = playerFromParam(pOther);
	if (!player || !other)
	{
		return FALSE;
	}
	PolygonTrigger *area = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!area)
	{
		return FALSE;
	}
	Int count = teodCountObjects(player, nullptr, area);
	Int countOther = teodCountObjects(other, nullptr, area);
	return teodCompareRatio(pComparisonParm->getInt(), count, countOther, ratio);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerHasComparisonRatioTypeAreaOther(Condition* pCondition, Parameter* pPlayerParm, Parameter* pComparisonParm, Real ratio, Parameter* objectType, Parameter* pTriggerParm, Parameter* pOther, Parameter* otherObjectType)
{
	Player *player = playerFromParam(pPlayerParm);
	Player *other = playerFromParam(pOther);
	if (!player || !other)
	{
		return FALSE;
	}
	PolygonTrigger *area = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!area)
	{
		return FALSE;
	}
	TeodTemplateList types;
	TeodTemplateList typesOther;
	teodTemplatesFromType(objectType->getString(), types);
	teodTemplatesFromType(otherObjectType->getString(), typesOther);
	Int count = teodCountObjects(player, &types, area);
	Int countOther = teodCountObjects(other, &typesOther, area);
	return teodCompareRatio(pComparisonParm->getInt(), count, countOther, ratio);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerHasComparisonRatioOtherRelation(Condition* pCondition, Parameter* pPlayerParm, Int pRelation, Parameter* pComparisonParm, Real ratio, Int pOtherRelation)
{
	Player *subject = playerFromParam(pPlayerParm);
	if (!subject)
	{
		return FALSE;
	}
	Int count = teodCountForceObjects(subject, pRelation, nullptr, nullptr);
	Int countOther = teodCountForceObjects(subject, pOtherRelation, nullptr, nullptr);
	return teodCompareRatio(pComparisonParm->getInt(), count, countOther, ratio);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerHasComparisonRatioTypeOtherRelation(Condition* pCondition, Parameter* pPlayerParm, Int pRelation, Parameter* pComparisonParm, Real ratio, Parameter* objectType, Int pOtherRelation, Parameter* otherObjectType)
{
	Player *subject = playerFromParam(pPlayerParm);
	if (!subject)
	{
		return FALSE;
	}
	TeodTemplateList types;
	TeodTemplateList typesOther;
	teodTemplatesFromType(objectType->getString(), types);
	teodTemplatesFromType(otherObjectType->getString(), typesOther);
	Int count = teodCountForceObjects(subject, pRelation, &types, nullptr);
	Int countOther = teodCountForceObjects(subject, pOtherRelation, &typesOther, nullptr);
	return teodCompareRatio(pComparisonParm->getInt(), count, countOther, ratio);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerHasComparisonRatioAreaOtherRelation(Condition* pCondition, Parameter* pPlayerParm, Int pRelation, Parameter* pComparisonParm, Real ratio, Parameter* pTriggerParm, Int pOtherRelation)
{
	Player *subject = playerFromParam(pPlayerParm);
	if (!subject)
	{
		return FALSE;
	}
	PolygonTrigger *area = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!area)
	{
		return FALSE;
	}
	Int count = teodCountForceObjects(subject, pRelation, nullptr, area);
	Int countOther = teodCountForceObjects(subject, pOtherRelation, nullptr, area);
	return teodCompareRatio(pComparisonParm->getInt(), count, countOther, ratio);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerHasComparisonRatioTypeAreaOtherRelation(Condition* pCondition, Parameter* pPlayerParm, Int pRelation, Parameter* pComparisonParm, Real ratio, Parameter* objectType, Parameter* pTriggerParm, Int pOtherRelation, Parameter* otherObjectType)
{
	Player *subject = playerFromParam(pPlayerParm);
	if (!subject)
	{
		return FALSE;
	}
	PolygonTrigger *area = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!area)
	{
		return FALSE;
	}
	TeodTemplateList types;
	TeodTemplateList typesOther;
	teodTemplatesFromType(objectType->getString(), types);
	teodTemplatesFromType(otherObjectType->getString(), typesOther);
	Int count = teodCountForceObjects(subject, pRelation, &types, area);
	Int countOther = teodCountForceObjects(subject, pOtherRelation, &typesOther, area);
	return teodCompareRatio(pComparisonParm->getInt(), count, countOther, ratio);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerTeamInstances(Parameter* pPlayerParm, Parameter* pComparisonParm, Int value, Parameter* pTeamParm)
{
	Player *player = playerFromParam(pPlayerParm);
	if (!player)
	{
		return FALSE;
	}
	const TeamPrototype *proto = TheTeamFactory->findTeamPrototype(pTeamParm->getString());
	if (!proto)
	{
		Team *team = TheScriptEngine->getTeamNamed(pTeamParm->getString());
		proto = team ? team->getPrototype() : nullptr;
	}
	Int count = 0;
	if (proto && proto->getControllingPlayer() == player)
	{
		for (DLINK_ITERATOR<Team> iter = proto->iterate_TeamInstanceList(); !iter.done(); iter.advance())
		{
			if (iter.cur()->isActive())
			{
				count++;
			}
		}
	}
	return teodCompareInt(pComparisonParm->getInt(), count, value);
}
