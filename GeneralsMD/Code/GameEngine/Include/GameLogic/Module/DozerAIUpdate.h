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

// FILE: DozerAIUpdate.h //////////////////////////////////////////////////////////////////////////
// Author: Colin Day, February 2002
// Desc:   Dozer AI behavior
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "GameLogic/Module/AIUpdate.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class AudioEventRTS;
class ThingTemplate;

//-------------------------------------------------------------------------------------------------
/** The Dozer primary state machine */
//-------------------------------------------------------------------------------------------------
class DozerPrimaryStateMachine : public StateMachine
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( DozerPrimaryStateMachine, "DozerPrimaryStateMachine" );

public:

  DozerPrimaryStateMachine( Object *owner );
	// virtual destructor prototypes provided by memory pool object

	//-----------------------------------------------------------------------------------------------
	// state transition conditions
	static Bool isBuildMostImportant( State *thisState, void* userData );
	static Bool isRepairMostImportant( State *thisState, void* userData );
	static Bool isFortifyMostImportant( State *thisState, void* userData );

protected:
	// snapshot interface
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;
};


//-------------------------------------------------------------------------------------------------
/** Dozer behaviors that use action sub state machines */
//-------------------------------------------------------------------------------------------------
enum DozerTask CPP_11(: Int) // These enums are saved in the game save file, so DO NOT renumber them. jba.
{
	DOZER_TASK_INVALID = -1,

	DOZER_TASK_BUILD,												///< go build something
	DOZER_TASK_REPAIR,											///< go repair something
	DOZER_TASK_FORTIFY,											///< go fortify something

	DOZER_NUM_TASKS,												// keep this last
	DOZER_TASK_FIRST = 0,
};

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
enum DozerDockPoint CPP_11(: Int)	 // These enums are saved in the game save file, so DO NOT renumber them. jba.
{
	DOZER_DOCK_POINT_START	= 0,
	DOZER_DOCK_POINT_ACTION	= 1,
	DOZER_DOCK_POINT_END		= 2,

	DOZER_NUM_DOCK_POINTS
};

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
enum DozerBuildSubTask CPP_11(: Int)  // These enums are saved in the game save file, so DO NOT renumber them. jba.
{
	DOZER_SELECT_BUILD_DOCK_LOCATION		= 0,
	DOZER_MOVING_TO_BUILD_DOCK_LOCATION	=	1,
	DOZER_DO_BUILD_AT_DOCK							= 2
};

// ------------------------------------------------------------------------------------------------
/** This is no longer a leaf behavior.  Someone else needs to combine this
	* with another major AIUpdate.  So provide an interface to satisfy the people
	* who look this up by name. */
// ------------------------------------------------------------------------------------------------
class DozerAIInterface
{

	// This is no longer a leaf behavior.  Someone else needs to combine this
	// with another major AIUpdate.  So provide an interface to satisfy the people
	// who look this up by name.

public:

	virtual void onDelete() = 0;

	virtual Real getRepairHealthPerSecond() const = 0;	///< get health to repair per second
	virtual Real getBoredTime() const = 0;							///< how long till we're bored
	virtual Real getBoredRange() const = 0;							///< when we're bored, we look this far away to do things

	virtual Bool canBuildTemplate( const ThingTemplate *what ) const = 0;	///< may we build or resume this
	virtual Bool canRepairObjects() const = 0;					///< do we have the repair ability at all

	// methods to override for the dozer behaviors
	virtual Object *construct( const ThingTemplate *what,
														 const Coord3D *pos, Real angle,
														 Player *owningPlayer,
														 Bool isRebuild ) = 0;

	/// TheSuperHackers @feature queue a construction as a ghost order: no foundation and no
	/// money spent until the builder arrives at the waypoint the order is bound to
	virtual Object *queueConstruct( const ThingTemplate *what,
																	const Coord3D *pos, Real angle,
																	Player *owningPlayer ) = 0;

	/// TheSuperHackers @feature read-only view of the queued ghost orders, so the client can
	/// draw a translucent preview at each build site. Defaulted here (rather than pure) so
	/// dozer-like implementations without a queue do not have to care about it.
	virtual Int getQueuedBuildCount() const { return 0; }
	virtual const ThingTemplate *getQueuedBuildTemplate( Int i ) const { return nullptr; }
	virtual const Coord3D *getQueuedBuildPosition( Int i ) const { return nullptr; }
	virtual Real getQueuedBuildAngle( Int i ) const { return 0.0f; }


	// get task information
	virtual DozerTask getMostRecentCommand() = 0;				///< return task that was most recently issued
	virtual Bool isTaskPending( DozerTask task ) = 0;					///< is there a desire to do the requested task
	virtual ObjectID getTaskTarget( DozerTask task ) = 0;			///< get target of task
	virtual Bool isAnyTaskPending() = 0;								///< is there any dozer task pending

	virtual DozerTask getCurrentTask() const = 0;							///< return the current task we're doing
	// the following should only be used from inside the Dozer state machine!
	// !!! *DO NOT CALL THIS AND SET THE TASK DIRECTLY TO AFFECT BEHAVIOR* !!! ///
	virtual void setCurrentTask( DozerTask task ) = 0;				///< set the current task of the dozer

	virtual Bool getIsRebuild() = 0;										///< get whether or not this is a rebuild.

	// task actions
	virtual void newTask( DozerTask task, Object *target ) = 0;	///< set a desire to do the requested task
	virtual void cancelTask( DozerTask task, Bool rememberTask = false ) = 0;	///< cancel this task from the queue, if it's the current task the dozer will stop working on it. Can remember the cancelled task for resumption.
	virtual void cancelAllTasks() = 0;													///< cancel all tasks from the queue, if it's the current task the dozer will stop working on it

	// internal methods to manage behavior from within the dozer state machine
	virtual void internalTaskComplete( DozerTask task ) = 0;					///< set a dozer task as successfully completed
	virtual void internalCancelTask( DozerTask task ) = 0;						///< cancel this task from the dozer
	virtual void internalTaskCompleteOrCancelled( DozerTask task ) = 0;	///< this is called when tasks are cancelled or completed

	/** return a dock point for the action and task (if valid) ... note it can return nullptr
	if no point has been set for the combination of task and point */
	virtual const Coord3D* getDockPoint( DozerTask task, DozerDockPoint point ) = 0;

	virtual void setBuildSubTask( DozerBuildSubTask subTask ) = 0;
	virtual DozerBuildSubTask getBuildSubTask() = 0;

	// repairing
	virtual Bool canAcceptNewRepair( Object *obj ) = 0;
	virtual void createBridgeScaffolding( Object *bridgeTower ) = 0;
	virtual void removeBridgeScaffolding( Object *bridgeTower ) = 0;

	virtual void startBuildingSound( const AudioEventRTS *sound, ObjectID constructionSiteID ) = 0;
	virtual void finishBuildingSound() = 0;

};

// ------------------------------------------------------------------------------------------------
/** What a dozer-like unit is allowed to work on. Shared by the Dozer and the Worker, which carry
	* the same fields but have no common module data to put them in. */
// ------------------------------------------------------------------------------------------------
class DozerRestrictions
{

public:

	DozerRestrictions();

	Bool isTemplateAllowedToBuild( const ThingTemplate *tmpl ) const;

	std::vector<AsciiString> m_allowedBuildObjects;		///< if not empty, only these may be built
	std::vector<AsciiString> m_forbiddenBuildObjects;	///< these may never be built, whatever the allow list says
	Bool m_canRepair;																	///< no removes the repair ability entirely

};

// ------------------------------------------------------------------------------------------------
/** NOTE: If you edit module data you must do it in both the Dozer *AND* the Worker */
// ------------------------------------------------------------------------------------------------
class DozerAIUpdateModuleData : public AIUpdateModuleData
{

public:

	DozerAIUpdateModuleData();

	// !!!
	// !!! NOTE: If you edit module data you must do it in both the Dozer *AND* the Worker !!!
	// !!!

	Real m_repairHealthPercentPerSecond;	///< how many health points per second the dozer repairs at
	Real m_boredTime;											///< after this many frames, a dozer will try to find something to do on its own
	Real m_boredRange;										///< range the dozers try to auto repair when they're bored
	DozerRestrictions m_restrictions;

	static void buildFieldParse( MultiIniFieldParse &p );

};

//-------------------------------------------------------------------------------------------------
/** The Dozer AI Update interface.  Dozers are workers that are capable of building all the
	* structures available to a player, as well as repairing building, and fortifying
	* civilian structures */
//-------------------------------------------------------------------------------------------------
class DozerAIUpdate : public AIUpdateInterface, public DozerAIInterface
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( DozerAIUpdate, "DozerAIUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( DozerAIUpdate, DozerAIUpdateModuleData )

public:
	static Bool findGoodBuildOrRepairPosition(const Object* me, const Object* target, Coord3D& positionOut);
	static Object* findGoodBuildOrRepairPositionAndTarget(Object* me, Object* target, Coord3D& positionOut);

public:

	DozerAIUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual DozerAIInterface* getDozerAIInterface() override {return this;}
	virtual const DozerAIInterface* getDozerAIInterface() const override {return this;}

	virtual void onDelete() override;
	virtual void onDisabledEdge(Bool nowDisabled) override;

	//
	// module data methods ... this is LAME, multiple inheritance off an interface with replicated
	// data and code, ick!
	// NOTE: If you edit module data you must do it in both the Dozer *AND* the Worker
	//
	virtual Real getRepairHealthPerSecond() const override;	///< get health to repair per second
	virtual Real getBoredTime() const override;							///< how long till we're bored
	virtual Real getBoredRange() const override;							///< when we're bored, we look this far away to do things

	virtual Bool canBuildTemplate( const ThingTemplate *what ) const override;
	virtual Bool canRepairObjects() const override;

	// methods to override for the dozer behaviors
	virtual Object* construct( const ThingTemplate *what,
														 const Coord3D *pos, Real angle,
														 Player *owningPlayer,
														 Bool isRebuild ) override;								///< construct an object

	virtual Object* queueConstruct( const ThingTemplate *what,
																	const Coord3D *pos, Real angle,
																	Player *owningPlayer ) override;					///< TheSuperHackers @feature queue a construction to build after the current path/tasks

	// get task information
	virtual DozerTask getMostRecentCommand() override;				///< return task that was most recently issued
	virtual Bool isTaskPending( DozerTask task ) override;					///< is there a desire to do the requested task
	virtual ObjectID getTaskTarget( DozerTask task ) override;			///< get target of task
	virtual Bool isAnyTaskPending() override;								///< is there any dozer task pending
	virtual DozerTask getCurrentTask() const override { return m_currentTask; }	///< return the current task we're doing
	virtual void setCurrentTask( DozerTask task ) override { m_currentTask = task; }	///< set the current task of the dozer

	virtual Bool getIsRebuild() override { return m_isRebuild; }	///< get whether or not this building is a rebuild.

	// task actions
	virtual void newTask( DozerTask task, Object *target ) override;	///< set a desire to do the requested task
	virtual void cancelTask( DozerTask task, Bool rememberTask = false ) override;							///< cancel this task from the queue, if it's the current task the dozer will stop working on it
	virtual void cancelAllTasks() override;													///< cancel all tasks from the queue, if it's the current task the dozer will stop working on it

	// internal methods to manage behavior from within the dozer state machine
	virtual void internalTaskComplete( DozerTask task ) override;					///< set a dozer task as successfully completed
	virtual void internalCancelTask( DozerTask task ) override;						///< cancel this task from the dozer
	virtual void internalTaskCompleteOrCancelled( DozerTask task ) override;	///< this is called when tasks are cancelled or completed

	/** return a dock point for the action and task (if valid) ... note it can return nullptr
	if no point has been set for the combination of task and point */
	virtual const Coord3D* getDockPoint( DozerTask task, DozerDockPoint point ) override;

	virtual void setBuildSubTask( DozerBuildSubTask subTask ) override { m_buildSubTask = subTask; };
	virtual DozerBuildSubTask getBuildSubTask() override { return m_buildSubTask; }

	virtual UpdateSleepTime update() override;											///< the update entry point

	// repairing
	virtual Bool canAcceptNewRepair( Object *obj ) override;
	virtual void createBridgeScaffolding( Object *bridgeTower ) override;
	virtual void removeBridgeScaffolding( Object *bridgeTower ) override;

	virtual void startBuildingSound( const AudioEventRTS *sound, ObjectID constructionSiteID ) override;
	virtual void finishBuildingSound() override;

	//
	// the following methods must be overridden so that if a player issues a command the dozer
	// can exit the internal state machine and do whatever the player says
	//
	virtual void aiDoCommand(const AICommandParms* parms) override;


protected:

	virtual void privateRepair( Object *obj, CommandSourceType cmdSource ) override;	///< repair the target
	virtual void privateResumeConstruction( Object *obj, CommandSourceType cmdSource ) override;  ///< resume construction on obj

	virtual void setPreviousTask(DozerTask task);					///< set the previous task
	virtual void resumePreviousTask();									///< resume the previous task if there was one
	virtual void clearPreviousTask();									///< clear the previous task

	struct DozerTaskInfo
	{
		DozerTaskInfo()
		{
			m_targetObjectID = INVALID_ID;
			m_taskOrderFrame = 0;
		}

		ObjectID m_targetObjectID;				///< target object ID of task
		UnsignedInt m_taskOrderFrame;			///< logic frame we decided we wanted to do this task
	} m_task[ DOZER_NUM_TASKS ];				///< tasks we want to do indexed by DozerTask

	DozerPrimaryStateMachine *m_dozerMachine;  ///< the custom state machine for Dozer behavior
	DozerTask m_currentTask;						///< current task the dozer is attending to (if any)
	DozerTask m_previousTask;						///< previous task the dozer was attending to (if any)
	DozerTaskInfo m_previousTaskInfo;		///< info on the previous task the dozer was attending to (if any)
	AudioEventRTS	m_buildingSound;			///< sound is pulled from the object we are building!
	Bool m_isRebuild;										///< is this a rebuild of a previous building?

	//
	// the following info array can be used if we want to have more complicated approaches
	// to our target depending on our task
	//
	struct DozerDockPointInfo
	{
		Bool valid;						///< this point has been set and is valid
		Coord3D location;			///< WORLD location
	} m_dockPoint[ DOZER_NUM_TASKS ][ DOZER_NUM_DOCK_POINTS ];

	DozerBuildSubTask m_buildSubTask;		///< for building and actually docking for the build

	//
	// TheSuperHackers @feature waypoint build queue — FIFO of foundations to construct after
	// the current path and any earlier queued builds are finished (waypoint building)
	//
	enum { DOZER_MAX_QUEUED_BUILDS = 16 };

	//
	// What we *intend* to build. Deliberately NOT ObjectIDs: while an order sits in here
	// no foundation exists and no money has been spent — this is the "ghost" order that the
	// client renders as a translucent preview. The foundation is only created (and paid for)
	// once we actually arrive at the waypoint this order is waiting on, which is also when
	// the placement gets re-validated (the world may have changed since the order was given).
	//
	const ThingTemplate *m_queuedBuildTemplates[ DOZER_MAX_QUEUED_BUILDS ];
	Coord3D m_queuedBuildPositions[ DOZER_MAX_QUEUED_BUILDS ];
	Real m_queuedBuildAngles[ DOZER_MAX_QUEUED_BUILDS ];
	Int m_queuedBuildCount;

	//
	// TheSuperHackers @feature For each queued build, the index into the unit's goal path of the
	// waypoint that was still ahead of us when the build got queued. The build may only start
	// once we have actually walked past that waypoint, so that "arrived at the waypoint" rather
	// than "happens to be standing still this frame" is what triggers the next order. -1 means
	// there was no path pending at queue time, so nothing has to be waited for.
	//
	Int m_queuedBuildWaypointIndex[ DOZER_MAX_QUEUED_BUILDS ];

	//
	// TheSuperHackers @bugfix For each queued entry, TRUE when it is a movement order rather
	// than a construction order. Builds and movement orders share one FIFO, so the sequence
	// the player issued is exactly the sequence that executes (build A, build B, move C,
	// build D, ...). A movement entry stores its destination in m_queuedBuildPositions[]
	// with a null template. Movement orders arriving while the queue is non-empty are
	// appended here instead of being executed at once: executing them immediately cancelled
	// the running construction and raised the next foundation while the builder was already
	// walking off to the new destination (see aiDoCommand()).
	//
	Bool m_queuedIsMove[ DOZER_MAX_QUEUED_BUILDS ];

private:

	void createMachines();		///< create our behavior machines we need

protected:

	/// TheSuperHackers @feature create the under-construction object at the given location
	/// (shared by construct and queueConstruct; does NOT assign any task)
	Object *createConstruction( const ThingTemplate *what, const Coord3D *pos, Real angle,
															Player *owningPlayer, Bool isRebuild );

	/// TheSuperHackers @feature promote the next queued construction into a real build task
	/// once the dozer has arrived at the waypoint the build was waiting on
	void processBuildQueue();

	void queueBuild( const ThingTemplate *what, const Coord3D *pos, Real angle, Int waypointIndex );

	/// TheSuperHackers @feature Turn a ghost order into a real foundation: re-validate the
	/// placement (the world may have changed since the order was issued), then create the
	/// under-construction object and take the money. Returns FALSE and reports why if the
	/// site is no longer buildable — the caller then just drops the order.
	Bool materializeQueuedBuild( const ThingTemplate *what, const Coord3D *pos, Real angle );

	/// TheSuperHackers @feature read-only view of the queued ghost orders, used by the client
	/// to draw the translucent build preview at each site (see W3dWaypointBuffer)
	Int getQueuedBuildCount() const override { return m_queuedBuildCount; }
	const ThingTemplate *getQueuedBuildTemplate( Int i ) const override;
	const Coord3D *getQueuedBuildPosition( Int i ) const override;
	Real getQueuedBuildAngle( Int i ) const override;

	/// TheSuperHackers @feature true once we have reached (or passed) the goal path waypoint
	/// recorded in the given index for a queued build
	Bool hasReachedQueuedWaypoint( Int waypointIndex ) const;

};
