#include "PreRTS.h"

#include "GameLogic/CommandSequence.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AIUpdate.h"	// the AIUpdateInterface definition
#include "GameLogic/AI.h"
#include "Common/Xfer.h"

#include "Common/ActionManager.h"
#include "Common/ModelState.h"		// MODELCONDITION_SPECIAL_CHEERING
#include "GameLogic/WeaponSet.h"	// WeaponLockType (LOCKED_TEMPORARILY / LOCKED_PERMANENTLY)
#include "Common/Player.h"			// Player::hasScience / getControllingPlayer
#include "Common/BuildAssistant.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Module/DeployStyleAIUpdate.h"
#include "GameLogic/Module/OverchargeBehavior.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/WeaponSet.h"
#include "GameLogic/Weapon.h"
#include "Common/GameCommon.h"		// ENEMIES
#include "GameLogic/PartitionManager.h"					// aiMoveToPosition, aiAttackObject, aiAttackMoveToPosition, ...

//=============================================================================
// CommandSequence - see triatomic/contraZH issue #122.
//
// P1 scope: the data layer only. The sequences can be built and inspected, but
// nothing executes them yet and nothing is broadcast. Wiring into the logic
// frame (advance) and into the network (commit) happens in later phases.
//=============================================================================

//-----------------------------------------------------------------------------
// The whitelist. Only these GUI command messages may enter a command sequence,
// and this table is also what decides which of them close the sequence.
//
// Deliberately absent: scatter and the other group-shaped commands, formation,
// and anything cyclic (the issue rules those out).
//-----------------------------------------------------------------------------
struct CommandSequenceInfo
{
	GameMessage::Type	cmdType;
	Bool							endCommand;		///< closes the sequence: nothing may follow
};

static const CommandSequenceInfo s_commandSequenceInfo[] =
{
	// --- movement and position ------------------------------------------------
	{ GameMessage::MSG_DO_MOVETO,															FALSE },
	{ GameMessage::MSG_DO_ATTACKMOVETO,												FALSE },
	{ GameMessage::MSG_DO_FORCEMOVETO,												FALSE },
	{ GameMessage::MSG_DO_REVERSE_MOVETO,											FALSE },
	{ GameMessage::MSG_DO_AUTO_FILL,													FALSE },
	{ GameMessage::MSG_SET_RALLY_POINT,												FALSE },

	// --- combat --------------------------------------------------------------
	{ GameMessage::MSG_DO_ATTACK_OBJECT,											FALSE },
	{ GameMessage::MSG_DO_FORCE_ATTACK_OBJECT,									FALSE },
	{ GameMessage::MSG_DO_FORCE_ATTACK_GROUND,									TRUE  },
	{ GameMessage::MSG_DO_GUARD_POSITION,											TRUE  },
	{ GameMessage::MSG_DO_GUARD_OBJECT,											TRUE  },
	{ GameMessage::MSG_DO_WEAPON,														TRUE  },
	{ GameMessage::MSG_DO_WEAPON_AT_LOCATION,									FALSE },	// end only with infinite shots, see isEndCommandType
	{ GameMessage::MSG_DO_WEAPON_AT_OBJECT,										FALSE },
	{ GameMessage::MSG_TOGGLE_FIRE_WEAPON,										FALSE },
	{ GameMessage::MSG_TOGGLE_HOLD_FIRE,											FALSE },
	{ GameMessage::MSG_TOGGLE_DEPLOY,													FALSE },
	{ GameMessage::MSG_TOGGLE_OVERCHARGE,											FALSE },
	{ GameMessage::MSG_SWITCH_WEAPONS,												FALSE },
	{ GameMessage::MSG_DO_CHEER,														FALSE },

	// --- special powers and abilities -------------------------------------------
	{ GameMessage::MSG_DO_SPECIAL_POWER,											FALSE },
	{ GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION,					FALSE },
	{ GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT,						FALSE },
	{ GameMessage::MSG_DO_SPECIAL_POWER_OVERRIDE_DESTINATION,	FALSE },
	{ GameMessage::MSG_DO_SPECIAL_POWER_AT_MULTIPLE_LOCATIONS,	FALSE },
	{ GameMessage::MSG_INTERNET_HACK,													TRUE  },
	{ GameMessage::MSG_DISABLEVEHICLE_HACK,										FALSE },
	{ GameMessage::MSG_STEALCASH_HACK,												FALSE },
	{ GameMessage::MSG_DISABLEBUILDING_HACK,									FALSE },
	{ GameMessage::MSG_SNIPE_VEHICLE,													FALSE },
	{ GameMessage::MSG_CAPTUREBUILDING,												FALSE },
	{ GameMessage::MSG_CONVERT_TO_CARBOMB,										FALSE },
	{ GameMessage::MSG_DO_SALVAGE,														FALSE },

	// --- transport and containment --------------------------------------------
	{ GameMessage::MSG_ENTER,																FALSE },	// explicitly not an end command
	{ GameMessage::MSG_EXIT,																FALSE },
	{ GameMessage::MSG_EVACUATE,														FALSE },
	{ GameMessage::MSG_DOCK,																FALSE },
	{ GameMessage::MSG_COMBATDROP_AT_LOCATION,								FALSE },
	{ GameMessage::MSG_COMBATDROP_AT_OBJECT,									FALSE },

	// --- construction and repair -------------------------------------------------
	{ GameMessage::MSG_DOZER_CONSTRUCT,												FALSE },
	{ GameMessage::MSG_DOZER_CONSTRUCT_LINE,									FALSE },
	{ GameMessage::MSG_DO_REPAIR,														FALSE },
	{ GameMessage::MSG_GET_REPAIRED,												FALSE },
	{ GameMessage::MSG_GET_HEALED,													FALSE },
	{ GameMessage::MSG_RESUME_CONSTRUCTION,										FALSE },
};

static const Int s_commandSequenceInfoCount =
		sizeof( s_commandSequenceInfo ) / sizeof( s_commandSequenceInfo[ 0 ] );

//=============================================================================
// CommandNode
//=============================================================================

CommandNode::CommandNode() :
	m_cmdType( GameMessage::MSG_INVALID ),
	m_targetID( INVALID_ID ),
	m_param( 0 ),
	m_endCommand( FALSE ),
	m_next( nullptr ),
	m_firstChild( nullptr )
{
	m_location.zero();
}

CommandNode::~CommandNode()
{
	// the chain is owned and freed by CommandSequence
}

//=============================================================================
// CommandSequence
//=============================================================================

CommandSequence::CommandSequence( ObjectID subject ) :
	m_subject( subject ),
	m_pendingHead( nullptr ),
	m_pendingTail( nullptr ),
	m_pendingCount( 0 ),
	m_activeHead( nullptr ),
	m_activeTail( nullptr ),
	m_activeCount( 0 ),
	m_current( nullptr ),
	m_currentDispatched( FALSE ),
	m_priorityTargetList( FALSE ),
	m_activeBuildTargetID( INVALID_ID )
{
}

CommandSequence::~CommandSequence()
{
	clear();
}

//-----------------------------------------------------------------------------
CommandNode *CommandSequence::buildNode( GameMessage::Type type, ObjectID targetID,
																				 const Coord3D *pos, Int param, Real angle ) const
{
	CommandNode *node = new CommandNode;
	node->m_cmdType = type;
	node->m_targetID = targetID;
	node->m_param = param;
	node->m_angle = angle;
	node->m_endCommand = CommandSequenceSystem::isEndCommandType( type, param );
	node->m_immediate = CommandSequenceSystem::isImmediateCommand( type );

	if( pos != nullptr )
		node->m_location = *pos;
	else
		node->m_location.zero();

	return node;
}

//-----------------------------------------------------------------------------
void CommandSequence::destroyChain( CommandNode *head )
{
	while( head != nullptr )
	{
		CommandNode *next = head->m_next;
		delete head;
		head = next;
	}
}

//-----------------------------------------------------------------------------
Bool CommandSequence::appendPending( GameMessage::Type type, ObjectID targetID,
																		 const Coord3D *pos, Int param, Real angle )
{
	if( !CommandSequenceSystem::isAllowedCommand( type ) )
		return FALSE;

	// an end command closes the sequence, so nothing may follow it
	if( m_pendingTail != nullptr && m_pendingTail->m_endCommand )
		return FALSE;

	if( m_pendingCount >= COMMAND_SEQUENCE_MAX_NODES_PER_SUBJECT )
		return FALSE;

	CommandNode *node = buildNode( type, targetID, pos, param, angle );

	if( m_pendingTail != nullptr )
		m_pendingTail->m_next = node;
	else
		m_pendingHead = node;

	m_pendingTail = node;
	m_pendingCount++;

	return TRUE;
}

//-----------------------------------------------------------------------------
void CommandSequence::clearPending()
{
	destroyChain( m_pendingHead );
	m_pendingHead = nullptr;
	m_pendingTail = nullptr;
	m_pendingCount = 0;
}

//-----------------------------------------------------------------------------
Bool CommandSequence::commit()
{
	// nothing plotted, nothing to commit
	if( m_pendingHead == nullptr )
		return FALSE;

	// P3: the pending chain is broadcast here as the logic-level messages that
	// build the sequence on every machine. Until then it is simply handed over.
	if( m_activeTail != nullptr )
		m_activeTail->m_next = m_pendingHead;
	else
		m_activeHead = m_pendingHead;

	m_activeTail = m_pendingTail;
	m_activeCount += m_pendingCount;

	if( m_current == nullptr )
	{
		m_current = m_activeHead;
		m_currentDispatched = FALSE;	// fresh head: needs its first dispatch
	}
	// else: a node is already executing (a build under way, an attack being fought).
	// Appending must not re-arm it: re-dispatching a build node would place the
	// building a second time and abandon the construction it interrupts.

	m_pendingHead = nullptr;
	m_pendingTail = nullptr;
	m_pendingCount = 0;

	return TRUE;
}

//-----------------------------------------------------------------------------
void CommandSequence::clear()
{
	clearPending();
	destroyChain( m_activeHead );
	m_activeHead = nullptr;
	m_activeTail = nullptr;
	m_activeCount = 0;
	m_current = nullptr;
	m_currentDispatched = FALSE;
	m_activeBuildTargetID = INVALID_ID;
}

//-----------------------------------------------------------------------------
// Hand the current node to the unit's AI. The mapping mirrors what the
// CommandTranslator does for a live order, with the command source marked as
// the player so the AI treats it with the same priority as a manual order.
//-----------------------------------------------------------------------------
Bool CommandSequence::dispatchCurrent( Object *subject )
{
	AIUpdateInterface *ai = ( subject != nullptr ) ? subject->getAIUpdateInterface() : nullptr;
	if( ai == nullptr || m_current == nullptr )
		return FALSE;

	m_activeBuildTargetID = INVALID_ID;

	// A plotted attack carries no shot budget of its own: it inherits the no-limit behaviour of
	// the direct orders (GameLogicDispatch passes NO_MAX_SHOTS_LIMIT for every attack), so the
	// unit keeps attacking until the target is gone rather than firing once and standing down.
	// Weapon-slot nodes store the slot in the param, never a shot count.
	const Int maxShots = NO_MAX_SHOTS_LIMIT;
	Object *target = ( m_current->m_targetID != INVALID_ID && TheGameLogic != nullptr )
			? TheGameLogic->findObjectByID( m_current->m_targetID ) : nullptr;
	const Coord3D *loc = &m_current->m_location;

	switch( m_current->m_cmdType )
	{
		case GameMessage::MSG_DO_MOVETO:
			ai->aiMoveToPosition( &m_current->m_location, CMD_FROM_PLAYER );
			break;

		case GameMessage::MSG_DO_FORCEMOVETO:
			ai->aiMoveToPositionEvenIfSleeping( &m_current->m_location, CMD_FROM_PLAYER );
			break;

		case GameMessage::MSG_DO_REVERSE_MOVETO:
			ai->aiReverseMoveToPosition( &m_current->m_location, CMD_FROM_PLAYER );
			break;

		case GameMessage::MSG_DO_ATTACKMOVETO:
			ai->aiAttackMoveToPosition( &m_current->m_location, maxShots, CMD_FROM_PLAYER );
			break;

		case GameMessage::MSG_DO_ATTACK_OBJECT:
			if( target != nullptr )
			{
				ai->aiAttackObject( target, maxShots, CMD_FROM_PLAYER );
			}
			else if( m_current->m_targetID == INVALID_ID )
			{
				// the target turned into something else here (see onTargetInvalid): keep
				// hitting the spot rather than dropping the order
				ai->aiAttackPosition( loc, maxShots, CMD_FROM_PLAYER );
			}
			else
			{
				return FALSE;
			}
			break;

		case GameMessage::MSG_DO_FORCE_ATTACK_OBJECT:
			if( target != nullptr )
			{
				ai->aiForceAttackObject( target, maxShots, CMD_FROM_PLAYER );
			}
			else if( m_current->m_targetID == INVALID_ID )
			{
				ai->aiAttackPosition( loc, maxShots, CMD_FROM_PLAYER );
			}
			else
			{
				return FALSE;
			}
			break;

		// --- stationary combat -----------------------------------------------------
		case GameMessage::MSG_DO_FORCE_ATTACK_GROUND:
			ai->aiAttackPosition( loc, maxShots, CMD_FROM_PLAYER );
			break;

		case GameMessage::MSG_DO_GUARD_POSITION:
			ai->aiGuardPosition( loc, GUARDMODE_NORMAL, CMD_FROM_PLAYER );
			break;

		case GameMessage::MSG_DO_GUARD_OBJECT:
			if( target != nullptr )
				ai->aiGuardObject( target, GUARDMODE_NORMAL, CMD_FROM_PLAYER );
			else if( m_current->m_targetID == INVALID_ID )
				ai->aiGuardPosition( loc, GUARDMODE_NORMAL, CMD_FROM_PLAYER );	// same spot, target transformed
			else
				return FALSE;
			break;

		case GameMessage::MSG_INTERNET_HACK:
			ai->aiHackInternet( CMD_FROM_PLAYER );
			break;

		// --- engineering ------------------------------------------------------------
		case GameMessage::MSG_DO_REPAIR:
			if( target == nullptr )
				return FALSE;
			ai->aiRepair( target, CMD_FROM_PLAYER );
			break;

		case GameMessage::MSG_RESUME_CONSTRUCTION:
			if( target == nullptr )
				return FALSE;
			ai->aiResumeConstruction( target, CMD_FROM_PLAYER );
			break;

		case GameMessage::MSG_GET_REPAIRED:
			if( target == nullptr )
				return FALSE;
			ai->aiGetRepaired( target, CMD_FROM_PLAYER );
			break;

		case GameMessage::MSG_GET_HEALED:
			if( target == nullptr )
				return FALSE;
			ai->aiGetHealed( target, CMD_FROM_PLAYER );
			break;

		// --- transport --------------------------------------------------------------
		case GameMessage::MSG_ENTER:
			if( target == nullptr )
				return FALSE;
			ai->aiEnter( target, CMD_FROM_PLAYER );
			break;

		case GameMessage::MSG_DOCK:
			if( target == nullptr )
				return FALSE;
			ai->aiDock( target, CMD_FROM_PLAYER );
			break;

		case GameMessage::MSG_EXIT:
			if( target == nullptr )
				return FALSE;
			ai->aiExit( target, CMD_FROM_PLAYER );
			break;

		case GameMessage::MSG_EVACUATE:
			ai->aiEvacuate( FALSE, CMD_FROM_PLAYER );
			break;

		// --- stance and state toggles ------------------------------------------------
		// Body copied from AIGroup::groupToggle* in AIGroup.cpp, minus the group walk.
		case GameMessage::MSG_TOGGLE_HOLD_FIRE:
			ai->setHoldingFire( !ai->isHoldingFire() );
			break;

		case GameMessage::MSG_TOGGLE_DEPLOY:
		{
			DeployStyleAIUpdate *deployAI = ai->getDeployStyleAIUpdate();
			if( deployAI == nullptr )
				return FALSE;
			deployAI->toggleManualDeploy();
			break;
		}

		case GameMessage::MSG_TOGGLE_OVERCHARGE:
		{
			Bool toggled = FALSE;
			for( BehaviorModule **bmi = subject->getBehaviorModules(); *bmi != nullptr; ++bmi )
			{
				OverchargeBehaviorInterface *obi = ( *bmi )->getOverchargeBehaviorInterface();
				if( obi != nullptr )
				{
					obi->toggle();
					toggled = TRUE;
				}
			}
			if( !toggled )
				return FALSE;
			break;
		}

		// --- special powers ----------------------------------------------------------
		// Shape copied from AIGroup::groupDoSpecialPower*: resolve the template, check the
		// science, ask the action manager whether it is legal, then call the module.
		case GameMessage::MSG_DO_SPECIAL_POWER:
		case GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION:
		case GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT:
		{
			const SpecialPowerTemplate *spTemplate =
					TheSpecialPowerStore->findSpecialPowerTemplateByID( (UnsignedInt)m_current->m_param );
			if( spTemplate == nullptr )
				return FALSE;

			if( spTemplate->getRequiredScience() != SCIENCE_INVALID &&
					subject->getControllingPlayer()->hasScience( spTemplate->getRequiredScience() ) == FALSE )
				return FALSE;

			SpecialPowerModuleInterface *mod = subject->getSpecialPowerModule( spTemplate );
			if( mod == nullptr )
				return FALSE;

			const UnsignedInt options = 0;

			if( m_current->m_cmdType == GameMessage::MSG_DO_SPECIAL_POWER )
			{
				if( !TheActionManager->canDoSpecialPower( subject, spTemplate, CMD_FROM_PLAYER, options ) )
					return FALSE;
				mod->doSpecialPower( options );
			}
			else if( m_current->m_cmdType == GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION )
			{
				if( !TheActionManager->canDoSpecialPowerAtLocation( subject, loc, CMD_FROM_PLAYER, spTemplate, nullptr, options ) )
					return FALSE;
				mod->doSpecialPowerAtLocation( loc, 0.0f, options );
			}
			else
			{
				if( target == nullptr )
					return FALSE;
				if( !TheActionManager->canDoSpecialPowerAtObject( subject, target, CMD_FROM_PLAYER, spTemplate, options ) )
					return FALSE;
				mod->doSpecialPowerAtObject( target, options );
			}

			// the power was used, so whatever cover it gave is gone
			subject->friend_setUndetectedDefector( FALSE );
			break;
		}

		// --- misc ------------------------------------------------------------------------
		case GameMessage::MSG_DO_CHEER:
			// Copied from AIGroup::groupCheer -- it is one call per member anyway: a three
			// second special model condition, purely cosmetic.
			subject->setSpecialModelConditionState( MODELCONDITION_SPECIAL_CHEERING, LOGICFRAMES_PER_SECOND * 3 );
			break;

		// --- weapons --------------------------------------------------------------------
		// Body copied from AIGroup::groupToggleFireWeapon / setWeaponLockForGroup: the group
		// versions just walk the members and call the same Object methods.
		case GameMessage::MSG_SWITCH_WEAPONS:
			if( !subject->setWeaponLock( (WeaponSlotType)m_current->m_param, LOCKED_PERMANENTLY ) )
				return FALSE;
			break;

		case GameMessage::MSG_TOGGLE_FIRE_WEAPON:
		{
			const WeaponSlotType slot = (WeaponSlotType)m_current->m_param;
			if( subject->isFiringWeaponSlot( slot ) )
				subject->stopFiringWeaponSlot( slot );
			else if( subject->setWeaponLock( slot, LOCKED_TEMPORARILY ) )
				ai->aiAttackPosition( nullptr, maxShots, CMD_FROM_PLAYER );	// nullptr = fire from where we stand
			else
				return FALSE;
			break;
		}

		case GameMessage::MSG_DO_WEAPON:
		{
			const WeaponSlotType slot = (WeaponSlotType)m_current->m_param;
			if( !subject->setWeaponLock( slot, LOCKED_TEMPORARILY ) )
				return FALSE;
			ai->aiAttackPosition( nullptr, maxShots, CMD_FROM_PLAYER );
			break;
		}

		case GameMessage::MSG_DO_WEAPON_AT_LOCATION:
		{
			const WeaponSlotType slot = (WeaponSlotType)m_current->m_param;
			if( !subject->setWeaponLock( slot, LOCKED_TEMPORARILY ) )
				return FALSE;
			ai->aiAttackPosition( loc, maxShots, CMD_FROM_PLAYER );
			break;
		}

		case GameMessage::MSG_DO_WEAPON_AT_OBJECT:
		{
			if( target == nullptr )
				return FALSE;
			const WeaponSlotType slot = (WeaponSlotType)m_current->m_param;
			if( !subject->setWeaponLock( slot, LOCKED_TEMPORARILY ) )
				return FALSE;
			ai->aiAttackObject( target, maxShots, CMD_FROM_PLAYER );
			break;
		}

		// --- combat drop ----------------------------------------------------------------
		case GameMessage::MSG_COMBATDROP_AT_OBJECT:
			if( target == nullptr )
				return FALSE;
			ai->aiCombatDrop( target, *target->getPosition(), CMD_FROM_PLAYER );
			break;

		case GameMessage::MSG_COMBATDROP_AT_LOCATION:
			ai->aiCombatDrop( nullptr, *loc, CMD_FROM_PLAYER );
			break;

		// --- construction --------------------------------------------------------------
		// Same call the live order makes in GameLogic::onDozerConstruct: place the building
		// now and let the dozer build it. The node's param carries the template id.
		case GameMessage::MSG_DOZER_CONSTRUCT:
		{
			const ThingTemplate *place = TheThingFactory->findByTemplateID( m_current->m_param );
			if( place == nullptr )
				return FALSE;
			// P4: remember the foundation so completion is judged on the BUILDING (see
			// update()), not on the dozer momentarily reporting idle between orders.
			Object *foundation = TheBuildAssistant->buildObjectNow( subject, place, loc, m_current->getAngle(), subject->getControllingPlayer() );
			m_activeBuildTargetID = ( foundation != nullptr ) ? foundation->getID() : INVALID_ID;
			break;
		}

		case GameMessage::MSG_DOZER_CONSTRUCT_LINE:
		{
			const ThingTemplate *place = TheThingFactory->findByTemplateID( m_current->m_param );
			if( place == nullptr )
				return FALSE;

			// A line order needs two ends but a node only carries one point, so the unit's own
			// position is the near end -- which is where a wall is dragged from in practice.
			const Coord3D startPos = *subject->getPosition();
			TheBuildAssistant->buildObjectLineNow( subject, place, &startPos, loc, 0.0f, subject->getControllingPlayer() );
			break;
		}

		default:
			// not wired up yet (weapon / special power / dozer construct / hack variants ...):
			// report the miss so the node is skipped rather than stalling the sequence.
			return FALSE;
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
void CommandSequence::update( Object *subject )
{
	// A priority list is a standing order, not an errand: nothing is dispatched from it and
	// there is nothing to advance. getPriorityTarget() reads it instead (issue R7).
	if( m_priorityTargetList )
		return;

	if( m_current == nullptr || subject == nullptr )
		return;

	AIUpdateInterface *ai = subject->getAIUpdateInterface();
	if( ai == nullptr )
		return;

	// TheSuperHackers @feature issue R6: a target that climbed into a tunnel, a transport
	// or a building cannot be chased any more, and the order must not keep leaking its
	// whereabouts. Treated as gone -- unlike a transformation, which onTargetInvalid is
	// told about separately and which keeps the order alive.
	if( m_current->m_targetID != INVALID_ID && TheGameLogic != nullptr )
	{
		Object *target = TheGameLogic->findObjectByID( m_current->m_targetID );
		if( target != nullptr && target->getContainedBy() != nullptr )
		{
			const Coord3D lastPos = *target->getPosition();
			onTargetInvalid( m_current->m_targetID, TRUE, &lastPos );
			return;
		}
	}

	if( !m_currentDispatched )
	{
		if( dispatchCurrent( subject ) )
		{
			// A toggle has no duration: it is done the moment it landed, so move straight on
			// rather than waiting for a unit that may never go idle in the way we expect.
			if( m_current->m_immediate )
			{
				m_current = m_current->m_next;
				m_currentDispatched = FALSE;
				return;
			}

			m_currentDispatched = TRUE;
			return;		// judge completion from the *next* frame on, or an idle unit would skip the order
		}

		// the command cannot run at all (target gone, or a type not wired up yet):
		// drop it and let the next frame try the following one.
		m_current = m_current->m_next;
		m_currentDispatched = FALSE;
		return;
	}

	// P3 completion rule: the unit went idle, so this command is done and the next
	// one may start. P4 refines this per command type (a build is finished when the
	// structure exists, not merely when the dozer pauses).
	if( ai->isIdle() )
	{
		// P4: a build node is done when the FOUNDATION is fully constructed (percent goes
		// negative on completion), not when the dozer briefly reports idle after laying it.
		// While the structure is still 0..100% the dozer must stay here and finish it.
		if( m_current->m_cmdType == GameMessage::MSG_DOZER_CONSTRUCT && m_activeBuildTargetID != INVALID_ID )
		{
			Object *foundation = ( TheGameLogic != nullptr ) ? TheGameLogic->findObjectByID( m_activeBuildTargetID ) : nullptr;
			if( foundation != nullptr && foundation->getConstructionPercent() >= 0.0f )
				return;	// still under construction -- keep the dozer on this node
		}

		// P4: every ability driven by a SpecialAbilityUpdate parks the AI in idle for its whole
		// run (the module even calls aiIdle itself at initiation) and raises the canonical
		// "using ability" status for that time -- the same signal the mood scan reads to veto
		// auto-acquisition. Reading the status instead of matching skill types makes the hold
		// universal: any present or future bypass ability keeps its node until it is done,
		// whatever its SpecialPowerType, and abilities without such a module never raise the
		// status and advance as before.
		if( subject->testStatus( OBJECT_STATUS_IS_USING_ABILITY ) )
			return;	// an ability is preparing or executing -- hold the node

		// P4: the status above is cleared the moment a preparation ends, but a packing ability
		// keeps running after that (unpack abilities re-pack on success AND on failure). Match
		// the node's own skill type here so that tail is also waited out; the type is precise,
		// so unrelated or persistent abilities never stall the chain.
		if( m_current->m_cmdType == GameMessage::MSG_DO_SPECIAL_POWER ||
				m_current->m_cmdType == GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION ||
				m_current->m_cmdType == GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT )
		{
			const SpecialPowerTemplate *spTemplate =
					TheSpecialPowerStore->findSpecialPowerTemplateByID( (UnsignedInt)m_current->m_param );
			if( spTemplate != nullptr )
			{
				SpecialAbilityUpdate *ability = subject->findSpecialAbilityUpdate( spTemplate->getSpecialPowerType() );
				if( ability != nullptr && ability->isActive() )
					return;	// the hack/capture/steal is still running -- hold the node
			}
		}

		m_current = m_current->m_next;
		m_currentDispatched = FALSE;
		m_activeBuildTargetID = INVALID_ID;
	}
}

//-----------------------------------------------------------------------------
// Rebuilds a chain without the nodes that pointed at targetID.
//-----------------------------------------------------------------------------
CommandNode *CommandSequence::removeTargetFromChain( CommandNode *head, ObjectID targetID,
																		 Int *countOut, CommandNode **tailOut )
{
	CommandNode *newHead = nullptr;
	CommandNode *newTail = nullptr;
	Int kept = 0;

	for( CommandNode *node = head; node != nullptr; )
	{
		CommandNode *next = node->m_next;

		if( node->m_targetID == targetID )
		{
			node->m_next = nullptr;
			delete node;
		}
		else
		{
			node->m_next = nullptr;
			if( newTail != nullptr )
				newTail->m_next = node;
			else
				newHead = node;
			newTail = node;
			kept++;
		}

		node = next;
	}

	if( countOut != nullptr )
		*countOut = kept;
	if( tailOut != nullptr )
		*tailOut = newTail;

	return newHead;
}

//-----------------------------------------------------------------------------
void CommandSequence::onTargetInvalid( ObjectID targetID, Bool wasKilled, const Coord3D *lastKnownPos )
{
	if( targetID == INVALID_ID )
		return;

	// The target is gone but the order is not dead: it changed into something else on
	// the spot (a Chinook that finished deploying into a Fire Base, for instance). Keep
	// the nodes and remember the place, so "attack that" keeps hitting what is there now.
	if( !wasKilled )
	{
		CommandNode *chains[ 2 ] = { m_pendingHead, m_activeHead };
		for( Int c = 0; c < 2; ++c )
		{
			for( CommandNode *node = chains[ c ]; node != nullptr; node = node->m_next )
			{
				if( node->m_targetID != targetID )
					continue;

				if( lastKnownPos != nullptr )
					node->m_location = *lastKnownPos;

				node->m_targetID = INVALID_ID;	// from now on this is a position order
			}
		}
		return;
	}

	m_pendingHead = removeTargetFromChain( m_pendingHead, targetID, &m_pendingCount, &m_pendingTail );
	m_activeHead  = removeTargetFromChain( m_activeHead,  targetID, &m_activeCount,  &m_activeTail );

	// the node being executed may have just been dropped, which would leave m_current
	// dangling. Restart from what is left rather than guessing where we were.
	Bool currentAlive = FALSE;
	for( CommandNode *node = m_activeHead; node != nullptr; node = node->m_next )
	{
		if( node == m_current )
		{
			currentAlive = TRUE;
			break;
		}
	}

	if( !currentAlive )
	{
		m_current = m_activeHead;
		m_currentDispatched = FALSE;
	}
}

//=============================================================================
// CommandSequenceSystem
//=============================================================================

CommandSequenceSystem *TheCommandSequence = nullptr;

//-----------------------------------------------------------------------------
CommandSequenceSystem::CommandSequenceSystem()
{
}

//-----------------------------------------------------------------------------
CommandSequenceSystem::~CommandSequenceSystem()
{
	reset();
}

//-----------------------------------------------------------------------------
void CommandSequenceSystem::init()
{
}

//-----------------------------------------------------------------------------
void CommandSequenceSystem::reset()
{
	for( std::map< ObjectID, CommandSequence * >::iterator it = m_sequences.begin();
			 it != m_sequences.end(); ++it )
	{
		delete it->second;
	}
	m_sequences.clear();
}

//-----------------------------------------------------------------------------
void CommandSequenceSystem::update()
{
	if( TheGameLogic == nullptr || !TheGameLogic->isInGame() )
		return;

	std::map< ObjectID, CommandSequence * >::iterator it = m_sequences.begin();
	while( it != m_sequences.end() )
	{
		CommandSequence *seq = it->second;
		Object *subject = TheGameLogic->findObjectByID( seq->getSubject() );

		// the subject is gone: the whole sequence goes with it
		if( subject == nullptr || subject->isEffectivelyDead() )
		{
			delete seq;
			it = m_sequences.erase( it );
			continue;
		}

		seq->update( subject );

		// the chain ran dry: the sequence is done -- remove it, so isExecutingSequence()
		// stops reporting the unit as busy and the memory does not linger.
		if( !seq->isPriorityTargetList() && seq->isFinished() )
		{
			delete seq;
			it = m_sequences.erase( it );
			continue;
		}

		++it;
	}
}

//-----------------------------------------------------------------------------
CommandSequence *CommandSequenceSystem::createSequence( ObjectID subject )
{
	if( subject == INVALID_ID )
		return nullptr;

	std::map< ObjectID, CommandSequence * >::iterator it = m_sequences.find( subject );
	if( it != m_sequences.end() )
		return it->second;

	CommandSequence *seq = new CommandSequence( subject );
	m_sequences[ subject ] = seq;
	return seq;
}

//-----------------------------------------------------------------------------
CommandSequence *CommandSequenceSystem::getSequence( ObjectID subject ) const
{
	std::map< ObjectID, CommandSequence * >::const_iterator it = m_sequences.find( subject );
	if( it == m_sequences.end() )
		return nullptr;

	return it->second;
}

//-----------------------------------------------------------------------------
void CommandSequenceSystem::destroySequence( ObjectID subject )
{
	std::map< ObjectID, CommandSequence * >::iterator it = m_sequences.find( subject );
	if( it == m_sequences.end() )
		return;

	delete it->second;
	m_sequences.erase( it );
}

//-----------------------------------------------------------------------------
Bool CommandSequenceSystem::appendCommand( ObjectID subject, GameMessage::Type type,
																					 ObjectID targetID, const Coord3D *pos, Int param )
{
	if( subject == INVALID_ID )
		return FALSE;

	CommandSequence *seq = getSequence( subject );
	if( seq == nullptr )
	{
		if( !isAllowedCommand( type ) )
			return FALSE;

		seq = createSequence( subject );
	}

	// nothing to keep if the append was refused and we just created it
	Bool ok = seq->appendPending( type, targetID, pos, param );
	if( !ok && seq->getPendingCount() == 0 && seq->getActiveCount() == 0 )
		destroySequence( subject );

	return ok;
}

//-----------------------------------------------------------------------------
// Decodes MSG_COMMAND_SEQUENCE_COMMIT and builds the active sequence. The wire
// format is (subjectID, nodeCount, then per node: type, targetID, location, param, angle),
// which is what SelectionTranslator/InGameUI encodes when the player commits.
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Decodes MSG_COMMAND_SEQUENCE_COMMIT. A message names every unit that shares the chain
// (a group order names all of them, so the nodes travel once) and the logic side gives
// each unit its own copy -- the state has to be per unit even though the traffic is not.
//
// Wire format: subjectCount, subjectID[subjectCount], nodeCount,
//              then nodeCount x (type, targetID, location, param, angle).
//-----------------------------------------------------------------------------
Bool CommandSequenceSystem::onCommitMessage( const GameMessage *msg )
{
	if( msg == nullptr || msg->getArgumentCount() < 3 )
		return FALSE;

	Int arg = 0;

	const Int subjectCount = msg->getArgument( arg++ )->integer;
	if( subjectCount <= 0 || subjectCount > COMMAND_SEQUENCE_MAX_SUBJECTS_PER_MESSAGE )
		return FALSE;

	ObjectID subjects[ COMMAND_SEQUENCE_MAX_SUBJECTS_PER_MESSAGE ];
	for( Int i = 0; i < subjectCount; ++i )
		subjects[ i ] = msg->getArgument( arg++ )->objectID;

	const Int nodeCount = msg->getArgument( arg++ )->integer;
	if( nodeCount < 0 || nodeCount > COMMAND_SEQUENCE_MAX_NODES_PER_SUBJECT )
		return FALSE;

	// read the chain once, then hand every subject its own copy of it
	struct StagedNode
	{
		GameMessage::Type	type;
		ObjectID				target;
		Coord3D					location;
		Int						param;
		Real						angle;
	};

	StagedNode staged[ COMMAND_SEQUENCE_MAX_NODES_PER_SUBJECT ];
	Int stagedCount = 0;

	for( Int i = 0; i < nodeCount && arg + 4 < msg->getArgumentCount(); ++i )
	{
		staged[ stagedCount ].type = (GameMessage::Type)msg->getArgument( arg++ )->integer;
		staged[ stagedCount ].target = msg->getArgument( arg++ )->objectID;
		staged[ stagedCount ].location = msg->getArgument( arg++ )->location;
		staged[ stagedCount ].param = msg->getArgument( arg++ )->integer;
		staged[ stagedCount ].angle = msg->getArgument( arg++ )->real;
		stagedCount++;
	}

	Bool anyCommitted = FALSE;

	for( Int s = 0; s < subjectCount; ++s )
	{
		if( subjects[ s ] == INVALID_ID )
			continue;

		// The commit carries only the nodes plotted in this session, so it APPENDS to what
		// the unit is already working through. Clearing here would drop the node currently
		// being executed -- a build under way would be abandoned and its foundation left
		// half-built. Replacing a running chain is what ordinary orders are for.
		CommandSequence *seq = getSequence( subjects[ s ] );
		if( seq == nullptr )
			seq = createSequence( subjects[ s ] );

		// An immobile defence cannot run an errand queue -- it cannot walk anywhere, build,
		// board or repair. What it can be given is an ordered list of things to kill, so its
		// sequence is kept as a standing priority list instead of an advancing queue (R7).
		{
			Object *subjectObj = ( TheGameLogic != nullptr ) ? TheGameLogic->findObjectByID( subjects[ s ] ) : nullptr;
			seq->setPriorityTargetList( subjectObj != nullptr && subjectObj->isKindOf( KINDOF_IMMOBILE ) );
		}

		for( Int i = 0; i < stagedCount; ++i )
			seq->appendPending( staged[ i ].type, staged[ i ].target, &staged[ i ].location, staged[ i ].param, staged[ i ].angle );

		seq->commit();
		anyCommitted = TRUE;
	}

	return anyCommitted;
}

//-----------------------------------------------------------------------------
void CommandSequenceSystem::onTargetInvalid( ObjectID targetID, Bool wasKilled, const Coord3D *lastKnownPos )
{
	if( targetID == INVALID_ID )
		return;

	for( std::map< ObjectID, CommandSequence * >::iterator it = m_sequences.begin();
			 it != m_sequences.end(); ++it )
	{
		it->second->onTargetInvalid( targetID, wasKilled, lastKnownPos );
	}
}

//-----------------------------------------------------------------------------
// TheSuperHackers @feature issue R7: what should an ordered-targets defence shoot at now?
//
// Walks the unit's standing list in order and returns the first entry that is alive, is an
// enemy, and is currently inside weapon range. Returning null is the whole point of the
// design: it means "nothing I was told to prioritise is in reach", and the caller then runs
// its ordinary target scan, so the tower keeps defending itself normally.
//
// Range is evaluated live rather than at queue time, which is what makes "shoot it as soon as
// it comes into range" work without listening for anything.
//-----------------------------------------------------------------------------
Object *CommandSequenceSystem::getPriorityTarget( Object *unit ) const
{
	if( unit == nullptr || TheGameLogic == nullptr )
		return nullptr;

	CommandSequence *seq = getSequence( unit->getID() );
	if( seq == nullptr || !seq->isPriorityTargetList() )
		return nullptr;

	Weapon *weapon = unit->getCurrentWeapon();
	if( weapon == nullptr )
		return nullptr;

	const Real rangeSqr = sqr( weapon->getAttackRange( unit ) );

	for( CommandNode *node = seq->getActiveHead(); node != nullptr; node = node->getNext() )
	{
		if( node->getCommandType() != GameMessage::MSG_DO_ATTACK_OBJECT )
			continue;

		Object *target = TheGameLogic->findObjectByID( node->getTargetID() );
		if( target == nullptr || target->isEffectivelyDead() )
			continue;

		// only real enemies: an ordered list must never become a way to order your own
		// units or an ally's about
		if( unit->getRelationship( target ) != ENEMIES )
			continue;

		const Real distSqr = ThePartitionManager->getDistanceSquared( unit, target, FROM_CENTER_3D );
		if( distSqr <= rangeSqr )
			return target;
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
Bool CommandSequenceSystem::isExecutingSequence( ObjectID subject ) const
{
	CommandSequence *seq = getSequence( subject );
	if( seq == nullptr )
		return FALSE;

	// A priority target list is a standing order, not work in progress: a defence reading
	// one is still free to fire its power.
	if( seq->isPriorityTargetList() )
		return FALSE;

	return ( seq->getCurrent() != nullptr );
}

//-----------------------------------------------------------------------------
// The player orders that REPLACE a plotted sequence. A unit handed a fresh move,
// attack or build order must not keep firing the remaining nodes of an old route
// afterwards -- that read as "the unit suddenly walks off again when I come back".
// The sequence dispatch itself issues AI calls directly (not messages), so these
// can only ever come from a real player order.
//-----------------------------------------------------------------------------
Bool CommandSequenceSystem::isCancellingPlayerOrder( GameMessage::Type type )
{
	switch( type )
	{
		case GameMessage::MSG_DO_MOVETO:
		case GameMessage::MSG_DO_ATTACKMOVETO:
		case GameMessage::MSG_DO_FORCEMOVETO:
		case GameMessage::MSG_DO_REVERSE_MOVETO:
		case GameMessage::MSG_ADD_WAYPOINT:
		case GameMessage::MSG_DO_ATTACK_OBJECT:
		case GameMessage::MSG_DO_FORCE_ATTACK_OBJECT:
		case GameMessage::MSG_DO_FORCE_ATTACK_GROUND:
		case GameMessage::MSG_DO_GUARD_POSITION:
		case GameMessage::MSG_DO_GUARD_OBJECT:
		case GameMessage::MSG_DO_STOP:
		case GameMessage::MSG_ENTER:
		case GameMessage::MSG_DOCK:
		case GameMessage::MSG_DO_REPAIR:
		case GameMessage::MSG_RESUME_CONSTRUCTION:
		case GameMessage::MSG_DOZER_CONSTRUCT:
		case GameMessage::MSG_DOZER_CONSTRUCT_LINE:
		case GameMessage::MSG_DOZER_WAYPOINT_BUILD:
			return TRUE;

		default:
			return FALSE;
	}
}

//-----------------------------------------------------------------------------
// Only the active chain goes into the CRC. The plotted (pending) chain is client
// side and is never the same on two machines, so it must stay out of it.
//-----------------------------------------------------------------------------
void CommandSequenceSystem::crc( Xfer *xfer )
{
	if( xfer == nullptr || xfer->getXferMode() != XFER_CRC )
		return;

	// std::map iterates in ObjectID order, which is what makes this deterministic
	Int count = (Int)m_sequences.size();
	xfer->xferInt( &count );

	for( std::map< ObjectID, CommandSequence * >::iterator it = m_sequences.begin();
			 it != m_sequences.end(); ++it )
	{
		UnsignedInt subject = (UnsignedInt)it->first;
		xfer->xferUnsignedInt( &subject );

		Int nodeCount = it->second->getActiveCount();
		xfer->xferInt( &nodeCount );

		for( CommandNode *node = it->second->getActiveHead(); node != nullptr; node = node->getNext() )
		{
			UnsignedInt type = (UnsignedInt)node->getCommandType();
			UnsignedInt target = (UnsignedInt)node->getTargetID();
			Coord3D location = *node->getLocation();
			Int param = node->getCommandParam();

			xfer->xferUnsignedInt( &type );
			xfer->xferUnsignedInt( &target );
			xfer->xferCoord3D( &location );
			xfer->xferInt( &param );
		}
	}
}

//-----------------------------------------------------------------------------
Int CommandSequenceSystem::getSequenceCount() const
{
	return m_sequences.size();
}

//-----------------------------------------------------------------------------
Bool CommandSequenceSystem::isAllowedCommand( GameMessage::Type type )
{
	for( Int i = 0; i < s_commandSequenceInfoCount; i++ )
	{
		if( s_commandSequenceInfo[ i ].cmdType == type )
			return TRUE;
	}

	return FALSE;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Commands that are a state flip rather than an errand. They complete on the frame
// they are dispatched, so update() advances past them immediately instead of waiting
// for AIUpdateInterface::isIdle() -- a unit that just toggled hold-fire is idle in
// the UI sense, but a unit told to deploy is not, and neither should stall the queue.
//-----------------------------------------------------------------------------
Bool CommandSequenceSystem::isImmediateCommand( GameMessage::Type type )
{
	switch( type )
	{
		case GameMessage::MSG_TOGGLE_HOLD_FIRE:
		case GameMessage::MSG_TOGGLE_DEPLOY:
		case GameMessage::MSG_TOGGLE_OVERCHARGE:
		case GameMessage::MSG_TOGGLE_FIRE_WEAPON:
		case GameMessage::MSG_SWITCH_WEAPONS:
		case GameMessage::MSG_SET_RALLY_POINT:
		case GameMessage::MSG_DO_AUTO_FILL:
		case GameMessage::MSG_DO_CHEER:
			return TRUE;

		default:
			return FALSE;
	}
}

//-----------------------------------------------------------------------------
Bool CommandSequenceSystem::isEndCommandType( GameMessage::Type type, Int param )
{
	// MSG_DO_WEAPON_AT_LOCATION only ends the sequence when it fires forever;
	// with a finite shot count the unit eventually stops and can move on. This
	// case has to be answered before the table lookup, which stores the plain
	// "not an end command" answer for it.
	if( type == GameMessage::MSG_DO_WEAPON_AT_LOCATION )
		return ( param <= 0 );

	// the ones whose end-ness does not depend on their arguments
	for( Int i = 0; i < s_commandSequenceInfoCount; i++ )
	{
		if( s_commandSequenceInfo[ i ].cmdType == type )
			return s_commandSequenceInfo[ i ].endCommand;
	}

	return FALSE;
}
