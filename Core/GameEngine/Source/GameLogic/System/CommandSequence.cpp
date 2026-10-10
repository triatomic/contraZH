// FILE: CommandSequence.cpp /////////////////////////////////////////////////////////////////////////////////
// Author: ShigureUi, September 2026
// Desc:   Advanced waypoint system
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "GameLogic/CommandSequence.h"

//-----------------------------------------------------------------------------
//         Public Data
//-----------------------------------------------------------------------------
CommandSequence *TheCommandSequence = nullptr;  ///< the command sequence singleton

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CommandSequence::CommandSequence()
{
}

CommandSequence::~CommandSequence()
{
}

void CommandSequence::init()
{
}

void CommandSequence::reset()
{
	IDToNodeMap::iterator it;
	for (it = m_allNodes.begin(); it != m_allNodes.end(); it++)
	{
		delete it->second;
	}
	m_allNodes.clear();
	m_objectCurNLastCommand.clear();
	m_uniqueID = 0;
	m_commandNodeCount = 0;
}

void CommandSequence::update()
{
}

void CommandSequence::crc(Xfer* xfer)
{
}

void CommandSequence::xfer(Xfer* xfer)
{
	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);

	xfer->xferUnsignedInt(&m_uniqueID);
	xfer->xferInt(&m_commandNodeCount);

	if (xfer->getXferMode() == XFER_LOAD)
	{
		for (Int i = 0; i < m_commandNodeCount; i++)
		{
			UnsignedInt nodeID;
			xfer->xferUnsignedInt(&nodeID);

			CommandNode *cmdNode = new CommandNode();
			m_allNodes.insert(std::make_pair(nodeID, cmdNode));

			xferCommandNode(xfer, cmdNode);
		}

		Int cnt;
		ObjectID objID;
		UnsignedInt curCmd, lastCmd;

		xfer->xferInt(&cnt);

		for (Int i = 0; i < cnt; i++)
		{
			xfer->xferObjectID(&objID);
			xfer->xferUnsignedInt(&curCmd);
			xfer->xferUnsignedInt(&lastCmd);
			m_objectCurNLastCommand.insert(std::make_pair(objID, std::make_pair(curCmd, lastCmd)));
		}
	}
	else
	{
		for (IDToNodeMap::iterator it = m_allNodes.begin(); it != m_allNodes.end(); it++)
		{
			CommandNode *cmdNode = it->second;
			xfer->xferUnsignedInt(&cmdNode->m_nodeID);

			xferCommandNode(xfer, cmdNode);
		}

		Int cnt;
		ObjectID objID;

		xfer->xferInt(&cnt);

		for (ObjectCommandMap::iterator it = m_objectCurNLastCommand.begin(); it != m_objectCurNLastCommand.end(); it++)
		{
			objID = it->first;
			xfer->xferObjectID(&objID);
			xfer->xferUnsignedInt(&it->second.first);
			xfer->xferUnsignedInt(&it->second.second);
		}
	}
}

void CommandSequence::loadPostProcess()
{
}

void CommandSequence::xferCommandNode(Xfer* xfer, CommandNode* cmdNode)
{
	xfer->xferInt((Int*)&cmdNode->m_commandType);

	switch (cmdNode->m_commandType)
	{
		// no argument
	case COMMAND_DO_EVACUATE:
	case COMMAND_INTERNET_HACK:
	case COMMAND_TOGGLE_OVERCHARGE:
	case COMMAND_TOGGLE_HOLD_FIRE:
	case COMMAND_TOGGLE_DEPLOY:
		break;
		// single pos
	case COMMAND_DO_MOVETO:
	case COMMAND_DO_REVERSE_MOVETO:
	case COMMAND_DO_GUARD_POSITION:
	{
		xfer->xferCoord3D(&cmdNode->m_loc);
		break;
	}
	// single target
	case COMMAND_DO_GUARD_OBJECT:
	case COMMAND_GET_REPAIRED:
	case COMMAND_GET_HEALED:
	case COMMAND_ENTER:
	case COMMAND_DO_EXIT:
	case COMMAND_DO_REPAIR:
	case COMMAND_DOCK:
	{
		xfer->xferObjectID(&cmdNode->m_targetID);
		break;
	}
	// pos and target
	case COMMAND_DO_COMBATDROP:
	{
		xfer->xferCoord3D(&cmdNode->m_loc);
		xfer->xferObjectID(&cmdNode->m_targetID);
		break;
	}
	// pos and maxshots
	case COMMAND_DO_ATTACK_MOVETO:
	{
		xfer->xferCoord3D(&cmdNode->m_loc);
		xfer->xferInt(&cmdNode->m_maxShotsToFire);
		break;
	}
	// do weapon at location
	case COMMAND_DO_WEAPON:
	case COMMAND_DO_WEAPON_AT_LOCATION:
	{
		xfer->xferInt((Int*)&cmdNode->m_weaponSlot);
		xfer->xferCoord3D(&cmdNode->m_loc);
		xfer->xferInt(&cmdNode->m_maxShotsToFire);
		xfer->xferBool(&cmdNode->m_releaseAfter);
		break;
	}
	// do attack object including do weapon at object
	case COMMAND_DO_ATTACK_OBJECT:
	case COMMAND_DO_FORCE_ATTACK_OBJECT:
	{
		xfer->xferInt((Int*)&cmdNode->m_weaponSlot);
		xfer->xferObjectID(&cmdNode->m_targetID);
		xfer->xferInt(&cmdNode->m_maxShotsToFire);
		break;
	}
	// dozer construct
	case COMMAND_DOZER_CONSTRUCT:
	case COMMAND_DOZER_CONSTRUCT_LINE:
	{
		xfer->xferCoord3D(&cmdNode->m_loc);
		xfer->xferUnsignedShort(&cmdNode->m_templateID);
		xfer->xferReal(&cmdNode->m_angle);
		if (cmdNode->m_commandType == COMMAND_DOZER_CONSTRUCT_LINE)
			xfer->xferCoord3D(&cmdNode->m_loc2);
		break;
	}
	// do special
	case COMMAND_DO_SPECIAL_POWER:
	{
		xfer->xferUnsignedInt(&cmdNode->m_specialPowerID);
		xfer->xferUnsignedInt(&cmdNode->m_specialOptions);
		break;
	}
	// do special at location
	case COMMAND_DO_SPECIAL_POWER_AT_POSITION:
	case COMMAND_DO_MOVING_SPECIAL_POWER_AT_POSITION:
	{
		xfer->xferUnsignedInt(&cmdNode->m_specialPowerID);
		xfer->xferCoord3D(&cmdNode->m_loc);
		xfer->xferReal(&cmdNode->m_angle);
		xfer->xferObjectID(&cmdNode->m_targetID);
		xfer->xferUnsignedInt(&cmdNode->m_specialOptions);
		break;
	}
	// do special at object
	case COMMAND_DO_SPECIAL_POWER_AT_OBJECT:
	{
		xfer->xferUnsignedInt(&cmdNode->m_specialPowerID);
		xfer->xferObjectID(&cmdNode->m_targetID);
		xfer->xferUnsignedInt(&cmdNode->m_specialOptions);
		break;
	}
	// switch weapon
	case COMMAND_SWITCH_WEAPON:
	{
		xfer->xferInt((Int*)&cmdNode->m_weaponSlot);
		break;
	}
	}

	if (xfer->getXferMode() == XFER_LOAD)
	{
		Int cnt;
		ObjectID objID;
		UnsignedInt nextCmd;
		xfer->xferInt(&cnt);
		for (Int i = 0; i < cnt; i++)
		{
			xfer->xferObjectID(&objID);
			xfer->xferUnsignedInt(&nextCmd);

			cmdNode->m_nextCommand.insert(std::make_pair(objID, nextCmd));
		}

		if (isTypicalMovingCommand(cmdNode->m_commandType))
		{
			Coord3D dest;
			xfer->xferInt(&cnt);
			for (Int i = 0; i < cnt; i++)
			{
				xfer->xferObjectID(&objID);
				xfer->xferCoord3D(&dest);

				cmdNode->m_allocatedDest.insert(std::make_pair(objID, dest));
			}
		}
	}
	else
	{
		Int cnt;
		ObjectID objID;
		cnt = cmdNode->m_nextCommand.size();
		xfer->xferInt(&cnt);

		for (ObjectToNodeMap::iterator it = cmdNode->m_nextCommand.begin(); it != cmdNode->m_nextCommand.end(); it++)
		{
			objID = it->first;
			xfer->xferObjectID(&objID);
			xfer->xferUnsignedInt(&it->second);
		}

		if (isTypicalMovingCommand(cmdNode->m_commandType))
		{
			cnt = cmdNode->m_allocatedDest.size();
			xfer->xferInt(&cnt);

			for (ObjectToLocMap::iterator it = cmdNode->m_allocatedDest.begin(); it != cmdNode->m_allocatedDest.end(); it++)
			{
				objID = it->first;
				xfer->xferObjectID(&objID);
				xfer->xferCoord3D(&it->second);
			}
		}
	}
}

Bool CommandSequence::hasAnyCommand(Object *obj)
{
	return getCurrentCommand(obj);
}

void CommandSequence::clearObjectCommand(Object *obj)
{
	while (removeCurrentCommand(obj));
}

CommandSequence::CommandNode *CommandSequence::newCommandNode(CommandType commandType)
{
	m_uniqueID++;
	CommandNode *commandNode = new CommandNode();
	commandNode->m_commandType = commandType;
	commandNode->m_nodeID = m_uniqueID;
	m_allNodes.insert(std::make_pair(m_uniqueID, commandNode));
	m_commandNodeCount++;
	return commandNode;
}


UnsignedInt CommandSequence::newNoArgumentCommand(CommandType commandType)
{
	switch (commandType)
	{
	case COMMAND_DO_EVACUATE:
	case COMMAND_INTERNET_HACK:
	case COMMAND_TOGGLE_OVERCHARGE:
	case COMMAND_TOGGLE_HOLD_FIRE:
	case COMMAND_TOGGLE_DEPLOY:
		break;
	default:
		return 0;
	}
	CommandNode* commandNode = newCommandNode(commandType);
	return commandNode->m_nodeID;
}

//ShigureUi 07/10/2026 note all sanity check needs a recheck
UnsignedInt CommandSequence::newSinglePosCommand(CommandType commandType, const Coord3D *pos)
{
	if (!pos)
		return 0;

	switch (commandType)
	{
	case COMMAND_DO_MOVETO:
	case COMMAND_DO_REVERSE_MOVETO:
	case COMMAND_DO_GUARD_POSITION:
		break;
	default:
		return 0;
	}
	CommandNode *commandNode = newCommandNode(commandType);
	commandNode->m_loc = *pos;
	return commandNode->m_nodeID;
}

UnsignedInt CommandSequence::newSingleTargetCommand(CommandType commandType, Object *obj)
{
	switch (commandType)
	{
	case COMMAND_DO_GUARD_OBJECT:
	case COMMAND_GET_REPAIRED:
	case COMMAND_GET_HEALED:
	case COMMAND_ENTER:
	case COMMAND_DO_EXIT:
	case COMMAND_DO_REPAIR:
	case COMMAND_DOCK:
		break;
	default:
		return 0;
	}

	CommandNode *commandNode = newCommandNode(commandType);
	commandNode->m_targetID = obj ? obj->getID() : INVALID_ID;
	return commandNode->m_nodeID;
}

UnsignedInt CommandSequence::newTargetNPosCommand(CommandType commandType, Object* obj, const Coord3D* pos)
{
	switch (commandType)
	{
	case COMMAND_DO_COMBATDROP:
		break;
	default:
		return 0;
	}

	CommandNode* commandNode = newCommandNode(commandType);
	commandNode->m_targetID = obj ? obj->getID() : INVALID_ID;
	commandNode->m_loc = *pos;
	return commandNode->m_nodeID;
}

UnsignedInt CommandSequence::newPosNMaxShotsCommand(CommandType commandType, const Coord3D *pos, Int maxShotsToFire)
{
	if (!pos)
		return 0;

	switch (commandType)
	{
	case COMMAND_DO_ATTACK_MOVETO:
		break;
	default:
		return 0;
	}

	CommandNode* commandNode = newCommandNode(commandType);
	commandNode->m_loc = *pos;
	commandNode->m_maxShotsToFire = maxShotsToFire;
	return commandNode->m_nodeID;
}

UnsignedInt CommandSequence::newDoWeaponAtLocCommand(CommandType commandType, WeaponSlotType weaponSlot, const Coord3D *pos, Int maxShotsToFire, Bool releaseAfter)
{
	if (!pos)
		return 0;

	switch (commandType)
	{
	case COMMAND_DO_WEAPON:
	case COMMAND_DO_WEAPON_AT_LOCATION:
		break;
	default:
		return 0;
	}

	CommandNode* commandNode = newCommandNode(commandType);
	commandNode->m_weaponSlot = weaponSlot;
	commandNode->m_loc = *pos;
	commandNode->m_maxShotsToFire = maxShotsToFire;
	commandNode->m_releaseAfter = releaseAfter;
	return commandNode->m_nodeID;
}

UnsignedInt CommandSequence::newDoWeaponAtObjCommand(CommandType commandType, WeaponSlotType weaponSlot, Object* obj, Int maxShotsToFire)
{
	if (!obj)
		return 0;

	switch (commandType)
	{
	case COMMAND_DO_ATTACK_OBJECT:
	case COMMAND_DO_FORCE_ATTACK_OBJECT:
		break;
	default:
		return 0;
	}

	CommandNode* commandNode = newCommandNode(commandType);
	commandNode->m_weaponSlot = weaponSlot;
	commandNode->m_targetID = obj->getID();
	commandNode->m_maxShotsToFire = maxShotsToFire;
	return commandNode->m_nodeID;
}

UnsignedInt CommandSequence::newDozerConstructCommand(CommandType commandType, const ThingTemplate *thing, const Coord3D *loc1, const Coord3D* loc2, Real angle)
{
	if (!thing)
		return 0;

	switch (commandType)
	{
	case COMMAND_DOZER_CONSTRUCT:
	case COMMAND_DOZER_CONSTRUCT_LINE:
		break;
	default:
		return 0;
	}

	if (commandType == COMMAND_DOZER_CONSTRUCT && !loc1)
		return 0;
	else if (commandType == COMMAND_DOZER_CONSTRUCT_LINE && (!loc1 || !loc2))
		return 0;

	CommandNode* commandNode = newCommandNode(commandType);
	commandNode->m_loc = *loc1;
	commandNode->m_templateID = thing->getTemplateID();
	commandNode->m_angle;
	if (commandType == COMMAND_DOZER_CONSTRUCT_LINE)
		commandNode->m_loc2;
	return commandNode->m_nodeID;
}

UnsignedInt CommandSequence::newDoSpecialCommand(CommandType commandType, UnsignedInt specialPowerID, UnsignedInt commandOptions)
{
	switch (commandType)
	{
	case COMMAND_DO_SPECIAL_POWER:
		break;
	default:
		return 0;
	}

	CommandNode* commandNode = newCommandNode(commandType);
	commandNode->m_specialPowerID = specialPowerID;
	commandNode->m_specialOptions = commandOptions;
	return commandNode->m_nodeID;
}

UnsignedInt CommandSequence::newDoSpecialAtLocCommand(CommandType commandType, UnsignedInt specialPowerID, const Coord3D *location, Real angle, const Object* objectInWay, UnsignedInt commandOptions)
{
	switch (commandType)
	{
	case COMMAND_DO_SPECIAL_POWER_AT_POSITION:
	case COMMAND_DO_MOVING_SPECIAL_POWER_AT_POSITION:
		break;
	default:
		return 0;
	}

	CommandNode* commandNode = newCommandNode(commandType);
	commandNode->m_specialPowerID = specialPowerID;
	commandNode->m_loc = *location;
	commandNode->m_angle = angle;
	commandNode->m_targetID = objectInWay == nullptr ? INVALID_ID : objectInWay->getID();
	commandNode->m_specialOptions = commandOptions;
	return commandNode->m_nodeID;
}

UnsignedInt CommandSequence::newDoSpecialAtObjCommand(CommandType commandType, UnsignedInt specialPowerID, Object* target, UnsignedInt commandOptions)
{
	switch (commandType)
	{
	case COMMAND_DO_SPECIAL_POWER_AT_OBJECT:
		break;
	default:
		return 0;
	}

	CommandNode* commandNode = newCommandNode(commandType);
	commandNode->m_specialPowerID = specialPowerID;
	commandNode->m_targetID = target->getID();
	commandNode->m_specialOptions = commandOptions;
	return commandNode->m_nodeID;
}

UnsignedInt CommandSequence::newSwitchWeaponCommand(CommandType commandType, WeaponSlotType weaponSlot)
{
	switch (commandType)
	{
	case COMMAND_SWITCH_WEAPON:
		break;
	default:
		return 0;
	}

	CommandNode* commandNode = newCommandNode(commandType);
	commandNode->m_weaponSlot = weaponSlot;
	return commandNode->m_nodeID;
}

Bool CommandSequence::includeUnitInCommand(UnsignedInt commandNodeID, Object *obj)
{
	if (!obj)
		return false;

	CommandNode *commandNode = getCommandNode(commandNodeID);
	CommandNode *lastCommandNode = getCommandNode(getLastCommand(obj));

	if (lastCommandNode && isEndCommand(lastCommandNode->m_commandType))
		return false;

	commandNode->m_nextCommand.insert(std::make_pair(obj->getID(), 0));

	if (lastCommandNode)
	{
		ObjectToNodeMap::iterator it = lastCommandNode->m_nextCommand.find(obj->getID());
		if (it == lastCommandNode->m_nextCommand.end() || it->second)
			// very wrong
			return false;
		it->second = commandNodeID;

		setObjectCommand(obj, 0, commandNodeID);
	}
	else
	{
		setObjectCommand(obj, commandNodeID, commandNodeID);
	}

	return true;
}

Bool CommandSequence::includeUnitInTypicalMovingCommand(UnsignedInt commandNodeID, Object *obj, Coord3D objDest)
{
	if (!includeUnitInCommand(commandNodeID, obj))
		return false;

	CommandNode *commandNode = getCommandNode(commandNodeID);

	if (isTypicalMovingCommand(commandNode->m_commandType))
		commandNode->m_allocatedDest.insert(std::make_pair(obj->getID(), objDest));

	return true;
}

Bool CommandSequence::isTypicalMovingCommand(CommandType commandType)
{
	switch (commandType)
	{
		case COMMAND_DO_MOVETO:
		case COMMAND_DO_REVERSE_MOVETO:
		case COMMAND_DO_MOVING_SPECIAL_POWER_AT_POSITION:
			return true;
		default:
			return false;
	}
}

inline Bool CommandSequence::isEndCommand(CommandType commandType)
{
	switch (commandType)
	{
	case COMMAND_DO_GUARD_POSITION:
		return true;
	default:
		return false;
	}
}

inline Bool CommandSequence::hasEndCommand(Object *obj)
{
	CommandNode *cmdNode = getCommandNode(getLastCommand(obj));
	if (isEndCommand(cmdNode->m_commandType))
		return true;
	return false;
}

void CommandSequence::cleanUpEmptyCommand(UnsignedInt commandNodeID)
{
	CommandNode *cmdNode = getCommandNode(commandNodeID);

	if (cmdNode->m_nextCommand.empty())
	{
		m_commandNodeCount--;
		delete cmdNode;
	}
}

CommandSequence::CommandNode *CommandSequence::getCommandNode(UnsignedInt commandNodeID)
{
	//if (m_prevNodeID == commandNodeID)
	//	return m_prevNode;

	std::map<UnsignedInt, CommandNode*>::iterator it;
	it = m_allNodes.find(commandNodeID);
	if (it == m_allNodes.end())
		return nullptr;

	return it->second;
}

UnsignedInt CommandSequence::getLastCommand(Object *obj)
{
	if (!obj)
		return 0;
	
	ObjectCommandMap::iterator it = m_objectCurNLastCommand.find(obj->getID());

	if (it == m_objectCurNLastCommand.end())
		return 0;
	else
		return it->second.second;
}

UnsignedInt CommandSequence::getCurrentCommand(Object *obj)
{
	if (!obj)
		return 0;

	ObjectCommandMap::iterator it = m_objectCurNLastCommand.find(obj->getID());

	if (it == m_objectCurNLastCommand.end())
		return 0;
	else
		return it->second.first;
}

UnsignedInt CommandSequence::removeCurrentCommand(Object *obj)
{
	CommandNode *node = getCommandNode(getCurrentCommand(obj));

	ObjectToNodeMap::iterator it = node->m_nextCommand.find(obj->getID());

	if (it == node->m_nextCommand.end())
		return 0;

	UnsignedInt ret = it->second;

	if (it->second)
	{
		setObjectCommand(obj, it->second, 0);
		node->m_nextCommand.erase(it);

		if (isTypicalMovingCommand(node->m_commandType))
		{
			ObjectToLocMap::iterator lit = node->m_allocatedDest.find(obj->getID());
			node->m_allocatedDest.erase(lit);
		}

		if (node->m_nextCommand.empty())
		{
			IDToNodeMap::iterator iit = m_allNodes.find(node->m_nodeID);
			if (iit != m_allNodes.end())
				m_allNodes.erase(iit);
			delete node;
		}
	}

	return ret;
}

void CommandSequence::setObjectCommand(Object *obj, UnsignedInt curCmdID, UnsignedInt lastCmdID)
{
	if (!obj)
		return;

	ObjectCommandMap::iterator it = m_objectCurNLastCommand.find(obj->getID());

	if (it == m_objectCurNLastCommand.end())
		m_objectCurNLastCommand.insert(std::make_pair(obj->getID(), std::make_pair(curCmdID, lastCmdID)));
	else
	{
		if (curCmdID)
			it->second.first = curCmdID;
		if (lastCmdID)
			it->second.second = lastCmdID;
	}
}
