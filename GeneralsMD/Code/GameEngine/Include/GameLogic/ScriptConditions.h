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

// FILE: ScriptConditions.h ///////////////////////////////////////////////////////////////////////////
// Script conditions evaluator for the scripting engine.
// Author: John Ahlquist, Nov 2001
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

class Condition;
class ObjectTypes;
class Parameter;
class Player;
//-----------------------------------------------------------------------------
// ScriptConditionsInterface
//-----------------------------------------------------------------------------
/** Pure virtual class for Script Conditions interface format */
//-----------------------------------------------------------------------------
class ScriptConditionsInterface : public SubsystemInterface
{

public:

	virtual ~ScriptConditionsInterface() override { };

	virtual Bool evaluateCondition( Condition *pCondition ) = 0; ///< evaluate a a script condition.

	virtual Bool evaluateSkirmishCommandButtonIsReady( Parameter *pSkirmishPlayerParm, Parameter *pTeamParm, Parameter *pCommandButtonParm, Bool allReady ) = 0;
	virtual Bool evaluateTeamIsContained(Parameter *pTeamParm, Bool allContained) = 0;

};
extern ScriptConditionsInterface *TheScriptConditions;   ///< singleton definition


//-----------------------------------------------------------------------------
// ScriptConditions
//-----------------------------------------------------------------------------
/** Implementation for the Script Conditions singleton */
//-----------------------------------------------------------------------------
class ScriptConditions : public ScriptConditionsInterface
{

public:
	ScriptConditions();
	virtual ~ScriptConditions() override;

public:

	virtual void init() override;		///< Init
	virtual void reset() override;		///< Reset
	virtual void update() override;	///< Update

	virtual Bool evaluateCondition( Condition *pCondition ) override;

protected:
	Player *playerFromParam(Parameter *pSideParm);			// Gets a player from a parameter.
	void objectTypesFromParam(Parameter *pTypeParm, ObjectTypes *outObjectTypes);		// Must pass in a valid objectTypes for outObjectTypes

	Bool evaluateAllDestroyed(Parameter *pSideParm);
	Bool evaluateAllBuildFacilitiesDestroyed(Parameter *pSideParm);
	Bool evaluateIsDestroyed(Parameter *pTeamParm);
	Bool evaluateBridgeBroken(Parameter *pBridgeParm);
	Bool evaluateBridgeRepaired(Parameter *pBridgeParm);
	Bool evaluateNamedUnitDestroyed(Parameter *pUnitParm);
	Bool evaluateNamedUnitExists(Parameter *pUnitParm);
	Bool evaluateNamedUnitDying(Parameter *pUnitParm);
	Bool evaluateNamedUnitTotallyDead(Parameter *pUnitParm);
	Bool evaluateHasUnits(Parameter *pTeamParm);

	Bool evaluateTeamEnteredAreaEntirely(Parameter *pTeamParm, Parameter *pTriggerParm, Parameter *pTypeParm);
	Bool evaluateTeamEnteredAreaPartially(Parameter *pTeamParm, Parameter *pTriggerParm, Parameter *pTypeParm);
	Bool evaluateTeamExitedAreaEntirely(Parameter *pTeamParm, Parameter *pTriggerParm, Parameter *pTypeParm);
	Bool evaluateTeamExitedAreaPartially(Parameter *pTeamParm, Parameter *pTriggerParm, Parameter *pTypeParm);
	Bool evaluateTeamInsideAreaEntirely(Parameter *pTeamParm, Parameter *pTriggerParm, Parameter *pTypeParm);
	Bool evaluateTeamInsideAreaPartially(Parameter *pUnitParm, Parameter *pTriggerParm, Parameter *pTypeParm);
	Bool evaluateTeamOutsideAreaEntirely(Parameter *pTeamParm, Parameter *pTriggerParm, Parameter *pTypeParm);

	Bool evaluateNamedEnteredArea(Parameter *pUnitParm, Parameter *pTriggerParm);
	Bool evaluateNamedExitedArea(Parameter *pUnitParm, Parameter *pTriggerParm);
	Bool evaluateNamedInsideArea(Parameter *pUnitParm, Parameter *pTriggerParm);
	Bool evaluateNamedOutsideArea(Parameter *pUnitParm, Parameter *pTriggerParm);

	Bool evaluateTeamStateIs(Parameter *pTeamParm, Parameter *pStateParm);
	Bool evaluateTeamStateIsNot(Parameter *pTeamParm, Parameter *pStateParm);
	Bool evaluatePlayerHasCredits(Parameter *pCreditsParm, Parameter* pComparisonParm, Parameter *pPlayerParm);
	Bool evaluateNamedCreated(Parameter* pUnitParm);	///< Implemented as evaluateNamedExists(...)
	Bool evaluateTeamCreated(Parameter* pTeamParm);		///< Implemented as evaluateTeamExists(...)
	Bool evaluateNamedOwnedByPlayer(Parameter *pUnitParm, Parameter *pPlayerParm);
	Bool evaluateTeamOwnedByPlayer(Parameter *pTeamParm, Parameter *pPlayerParm);
	Bool evaluateMultiplayerAlliedVictory();
	Bool evaluateMultiplayerAlliedDefeat();
	Bool evaluateMultiplayerPlayerDefeat();
	Bool evaluateNamedAttackedByType(Parameter *pUnitParm, Parameter *pTypeParm);
	Bool evaluateTeamAttackedByType(Parameter *pTeamParm, Parameter *pTypeParm);
	Bool evaluateNamedAttackedByPlayer(Parameter *pUnitParm, Parameter *pPlayerParm);
	Bool evaluateTeamAttackedByPlayer(Parameter *pTeamParm, Parameter *pPlayerParm);
	Bool evaluateBuiltByPlayer(Condition *pCondition, Parameter* pTypeParm, Parameter* pPlayerParm);
	Bool evaluatePlayerHasNOrFewerBuildings(Parameter *pBuildingCountParm, Parameter *pPlayerParm);
	Bool evaluatePlayerHasNOrFewerFactionBuildings(Parameter *pBuildingCountParm, Parameter *pPlayerParm);
	Bool evaluatePlayerHasPower(Parameter *pPlayerParm);
	Bool evaluateNamedReachedWaypointsEnd(Parameter *pUnitParm, Parameter* pWaypointPathParm);
	Bool evaluateTeamReachedWaypointsEnd(Parameter *pTeamParm, Parameter* pWaypointPathParm);
	Bool evaluateNamedSelected(Condition *pCondition, Parameter *pUnitParm);
	Bool evaluateVideoHasCompleted(Parameter *pVideoParm);
	Bool evaluateSpeechHasCompleted(Parameter *pSpeechParm);
	Bool evaluateAudioHasCompleted(Parameter *pAudioParm);
	Bool evaluateNamedDiscovered(Parameter *pItemParm, Parameter* pPlayerParm);
	Bool evaluateTeamDiscovered(Parameter *pTeamParm, Parameter *pPlayerParm);
	Bool evaluateBuildingEntered(Parameter *pItemParm, Parameter *pUnitParm );
	Bool evaluateIsBuildingEmpty(Parameter *pItemParm);
	Bool evaluateEnemySighted(Parameter *pItemParm, Parameter *pAllianceParm, Parameter* pPlayerParm);
	Bool evaluateTypeSighted(Parameter *pItemParm, Parameter *pTypeParm, Parameter* pPlayerParm);
	Bool evaluateUnitHealth(Parameter *pItemParm, Parameter* pComparisonParm, Parameter *pHealthPercent);
	Bool evaluatePlayerUnitCondition(Condition *pCondition, Parameter *pPlayerParm, Parameter *pComparisonParm, Parameter *pCountParm, Parameter *pUnitTypeParm);
	Bool evaluatePlayerSpecialPowerFromUnitTriggered(Parameter *pPlayerParm, Parameter *pSpecialPowerParm, Parameter* pUnitParm);
	Bool evaluatePlayerSpecialPowerFromUnitMidway		(Parameter *pPlayerParm, Parameter *pSpecialPowerParm, Parameter* pUnitParm);
	Bool evaluatePlayerSpecialPowerFromUnitComplete	(Parameter *pPlayerParm, Parameter *pSpecialPowerParm, Parameter* pUnitParm);
	Bool evaluateUpgradeFromUnitComplete						(Parameter *pPlayerParm, Parameter *pUpgradeParm,			 Parameter* pUnitParm);
	Bool evaluateScienceAcquired										(Parameter *pPlayerParm, Parameter *pScienceParm);
	Bool evaluateCanPurchaseScience									(Parameter *pPlayerParm, Parameter *pScienceParm);
	Bool evaluateSciencePurchasePoints							(Parameter *pPlayerParm, Parameter *pSciencePointParm);
	Bool evaluateNamedHasFreeContainerSlots(Parameter *pUnitParm);
	Bool evaluatePlayerDestroyedNOrMoreBuildings(Parameter *pPlayerParm, Parameter *pNumParm, Parameter *pOppenentParm);
	Bool evaluateUnitHasObjectStatus(Parameter *pUnitParm, Parameter *pObjectStatus);
	Bool evaluateTeamHasObjectStatus(Parameter *pTeamParm, Parameter *pObjectStatus, Bool entireTeam);
	Bool evaluatePlayerHasComparisonPercentPower(Parameter *pPlayerParm, Parameter *pComparisonParm, Parameter *pPercent);
	Bool evaluatePlayerHasComparisonValueExcessPower(Parameter *pPlayerParm, Parameter *pComparisonParm, Parameter *pKWHParm);
	Bool evaluatePlayerHasUnitTypeInArea(Condition *pCondition, Parameter *pPlayerParm, Parameter *pComparisonParm, Parameter *pCountParm, Parameter *pTypeParm, Parameter *pTriggerParm);
	Bool evaluatePlayerHasUnitKindInArea(Condition *pCondition, Parameter *pPlayerParm, Parameter *pComparisonParm, Parameter *pCountParm,Parameter *pKindParm, Parameter *pTriggerParm);
	Bool evaluateUnitHasEmptied(Parameter *pUnitParm);
	virtual Bool evaluateTeamIsContained(Parameter *pTeamParm, Bool allContained) override;
	Bool evaluateMusicHasCompleted(Parameter *pMusicParm, Parameter *pIntParm);
	Bool evaluatePlayerLostObjectType(Parameter *pPlayerParm, Parameter *pTypeParm);

	// Skirmish Scripts. Please note that ALL Skirmish conditions should first pass a pSkirmishPlayerParm to
	// prevent the necessity of having to write additional scripts for other players / skirmish types later.
	Bool evaluateSkirmishSpecialPowerIsReady(Parameter *pSkirmishPlayerParm, Parameter *pPower);
	Bool evaluateSkirmishValueInArea(Condition *pCondition, Parameter *pSkirmishPlayerParm, Parameter *pComparisonParm, Parameter *pMoneyParm, Parameter *pTriggerParm);
	Bool evaluateSkirmishPlayerIsFaction(Parameter *pSkirmishPlayerParm, Parameter *pFactionParm);
	Bool evaluateSkirmishSuppliesWithinDistancePerimeter(Parameter *pSkirmishPlayerParm, Parameter *pDistanceParm, Parameter *pLocationParm, Parameter *pValueParm);
	Bool evaluateSkirmishPlayerTechBuildingWithinDistancePerimeter(Condition *pCondition, Parameter *pSkirmishPlayerParm, Parameter *pDistanceParm, Parameter *pLocationParm);
	virtual Bool evaluateSkirmishCommandButtonIsReady( Parameter *pSkirmishPlayerParm, Parameter *pTeamParm, Parameter *pCommandButtonParm, Bool allReady ) override;
	Bool evaluateSkirmishUnownedFactionUnitComparison( Parameter *pSkirmishPlayerParm, Parameter *pComparisonParm, Parameter *pCountParm );
	Bool evaluateSkirmishPlayerHasPrereqsToBuild( Parameter *pSkirmishPlayerParm, Parameter *pObjectTypeParm );
	Bool evaluateSkirmishPlayerHasComparisonGarrisoned(Parameter *pSkirmishPlayerParm, Parameter *pComparisonParm, Parameter *pCountParm );
	Bool evaluateSkirmishPlayerHasComparisonCapturedUnits(Parameter *pSkirmishPlayerParm, Parameter *pComparisonParm, Parameter *pCountParm );
	Bool evaluateSkirmishNamedAreaExists(Parameter *pSkirmishPlayerParm, Parameter *pTriggerParm);
	Bool evaluateSkirmishPlayerHasUnitsInArea(Condition *pCondition, Parameter *pSkirmishPlayerParm, Parameter *pTriggerParm );
	Bool evaluateSkirmishPlayerHasBeenAttackedByPlayer(Parameter *pSkirmishPlayerParm, Parameter *pAttackedByParm );
	Bool evaluateSkirmishPlayerIsOutsideArea(Condition *pCondition, Parameter *pSkirmishPlayerParm, Parameter *pTriggerParm );
	Bool evaluateSkirmishPlayerHasDiscoveredPlayer(Parameter *pSkirmishPlayerParm, Parameter *pDiscoveredByParm );
	Bool evaluateSkirmishSupplySourceSafe(Condition *pCondition, Parameter *pSkirmishPlayerParm, Parameter *pMinAmountOfSupplies );
	Bool evaluateSkirmishSupplySourceAttacked(Parameter *pSkirmishPlayerParm );
	Bool evaluateSkirmishStartPosition(Parameter *pSkirmishPlayerParm, Parameter *startNdx );
	Bool evaluateSkirmishShipsEnabled(void);


	// Stubs
	Bool evaluateMissionAttempts(Parameter *pPlayerParm, Parameter *pComparisonParm, Parameter *pAttemptsParm);

	// TEOD script conditions ported from CookieLandProjects/CLP_AI
	Bool evaluateTEODCondition( Condition *pCondition, Bool *result );
	Bool evaluatePlayerRelation(const AsciiString& playerSrcName, Int relationType, const AsciiString& playerDstName);
	Bool evaluateEmptySpot(Parameter* pStartNdx, Bool isEmpty);
	Bool evaluateActivePlayerCount(Parameter* pComparisonParm, Int pPlayerCount);
	Bool evaluateEmptySpotCount(Parameter* pComparisonParm, Int pEmptySpotCount);
	Bool evaluateNeighbouringSpot(Parameter* pPlayerParm, Parameter* pStartNdx);
	Bool evaluateNeighbouringSpotsEmpty(Parameter* pPlayerParm, Parameter* pComparisonParm, Int pCount);
	Bool evaluateNeighbouringSpotsRelation(Parameter* pPlayerParm, Parameter* pComparisonParm, Int pCount, Int relationType);
	Bool evaluateSpotNeighbouringRelation(Parameter* pPlayerParm, Int relationType, Int pStartNdx);
	Bool evaluateMapSize(Parameter* pComparisonParm, Int pMapSize);
	Bool evaluateStartingCash(Parameter* pComparisonParm, Int pAmount);
	Bool evaluateClosestRelationUnitToMySpawn(Int relationType, Parameter* pPlayerParm, Parameter* pComparisonParm, Real pDist);
	Bool evaluateHunted(Parameter* pPlayerParm);
	Bool evaluatePlayerLostTypeInArea(Condition* pCondition, Parameter* pPlayerParm, Parameter* pObjectType, Parameter* pArea);
	Bool evaluatePlayerLostUnit(Condition* pCondition, Parameter* pPlayerParm);
	Bool evaluateTeamSightedRelationType(Parameter* pTeam, Int relationType, Parameter* pObjectType);
	Bool evaluateSkirmishAnyRelationFaction(Parameter* pPlayerParm, Int relationType, Parameter* pFactionParm);
	Bool evaluatePlayerDestroyedEnemyType(Condition* pCondition, Parameter* pPlayerParm, Parameter* pObjectType);
	Bool evaluatePlayerDestroyedEnemyUnit(Condition* pCondition, Parameter* pPlayerParm);
	Bool evaluateMapControl(Parameter* pPlayerParm, Parameter* pComparisonParm, Real pValue);
	Bool evaluateRelationMapControl(Parameter* pPlayerParm, Int relationType, Parameter* pComparisonParm, Real pValue);
	Bool evaluateMapControlArea(Parameter* pPlayerParm, Parameter* pComparisonParm, Real pValue, Parameter* pTriggerParm);
	Bool evaluateRelationMapControlArea(Parameter* pPlayerParm, Int relationType, Parameter* pComparisonParm, Real pValue, Parameter* pTriggerParm);
	Bool evaluateTeamLostType(Condition* pCondition, Parameter* pTeamParm, Parameter* objectType);
	Bool evaluateTeamLostUnit(Condition* pCondition, Parameter* pTeamParm);
	Bool evaluatePlayerSightedRelationType(Parameter* pPlayerParm, Int relationType, Parameter* pObjectType);
	Bool evaluateRelationPlayerSightedRelationType(Parameter* pPlayerParm, Int playerRelationType, Int relationType, Parameter* pObjectType);
	Bool evaluateRelationPlayerSightedRelationArea(Parameter* pPlayerParm, Int playerRelationType, Int relationType, Parameter* pTriggerParm);
	Bool evaluateRelationPlayerSightedRelationTypeArea(Parameter* pPlayerParm, Int playerRelationType, Int relationType, Parameter* pObjectType, Parameter* pTriggerParm);
	Bool evaluateRelationPlayerValueArea(Condition* pCondition, Parameter* pPlayerParm, Int relationType, Parameter* pComparisonParm, Int value, Parameter* pTriggerParm);
	Bool evaluateRelationPlayerOwnsComparisonType(Condition* pCondition, Parameter* pPlayerParm, Int relationType, Parameter* pComparisonParm, Int value, Parameter* objectType);
	Bool evaluatePlayerAttackedInArea(Parameter* pPlayerParm, Parameter* pTriggerParm);
	Bool evaluatePlayerBuildingBeingCaptured(Parameter* pPlayerParm);
	Bool evaluatePlayerBuildingBeingCapturedType(Parameter* pPlayerParm, Parameter* objectType);
	Bool evaluatePlayerBuildingBeingCapturedArea(Parameter* pPlayerParm, Parameter* pTriggerParm);
	Bool evaluatePlayerBuildingBeingCapturedTypeArea(Parameter* pPlayerParm, Parameter* objectType, Parameter* pTriggerParm);
	Bool evaluateRelationPlayerComparisonTypeArea(Condition* pCondition, Parameter* pPlayerParm, Int relationType, Parameter* pComparisonParm, Int value, Parameter* objectType, Parameter* pTriggerParm);
	Bool evaluateTeamIdle(Parameter* pTeamParm);
	Bool evaluateTeamIdleFrames(Parameter* pTeam, Int value);
	Bool evaluateTeamSeen(Parameter* pTeamParm);
	Bool evaluatePlayerHasComparisonRatioOther(Condition* pCondition, Parameter* pPlayerParm, Parameter* pComparisonParm, Real ratio, Parameter* pOther);
	Bool evaluatePlayerHasComparisonRatioTypeOther(Condition* pCondition, Parameter* pPlayerParm, Parameter* pComparisonParm, Real ratio, Parameter* objectType, Parameter* pOther, Parameter* otherObjectType);
	Bool evaluatePlayerHasComparisonRatioAreaOther(Condition* pCondition, Parameter* pPlayerParm, Parameter* pComparisonParm, Real ratio, Parameter* pTriggerParm, Parameter* pOther);
	Bool evaluatePlayerHasComparisonRatioTypeAreaOther(Condition* pCondition, Parameter* pPlayerParm, Parameter* pComparisonParm, Real ratio, Parameter* objectType, Parameter* pTriggerParm, Parameter* pOther, Parameter* otherObjectType);
	Bool evaluateRelationPlayerHasComparisonRatioOtherRelation(Condition* pCondition, Parameter* pPlayerParm, Int pRelation, Parameter* pComparisonParm, Real ratio, Int pOtherRelation);
	Bool evaluateRelationPlayerHasComparisonRatioTypeOtherRelation(Condition* pCondition, Parameter* pPlayerParm, Int pRelation, Parameter* pComparisonParm, Real ratio, Parameter* objectType, Int pOtherRelation, Parameter* otherObjectType);
	Bool evaluateRelationPlayerHasComparisonRatioAreaOtherRelation(Condition* pCondition, Parameter* pPlayerParm, Int pRelation, Parameter* pComparisonParm, Real ratio, Parameter* pTriggerParm, Int pOtherRelation);
	Bool evaluateRelationPlayerHasComparisonRatioTypeAreaOtherRelation(Condition* pCondition, Parameter* pPlayerParm, Int pRelation, Parameter* pComparisonParm, Real ratio, Parameter* objectType, Parameter* pTriggerParm, Int pOtherRelation, Parameter* otherObjectType);
	Bool evaluateTeamContainsType(Parameter* pTeamParm, Parameter* objectType);
	Bool evaluateTeamContainsComparisonType(Parameter* pTeamParm, Parameter* pComparisonParm, Int value, Parameter* objectType);
	Bool evaluateTeamSingleBelowHealth(Parameter* pTeamParm, Real value);
	Bool evaluateTeamBelowHealth(Parameter* pTeamParm, Real value);
	Bool evaluatePlayerTeamInstances(Parameter* pPlayerParm, Parameter* pComparisonParm, Int value, Parameter* pTeamParm);
	Bool evaluateTeamHasComparisonRatioSighted(Parameter* pTeamParm, Parameter* pComparisonParm, Real ratio);
	Bool evaluateTeamHasComparisonRatioTypeSighted(Parameter* pTeamParm, Parameter* pComparisonParm, Real ratio, Parameter* objectType, Parameter* otherObjectType);
	Bool evaluateTeamClosestRelationType(Parameter* pTeamParm, Int pRelation, Parameter* objectType, Parameter* pComparisonParm, Real value);
	Bool evaluateUnitClosestRelationType(Parameter* pUnitParm, Int pRelation, Parameter* objectType, Parameter* pComparisonParm, Real value);
	Bool evaluateTeamAllClear(Parameter* pTeamParm);
	Bool evaluateNoTeams(Bool isFFA);
	Bool evaluateTeamApart(Parameter* pTeamParm, Parameter* pComparisonParm, Real value);
	Bool evaluateTeamCanLoadTransports(Parameter* pTeamParm);
	Bool evaluateClosestTeamMemberToEnemyHasSightedComparisonRatioRadius(Parameter* pTeamParm, Parameter* pComparisonParm, Real ratio, Real radius);
	Bool evaluateClosestTeamMemberToEnemyHasSightedComparisonTypeRatioRadius(Parameter* pTeamParm, Parameter* pComparisonParm, Real ratio, Parameter* objectType, Parameter* otherObjectType, Real radius);
	Bool evaluateUnitIsReloaded(Parameter* pUnitParm, Bool partially);
	Bool evaluateTeamIsReloaded(Parameter* pTeamParm, Bool partially);


};
