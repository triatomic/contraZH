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

// FILE: WorkerAIUpdate.cpp ///////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood, June 2002
// Desc:   A Worker is a unit that is both a Dozer and a Supply Truck.
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ActionManager.h"
#include "Common/Team.h"
#include "Common/StateMachine.h"
#include "Common/BuildAssistant.h"
#include "Common/GameState.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/Player.h"
#include "Common/Money.h"
#include "Common/Radar.h"
#include "Common/RandomValue.h"
#include "Common/GlobalData.h"
#include "Common/ResourceGatheringManager.h"
#include "Common/Upgrade.h"

#include "GameClient/Drawable.h"
#include "GameClient/GameText.h"
#include "GameClient/InGameUI.h"

#include "GameLogic/AIPathfind.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/BridgeBehavior.h"
#include "GameLogic/Module/BridgeTowerBehavior.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Module/SupplyTruckAIUpdate.h"
#include "GameLogic/Module/SupplyCenterDockUpdate.h"
#include "GameLogic/Module/SupplyWarehouseDockUpdate.h"
#include "GameLogic/Module/WorkerAIUpdate.h"




// FORWARD DECLARATIONS ///////////////////////////////////////////////////////////////////////////
enum
{
	AS_DOZER,				///< When not actively Gathering, or when actively building, I am a dozer
	AS_SUPPLY_TRUCK	///< When told explicitly by player or other object, I become a supply truck
};


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
WorkerAIUpdate::WorkerAIUpdate( Thing *thing, const ModuleData* moduleData ) :
							 AIUpdateInterface( thing, moduleData )

{

	//
	// initialize the dozer machine to nullptr, we want to do this and create it during the update
	// implementation because at this point we don't have the object all setup
	m_isRebuild = FALSE;
	m_dozerMachine = nullptr;
	for( Int i = 0; i < DOZER_NUM_TASKS; i++ )
	{
		m_task[ i ].m_targetObjectID = INVALID_ID;
		m_task[ i ].m_taskOrderFrame = 0;
		for( Int j = 0; j < DOZER_NUM_DOCK_POINTS; j++ )
		{
			m_dockPoint[ i ][ j ].valid = FALSE;
			m_dockPoint[ i ][ j ].location.zero();
		}
	}
	m_currentTask = DOZER_TASK_INVALID;
	m_previousTask = DOZER_TASK_INVALID;
	m_buildSubTask = DOZER_SELECT_BUILD_DOCK_LOCATION;  // irrelevant, but I want non-garbage value

	// TheSuperHackers @feature waypoint build queue
	m_queuedBuildCount = 0;
	for( Int q = 0; q < DOZER_MAX_QUEUED_BUILDS; q++ )
	{
		m_queuedBuildTemplates[ q ] = nullptr;
		m_queuedBuildPositions[ q ].zero();
		m_queuedBuildAngles[ q ] = 0.0f;
		m_queuedBuildWaypointIndex[ q ] = -1;
		m_queuedIsMove[ q ] = FALSE;
	}

	m_supplyTruckStateMachine = nullptr;
	m_numberBoxes = 0;
	m_forcePending = FALSE;
	m_forcedBusyPending = FALSE;

	m_workerMachine = nullptr;

 	m_suppliesDepletedVoice = getWorkerAIUpdateModuleData()->m_suppliesDepletedVoice;

	createMachines();

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
WorkerAIUpdate::~WorkerAIUpdate()
{

	// delete our behavior state machine
	deleteInstance(m_dozerMachine);
	deleteInstance(m_supplyTruckStateMachine);
	deleteInstance(m_workerMachine);

}

//-------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::isCurrentlyFerryingSupplies() const
{
	if (m_supplyTruckStateMachine)
	{
		switch (m_supplyTruckStateMachine->getCurrentStateID())
		{
			case ST_IDLE:
			case ST_BUSY:
			case ST_REGROUPING:
				return false;
			case ST_WANTING:
			case ST_DOCKING:
				return true;
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::isAvailableForSupplying() const
{
	return true;
}

// ------------------------------------------------------------------------------------------------
Real WorkerAIUpdate::getRepairHealthPerSecond() const
{
	return getWorkerAIUpdateModuleData()->m_repairHealthPercentPerSecond;
}
// ------------------------------------------------------------------------------------------------
Real WorkerAIUpdate::getBoredTime() const
{
	return getWorkerAIUpdateModuleData()->m_boredTime;
}
// ------------------------------------------------------------------------------------------------
Real WorkerAIUpdate::getBoredRange() const
{
	return getWorkerAIUpdateModuleData()->m_boredRange;
}

// ------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::canBuildTemplate( const ThingTemplate *what ) const
{
	return getWorkerAIUpdateModuleData()->m_restrictions.isTemplateAllowedToBuild( what );
}

// ------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::canRepairObjects() const
{
	return getWorkerAIUpdateModuleData()->m_restrictions.m_canRepair;
}

// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::createMachines()
{

	if( m_workerMachine == nullptr )
	{
		m_workerMachine = newInstance(WorkerStateMachine)( getObject() );

		if( m_dozerMachine == nullptr )
		{
			m_dozerMachine = newInstance(DozerPrimaryStateMachine)( getObject() );
			m_dozerMachine->initDefaultState();
		}

		if( m_supplyTruckStateMachine == nullptr )
		{
			m_supplyTruckStateMachine = newInstance(SupplyTruckStateMachine)( getObject() );
			m_supplyTruckStateMachine->initDefaultState();
		}

		m_workerMachine->initDefaultState();// this has to wait until all three are in place since
		// an immediate transition check will ask questions of the machines.

//#ifdef RTS_DEBUG
//		m_workerMachine->setDebugOutput(TRUE);
//		m_dozerMachine->setDebugOutput(TRUE);
//		m_supplyTruckStateMachine->setDebugOutput(TRUE);
//#endif
	}

}

//-------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
UnsignedInt WorkerAIUpdate::getActionDelayForDock( Object *dock )
{
	// Decide whether to use my Center or Warehouse delay time
	static const NameKeyType key_warehouseUpdate = NAMEKEY("SupplyWarehouseDockUpdate");
	SupplyWarehouseDockUpdate *warehouseModule = (SupplyWarehouseDockUpdate*) dock->findUpdateModule( key_warehouseUpdate );
	if (warehouseModule) {
		return getWorkerAIUpdateModuleData()->m_warehouseDelay;
	}
	static const NameKeyType key_centerUpdate = NAMEKEY("SupplyCenterDockUpdate");
	SupplyCenterDockUpdate *centerModule = (SupplyCenterDockUpdate*) dock->findUpdateModule( key_centerUpdate );
	if (centerModule) {
		return getWorkerAIUpdateModuleData()->m_centerDelay;
	}

	return 0;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Real WorkerAIUpdate::getWarehouseScanDistance() const
{
	// Ai players get larger scan range.  jba.
	if (getObject()->getControllingPlayer()->getPlayerType() == PLAYER_COMPUTER) {
		return 2 * getWorkerAIUpdateModuleData()->m_warehouseScanDistance;
	}
	return getWorkerAIUpdateModuleData()->m_warehouseScanDistance;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpdateSleepTime WorkerAIUpdate::update()
{
	// Suspend worker tasks (build/repair/supply) while disabled; only the locomotor runs.
	if (isAiSuspendedByDisable())
		return AIUpdateInterface::update();

	//
	// NOTE: Any changes to DozerAIUpdate::* you probably want to reflect and copy into
	// WorkerAIUPdate:* as well ... sigh
	//

	//
	// now that we're really executing we have all the necessary object modules in place to
	// correctly create a state machine and set the default state
	//
	// create all the machines if they don't yet exist
	createMachines();

	// DO NOT set us as being to able to move with super precision off grid locations
	// Causes workers to get stuck.  jba.
	//if( getCurLocomotor() )
		//getCurLocomotor()->setUltraAccurate( TRUE );

	// extend the normal AI system
	AIUpdateInterface::update();

	// do nothing if we're dead
	///@todo shouldn't this be at a higher level?
	if( getObject()->isEffectivelyDead() )
		return UPDATE_SLEEP_NONE;

	// run our own state machine, and the appropriate sub machine
	m_workerMachine->updateStateMachine();

	if( m_workerMachine->getCurrentStateID() == AS_DOZER )
	{

		// get and validate our current task
		DozerTask currentTask = getCurrentTask();
		if( currentTask != DOZER_TASK_INVALID )
		{
			ObjectID taskTarget = getTaskTarget( currentTask );
			Object *targetObject = TheGameLogic->findObjectByID( taskTarget );
			Bool invalidTask = FALSE;

			// validate the task and the target
			// TheSuperHackers @bugfix Stubbjax 16/11/2025 Invalidate the task when the build scaffold is destroyed.
			if( currentTask == DOZER_TASK_REPAIR &&
					TheActionManager->canRepairObject( getObject(), targetObject, getLastCommandSource() ) == FALSE )
				invalidTask = TRUE;
#if !RETAIL_COMPATIBLE_CRC
			else if (currentTask == DOZER_TASK_BUILD && targetObject == nullptr)
				invalidTask = TRUE;
#endif

			// cancel the task if it's now invalid
			if( invalidTask == TRUE )
				cancelTask( currentTask );

		}

		// TheSuperHackers @feature waypoint build queue — start the next queued construction
		// if we're in dozer mode, idle, and no longer moving
		processBuildQueue();

		// update dozer behavior
		m_dozerMachine->updateStateMachine();

	}
	else
	{
		m_supplyTruckStateMachine->updateStateMachine();
		// If we are harvesting, we can be diverted to clear mines.  jba.
		getObject()->setWeaponSetFlag(WEAPONSET_MINE_CLEARING_DETAIL);//maybe go clear some mines, if I feel like it
	}
	return UPDATE_SLEEP_NONE;
}


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
/** The entry point of a construct command to the Dozer */
//-------------------------------------------------------------------------------------------------
Object *WorkerAIUpdate::construct( const ThingTemplate *what,
																	 const Coord3D *pos,
																	 Real angle,
																	 Player *owningPlayer,
																	 Bool isRebuild )
{

	// !!! NOTE: If you modify this you must modify the dozer too !!!
	// !!! Graham: Please please please have inspiration for how to *not* duplicate this code
	// GS - Construct needs to be an AI primitive.  Inheriting off of AIUpdate means you are writing a
	// master brain that will call AI primitives on the object, not something that does stuff itself.
  // SupplyTruckAI decides who to call AIDock on.  Worker should decide to AIDock or AIConstruct
	// or AIRepair.  Dozer should just use the latter two.  No construction logic should be in
	// the inherited AIUpdates at all.

	// create our machines if they don't yet exist
	///@todo make 'construct' a real AI command and you won't need a special case

	createMachines();

	// sanity
	if( what == nullptr || pos == nullptr || owningPlayer == nullptr )
		return nullptr;

	// sanity
	DEBUG_ASSERTCRASH( getObject()->getControllingPlayer() == owningPlayer,
										 ("Dozer::Construct - The controlling player of the Dozer is not the owning player passed in") );

	// a rebuild hole and an AI player skip the checks below, but neither may ignore the build list
	if( canBuildTemplate( what ) == FALSE )
		return nullptr;

	// if we're not rebuilding, we have a few checks to pass first for sanity
	if( isRebuild == FALSE )
	{

		// AI has weaker restriction on building
		Bool dozerIsAI = owningPlayer->getPlayerType() == PLAYER_COMPUTER;
		if( dozerIsAI )
		{

			// validate the the position to build at is valid
			if( TheBuildAssistant->isLocationLegalToBuild( pos, what, angle,
																										 BuildAssistant::CLEAR_PATH |
																										 BuildAssistant::NO_OBJECT_OVERLAP,
																										 getObject(), nullptr ) != LBC_OK )
				return nullptr;

		}
		else
		{

			// make sure the player is capable of building this
			if( TheBuildAssistant->canMakeUnit( getObject(), what ) != CANMAKE_OK )
				return nullptr;

			// validate the the position to build at is valid
			if( TheBuildAssistant->isLocationLegalToBuild( pos, what, angle,
																										 BuildAssistant::TERRAIN_RESTRICTIONS |
																										 BuildAssistant::CLEAR_PATH |
																										 BuildAssistant::NO_OBJECT_OVERLAP |
																										 BuildAssistant::SHROUD_REVEALED,
																										 getObject(), nullptr ) != LBC_OK )
				return nullptr;

		}

	}

	// create the foundation for the new structure (TheSuperHackers @feature: extracted from construct)
	Object *obj = createConstruction( what, pos, angle, owningPlayer, isRebuild );
	if( obj == nullptr )
		return nullptr;

	// we have a construction pending
	newTask( DOZER_TASK_BUILD, obj );

	return obj;

}

// ------------------------------------------------------------------------------------------------
/** TheSuperHackers @feature Create the under-construction object at the given location and
	* withdraw the money for it.  This does NOT assign any task to the worker — callers must
	* either newTask( DOZER_TASK_BUILD, obj ) or queue the object via queueBuild().
	* NOTE: If you modify this you must modify the dozer too !!! */
// ------------------------------------------------------------------------------------------------
Object *WorkerAIUpdate::createConstruction( const ThingTemplate *what,
																						const Coord3D *pos,
																						Real angle,
																						Player *owningPlayer,
																						Bool isRebuild )
{
	m_isRebuild = isRebuild;

	// what will our initial status bits
	ObjectStatusMaskType statusBits = MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_UNDER_CONSTRUCTION );
	if( isRebuild )
		statusBits.set( OBJECT_STATUS_RECONSTRUCTING );

	// create an object at the destination location
	Object *obj = TheThingFactory->newObject( what, owningPlayer->getDefaultTeam(), statusBits );

	// even though we haven't actually built anything yet, this keeps things tidy
	obj->setProducer( getObject() );
	obj->setBuilder( getObject() );

	// leave the supply truck state and now behave like a dozer.
	exitingSupplyTruckState();

	// take the required money away from the player
	if( isRebuild == FALSE )
	{
		Money *money = owningPlayer->getMoney();

		money->withdraw( what->calcCostToBuild( owningPlayer ) );

	}

	//
	// set a bit that this object is under construction, it is important to do this early
	// before the hooks add/subtract power from a player are executed
	//
	obj->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_UNDER_CONSTRUCTION ) );

	// initialize object
	obj->setPosition( pos );
	obj->setOrientation( angle );

	// Do not flatten shipyards
	if (!obj->isKindOf(KINDOF_SHIPYARD)) {

		// Flatten the terrain underneath the object, then adjust to the flattened height. jba.
		TheTerrainLogic->flattenTerrain(obj);
		Coord3D adjustedPos = *pos;
		adjustedPos.z = TheTerrainLogic->getGroundHeight(pos->x, pos->y);
		obj->setPosition(&adjustedPos);
	}

	// Note - very important that we add to map AFTER we flatten terrain. jba.
	TheAI->pathfinder()->addObjectToPathfindMap( obj );

	// "callback" event for structure created (note that it's not yet "complete")
	owningPlayer->onStructureCreated( getObject(), obj );

	// set a construction percent for the new object to zero and a status for under construction
	obj->setConstructionPercent( 0.0 );

	// newly constructed objects start at one hit point
	BodyModuleInterface *body = obj->getBodyModule();
	body->internalChangeHealth( -body->getHealth() + 1.0f );

	// set the model action state to awaiting construction
	obj->clearAndSetModelConditionFlags(
		MAKE_MODELCONDITION_MASK2(MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED),
		MAKE_MODELCONDITION_MASK(MODELCONDITION_AWAITING_CONSTRUCTION)
	);

	return obj;

}

// ------------------------------------------------------------------------------------------------
/** TheSuperHackers @feature Queue a construction: create the foundation now (money is spent
	* now) but instead of tasking the worker immediately, put it in the build queue.  The worker
	* will build it once it has arrived at the waypoint it was heading for when the order was
	* queued (see processBuildQueue).  This allows
	* waypoint-style orders.  NOTE: If you modify this you must modify the dozer too !!! */
// ------------------------------------------------------------------------------------------------
Object *WorkerAIUpdate::queueConstruct( const ThingTemplate *what,
																				const Coord3D *pos,
																				Real angle,
																				Player *owningPlayer )
{

	// create our machines if they don't yet exist
	createMachines();

	// sanity
	if( what == nullptr || pos == nullptr || owningPlayer == nullptr )
		return nullptr;

	// sanity
	DEBUG_ASSERTCRASH( getObject()->getControllingPlayer() == owningPlayer,
										 ("Worker::QueueConstruct - The controlling player of the Worker is not the owning player passed in") );

	// make sure the player is capable of building this
	if( TheBuildAssistant->canMakeUnit( getObject(), what ) != CANMAKE_OK )
		return nullptr;

	// validate the the position to build at is valid
	const LegalBuildCode queueLbc = TheBuildAssistant->isLocationLegalToBuild( pos, what, angle,
																								 BuildAssistant::TERRAIN_RESTRICTIONS |
																								 BuildAssistant::CLEAR_PATH |
																								 BuildAssistant::NO_OBJECT_OVERLAP |
																								 BuildAssistant::SHROUD_REVEALED,
																								 getObject(), nullptr );
	if( queueLbc != LBC_OK )
	{
		// TheSuperHackers @bugfix Say why the order was refused instead of silently dropping it.
		// Nothing was built and no money was taken yet, so there is nothing to undo.
		if( owningPlayer->isLocalPlayer() && TheInGameUI )
			TheInGameUI->displayCantBuildMessage( queueLbc );
		return nullptr;
	}

	// Do NOT append the build site to our goal path. Walking onto the middle of the future
	// building leaves the worker standing right on the foundation, which makes
	// findGoodBuildOrRepairPositionAndTarget() fail inside newTask() — the foundation gets
	// created but no build task is ever recorded, so the worker just wanders off and never
	// builds. (privateFollowPathAppend()'s fallback also clears the whole state machine.)
	// The site is already visible through the ghost preview, so the route line does not need
	// to reach it.
	//
	// What we record instead is where our *current* route ends: the order may only fire once
	// the player's own movement waypoints are done, so bind it to the last node of the path we
	// are already walking. With no path at all we bind to nothing, which means build right away.
	// friend_getWaypointGoalPathSize() already returns 0 unless we are in AI_FOLLOW_PATH.
	Int goalPathSize = friend_getWaypointGoalPathSize();
	Int waypointIndex = ( goalPathSize > 0 ) ? ( goalPathSize - 1 ) : -1;

	queueBuild( what, pos, angle, waypointIndex );

	// No object yet — only a ghost order. The foundation is created when the order fires.
	return nullptr;

}

// ------------------------------------------------------------------------------------------------
/** TheSuperHackers @feature Add a foundation to the build queue.  If there is no waypoint left
	* to walk to before this order, the task starts immediately; otherwise it waits in the FIFO
	* until we arrive at that waypoint. */
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::queueBuild( const ThingTemplate *what, const Coord3D *pos, Real angle, Int waypointIndex )
{

	if( what == nullptr || pos == nullptr )
		return;

	// nothing left to walk to before this order (or there never was a waypoint) — turn it
	// into a real foundation right away instead of parking it in the queue
	if( isTaskPending( DOZER_TASK_BUILD ) == FALSE &&
			hasReachedQueuedWaypoint( waypointIndex ) )
	{
		materializeQueuedBuild( what, pos, angle );
		return;
	}

	// room left in the queue?
	if( m_queuedBuildCount >= DOZER_MAX_QUEUED_BUILDS )
	{
		// queue is full. Nothing was created and no money was taken for this order yet,
		// so there is nothing to refund and nothing to destroy — just drop the order.
		return;
	}

	m_queuedBuildTemplates[ m_queuedBuildCount ] = what;
	m_queuedBuildPositions[ m_queuedBuildCount ] = *pos;
	m_queuedBuildAngles[ m_queuedBuildCount ] = angle;
	m_queuedBuildWaypointIndex[ m_queuedBuildCount ] = waypointIndex;
	m_queuedIsMove[ m_queuedBuildCount ] = FALSE;
	m_queuedBuildCount++;

}

// ------------------------------------------------------------------------------------------------
/** TheSuperHackers @feature Turn a ghost order into a real foundation. This is the *second*
	* validation pass: the order was already checked when the player issued it, but by the time
	* we actually walk to the waypoint the world may have changed — someone else may have
	* claimed the ground, we may have run out of money, or the site may be shrouded again.
	* Only if it still checks out do we create the foundation and take the money. */
// ------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::materializeQueuedBuild( const ThingTemplate *what, const Coord3D *pos, Real angle )
{

	if( what == nullptr || pos == nullptr )
		return FALSE;

	Object *builder = getObject();
	if( builder == nullptr )
		return FALSE;

	Player *owningPlayer = builder->getControllingPlayer();
	if( owningPlayer == nullptr )
		return FALSE;

	if( TheBuildAssistant->canMakeUnit( builder, what ) != CANMAKE_OK )
		return FALSE;

	LegalBuildCode lbc = TheBuildAssistant->isLocationLegalToBuild( pos, what, angle,
																					BuildAssistant::TERRAIN_RESTRICTIONS |
																					BuildAssistant::CLEAR_PATH |
																					BuildAssistant::NO_OBJECT_OVERLAP |
																					BuildAssistant::SHROUD_REVEALED,
																					builder, nullptr );
	if( lbc != LBC_OK )
	{
		// Let the local player know why this queued order could not be built after all.
		// Purely a UI message, it changes no simulation state.
		if( owningPlayer->isLocalPlayer() )
			TheInGameUI->displayCantBuildMessage( lbc );
		return FALSE;
	}

	// now — and only now — raise the foundation and take the money
	Object *obj = createConstruction( what, pos, angle, owningPlayer, FALSE );
	if( obj == nullptr )
		return FALSE;

	// Hand the construction over to the AI state machine: newTask() records the build task and
	// its dock points, and the worker machine then drives the worker to the site and builds it.
	//
	// Do NOT call aiIdle() here. processBuildQueue() runs from within update(), and resetting
	// the AI state machine on that stack destroys the context the caller is still using — that
	// crashed the game the very moment a queued order finally triggered.
	newTask( DOZER_TASK_BUILD, obj );
	return TRUE;

}

// ------------------------------------------------------------------------------------------------
/** TheSuperHackers @feature Read-only accessors for the queued ghost orders. The client uses
	* these to draw a translucent preview of each pending build at its recorded site and
	* angle — see W3dWaypointBuffer::drawWaypoints. */
// ------------------------------------------------------------------------------------------------
const ThingTemplate *WorkerAIUpdate::getQueuedBuildTemplate( Int i ) const
{
	if( i < 0 || i >= m_queuedBuildCount )
		return nullptr;
	return m_queuedBuildTemplates[ i ];
}

const Coord3D *WorkerAIUpdate::getQueuedBuildPosition( Int i ) const
{
	if( i < 0 || i >= m_queuedBuildCount )
		return nullptr;
	return &m_queuedBuildPositions[ i ];
}

Real WorkerAIUpdate::getQueuedBuildAngle( Int i ) const
{
	if( i < 0 || i >= m_queuedBuildCount )
		return 0.0f;
	return m_queuedBuildAngles[ i ];
}

// ------------------------------------------------------------------------------------------------
/** TheSuperHackers @feature True once this unit has actually arrived at (or passed) the given
	* goal path waypoint.
	*
	* IMPORTANT: Do not fall back on "isMoving() == FALSE" here. A builder that is momentarily
	* blocked, or sitting between two pathfinding requests, also stops moving for a frame or
	* two, and would start building halfway along the route it was ordered to walk first.
	*
	* Semantics of the running path, per AIFollowPathState::update(): while travelling to node
	* i, friend_getCurrentGoalPathIndex() == i; on arrival it becomes i+1, and once it runs off
	* the end the state finishes and the index is reset to -1.
	*/
// ------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::hasReachedQueuedWaypoint( Int waypointIndex ) const
{

	// No waypoint was pending when this order was queued, so there is nothing to wait for.
	if( waypointIndex < 0 )
		return TRUE;

	if( getAIStateType() == AI_FOLLOW_PATH )
	{
		Int currentIndex = friend_getCurrentGoalPathIndex();

		// The order must fire as soon as the *previous* waypoint is finished, i.e. the moment
		// this build's waypoint becomes the one we are heading for — not once we have physically
		// walked all the way onto the build site. Reaching that site is what the resulting
		// construction task does afterwards.
		// While travelling to an earlier node (currentIndex < waypointIndex) we are still on our
		// way, so keep waiting.
		if( currentIndex >= 0 && currentIndex < waypointIndex )
		{
			// TheSuperHackers @bugfix A path that is now shorter than the bound waypoint was
			// replaced underneath us (e.g. a fresh move order). Keep waiting instead of firing
			// immediately: the order fires in queue order once this route is walked to its end
			// (the FOLLOW_PATH state then ends), or once a later route grows past the index.
			return FALSE;
		}
	}

	return TRUE;

}

// ------------------------------------------------------------------------------------------------
/** TheSuperHackers @feature Promote the next queued construction into a real build task once
	* the worker has arrived at the waypoint that construction was waiting on.  Skips foundations
	* that died or were completed by someone else in the meantime. */
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::processBuildQueue()
{

	if( m_queuedBuildCount <= 0 )
		return;

	// TheSuperHackers @feature A disabled builder (EMP-paralysed, driver sniped, subverted...)
	// has lost control of itself, so its queued orders are voided outright — losing control
	// cancels the pending intentions. They are only ghost orders (nothing was created,
	// no money taken), so there is nothing to refund.
	if( getObject()->isDisabled() )
	{
		m_queuedBuildCount = 0;
		for( Int q = 0; q < DOZER_MAX_QUEUED_BUILDS; q++ )
		{
			m_queuedBuildTemplates[ q ] = nullptr;
			m_queuedBuildPositions[ q ].zero();
			m_queuedBuildAngles[ q ] = 0.0f;
			m_queuedBuildWaypointIndex[ q ] = -1;
			m_queuedIsMove[ q ] = FALSE;
		}
		return;
	}

	// TheSuperHackers @bugfix One pass over the unified order list: construction orders and
	// movement orders fire strictly in the sequence the player issued them.
	while( m_queuedBuildCount > 0 )
	{
		// --- eligibility of the head entry, per its kind ---
		if( m_queuedIsMove[ 0 ] )
		{
			// movement waits for every running task: a construction (or any other work)
			// in progress is never abandoned for the sake of a queued destination
			if( isAnyTaskPending() )
				return;
		}
		else
		{
			// never interrupt a running build task
			if( isTaskPending( DOZER_TASK_BUILD ) == TRUE )
				return;

			// the head of the queue only becomes eligible once we walked up to its waypoint
			if( hasReachedQueuedWaypoint( m_queuedBuildWaypointIndex[ 0 ] ) == FALSE )
				return;

			// still walking a route (movement orders ahead of us that are being walked, or
			// the player's own earlier route): a ghost order may only become a foundation
			// once we are ready to actually build it - at the end of that walk
			if( getAIStateType() == AI_FOLLOW_PATH && friend_getWaypointGoalPathSize() > 0 )
				return;
		}

		// pop the front entry
		const Bool isMove = m_queuedIsMove[ 0 ];
		const ThingTemplate *what = m_queuedBuildTemplates[ 0 ];
		Coord3D pos = m_queuedBuildPositions[ 0 ];
		Real angle = m_queuedBuildAngles[ 0 ];

		for( Int i = 1; i < m_queuedBuildCount; i++ )
		{
			m_queuedBuildTemplates[ i - 1 ] = m_queuedBuildTemplates[ i ];
			m_queuedBuildPositions[ i - 1 ] = m_queuedBuildPositions[ i ];
			m_queuedBuildAngles[ i - 1 ] = m_queuedBuildAngles[ i ];
			m_queuedBuildWaypointIndex[ i - 1 ] = m_queuedBuildWaypointIndex[ i ];
			m_queuedIsMove[ i - 1 ] = m_queuedIsMove[ i ];
		}
		m_queuedBuildCount--;
		m_queuedBuildTemplates[ m_queuedBuildCount ] = nullptr;
		m_queuedBuildPositions[ m_queuedBuildCount ].zero();
		m_queuedBuildAngles[ m_queuedBuildCount ] = 0.0f;
		m_queuedBuildWaypointIndex[ m_queuedBuildCount ] = -1;
		m_queuedIsMove[ m_queuedBuildCount ] = FALSE;

		if( isMove )
		{
			if( getAIStateType() == AI_FOLLOW_PATH && friend_getWaypointGoalPathSize() > 0 )
			{
				// already walking a route (this or an earlier queued movement order):
				// chain onto the end of it instead of replacing the route we are on
				friend_addToWaypointGoalPath( &pos );
			}
			else
			{
				// idle: this starts the walk. privateFollowPathAppend()'s fallback used to
				// be reachable with a build task running (it clears the whole state machine
				// and cancelled the construction) - the isAnyTaskPending() gate above keeps
				// that path free of standing work now.
				privateFollowPathAppend( &pos, CMD_FROM_PLAYER );
			}
			continue;	// consecutive movement orders chain together in this same pass
		}

		// materializeQueuedBuild() re-validates the site; if it is no longer buildable it
		// reports why and returns FALSE, and we simply drop that order and try the next.
		if( materializeQueuedBuild( what, &pos, angle ) )
			return;	// a task was created: the next entry waits for it
	}

}

// ------------------------------------------------------------------------------------------------
/** We just exited from a supply truck task and are now idle, we should go back to Dozer idle
		mode */
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::exitingSupplyTruckState()
{
	if( m_workerMachine->getCurrentStateID() == AS_SUPPLY_TRUCK )
	{
 		// We've been given a Dozer specific order that the Supply Truck machine doesn't recognize
 		// as BUSY (because this command also recognizes its own busy and is likewise waiting).
 		// Explicitly slap it upside the head.
 		if( getObject()->getAIUpdateInterface() )
 		{
 			getObject()->getAIUpdateInterface()->aiIdle(CMD_FROM_AI);
 		}
 		m_workerMachine->setState( AS_DOZER );
 		// To clarify, I leave supply truck mode when I notice I am doing something not supply
 		// truck related.  When given a construct command, I wait to do anything until I notice
 		// I'm not busy.  Both states are being polite, so I must force the switch.
	}
}


// ------------------------------------------------------------------------------------------------
/** Given our current task and repair target, can we accept this as a new repair target */
// ------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::canAcceptNewRepair( Object *obj )
{

	// sanity
	if( obj == nullptr )
		return FALSE;

	// if we're not repairing right now, we don't have any accept restrictions
	if( getCurrentTask() != DOZER_TASK_REPAIR )
		return TRUE;

	// get current repair target
	Object *currentRepair = TheGameLogic->findObjectByID( m_task[ DOZER_TASK_REPAIR ].m_targetObjectID );

	if( currentRepair )
	{

		// check for same object
		if( currentRepair == obj )
			return FALSE;

		// check for repairing any tower on the same bridge
		if( currentRepair->isKindOf( KINDOF_BRIDGE_TOWER ) &&
				obj->isKindOf( KINDOF_BRIDGE_TOWER ) )
		{
			BridgeTowerBehaviorInterface *currentTowerInterface = nullptr;
			BridgeTowerBehaviorInterface *newTowerInterface = nullptr;

			currentTowerInterface = BridgeTowerBehavior::getBridgeTowerBehaviorInterfaceFromObject( currentRepair );
			newTowerInterface = BridgeTowerBehavior::getBridgeTowerBehaviorInterfaceFromObject( obj );

			// sanity
			if( currentTowerInterface == nullptr || newTowerInterface == nullptr )
			{

				DEBUG_CRASH(( "Unable to find bridge tower interface on object" ));
				return FALSE;

			}

			// if they are part of the same bridge, ignore this repair command
			if( currentTowerInterface->getBridgeID() == newTowerInterface->getBridgeID() )
				return FALSE;

		}

	}

	// all is well
	return TRUE;

}

//----------------------------------------------------------------------------------------
void WorkerAIUpdate::privateIdle(CommandSourceType cmdSource)
{
// Leaving this commented out to show that although the regular supply truck does this, the
	// worker's dozer brain will get completely screwed.

	// If the user gives a stop command, I have to turn off autopilot
//	if( cmdSource == CMD_FROM_PLAYER )
//		setForceBusyState(TRUE);

	AIUpdateInterface::privateIdle(cmdSource);
}

//----------------------------------------------------------------------------------------
void WorkerAIUpdate::privateDock( Object *dock, CommandSourceType cmdSource )
{
	AIUpdateInterface::privateDock( dock, cmdSource );

	// If this is a command from a player, I will remember this as my favorite dock to override
	// ResourceManager searches.
	if ((cmdSource == CMD_FROM_PLAYER) && dock)
	{
		// Please note, there is not a separate Warehouse and Center memory by Design.  Because
		// we lack a UI way to click Warehouse and drag to center to set up a specific path, the
		// practical realization has been made that you do not want separate memory.
		m_preferredDock = dock->getID();
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::privateRepair( Object *obj, CommandSourceType cmdSource )
{
	Object *dozer = getObject();

	// sanity, if we can't repair the object then get out of there
	if( TheActionManager->canRepairObject( dozer, obj, cmdSource ) == FALSE )
		return;

	// if we are already repairing this target do nothing
	if( canAcceptNewRepair( obj ) == FALSE )
		return;

	//
	// if this object has already been targeted for repair by an object we won't also try to
	// go repair it
	ObjectID currentRepairer = obj->getSoleHealingBenefactor();
	if( currentRepairer != INVALID_ID && currentRepairer != dozer->getID() )
		return;

	// start the new task
	newTask( DOZER_TASK_REPAIR, obj );

}

// ------------------------------------------------------------------------------------------------
/** Resume construction on a building */
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::privateResumeConstruction( Object *obj, CommandSourceType cmdSource )
{

	// sanity
	if( obj == nullptr )
		return;

	// make sure we can resume construction on this
	if( TheActionManager->canResumeConstructionOf( getObject(), obj, cmdSource ) == FALSE )
		return;

	// start the new task for construction
	newTask( DOZER_TASK_BUILD, obj );

}

//-------------------------------------------------------------------------------------------------
/** Issue and order to the dozer */
//-------------------------------------------------------------------------------------------------
void WorkerAIUpdate::newTask( DozerTask task, Object* target )
{

	// sanity
	DEBUG_ASSERTCRASH( task >= 0 && task < DOZER_NUM_TASKS, ("Illegal dozer task '%d'", task) );

	// sanity
	if( target == nullptr )
		return;

	m_preferredDock = INVALID_ID; // If we are dozing, we don't want any supply truck stuff going on. jba.

	//
	// special check for the build task, we should never be given more than one of them ...
	// for the other tasks we just forget what we were doing and the new target takes
	// precedence for the task
	//
	if( task == DOZER_TASK_BUILD || task == DOZER_TASK_REPAIR )
	{

		// handle getting two tasks
		if( isTaskPending( task ) == TRUE )
			cancelTask( task );

		// get our object
		Object *me = getObject();

		Coord3D position;
		target = DozerAIUpdate::findGoodBuildOrRepairPositionAndTarget(me, target, position);
		if (target == nullptr)
			return;	// could happen for some bridges

		//
		// for building, we say that even "thinking" about building or rebuilding an object
		// sets us as the current builder of that object.  this allows any dozers that are
		// ordered later to resume construction on something to see that somebody is already taking
		// care of it and then they won't be even try to resume a build since we don't allow
		// multiple dozers/workers to double up on construction efforts
		//
		if( task == DOZER_TASK_BUILD )
			target->setBuilder( me );

		m_dockPoint[ task ][ DOZER_DOCK_POINT_START ].valid			= TRUE;
		m_dockPoint[ task ][ DOZER_DOCK_POINT_START ].location	= position;
		m_dockPoint[ task ][ DOZER_DOCK_POINT_ACTION ].valid		= TRUE;
		m_dockPoint[ task ][ DOZER_DOCK_POINT_ACTION ].location = position;
		m_dockPoint[ task ][ DOZER_DOCK_POINT_END ].valid				= TRUE;
		m_dockPoint[ task ][ DOZER_DOCK_POINT_END ].location		= position;

	}

	// set the new task target and the frame in which we got this order
	m_task[ task ].m_targetObjectID = target->getID();
	m_task[ task ].m_taskOrderFrame = TheGameLogic->getFrame();

	// reset the dozer behavior so that it can re-evaluate which task to continue working on
	m_dozerMachine->resetToDefaultState();

	// reset the workermachine, if we've been acting like a supply truck
	if( m_workerMachine->getCurrentStateID() == AS_SUPPLY_TRUCK )
	{
		// We've been given a Dozer specific order that the Supply Truck machine doesn't recognize
		// as BUSY (because this command also recognizes its own busy and is likewise waiting).
		// Explicitly slap it upside the head.
		if( getObject()->getAIUpdateInterface() )
		{
			getObject()->getAIUpdateInterface()->aiIdle(CMD_FROM_AI);
		}
		m_workerMachine->setState( AS_DOZER );
		// To clarify, I leave supply truck mode when I notice I am doing something not supply
		// truck related.  When given a construct command, I wait to do anything until I notice
		// I'm not busy.  Both states are being polite, so I must force the switch.
	}

}

//-------------------------------------------------------------------------------------------------
/** Cancel a task and reset the dozer behavior state machine so that it can
	* re-evaluate what it wants to do if it was working on the task being
	* cancelled */
//-------------------------------------------------------------------------------------------------
void WorkerAIUpdate::cancelTask( DozerTask task, Bool rememberTask )
{
	if (rememberTask)
		setPreviousTask(task);
	else
		clearPreviousTask();

	// clear the order
	internalCancelTask( task );

	// reset the machine to we can re-evaluate what we want to do
	m_dozerMachine->resetToDefaultState();

}

void WorkerAIUpdate::cancelAllTasks()
{
	clearPreviousTask();

	for (UnsignedInt task = DOZER_TASK_FIRST; task < DOZER_NUM_TASKS; ++task)
		internalCancelTask((DozerTask)task);

	// TheSuperHackers @feature also drop any queued waypoint orders (builds and queued
	// movement). These are ghost orders only — no foundation was ever created and no money
	// was taken, so there is nothing to refund or destroy here.
	m_queuedBuildCount = 0;
	for( Int q = 0; q < DOZER_MAX_QUEUED_BUILDS; q++ )
	{
		m_queuedBuildTemplates[ q ] = nullptr;
		m_queuedBuildPositions[ q ].zero();
		m_queuedBuildAngles[ q ] = 0.0f;
		m_queuedBuildWaypointIndex[ q ] = -1;
		m_queuedIsMove[ q ] = FALSE;
	}

	m_dozerMachine->resetToDefaultState();
}

//-------------------------------------------------------------------------------------------------
/** Set the previous task so that we may return to it if we become temporarily incapacitated */
//-------------------------------------------------------------------------------------------------
void WorkerAIUpdate::setPreviousTask(DozerTask task)
{
	if (task == DOZER_TASK_INVALID)
		return;

	DEBUG_ASSERTCRASH(m_previousTask == DOZER_TASK_INVALID, ("Worker already remembers a previous task"));

	m_previousTask = task;
	m_previousTaskInfo = m_task[task];
}

//-------------------------------------------------------------------------------------------------
/** Attempt to resume the previous task */
//-------------------------------------------------------------------------------------------------
void WorkerAIUpdate::resumePreviousTask()
{
	if (m_previousTask == DOZER_TASK_INVALID)
		return;

	if (m_previousTask == DOZER_TASK_BUILD)
	{
		Object* target = TheGameLogic->findObjectByID(m_previousTaskInfo.m_targetObjectID);
		if (target && target->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
			newTask(m_previousTask, target);
	}
	else if (m_previousTask == DOZER_TASK_REPAIR || m_previousTask == DOZER_TASK_FORTIFY)
	{
		Object* target = TheGameLogic->findObjectByID(m_previousTaskInfo.m_targetObjectID);
		if (target)
			newTask(m_previousTask, target);
	}

	clearPreviousTask();
}

void WorkerAIUpdate::clearPreviousTask()
{
	m_previousTask = DOZER_TASK_INVALID;
	m_previousTaskInfo = DozerTaskInfo();
}

//-------------------------------------------------------------------------------------------------
/** Is there a given task waiting to be done */
//-------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::isTaskPending( DozerTask task )
{

	// sanity
	DEBUG_ASSERTCRASH( task >= 0 && task < DOZER_NUM_TASKS, ("Illegal dozer task '%d'", task) );

	return m_task[ task ].m_targetObjectID != 0 ? TRUE : FALSE;

}

//-------------------------------------------------------------------------------------------------
/** Is there any task pending */
//-------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::isAnyTaskPending()
{

	for( Int i = 0; i < DOZER_NUM_TASKS; i++ )
		if( isTaskPending( (DozerTask)i ) )
			return TRUE;

	return FALSE;

}

//-------------------------------------------------------------------------------------------------
/** Get the target object of a given task */
//-------------------------------------------------------------------------------------------------
ObjectID WorkerAIUpdate::getTaskTarget( DozerTask task )
{

	// sanity
	DEBUG_ASSERTCRASH( task >= 0 && task < DOZER_NUM_TASKS, ("Illegal dozer task '%d'", task) );

	return m_task[ task ].m_targetObjectID;

}

//-------------------------------------------------------------------------------------------------
/** Set a task as successfully completed */
//-------------------------------------------------------------------------------------------------
void WorkerAIUpdate::internalTaskComplete( DozerTask task )
{

	// sanity
	DEBUG_ASSERTCRASH( task >= 0 && task < DOZER_NUM_TASKS, ("Illegal dozer task '%d'", task) );

	// call the single method that gets called for completing and canceling tasks
	internalTaskCompleteOrCancelled( task );

	// remove the info for this task
	m_task[ task ].m_targetObjectID = INVALID_ID;
	m_task[ task ].m_taskOrderFrame = 0;

	clearPreviousTask();

	// remove dock point info for this task
	for( Int i = 0; i < DOZER_NUM_DOCK_POINTS; i++ )
		m_dockPoint[ task ][ i ].valid = FALSE;

}

//-------------------------------------------------------------------------------------------------
/** Clear a task from the Dozer for consideration, we can use this when a goal object becomes
	* invalid/destroyed etc. */
//-------------------------------------------------------------------------------------------------
void WorkerAIUpdate::internalCancelTask( DozerTask task )
{

	// sanity
	DEBUG_ASSERTCRASH( task >= 0 && task < DOZER_NUM_TASKS, ("Illegal dozer task '%d'", task) );

	if(task < 0 || task >= DOZER_NUM_TASKS)
		return;  //DAMNIT!  You CANNOT assert and then not handle the damn error!  The.  Code.  Must.  Not.  Crash.

	// call the single method that gets called for completing and canceling tasks
	internalTaskCompleteOrCancelled( task );

	// remove the info for this task
	m_task[ task ].m_targetObjectID = INVALID_ID;
	m_task[ task ].m_taskOrderFrame = 0;

	// remove dock point info for this task
	for( Int i = 0; i < DOZER_NUM_DOCK_POINTS; i++ )
		m_dockPoint[ task ][ i ].valid = FALSE;

	// stop the dozer from moving
	AIUpdateInterface *ai = getObject()->getAIUpdateInterface();
	if( !ai )
	{
		return;
	}
	/// @todo we really need a stop command instead of making it move to it's current location
	ai->aiMoveToPosition( getObject()->getPosition(), CMD_FROM_AI );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::internalTaskCompleteOrCancelled( DozerTask task )
{

	switch( task )
	{

		// --------------------------------------------------------------------------------------------
		case DOZER_TASK_INVALID:
		{

			break;  // do nothing, this is really no task

		}

		// --------------------------------------------------------------------------------------------
		case DOZER_TASK_BUILD:
		{

			// the builder is no longer actively building something
			getObject()->clearModelConditionState( MODELCONDITION_ACTIVELY_CONSTRUCTING );

			// And the thing we were working on is no longer being actively built

			///@todo This would be correct except that we don't have idle crane animations and it is December.
//			Object* goalObject = TheGameLogic->findObjectByID(m_task[task].m_targetObjectID);
//			if (goalObject != nullptr)
//			{
//				goalObject->clearModelConditionState(MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED);
//			}
			break;

		}

		// --------------------------------------------------------------------------------------------
		case DOZER_TASK_REPAIR:
		{
			Object *obj = nullptr;

			// the builder is no longer actively repairing something
			getObject()->clearModelConditionState( MODELCONDITION_ACTIVELY_CONSTRUCTING );

			// get object to reapir (if present)
			obj = TheGameLogic->findObjectByID( m_task[ task ].m_targetObjectID );

			if( obj )
			{
 				// when we're done repairing bridges, tell the scaffolding to go away
 				if( obj->isKindOf( KINDOF_BRIDGE_TOWER ) )
 					removeBridgeScaffolding( obj );

			}

			break;

		}

		// --------------------------------------------------------------------------------------------
		case DOZER_TASK_FORTIFY:
		{

			break;

		}

		// --------------------------------------------------------------------------------------------
		default:
		{

			DEBUG_CRASH(( "internalTaskCompleteOrCancelled: Unknown Dozer task '%d'", task ));
			break;

		}

	}

}

//-------------------------------------------------------------------------------------------------
/** If we were building something, kill the active-construction flag on it */
//-------------------------------------------------------------------------------------------------
void WorkerAIUpdate::onDelete()
{
	Int i;

	// cancel any of the tasks we had queued up
	for( i = DOZER_TASK_FIRST; i < DOZER_NUM_TASKS; ++i )
	{

		if( isTaskPending( (DozerTask)i ) )
			cancelTask( (DozerTask)i );

	}

	for( i = 0; i < DOZER_NUM_TASKS; i++ )
	{
		Object* goalObject = TheGameLogic->findObjectByID(m_task[i].m_targetObjectID);
		if (goalObject != nullptr)
		{
			goalObject->clearModelConditionState(MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED);
		}
	}
}

void WorkerAIUpdate::onDisabledEdge(Bool nowDisabled)
{
	if (nowDisabled)
	{
		// Have to say goodbye to the thing we might be building or repairing so someone else can do it.
		if (getCurrentTask() != DOZER_TASK_INVALID)
		{
			// TheSuperHackers @info We want to explicitly define what types to resume from as some types
			// are undesirable (e.g. DISABLED_HELD via entering/exiting a container).
			Bool rememberTask = getObject()->isDisabledByType(DISABLED_EMP) ||
				getObject()->isDisabledByType(DISABLED_HACKED) ||
				getObject()->isDisabledByType(DISABLED_SUBDUED) ||
				getObject()->isDisabledByType(DISABLED_FROZEN) ||
				getObject()->isDisabledByType(DISABLED_UNDERPOWERED);

			cancelTask(getCurrentTask(), rememberTask);
		}
	}
	else
	{
#if !RETAIL_COMPATIBLE_CRC
		// TheSuperHackers @bugfix Stubbjax 17/11/2025 Resume previous task when re-enabled.
		resumePreviousTask();
#endif
	}
}

//-------------------------------------------------------------------------------------------------
/** Get the most recently issued task */
//-------------------------------------------------------------------------------------------------
DozerTask WorkerAIUpdate::getMostRecentCommand()
{
	Int i;
	DozerTask mostRecentTask = DOZER_TASK_INVALID;
	UnsignedInt mostRecentFrame = 0;

	for( i = 0; i < DOZER_NUM_TASKS; i++ )
	{
		if( isTaskPending( (DozerTask)i ) )
		{
			if( m_task[ i ].m_taskOrderFrame > mostRecentFrame )
			{
				mostRecentTask = (DozerTask)i;
				mostRecentFrame = m_task[ i ].m_taskOrderFrame;
			}
		}
	}
	return mostRecentTask;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
const Coord3D* WorkerAIUpdate::getDockPoint( DozerTask task, DozerDockPoint point )
{

	// sanity
	if( task < 0 || task >= DOZER_NUM_TASKS )
		return nullptr;

	// sanity
	if( point < 0 || point >= DOZER_NUM_DOCK_POINTS )
		return nullptr;

	// if the point has been set (is valid) then return it
	if( m_dockPoint[ task ][ point ].valid )
		return &m_dockPoint[ task ][ point ].location;

	// no valid point has been set for this dock point on this task
	return nullptr;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
void WorkerAIUpdate::aiDoCommand(const AICommandParms* parms)
{

	//
	// anytime we get a command, just remove any model condition that has us actively building
	// if we need to show that, that bit will be set anyway again during the build process
	//
	getObject()->clearModelConditionState( MODELCONDITION_ACTIVELY_CONSTRUCTING );

	if (!isAllowedToRespondToAiCommands(parms))
		return;

	// TheSuperHackers @bugfix Same as DozerAIUpdate: a movement order (plain move, waypoint-mode
	// append) while other orders are standing is appended to the unified FIFO instead of being
	// executed at once (executing it cancelled the running construction and raised the next
	// foundation while we were already walking away). Builds and moves execute strictly in the
	// sequence the player issued them.
	// Every other player-issued command still abandons the queued ghost
	// orders (nothing was created, nothing to refund).
	// queueConstruct() does not go through here, so queueing builds never wipes the queue.
	if( parms->m_cmdSource == CMD_FROM_PLAYER &&
			( parms->m_cmd == AICMD_MOVE_TO_POSITION ||
			parms->m_cmd == AICMD_MOVE_TO_POSITION_EVEN_IF_SLEEPING ||
			parms->m_cmd == AICMD_FOLLOW_PATH_APPEND ) &&
			m_queuedBuildCount > 0 )
	{
		if( m_queuedBuildCount < DOZER_MAX_QUEUED_BUILDS )
		{
			m_queuedBuildTemplates[ m_queuedBuildCount ] = nullptr;
			m_queuedBuildPositions[ m_queuedBuildCount ] = parms->m_pos;
			m_queuedBuildAngles[ m_queuedBuildCount ] = 0.0f;
			m_queuedBuildWaypointIndex[ m_queuedBuildCount ] = -1;
			m_queuedIsMove[ m_queuedBuildCount ] = TRUE;
			m_queuedBuildCount++;
		}
		// queue full: drop the order (nothing was created, nothing to refund)
		return;
	}

	// TheSuperHackers @bugfix Same as DozerAIUpdate: a waypoint-mode append arrives as
	// AICMD_FOLLOW_PATH_APPEND and must not wipe the standing queued orders.
	if( parms->m_cmdSource == CMD_FROM_PLAYER &&
			parms->m_cmd != AICMD_FOLLOW_PATH_APPEND )
	{
		// queued movement orders die with the queue: this command takes us over
		m_queuedBuildCount = 0;
		for( Int q = 0; q < DOZER_MAX_QUEUED_BUILDS; q++ )
		{
			m_queuedBuildTemplates[ q ] = nullptr;
			m_queuedBuildPositions[ q ].zero();
			m_queuedBuildAngles[ q ] = 0.0f;
			m_queuedBuildWaypointIndex[ q ] = -1;
			m_queuedIsMove[ q ] = FALSE;
		}
	}

	// create our machines if they don't yet exist
	createMachines();

	switch( parms->m_cmd )
	{

		// --------------------------------------------------------------------------------------------
		case AICMD_REPAIR:
		{

			// if we have no task right now, go idle so we can immediately respond to this
			if( getCurrentTask() == DOZER_TASK_INVALID )
				aiIdle( CMD_FROM_AI );

			// do the repair
			privateRepair(parms->m_obj, parms->m_cmdSource);
			break;

		}

		// --------------------------------------------------------------------------------------------
		case AICMD_RESUME_CONSTRUCTION:
		{

			// if we have no task right now, go idle so we can immediately respond to this
			if( getCurrentTask() == DOZER_TASK_INVALID )
				aiIdle( CMD_FROM_AI );

			// do the command
			privateResumeConstruction( parms->m_obj, parms->m_cmdSource );
			break;

		}

		// --------------------------------------------------------------------------------------------
		default:
		{

			// if this is from the player, cancel our current task
			if( parms->m_cmdSource == CMD_FROM_PLAYER && getCurrentTask() != DOZER_TASK_INVALID )
				cancelTask( getCurrentTask() );

			// issue the command
			AIUpdateInterface::aiDoCommand(parms);

			// when a player issues commands, this will cause the dozer to re-evaluate what it's doing
			if( parms->m_cmdSource == CMD_FROM_PLAYER )
				m_dozerMachine->resetToDefaultState();
			break;

		}

	}

	if (isClearingMines() && m_numberBoxes > 0 )
	{
		// if clearing mines, we drop any boxes we were carrying
		m_numberBoxes = 0;
		Drawable *draw = getObject()->getDrawable();
		if( draw )
		{
			draw->updateDrawableSupplyStatus( getWorkerAIUpdateModuleData()->m_maxBoxesData, m_numberBoxes );
		}
	}

}


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// SupplyTruck stuff

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::loseOneBox()
{
	if( m_numberBoxes == 0 )
		return FALSE;

	m_numberBoxes--;

	Drawable *draw = getObject()->getDrawable();
	if( draw )
	{
		draw->updateDrawableSupplyStatus( getWorkerAIUpdateModuleData()->m_maxBoxesData, m_numberBoxes );
	}

	return TRUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::gainOneBox( Int remainingStock )
{
	if( getWorkerAIUpdateModuleData() && m_numberBoxes >= getWorkerAIUpdateModuleData()->m_maxBoxesData )
		return FALSE;

	++m_numberBoxes;

	//if I just took the last box,
	//i will announce that this supply source is now empty
	if (remainingStock == 0)
	{
		Object* bestWarehouse = getObject()->getControllingPlayer()->getResourceGatheringManager()->findBestSupplyWarehouse( getObject() );

		Bool playDepleted = FALSE;
		if ( bestWarehouse )
		{
			//figure out whether the best one is considerably far from the previous one (current position)
			Coord3D delta = *getObject()->getPosition();
			delta.sub( *bestWarehouse->getPosition() );
			if ( delta.length() > getWarehouseScanDistance()/4)
			playDepleted = TRUE;
		}
		else
			playDepleted = TRUE;

		if (playDepleted && m_suppliesDepletedVoice.getEventName().isEmpty() == false)
		{
			m_suppliesDepletedVoice.setObjectID(getObject()->getID());
			m_suppliesDepletedVoice.setPlayingHandle(TheAudio->addAudioEvent(&m_suppliesDepletedVoice));
		}
	}


	Drawable *draw = getObject()->getDrawable();
	if( draw )
	{
		draw->updateDrawableSupplyStatus( getWorkerAIUpdateModuleData()->m_maxBoxesData, m_numberBoxes );
	}

	return TRUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::isSupplyTruckBrainActiveAndBusy()
{
	return (m_workerMachine->getCurrentStateID() == AS_SUPPLY_TRUCK)
				&& (m_supplyTruckStateMachine->getCurrentStateID() == ST_BUSY);
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::resetSupplyTruckBrain()
{
	m_supplyTruckStateMachine->resetToDefaultState();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::resetDozerBrain()
{
	m_dozerMachine->resetToDefaultState();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// Worker's master state machine, that controls Dozerness or Supplytruckness
class ActAsDozerState :  public State
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ActAsDozerState, "ActAsDozerState")
protected:
	// snapshot interface STUBBED.
	virtual void crc( Xfer *xfer ) override {};
	virtual void xfer( Xfer *xfer ) override {};
	virtual void loadPostProcess() override {};

public:
	ActAsDozerState( StateMachine *machine ) :State( machine, "ActAsDozerState" ){}
	virtual StateReturnType onEnter() override;
	virtual StateReturnType update() override;
	virtual StateReturnType onExit();
};
EMPTY_DTOR(ActAsDozerState)

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
class ActAsSupplyTruckState :  public State
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ActAsSupplyTruckState, "ActAsSupplyTruckState")
protected:
	// snapshot interface STUBBED.
	virtual void crc( Xfer *xfer ) override {};
	virtual void xfer( Xfer *xfer ) override {};
	virtual void loadPostProcess() override {};

public:
	ActAsSupplyTruckState( StateMachine *machine ) :State( machine, "ActAsSupplyTruckState" ){}
	virtual StateReturnType onEnter() override;
	virtual StateReturnType update() override;
	virtual StateReturnType onExit();
};
EMPTY_DTOR(ActAsSupplyTruckState)

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
WorkerStateMachine::WorkerStateMachine( Object *owner ) : StateMachine( owner, "WorkerStateMachine" )
{
	static const StateConditionInfo asDozerConditions[] =
	{
		StateConditionInfo(supplyTruckSubMachineWantsToEnter, AS_SUPPLY_TRUCK, nullptr),
		StateConditionInfo(nullptr, INVALID_STATE_ID, nullptr)
	};

	static const StateConditionInfo asTruckConditions[] =
	{
		StateConditionInfo(supplyTruckSubMachineReadyToLeave, AS_DOZER, nullptr),
		StateConditionInfo(nullptr, INVALID_STATE_ID, nullptr)
	};

	// order matters: first state is the default state.
	defineState( AS_DOZER,					newInstance(ActAsDozerState)( this ), INVALID_STATE_ID, INVALID_STATE_ID, asDozerConditions );
	defineState( AS_SUPPLY_TRUCK,		newInstance(ActAsSupplyTruckState)( this ), INVALID_STATE_ID, INVALID_STATE_ID, asTruckConditions );
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
WorkerStateMachine::~WorkerStateMachine()
{
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void WorkerStateMachine::crc( Xfer *xfer )
{
	StateMachine::crc(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Xfer Method */
// ------------------------------------------------------------------------------------------------
void WorkerStateMachine::xfer( Xfer *xfer )
{
	XferVersion cv = 1;
	XferVersion v = cv;
	xfer->xferVersion( &v, cv );

	StateMachine::xfer(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void WorkerStateMachine::loadPostProcess()
{
	StateMachine::loadPostProcess();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool WorkerStateMachine::supplyTruckSubMachineWantsToEnter( State *thisState, void* userData )
{
	Object *owner = thisState->getMachineOwner();
	WorkerAIUpdate *update = (WorkerAIUpdate*)owner->getAIUpdateInterface();
	if( !update )
	{
		return false;
	}
	AIStateType masterState = update->getAIStateType();

	//If I detect a Supply force message, or if I have been put straight in dock,
	//then the worker master part of me wants to switch to the Supply sub-brain

	return (update->isForcedIntoWantingState() && !owner->isContained()) || (masterState == AI_DOCK);
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool WorkerStateMachine::supplyTruckSubMachineReadyToLeave( State *thisState, void* userData )
{
	Object *owner = thisState->getMachineOwner();
	WorkerAIUpdate *update = (WorkerAIUpdate*)owner->getAIUpdateInterface();
	if( !update )
	{
		return false;
	}
//	AIStateType masterState = update->getAIStateType();

	// It isn't ready to leave if it is on its way in.  The first clause
	// allow a latch for a moment
	// so there is no transition out on the way in.  Active and Busy means it isn't doing
	// anything Supply related.

	return !supplyTruckSubMachineWantsToEnter( thisState, nullptr )
				&& update->isSupplyTruckBrainActiveAndBusy();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
StateReturnType ActAsDozerState::onEnter()
{
	return STATE_CONTINUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
StateReturnType ActAsDozerState::update()
{
	return STATE_CONTINUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
StateReturnType ActAsDozerState::onExit()
{
	Object *owner = getMachineOwner();
	WorkerAIUpdate *update = (WorkerAIUpdate*)owner->getAIUpdateInterface();
	if( !update )
	{
		return STATE_FAILURE;
	}

	update->resetDozerBrain();

	return STATE_CONTINUE;
}


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
StateReturnType ActAsSupplyTruckState::onEnter()
{
	return STATE_CONTINUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
StateReturnType ActAsSupplyTruckState::update()
{
	return STATE_CONTINUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
StateReturnType ActAsSupplyTruckState::onExit()
{
	Object *owner = getMachineOwner();
	WorkerAIUpdate *update = (WorkerAIUpdate*)owner->getAIUpdateInterface();
	if( !update )
	{
		return STATE_FAILURE;
	}

	update->resetSupplyTruckBrain();

	return STATE_CONTINUE;
}

// ------------------------------------------------------------------------------------------------
/** Create the bridge scaffolding if necessary for the bridge that is attached to this tower */
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::createBridgeScaffolding( Object *bridgeTower )
{

	// sanity
	if( bridgeTower == nullptr )
		return;

	// get the bridge behavior interface from the bridge object that this tower is a part of
	BridgeTowerBehaviorInterface *btbi = BridgeTowerBehavior::getBridgeTowerBehaviorInterfaceFromObject( bridgeTower );
	if( btbi == nullptr )
		return;
	Object *bridgeObject = TheGameLogic->findObjectByID( btbi->getBridgeID() );
	if( bridgeObject == nullptr )
		return;
	BridgeBehaviorInterface *bbi = BridgeBehavior::getBridgeBehaviorInterfaceFromObject( bridgeObject );
	if( bbi == nullptr )
		return;

	// tell the bridge to create scaffolding if necessary
	bbi->createScaffolding();

}

// ------------------------------------------------------------------------------------------------
/** Remove the bridge scaffolding from the bridge object that is attached to this tower */
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::removeBridgeScaffolding( Object *bridgeTower )
{

	// sanity
	if( bridgeTower == nullptr )
		return;

	// get the bridge behavior interface from the bridge object that this tower is a part of
	BridgeTowerBehaviorInterface *btbi = BridgeTowerBehavior::getBridgeTowerBehaviorInterfaceFromObject( bridgeTower );
	if( btbi == nullptr )
		return;
	Object *bridgeObject = TheGameLogic->findObjectByID( btbi->getBridgeID() );
	if( bridgeObject == nullptr )
		return;
	BridgeBehaviorInterface *bbi = BridgeBehavior::getBridgeBehaviorInterfaceFromObject( bridgeObject );
	if( bbi == nullptr )
		return;

	// tell the bridge to end any scaffolding from repairing
	bbi->removeScaffolding();

}

//------------------------------------------------------------------------------------------------
void WorkerAIUpdate::startBuildingSound( const AudioEventRTS *sound, ObjectID constructionSiteID )
{
	m_buildingSound = *sound;
	m_buildingSound.setObjectID( constructionSiteID );
	m_buildingSound.setPlayingHandle( TheAudio->addAudioEvent( &m_buildingSound ) );
}

//------------------------------------------------------------------------------------------------
void WorkerAIUpdate::finishBuildingSound()
{
	TheAudio->removeAudioEvent( m_buildingSound.getPlayingHandle() );
}

//------------------------------------------------------------------------------------------------
Int WorkerAIUpdate::getUpgradedSupplyBoost() const
{
	Player *player = getObject()->getControllingPlayer();
	static const UpgradeTemplate *workerShoeTemplate = TheUpgradeCenter->findUpgrade( "Upgrade_GLAWorkerShoes" );

	if (player && workerShoeTemplate && player->hasUpgradeComplete(workerShoeTemplate))
		return getWorkerAIUpdateModuleData()->m_upgradedSupplyBoost;
	else
		return 0;
}


// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::crc( Xfer *xfer )
{
	// extend base class
	AIUpdateInterface::crc(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version
	* 2: TheSuperHackers @tweak Stubbjax 17/11/2025 Save the worker's previous task
	* 3: TheSuperHackers @feature Save the waypoint build queue
	* 4: TheSuperHackers @feature Save the goal path waypoint each queued build is waiting on
	* 5: TheSuperHackers @feature Queued entries are ghost orders (ThingTemplate + position +
	*    angle); the foundation is only created once the builder arrives at the waypoint
	* 6: TheSuperHackers @bugfix Unified build/move FIFO: each queued entry carries a movement flag
	*/
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::xfer( Xfer *xfer )
{
#if RETAIL_COMPATIBLE_XFER_SAVE
	XferVersion currentVersion = 1;
#else
	XferVersion currentVersion = 6;
#endif
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

 // extend base class
	AIUpdateInterface::xfer(xfer);


	//------------------------- xfer Dozer info

	Int numTasks = DOZER_NUM_TASKS;
	xfer->xferInt(&numTasks);
	if (numTasks != DOZER_NUM_TASKS) {
		DEBUG_CRASH(("DOZER_NUM_TASKS changed unexpectedly."));
		throw SC_INVALID_DATA;
	}
	Int i, j;
	for (i=0; i<DOZER_NUM_TASKS; i++) {
		xfer->xferObjectID(&m_task[i].m_targetObjectID);
		xfer->xferUnsignedInt(&m_task[i].m_taskOrderFrame);
	}
	xfer->xferSnapshot(m_dozerMachine);
	xfer->xferUser(&m_currentTask, sizeof(m_currentTask));

	if (version >= 2)
	{
		xfer->xferUser(&m_previousTask, sizeof(m_previousTask));
		xfer->xferUser(&m_previousTaskInfo, sizeof(m_previousTaskInfo));
	}

	Int dockPoints = DOZER_NUM_DOCK_POINTS;
	xfer->xferInt(&dockPoints);
	if (dockPoints!=DOZER_NUM_DOCK_POINTS) {
		DEBUG_CRASH(("DOZER_NUM_DOCK_POINTS changed unexpectedly."));
		throw SC_INVALID_DATA;
	}
	for (i=0; i<DOZER_NUM_TASKS; i++) {
		for (j=0; j<DOZER_NUM_DOCK_POINTS; j++) {
			xfer->xferBool(&m_dockPoint[i][j].valid);
			xfer->xferCoord3D(&m_dockPoint[i][j].location);
		}
	}
	xfer->xferUser(&m_buildSubTask, sizeof(m_buildSubTask));

	// TheSuperHackers @feature waypoint build queue (version 3/4)
	// NOTE: must guard on the STREAM version ("version"), not the compile-time
	// currentVersion, otherwise loading an older save reads past the v2 layout.
	if (version >= 3)
	{
		Int queuedCount = m_queuedBuildCount;
		xfer->xferInt(&queuedCount);
		if (xfer->getXferMode() == XFER_LOAD)
		{
			if (queuedCount < 0 || queuedCount > DOZER_MAX_QUEUED_BUILDS)
			{
				DEBUG_CRASH(("WorkerAIUpdate::xfer - Invalid queued build count '%d'", queuedCount));
				throw SC_INVALID_DATA;
			}
			m_queuedBuildCount = queuedCount;
		}
		if (version < 5)
		{
			// Legacy layout (version 3/4): the foundation had already been created when the
			// order was issued, so entries were plain ObjectIDs. A ghost order has no object,
			// but we must still consume the data to keep the stream aligned. Queued orders
			// coming from an older save are therefore dropped — nothing had been refundable
			// at queue time under the new scheme anyway.
			for (Int q = 0; q < m_queuedBuildCount; q++)
			{
				ObjectID legacyID = INVALID_ID;
				xfer->xferObjectID(&legacyID);
				if (version >= 4)
				{
					Int legacyWaypointIndex = -1;
					xfer->xferInt(&legacyWaypointIndex);
				}
			}
			if (xfer->getXferMode() == XFER_LOAD)
				m_queuedBuildCount = 0;
		}
		else
		{
			// Ghost orders: what to build, where, and at which angle — the angle is part of
			// the order so the preview and the finished building face the same way.
			for (Int q = 0; q < m_queuedBuildCount; q++)
			{
				AsciiString tmplName;
				if (xfer->getXferMode() == XFER_SAVE && m_queuedBuildTemplates[q] != nullptr)
					tmplName = m_queuedBuildTemplates[q]->getName();

				xfer->xferAsciiString(&tmplName);
				xfer->xferCoord3D(&m_queuedBuildPositions[q]);
				xfer->xferReal(&m_queuedBuildAngles[q]);
				xfer->xferInt(&m_queuedBuildWaypointIndex[q]);

				// version 6: entries may be queued movement orders; a v5 stream only
				// ever contains construction orders.
				if (version >= 6)
					xfer->xferBool(&m_queuedIsMove[q]);
				else if (xfer->getXferMode() == XFER_LOAD)
					m_queuedIsMove[q] = FALSE;

				if (xfer->getXferMode() == XFER_LOAD)
				{
					// check=FALSE: a mod removing a template must not hard-crash the save load.
					m_queuedBuildTemplates[q] = TheThingFactory->findTemplate( tmplName, FALSE );
					if (m_queuedBuildTemplates[q] == nullptr)
						m_queuedBuildWaypointIndex[q] = -1;	// drop it on next process
				}
			}
		}
	}


	//------------------------- xfer Supply Truck info
	xfer->xferSnapshot(m_supplyTruckStateMachine);
	xfer->xferObjectID(&m_preferredDock);
	xfer->xferInt(&m_numberBoxes);
	xfer->xferBool(&m_forcePending);

	//-------------------------- xfer Worker info
	xfer->xferSnapshot(m_workerMachine);

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::loadPostProcess()
{
 // extend base class
	AIUpdateInterface::loadPostProcess();
}
