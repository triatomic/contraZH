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

// FILE: ScriptConditionsCLP.cpp //////////////////////////////////////////////////////////////////
// Dispatch for the CLP_AI script conditions ported from CookieLandProjects/CLP_AI.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/ScriptConditions.h"
#include "GameLogic/ScriptEngine.h"

//-------------------------------------------------------------------------------------------------
/** Evaluates a CLP condition into result. Returns false if the condition is not one of them. */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateCLPCondition( Condition *pCondition, Bool *result )
{
	switch (pCondition->getConditionType())
	{
		default:
			return FALSE;
		case Condition::KD_RATIO:
			*result = TheScriptEngine->evaluateKDRatio(pCondition);
			return TRUE;
		case Condition::COMPARE_COUNTERS:
			*result = TheScriptEngine->evaluateTwoCounters(pCondition);
			return TRUE;
		case Condition::COUNTER_DIVISIBLE:
			*result = TheScriptEngine->evaluateCounterDivisible(pCondition);
			return TRUE;
		case Condition::PLAYER_RELATION_FACTION:
			*result = evaluateSkirmishAnyRelationFaction(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2));
			return TRUE;
		case Condition::RELATION_IS:
			*result = evaluatePlayerRelation(pCondition->getParameter(0)->getString(), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2)->getString());
			return TRUE;
		case Condition::SPOT_EMPTY:
			*result = evaluateEmptySpot(pCondition->getParameter(0), pCondition->getParameter(1)->getInt());
			return TRUE;
		case Condition::SPOT_NEIGHBOURING:
			*result = evaluateNeighbouringSpot(pCondition->getParameter(0), pCondition->getParameter(1));
			return TRUE;
		case Condition::NEIGHBOURING_SPOTS_EMPTY:
			*result = evaluateNeighbouringSpotsEmpty(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getInt());
			return TRUE;
		case Condition::NEIGHBOURING_SPOTS_RELATION:
			*result = evaluateNeighbouringSpotsRelation(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getInt(), pCondition->getParameter(3)->getInt());
			return TRUE;
		case Condition::MAP_COMPARE_SIZE:
			*result = evaluateMapSize(pCondition->getParameter(0), pCondition->getParameter(1)->getInt());
			return TRUE;
		case Condition::MAP_ACTIVE_PLAYERS:
			*result = evaluateActivePlayerCount(pCondition->getParameter(0), pCondition->getParameter(1)->getInt());
			return TRUE;
		case Condition::MAP_EMPTY_SPOTS:
			*result = evaluateEmptySpotCount(pCondition->getParameter(0), pCondition->getParameter(1)->getInt());
			return TRUE;
		case Condition::STARTING_CASH_COMPARE:
			*result = evaluateStartingCash(pCondition->getParameter(0), pCondition->getParameter(1)->getInt());
			return TRUE;
		case Condition::CLOSEST_ENEMY_DISTANCE:
			*result = evaluateClosestRelationUnitToMySpawn(pCondition->getParameter(0)->getInt(), pCondition->getParameter(1), pCondition->getParameter(2), pCondition->getParameter(3)->getReal());
			return TRUE;
		case Condition::PLAYER_HUNTED:
			*result = evaluateHunted(pCondition->getParameter(0));
			return TRUE;
		case Condition::PLAYER_LOST_TYPE_AREA:
			*result = evaluatePlayerLostTypeInArea(pCondition, pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
			return TRUE;
		case Condition::PLAYER_LOST_UNIT:
			*result = evaluatePlayerLostUnit(pCondition, pCondition->getParameter(0));
			return TRUE;
		case Condition::TEAM_SIGHTED_RELATION_TYPE:
			*result = evaluateTeamSightedRelationType(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2));
			return TRUE;
		case Condition::TEAM_LOST_TYPE:
			*result = evaluateTeamLostType(pCondition, pCondition->getParameter(0), pCondition->getParameter(1));
			return TRUE;
		case Condition::TEAM_LOST_UNIT:
			*result = evaluateTeamLostUnit(pCondition, pCondition->getParameter(0));
			return TRUE;
		case Condition::PLAYER_DESTROYED_ENEMY_TYPE:
			*result = evaluatePlayerDestroyedEnemyType(pCondition, pCondition->getParameter(0), pCondition->getParameter(1));
			return TRUE;
		case Condition::PLAYER_DESTROYED_ENEMY_UNIT:
			*result = evaluatePlayerDestroyedEnemyUnit(pCondition, pCondition->getParameter(0));
			return TRUE;
		case Condition::PLAYER_MAPCONTROL:
			*result = evaluateMapControl(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getReal());
			return TRUE;
		case Condition::PLAYER_RELATION_MAPCONTROL:
			*result = evaluateRelationMapControl(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getReal());
			return TRUE;
		case Condition::PLAYER_MAPCONTROL_AREA:
			*result = evaluateMapControlArea(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getReal(), pCondition->getParameter(3));
			return TRUE;
		case Condition::PLAYER_RELATION_MAPCONTROL_AREA:
			*result = evaluateRelationMapControlArea(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getReal(), pCondition->getParameter(4));
			return TRUE;
		case Condition::PLAYER_SIGHTED_RELATION_TYPE:
			*result = evaluatePlayerSightedRelationType(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2));
			return TRUE;
		case Condition::RELATION_PLAYER_SIGHTED_RELATION_TYPE:
			*result = evaluateRelationPlayerSightedRelationType(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2)->getInt(), pCondition->getParameter(3));
			return TRUE;
		case Condition::RELATION_PLAYER_VALUE_AREA:
			*result = evaluateRelationPlayerValueArea(pCondition, pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getInt(),pCondition->getParameter(4));
			return TRUE;
		case Condition::RELATION_PLAYER_OWNS_COMPARISON_TYPE:
			*result = evaluateRelationPlayerOwnsComparisonType(pCondition, pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getInt(), pCondition->getParameter(4));
			return TRUE;
		case Condition::PLAYER_ATTACKED_AREA:
			*result = evaluatePlayerAttackedInArea(pCondition->getParameter(0), pCondition->getParameter(1));
			return TRUE;
		case Condition::PLAYER_BUILDING_BEING_CAPTURED:
			*result = evaluatePlayerBuildingBeingCaptured(pCondition->getParameter(0));
			return TRUE;
		case Condition::PLAYER_BUILDING_BEING_CAPTURED_TYPE:
			*result = evaluatePlayerBuildingBeingCapturedType(pCondition->getParameter(0), pCondition->getParameter(1));
			return TRUE;
		case Condition::PLAYER_BUILDING_BEING_CAPTURED_AREA:
			*result = evaluatePlayerBuildingBeingCapturedArea(pCondition->getParameter(0), pCondition->getParameter(1));
			return TRUE;
		case Condition::PLAYER_BUILDING_BEING_CAPTURED_TYPE_AREA:
			*result = evaluatePlayerBuildingBeingCapturedTypeArea(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
			return TRUE;
		case Condition::RELATION_PLAYER_COMPARISON_TYPE_AREA:
			*result = evaluateRelationPlayerComparisonTypeArea(pCondition, pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getInt(), pCondition->getParameter(4), pCondition->getParameter(5));
			return TRUE;
		case Condition::PLAYER_COMPARISON_RATIO_OTHER:
			*result = evaluatePlayerHasComparisonRatioOther(pCondition, pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getReal(), pCondition->getParameter(3));
			return TRUE;
		case Condition::PLAYER_COMPARISON_RATIO_TYPE_OTHER:
			*result = evaluatePlayerHasComparisonRatioTypeOther(pCondition, pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getReal(), pCondition->getParameter(3), pCondition->getParameter(4), pCondition->getParameter(5));
			return TRUE;
		case Condition::PLAYER_COMPARISON_RATIO_AREA_OTHER:
			*result = evaluatePlayerHasComparisonRatioAreaOther(pCondition, pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getReal(), pCondition->getParameter(3), pCondition->getParameter(4));
			return TRUE;
		case Condition::PLAYER_COMPARISON_RATIO_TYPE_AREA_OTHER:
			*result = evaluatePlayerHasComparisonRatioTypeAreaOther(pCondition, pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getReal(), pCondition->getParameter(3), pCondition->getParameter(4), pCondition->getParameter(5), pCondition->getParameter(6));
			return TRUE;
		case Condition::RELATION_PLAYER_COMPARISON_RATIO_OTHER_RELATION:
			*result = evaluateRelationPlayerHasComparisonRatioOtherRelation(pCondition, pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getReal(), pCondition->getParameter(4)->getInt());
			return TRUE;
		case Condition::RELATION_PLAYER_COMPARISON_RATIO_TYPE_OTHER_RELATION:
			*result = evaluateRelationPlayerHasComparisonRatioTypeOtherRelation(pCondition, pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getReal(), pCondition->getParameter(4), pCondition->getParameter(5)->getInt(), pCondition->getParameter(6));
			return TRUE;
		case Condition::RELATION_PLAYER_COMPARISON_RATIO_AREA_OTHER_RELATION:
			*result = evaluateRelationPlayerHasComparisonRatioAreaOtherRelation(pCondition, pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getReal(), pCondition->getParameter(4), pCondition->getParameter(5)->getInt());
			return TRUE;
		case Condition::RELATION_PLAYER_COMPARISON_RATIO_TYPE_AREA_OTHER_RELATION:
			*result = evaluateRelationPlayerHasComparisonRatioTypeAreaOtherRelation(pCondition, pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getReal(), pCondition->getParameter(4), pCondition->getParameter(5), pCondition->getParameter(6)->getInt(), pCondition->getParameter(7));
			return TRUE;
		case Condition::RELATION_PLAYER_SIGHTED_RELATION_AREA:
			*result = evaluateRelationPlayerSightedRelationArea(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2)->getInt(), pCondition->getParameter(3));
			return TRUE;
		case Condition::RELATION_PLAYER_SIGHTED_RELATION_TYPE_AREA:
			*result = evaluateRelationPlayerSightedRelationTypeArea(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2)->getInt(), pCondition->getParameter(3), pCondition->getParameter(4));
			return TRUE;
		case Condition::TEAM_IDLE:
			*result = evaluateTeamIdle(pCondition->getParameter(0));
			return TRUE;
		case Condition::TEAM_CONTAINS_TYPE:
			*result = evaluateTeamContainsType(pCondition->getParameter(0), pCondition->getParameter(1));
			return TRUE;
		case Condition::TEAM_CONTAINS_COMPARISON_TYPE:
			*result = evaluateTeamContainsComparisonType(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getInt(), pCondition->getParameter(3));
			return TRUE;
		case Condition::TEAM_SINGLE_BELOW_HEALTH:
			*result = evaluateTeamSingleBelowHealth(pCondition->getParameter(0), pCondition->getParameter(1)->getReal());
			return TRUE;
		case Condition::TEAM_BELOW_HEALTH:
			*result = evaluateTeamBelowHealth(pCondition->getParameter(0), pCondition->getParameter(1)->getReal());
			return TRUE;
		case Condition::TEAM_IDLE_FRAMES:
			*result = evaluateTeamIdleFrames(pCondition->getParameter(0), pCondition->getParameter(1)->getInt());
			return TRUE;
		case Condition::TEAM_SEEN:
			*result = evaluateTeamSeen(pCondition->getParameter(0));
			return TRUE;
		case Condition::PLAYER_COMPARISON_TEAM_INSTANCES:
			*result = evaluatePlayerTeamInstances(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getInt(), pCondition->getParameter(3));
			return TRUE;
		case Condition::TEAM_CONTAINS_COMPARISON_RATIO_SIGHTED:
			*result = evaluateTeamHasComparisonRatioSighted(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getReal());
			return TRUE;
		case Condition::TEAM_CONTAINS_COMPARISON_TYPE_RATIO_SIGHTED:
			*result = evaluateTeamHasComparisonRatioTypeSighted(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getReal(), pCondition->getParameter(3), pCondition->getParameter(4));
			return TRUE;
		case Condition::SPOT_NEIGHBOURING_RELATION:
			*result = evaluateSpotNeighbouringRelation(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2)->getInt());
			return TRUE;
		case Condition::TEAM_CLOSEST_RELATION_TYPE:
			*result = evaluateTeamClosestRelationType(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3), pCondition->getParameter(4)->getReal());
			return TRUE;
		case Condition::UNIT_CLOSEST_RELATION_TYPE:
			*result = evaluateUnitClosestRelationType(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3), pCondition->getParameter(4)->getReal());
			return TRUE;
		case Condition::TEAM_ALL_CLEAR:
			*result = evaluateTeamAllClear(pCondition->getParameter(0));
			return TRUE;
		case Condition::NO_TEAMS:
			*result = evaluateNoTeams(pCondition->getParameter(0)->getInt());
			return TRUE;
		case Condition::TEAM_COMPARISON_APART:
			*result = evaluateTeamApart(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getReal());
			return TRUE;
		case Condition::TEAM_CAN_LOAD_TRANSPORTS:
			*result = evaluateTeamCanLoadTransports(pCondition->getParameter(0));
			return TRUE;
		case Condition::TEAM_CLOSEST_TO_ENEMY_COMPARISON_RATIO_SIGHTED_RADIUS:
			*result = evaluateClosestTeamMemberToEnemyHasSightedComparisonRatioRadius(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getReal(), pCondition->getParameter(3)->getReal());
			return TRUE;
		case Condition::TEAM_CLOSEST_TO_ENEMY_COMPARISON_TYPE_RATIO_SIGHTED_RADIUS:
			*result = evaluateClosestTeamMemberToEnemyHasSightedComparisonTypeRatioRadius(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getReal(), pCondition->getParameter(3), pCondition->getParameter(4), pCondition->getParameter(5)->getReal());
			return TRUE;
		case Condition::UNIT_RELOADED:
			*result = evaluateUnitIsReloaded(pCondition->getParameter(0), pCondition->getParameter(1)->getInt());
			return TRUE;
		case Condition::TEAM_RELOADED:
			*result = evaluateTeamIsReloaded(pCondition->getParameter(0), pCondition->getParameter(1)->getInt());
			return TRUE;
	}
}
