// FILE: CommandSequence.h /////////////////////////////////////////////////////////////////////////////////
// Author: ShigureUi, September 2026
// Desc:   Advanced waypoint system
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/BitFlags.h"
#include "Common/Snapshot.h"
#include "Common/STLTypedefs.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

class CommandSequence : public SubsystemInterface, public Snapshot
{
public:
	enum CommandType
	{
		COMMAND_NONE = 0,
		COMMAND_DO_MOVETO,
		COMMAND_DO_REVERSE_MOVETO,
		COMMAND_DO_ATTACK_MOVETO,
		COMMAND_DO_GUARD_POSITION,
		COMMAND_DO_GUARD_OBJECT,
		COMMAND_DO_WEAPON,
		COMMAND_DO_WEAPON_AT_LOCATION,
		COMMAND_DO_ATTACK_OBJECT,
		COMMAND_DO_FORCE_ATTACK_OBJECT,
		COMMAND_DOZER_CONSTRUCT,
		COMMAND_DOZER_CONSTRUCT_LINE,
		COMMAND_DO_SPECIAL_POWER,
		COMMAND_DO_SPECIAL_POWER_AT_POSITION,
		COMMAND_DO_MOVING_SPECIAL_POWER_AT_POSITION,
		COMMAND_DO_SPECIAL_POWER_AT_OBJECT,
		COMMAND_DO_EXIT,
		COMMAND_DO_EVACUATE,
		COMMAND_DO_COMBATDROP,
		COMMAND_GET_REPAIRED,
		COMMAND_GET_HEALED,
		COMMAND_DO_REPAIR,
		COMMAND_RESUME_CONSTRUCTION,
		COMMAND_ENTER,
		COMMAND_DOCK,
		COMMAND_INTERNET_HACK,
		COMMAND_TOGGLE_OVERCHARGE,
		COMMAND_SWITCH_WEAPON,
		COMMAND_TOGGLE_HOLD_FIRE,
		COMMAND_TOGGLE_DEPLOY
	};

protected:

	typedef std::map<ObjectID, UnsignedInt> ObjectToNodeMap;
	typedef std::map<ObjectID, Coord3D> ObjectToLocMap;

	struct CommandNode
	{
		UnsignedInt m_nodeID;
		CommandType m_commandType;
		ObjectID m_targetID;
		Real m_angle;
		Coord3D m_loc, m_loc2;
		WeaponSlotType m_weaponSlot;
		UnsignedShort m_templateID;
		UnsignedInt m_specialPowerID;
		UnsignedInt m_specialOptions;
		SpecialPowerType m_spType;
		Int m_maxShotsToFire;
		ObjectToNodeMap m_nextCommand;
		ObjectToLocMap m_allocatedDest;
		Bool m_releaseAfter; // For onDoForceAttackGround() weird release
		GuardMode m_guardMode;
	};

private:
	typedef std::map<UnsignedInt, CommandNode*> IDToNodeMap;
	IDToNodeMap m_allNodes;

	std::map<ObjectID, Int> m_constructionCount;

	Int m_commandNodeCount;
	UnsignedInt m_uniqueID;

public:

	CommandSequence();
	virtual ~CommandSequence() override;

	// --------------- inherited from Subsystem interface -------------
	virtual void init() override;			///< initialize
	virtual void reset() override;			///< system reset
	virtual void update() override;		///< system update
	// ----------------------------------------------------------------

	// --------------- inherited from Snapshot interface --------------
	virtual void crc(Xfer *xfer) override;
	virtual void xfer(Xfer *xfer) override;
	virtual void loadPostProcess() override;

protected:
	void xferCommandNode(Xfer *xfer, CommandNode *cmdNode);

public:
	Bool hasAnyCommand(Object *obj);
	void clearObjectCommand(Object *obj);
	UnsignedInt newNoArgumentCommand(CommandType commandType);
	UnsignedInt newSinglePosCommand(CommandType commandType, const Coord3D *pos);
	UnsignedInt newSingleTargetCommand(CommandType commandType, Object *obj);
	UnsignedInt newTargetNPosCommand(CommandType commandType, Object* obj, const Coord3D* pos);
	UnsignedInt newPosNMaxShotsCommand(CommandType commandType, const Coord3D *pos, Int maxShotsToFire);
	UnsignedInt newGuardCommand(CommandType commandType, const Coord3D *pos, Object *obj, GuardMode guardMode);
	UnsignedInt newDoWeaponAtLocCommand(CommandType commandType, WeaponSlotType weaponSlot, const Coord3D *pos, Int maxShotsToFire, Bool releaseAfter);
	UnsignedInt newDoWeaponAtObjCommand(CommandType commandType, WeaponSlotType weaponSlot, Object* obj, Int maxShotsToFire);
	UnsignedInt newDozerConstructCommand(CommandType commandType, const ThingTemplate *thing, const Coord3D *pos1, const Coord3D *pos2, Real angle);
	UnsignedInt newDoSpecialCommand(CommandType commandType, UnsignedInt specialPowerID, UnsignedInt commandOptions);
	UnsignedInt newDoSpecialAtLocCommand(CommandType commandType, UnsignedInt specialPowerID, const Coord3D *location, Real angle, const Object *objectInWay, UnsignedInt commandOptions);
	UnsignedInt newDoSpecialAtObjCommand(CommandType commandType, UnsignedInt specialPowerID, Object *target, UnsignedInt commandOptions);
	UnsignedInt newSwitchWeaponCommand(CommandType commandType, WeaponSlotType weaponSlot);

	// ShigreUi 01/10/2026 only do it on newly created command
	Bool includeUnitInCommand(UnsignedInt commandNodeID, Object *obj);
	// ShigreUi 01/10/2026 Moving comand include reverse move/attack move/etc, only do it on newly created command
	Bool includeUnitInTypicalMovingCommand(UnsignedInt commandNodeID, Object* obj, Coord3D objDest);

	inline Bool isTypicalMovingCommand(CommandType commandType);
	inline Bool isEndCommand(CommandType commandType);
	inline Bool hasEndCommand(Object *obj);
	inline Bool isLocatableCommand(UnsignedInt commandNodeID);

	void cleanUpEmptyCommand(UnsignedInt commandNodeID);
	UnsignedInt getFirstLocatableCommand(Object *obj);
	UnsignedInt getNextLocatableCommand(Object *obj, UnsignedInt commandNodeID);
	Bool getCommandLoc(UnsignedInt commandNodeID, Coord3D &loc);

	Bool isStillAwaiting(Object *obj);
	void proceedNextCommand(Object *obj);
	inline void prepareCommandForVerification(Object *obj);
	Bool dozerPendingTask(Object *obj);
	Bool oclPending(Object *obj);
	Bool terminateDozerPending(Object *obj);
	Bool terminateOCLPending(Object *obj);
	Bool verifyOCLPendingCommand(Object *obj);
	Bool verifyObjectCommand(Object *obj);
	Bool unitPendingCommand(Object *obj);
	Bool unitOnHeld(Object *obj);
	Bool moveToAndEvacPending(Object *obj);
	Bool verifyExitCommand(Object *objWantToExit, Object *objectContainingExiter);
	Bool terminateVerifiedCommand(Object *obj);
	Bool confirmExitCommand(Object *objWantToExit, Object *objectContainingExiter);
	Bool terminateOCLVerifiedCommand(Object *obj);
	Bool verifyFollowPathCommand(Object *obj);
	Bool terminateVerifiedFollowPathCommand(Object* obj);

	UnsignedInt removeCurrentCommand(Object *obj, Bool doCleanUp = true);
	void updateConstructionCount(Object *obj, Int delta);
	Int getConstructionCount(Object *obj);

	Bool isCurrentExitCommand(Object *obj);
	Bool isExitCommandComplete(Object *obj);

protected:
	CommandNode *newCommandNode(CommandType commandType);
	CommandNode *findCommandByID(UnsignedInt commandNodeID);
	CommandNode *nextCommand(Object *obj, CommandNode *cmdNode);

	void doCurrentCommand(Object *obj, CommandVerifyStatus cvs);
};

extern CommandSequence* TheCommandSequence;
