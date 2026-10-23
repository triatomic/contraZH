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

// FILE: ScriptActionsCLP.cpp /////////////////////////////////////////////////////////////////////
// Dispatch for the CLP_AI script actions ported from CookieLandProjects/CLP_AI.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/ScriptActions.h"
#include "GameLogic/ScriptEngine.h"

//-------------------------------------------------------------------------------------------------
/** Runs a CLP action. Returns false if the action is not one of them. */
//-------------------------------------------------------------------------------------------------
Bool ScriptActions::executeCLPAction( ScriptAction *pAction )
{
	switch (pAction->getActionType())
	{
		default:
			return FALSE;
		case ScriptAction::SET_COUNTER_RANDOM:
			TheScriptEngine->setCounterRandom(pAction);
			return TRUE;
		case ScriptAction::COPY_COUNTER:
			TheScriptEngine->copyCounter(pAction);
			return TRUE;
		case ScriptAction::SET_KD_RATIO:
			TheScriptEngine->setKDRatio(pAction);
			return TRUE;
		case ScriptAction::INCREMENT_KD_RATIO_KILLS:
			TheScriptEngine->addKDRatioKills(pAction);
			return TRUE;
		case ScriptAction::INCREMENT_KD_RATIO_DEATHS:
			TheScriptEngine->addKDRatioDeaths(pAction);
			return TRUE;
		case ScriptAction::DECREMENT_KD_RATIO_KILLS:
			TheScriptEngine->subKDRatioKills(pAction);
			return TRUE;
		case ScriptAction::DECREMENT_KD_RATIO_DEATHS:
			TheScriptEngine->subKDRatioDeaths(pAction);
			return TRUE;
		case ScriptAction::UPDATE_KD_RATIO_KILLS_BUILDCOST:
			TheScriptEngine->updateKDRatioKills(pAction);
			return TRUE;
		case ScriptAction::UPDATE_KD_RATIO_DEATHS_BUILDCOST:
			TheScriptEngine->updateKDRatioDeaths(pAction);
			return TRUE;
		case ScriptAction::COPY_KD_RATIO_ONTO_COUNTERS:
			TheScriptEngine->copyKDRatioOntoCounters(pAction);
			return TRUE;
		case ScriptAction::ADD_PRIORITY_SET_TO_OTHER:
			TheScriptEngine->addPrioritySetToOther(pAction);
			return TRUE;
		case ScriptAction::INCREASE_PRIORITY_IN_SET_TYPE:
			TheScriptEngine->increasePriorityOfType(pAction);
			return TRUE;
		case ScriptAction::DECREASE_PRIORITY_IN_SET_TYPE:
			TheScriptEngine->decreasePriorityOfType(pAction);
			return TRUE;
		case ScriptAction::AI_PLAYER_BUILDS_UNNAMED:
			doPlayerBuildUnit(pAction->getParameter(1)->getString(), pAction->getParameter(0)->getString());
			return TRUE;
		case ScriptAction::AI_PLAYER_BUILD_TYPE_NEAREST_TYPE_ROTATED:
			doBuildObjectNearestTypeAngle(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString(), pAction->getParameter(3)->getReal(), pAction->getParameter(4)->getInt());
			return TRUE;
		case ScriptAction::AI_PLAYER_BUILD_TYPE_NEAREST_KINDOF_ROTATED:
			doBuildObjectNearestKindOfAngle(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt(), pAction->getParameter(3)->getReal(), pAction->getParameter(4)->getInt());
			return TRUE;
		case ScriptAction::AI_PLAYER_BUILD_TYPE_NEAREST_TEAM_ROTATED:
			doBuildObjectNearestTeamAngle(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString(), pAction->getParameter(3)->getReal());
			return TRUE;
		case ScriptAction::AI_PLAYER_BUILD_TYPE_NEAREST_SUPPLY_ROTATED:
			doBuildSupplyCenterAngle(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt(), pAction->getParameter(3)->getReal());
			return TRUE;
		case ScriptAction::AI_PLAYER_BUILD_DEFENSE_FRONT_FROM_VECTOR_ROTATED_PERCENT:
			doAIPlayerBuildDefenseStructureFromVector(false, pAction->getParameter(0)->getReal(), pAction->getParameter(1)->getReal());
			return TRUE;
		case ScriptAction::AI_PLAYER_BUILD_DEFENSE_FLANK_FROM_VECTOR_ROTATED_PERCENT:
			doAIPlayerBuildDefenseStructureFromVector(true, pAction->getParameter(0)->getReal(), pAction->getParameter(1)->getReal());
			return TRUE;
		case ScriptAction::AI_PLAYER_ADD_DEFENSE_TO_VECTOR:
			doAIPlayerAddBaseDefenseStructure(pAction->getParameter(0)->getString());
			return TRUE;
		case ScriptAction::AI_PLAYER_REMOVE_DEFENSE_FROM_VECTOR:
			doAIPlayerRemoveBaseDefenseStructure(pAction->getParameter(0)->getString());
			return TRUE;
		case ScriptAction::AI_PLAYER_BUILD_DEFENSE_FRONT_FROM_VECTOR_ROTATED_PERCENT_AT_PLAYER:
			doAIPlayerBuildDefenseStructureFromVectorAtPlayer(false, pAction->getParameter(0)->getReal(), pAction->getParameter(1)->getReal(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::AI_PLAYER_BUILD_DEFENSE_FLANK_FROM_VECTOR_ROTATED_PERCENT_AT_PLAYER:
			doAIPlayerBuildDefenseStructureFromVectorAtPlayer(true, pAction->getParameter(0)->getReal(), pAction->getParameter(1)->getReal(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::PLAYER_SURRENDER:
			doPlayerSurrender(pAction->getParameter(0)->getString());
			return TRUE;
		case ScriptAction::PLAYER_MERGE_KINDOF:
			doPlayerMergeKindOf(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::PLAYER_MERGE_TYPE:
			doPlayerMergeType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::PLAYER_DISBAND_KINDOF:
			doPlayerDisbandKindOf(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::PLAYER_DISBAND_TYPE:
			doPlayerDisbandType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::PLAYER_GARRISON_BUILDINGS_WITH_MAX_NUMBER:
			doPlayerGarrisonMaxEach(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::PLAYER_GARRISON_NUMBER_BUILDINGS:
			doPlayerGarrisonEqually(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::TEAM_MOVE_RELATIVE:
		{
			Coord3D posA;
			pAction->getParameter(1)->getCoord3D(&posA);
			doTeamMoveRelative(pAction->getParameter(0)->getString(), &posA);
			return TRUE;
		}
		case ScriptAction::TEAM_MOVE_NEAREST_BELONGING_TO_PLAYER:
			doTeamMoveNearestBelongingToPlayer(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString(), pAction->getParameter(3)->getInt());
			return TRUE;
		case ScriptAction::TEAM_MOVE_AWAY_FROM_RELATION:
			doTeamMoveAwayFromRelation(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getReal(), pAction->getParameter(2)->getInt(), pAction->getParameter(3)->getInt());
			return TRUE;
		case ScriptAction::TEAM_MOVE_AWAY_FROM_RELATION_TYPE:
			doTeamMoveAwayFromRelationType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getReal(), pAction->getParameter(2)->getInt(), pAction->getParameter(3)->getString(), pAction->getParameter(4)->getInt());
			return TRUE;
		case ScriptAction::TEAM_MOVE_TOWARDS_RELATION:
			doTeamMoveTowardsRelation(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getReal(), pAction->getParameter(2)->getInt(), pAction->getParameter(3)->getInt());
			return TRUE;
		case ScriptAction::TEAM_MOVE_TOWARDS_RELATION_TYPE:
			doTeamMoveTowardsRelationType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getReal(), pAction->getParameter(2)->getInt(), pAction->getParameter(3)->getString(), pAction->getParameter(4)->getInt());
			return TRUE;
		case ScriptAction::TEAM_MEET:
			doTeamMeet(pAction->getParameter(0)->getString());
			return TRUE;
		case ScriptAction::TEAM_MEET_AT_KINDOF:
			doTeamMeetKindOf(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::TEAM_MEET_AT_TYPE:
			doTeamMeetType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::TEAM_USE_COMMAND_BUTTON_ABILITY_WITH_TYPE:
			doTeamUseCommandButtonAbilityOnType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::TEAM_USE_COMMAND_BUTTON_ABILITY_ON_TEAM:
			doTeamUseCommandButtonAbilityOnTeam(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::TEAM_MERGE_KINDOF:
			doTeamMergeKindOf(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::TEAM_MERGE_TYPE:
			doTeamMergeType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::TEAM_DISBAND_KINDOF:
			doTeamDisbandKindOf(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::TEAM_DISBAND_TYPE:
			doTeamDisbandType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::TEAM_GARRISON_BUILDINGS_WITH_MAX_NUMBER:
			doTeamGarrisonMaxEach(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::TEAM_GARRISON_NUMBER_BUILDINGS:
			doTeamGarrisonEqually(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::TEAM_MOVE_TO_TEAM:
			doTeamMoveToTeam(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::TEAM_MEET_TEAM:
			doTeamMeetTeam(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::TEAM_LOAD_EVENLY:
			doTeamLoadAllTransportsEvenly(pAction->getParameter(0)->getString());
			return TRUE;
		case ScriptAction::UNIT_MOVE_RELATIVE:
		{
			Coord3D posB;
			pAction->getParameter(1)->getCoord3D(&posB);
			doUnitMoveRelative(pAction->getParameter(0)->getString(), &posB);
			return TRUE;
		}
		case ScriptAction::UNIT_MOVE_NEAREST_BELONGING_TO_PLAYER:
			doUnitMoveNearestBelongingToPlayer(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString(), pAction->getParameter(3)->getInt());
			return TRUE;
		case ScriptAction::UNIT_MOVE_AWAY_FROM_RELATION:
			doUnitMoveAwayFromRelation(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getReal(), pAction->getParameter(2)->getInt(), pAction->getParameter(3)->getInt());
			return TRUE;
		case ScriptAction::UNIT_MOVE_AWAY_FROM_RELATION_TYPE:
			doUnitMoveAwayFromRelationType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getReal(), pAction->getParameter(2)->getInt(), pAction->getParameter(3)->getString(), pAction->getParameter(4)->getInt());
			return TRUE;
		case ScriptAction::UNIT_MOVE_TOWARDS_RELATION:
			doUnitMoveTowardsRelation(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getReal(), pAction->getParameter(2)->getInt(), pAction->getParameter(3)->getInt());
			return TRUE;
		case ScriptAction::UNIT_MOVE_TOWARDS_RELATION_TYPE:
			doUnitMoveTowardsRelationType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getReal(), pAction->getParameter(2)->getInt(), pAction->getParameter(3)->getString(), pAction->getParameter(4)->getInt());
			return TRUE;
		case ScriptAction::CREATE_TEAM_FROM_TEAMLESS:
			doPlayerTeamlessMerge(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::CREATE_TEAM_FROM_TEAMLESS_TYPE:
			doPlayerTeamlessMergeType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::CREATE_TEAM_FROM_TEAMLESS_KINDOF:
			doPlayerTeamlessMergeKindOf(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::SKIRMISH_FIRE_SPECIAL_POWER_AT_MOST_COST_TYPE:
			doSkirmishFireSpecialPowerAtMostCostType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::PLAYER_HUNT_TYPE:
			doPlayerHuntType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::TEAM_HUNT_TYPE:
			doTeamHuntType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::PLAYER_SELL_ALL_BUILDINGS_IN_CAPTURE_PROCESS_PERCENTAGE:
			doPlayerSellAllBuildingsInCaptureProcessPercentage(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::PLAYER_SELL_ALL_BUILDINGS_TYPE_IN_CAPTURE_PROCESS_PERCENTAGE:
			doPlayerSellAllBuildingsTypeInCaptureProcessPercentage(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt());
			return TRUE;
		case ScriptAction::PLAYER_SELL_ALL_BUILDINGS_AREA_IN_CAPTURE_PROCESS_PERCENTAGE:
			doPlayerSellAllBuildingsAreaInCaptureProcessPercentage(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt());
			return TRUE;
		case ScriptAction::PLAYER_SELL_ALL_BUILDINGS_TYPE_AREA_IN_CAPTURE_PROCESS_PERCENTAGE:
			doPlayerSellAllBuildingsTypeAreaInCaptureProcessPercentage(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString(), pAction->getParameter(3)->getInt());
			return TRUE;
		case ScriptAction::PLAYER_CANCEL_ALL_CONSTRUCTIONS:
			doPlayerCancelAllConstructions(pAction->getParameter(0)->getString());
			return TRUE;
		case ScriptAction::PLAYER_CANCEL_ALL_CONSTRUCTIONS_TYPE:
			doPlayerCancelAllConstructionsType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::PLAYER_CANCEL_ALL_CONSTRUCTIONS_AREA:
			doPlayerCancelAllConstructionsArea(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::PLAYER_CANCEL_ALL_CONSTRUCTIONS_TYPE_AREA:
			doPlayerCancelAllConstructionsTypeArea(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::PLAYER_SELL_ALL_BUILDINGS_TYPE:
			doPlayerSellAllBuildingsType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::PLAYER_SELL_ALL_BUILDINGS_AREA:
			doPlayerSellAllBuildingsArea(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::PLAYER_SELL_ALL_BUILDINGS_TYPE_AREA:
			doPlayerSellAllBuildingsTypeArea(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::AI_PLAYER_BUILD_TYPE_NEAREST_KINDOF_ROTATED_AREA:
			doBuildObjectNearestKindOfAngleArea(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt(), pAction->getParameter(3)->getString(), pAction->getParameter(4)->getReal(), pAction->getParameter(5)->getInt());
			return TRUE;
		case ScriptAction::AI_PLAYER_BUILD_TYPE_NEAREST_TYPE_ROTATED_AREA:
			doBuildObjectNearestTypeAngleArea(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString(), pAction->getParameter(3)->getString(), pAction->getParameter(4)->getReal(), pAction->getParameter(5)->getInt());
			return TRUE;
		case ScriptAction::TEAM_ATTACK_TYPE:
			doTeamAttackType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt());
			return TRUE;
		case ScriptAction::TEAM_ATTACK_TYPE_AREA:
			doTeamAttackTypeArea(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString(), pAction->getParameter(3)->getInt());
			return TRUE;
		case ScriptAction::TEAM_ATTACK_SEEN_UNIT:
			doTeamAttackSeenUnit(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::TEAM_ATTACK_SEEN_TYPE:
			doTeamAttackSeenType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt());
			return TRUE;
		case ScriptAction::TEAM_ATTACK_SEEN_AREA:
			doTeamAttackSeenArea(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt());
			return TRUE;
		case ScriptAction::TEAM_ATTACK_SEEN_TYPE_AREA:
			doTeamAttackSeenTypeArea(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString(), pAction->getParameter(3)->getInt());
			return TRUE;
		case ScriptAction::TEAM_ATTACKMOVE_WAYPOINT:
			doTeamAttackMoveLocation(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::TEAM_ATTACKMOVE_TYPE:
			doTeamAttackMoveType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt());
			return TRUE;
		case ScriptAction::TEAM_ATTACKMOVE_TYPE_AREA:
			doTeamAttackMoveTypeArea(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString(), pAction->getParameter(3)->getInt());
			return TRUE;
		case ScriptAction::TEAM_EVACUATE_DESTROYED_PERCENT:
			doTeamEvacuateDestroyedPercent(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getReal());
			return TRUE;
		case ScriptAction::TEAM_ATTACKMOVE_AREA:
			doTeamAttackMoveArea(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt());
			return TRUE;
		case ScriptAction::TEAM_ATTACKMOVE_SEEN_UNIT:
			doTeamAttackMoveSeenUnit(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::TEAM_ATTACKMOVE_SEEN_TYPE:
			doTeamAttackMoveSeenType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt());
			return TRUE;
		case ScriptAction::TEAM_ATTACKMOVE_SEEN_AREA:
			doTeamAttackMoveSeenArea(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt());
			return TRUE;
		case ScriptAction::TEAM_ATTACKMOVE_SEEN_TYPE_AREA:
			doTeamAttackMoveSeenTypeArea(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString(), pAction->getParameter(3)->getInt());
			return TRUE;
		case ScriptAction::TEAM_ATTACKMOVE_PATH:
			doTeamAttackMovePath(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::TEAM_MOVE_CAPTURED:
			doTeamMoveCaptured(pAction->getParameter(0)->getString());
			return TRUE;
		case ScriptAction::TEAM_MOVE_CAPTURED_TYPE:
			doTeamMoveCapturedType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::TEAM_MOVE_TYPE_UNDER_FOG:
			doTeamMoveNearestUnderFog(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt());
			return TRUE;
		case ScriptAction::TEAM_MOVE_TYPE_AREA_UNDER_FOG:
			doTeamMoveNearestUnderFogInArea(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString(), pAction->getParameter(3)->getInt());
			return TRUE;
		case ScriptAction::TEAM_MOVE_TYPE_NOT_UNDER_FOG:
			doTeamMoveNearestNotUnderFog(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt());
			return TRUE;
		case ScriptAction::TEAM_MOVE_TYPE_AREA_NOT_UNDER_FOG:
			doTeamMoveNearestNotUnderFogInArea(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString(), pAction->getParameter(3)->getInt());
			return TRUE;
		case ScriptAction::UNIT_TELEPORT_TO_LOCATION:
			doUnitTeleportLocation(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::TEAM_TELEPORT_TO_LOCATION:
			doTeamTeleportLocation(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::PLAYER_APPLY_ATTACK_PRIORITY_SET:
			doPlayerApplyPrioritySet(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::PLAYER_APPLY_ATTACK_PRIORITY_SET_TYPE:
			doPlayerApplyPrioritySetType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::TEAM_APPLY_ATTACK_PRIORITY_SET_TYPE:
			doTeamApplyPrioritySetType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::TEAM_KEEP_DISTANCE_RELATION:
			doTeamKeepDistanceFromRelation(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getReal(), pAction->getParameter(2)->getInt(), pAction->getParameter(3)->getInt());
			return TRUE;
		case ScriptAction::UNIT_KEEP_DISTANCE_RELATION:
			doUnitKeepDistanceFromRelation(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getReal(), pAction->getParameter(2)->getInt(), pAction->getParameter(3)->getInt());
			return TRUE;
		case ScriptAction::TEAM_KEEP_DISTANCE_RELATION_TYPE:
			doTeamKeepDistanceFromRelationType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getReal(), pAction->getParameter(2)->getInt(), pAction->getParameter(3)->getString(), pAction->getParameter(4)->getInt());
			return TRUE;
		case ScriptAction::UNIT_KEEP_DISTANCE_RELATION_TYPE:
			doUnitKeepDistanceFromRelationType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getReal(), pAction->getParameter(2)->getInt(), pAction->getParameter(3)->getString(), pAction->getParameter(4)->getInt());
			return TRUE;
		case ScriptAction::SET_WILLINGNESS_TO_CRUSH_PLAYER:
			doPlayerSetWillingnessToCrush(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::SET_WILLINGNESS_TO_CRUSH_TEAM:
			doTeamSetWillingnessToCrush(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::SET_WILLINGNESS_TO_CRUSH_UNIT:
			doUnitSetWillingnessToCrush(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::PLAYER_HUNT_AREA:
			doPlayerHuntArea(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::TEAM_CUSTOM_COLOR:
			doTeamCustomColor(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::PLAYER_CUSTOM_COLOR:
			doPlayerCustomColor(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::CREATE_WAYPOINT_LOCATION:
		{
			Coord3D pos;
			pAction->getParameter(1)->getCoord3D(&pos);
			doCreateWaypoint(
				pAction->getParameter(0)->getString(),
				pos,
				pAction->getParameter(2)->getString(),
				pAction->getParameter(3)->getString(),
				pAction->getParameter(4)->getString(),
				pAction->getParameter(5)->getInt(),
				pAction->getParameter(6)->getInt()
			); return TRUE;
		}
		case ScriptAction::CREATE_WAYPOINT_UNIT:
			doCreateWaypointAtUnit(
				pAction->getParameter(0)->getString(),
				pAction->getParameter(1)->getString(),
				pAction->getParameter(2)->getString(),
				pAction->getParameter(3)->getString(),
				pAction->getParameter(4)->getString(),
				pAction->getParameter(5)->getInt()
				); return TRUE;
		case ScriptAction::CREATE_WAYPOINT_TEAM:
			doCreateWaypointAtTeam(
				pAction->getParameter(0)->getString(),
				pAction->getParameter(1)->getString(),
				pAction->getParameter(2)->getString(),
				pAction->getParameter(3)->getString(),
				pAction->getParameter(4)->getString(),
				pAction->getParameter(5)->getInt()
			); return TRUE;
		case ScriptAction::CREATE_WAYPOINT_TYPE:
			doCreateWaypointAtType(
				pAction->getParameter(0)->getString(),
				pAction->getParameter(1)->getString(),
				pAction->getParameter(2)->getString(),
				pAction->getParameter(3)->getString(),
				pAction->getParameter(4)->getString(),
				pAction->getParameter(5)->getInt(),
				pAction->getParameter(6)->getInt()
			); return TRUE;
		case ScriptAction::REMOVE_WAYPOINT:
			doRemoveWaypoint(pAction->getParameter(0)->getString());
			return TRUE;
		case ScriptAction::SET_WAYPOINT_LINK:
			doConnectWaypoint(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::REMOVE_WAYPOINT_LINK:
			doDisconnectWaypoint(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::SET_WAYPOINT_LABELS:
			doSetWaypointPathLabels(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString(), pAction->getParameter(3)->getString());
			return TRUE;
		case ScriptAction::CLEAR_WAYPOINT_LABELS:
			doClearWaypointPathLabels(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt(), pAction->getParameter(2)->getInt(), pAction->getParameter(3)->getInt());
			return TRUE;
		case ScriptAction::RELOCATE_WAYPOINT_LOCATION:
		{
			Coord3D pos;
			pAction->getParameter(1)->getCoord3D(&pos);
			doRelocateWaypoint(pAction->getParameter(0)->getString(), pos, pAction->getParameter(2)->getInt());
			return TRUE;
		}
		case ScriptAction::RELOCATE_WAYPOINT_UNIT:
			doRelocateWaypointUnit(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::RELOCATE_WAYPOINT_TEAM:
			doRelocateWaypointTeam(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::RELOCATE_WAYPOINT_TYPE:
			doRelocateWaypointType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt());
			return TRUE;
		case ScriptAction::SET_WAYPOINT_BIDIRECTIONAL:
			doSetWaypointBiDirectional(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::TEAM_HUNT_WITH_COMMAND_BUTTON_TYPE:
			doTeamHuntWithCommandButtonType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::SKIRMISH_SET_PARTICLE_CANNON_TELEPORT_MODE:
			doSkirmishUseParticleCannonTeleportMode(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::TEAM_GARRISON_TYPE:
			doTeamGarrisonType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::PLAYER_GARRISON_BUILDINGS_TYPE_WITH_MAX_NUMBER:
			doPlayerGarrisonMaxEachType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt());
			return TRUE;
		case ScriptAction::TEAM_GARRISON_BUILDINGS_TYPE_WITH_MAX_NUMBER:
			doTeamGarrisonMaxEachType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt());
			return TRUE;
		case ScriptAction::PLAYER_GARRISON_NUMBER_BUILDINGS_TYPE:
			doPlayerGarrisonEquallyType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::TEAM_GARRISON_NUMBER_BUILDINGS_TYPE:
			doTeamGarrisonEquallyType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt(), pAction->getParameter(2)->getString());
			return TRUE;
		case ScriptAction::TEAM_LOAD_TYPE_EVENLY:
			doTeamLoadAllTransportsEvenlyType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
		case ScriptAction::TEAM_REPAIR:
			doTeamSendToRepair(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getInt());
			return TRUE;
		case ScriptAction::UNIT_REPAIR:
			doUnitSendToRepair(pAction->getParameter(0)->getString());
			return TRUE;
		case ScriptAction::TEAM_REPAIR_TYPE:
			doTeamSendToRepairType(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), pAction->getParameter(2)->getInt());
			return TRUE;
		case ScriptAction::AI_PLAYER_CLOSEST_DOZER_BUILDING_PRIORITY:
			doSetAIDataClosestBuildingToDozerPriority(pAction->getParameter(0)->getInt());
			return TRUE;
		case ScriptAction::AI_CLEAR_BUILDLIST:
			doAIClearBuildList(pAction->getParameter(0)->getString());
			return TRUE;
		case ScriptAction::AI_ADD_TO_BUILDLIST:
		{
			Coord3D posC;
			pAction->getParameter(2)->getCoord3D(&posC);
			doAIAddToBuildList(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString(), &posC, pAction->getParameter(3)->getReal(), pAction->getParameter(4)->getInt(), pAction->getParameter(5)->getInt(), pAction->getParameter(6)->getInt());
			return TRUE;
		}
		case ScriptAction::AREA_TRANSFER_TO_PLAYER:
			doTransferAreaToPlayer(pAction->getParameter(0)->getString(), pAction->getParameter(1)->getString());
			return TRUE;
	}
}
