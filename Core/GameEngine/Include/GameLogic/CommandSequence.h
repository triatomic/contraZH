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
	typedef std::map<ObjectID, std::pair<UnsignedInt, UnsignedInt>> ObjectCommandMap;

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
	};

private:
	typedef std::map<UnsignedInt, CommandNode*> IDToNodeMap;
	IDToNodeMap m_allNodes;

	ObjectCommandMap m_objectCurNLastCommand;

	Int m_commandNodeCount;
	UnsignedInt m_uniqueID;
	//UnsignedInt m_prevNodeID;
	//CommandNode *m_prevNode;

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
	void cleanUpEmptyCommand(UnsignedInt commandNodeID);

protected:
	CommandNode *newCommandNode(CommandType commandType);
	CommandNode *getCommandNode(UnsignedInt commandNodeID);
	UnsignedInt getLastCommand(Object *obj);
	UnsignedInt getCurrentCommand(Object *obj);
	UnsignedInt removeCurrentCommand(Object *obj);
	void setObjectCommand(Object *obj, UnsignedInt curCmdID, UnsignedInt lastCmdID);
};

extern CommandSequence* TheCommandSequence;
