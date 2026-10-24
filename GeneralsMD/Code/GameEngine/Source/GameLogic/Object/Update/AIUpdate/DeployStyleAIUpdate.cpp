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

// DeployStyleAIUpdate.cpp ////////////
// Author: Kris Morness, August 2002
// Desc:   A standard ai update that also handles units that must deploy to attack and pack before moving.

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"

#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"

#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Weapon.h"

#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/DeployStyleAIUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
DeployStyleAIUpdate::DeployStyleAIUpdate( Thing *thing, const ModuleData* moduleData ) : AIUpdateInterface( thing, moduleData )
{
	m_state = READY_TO_MOVE;
	m_frameToWaitForDeploy = 0;
	m_manualDeploy = FALSE;
	m_manualPack = FALSE;
}

//-------------------------------------------------------------------------------------------------
DeployStyleAIUpdate::~DeployStyleAIUpdate()
{
}

//-------------------------------------------------------------------------------------------------
Bool DeployStyleAIUpdate::isIdle() const
{
	return AIUpdateInterface::isIdle();
}

//-------------------------------------------------------------------------------------------------
void DeployStyleAIUpdate::aiDoCommand( const AICommandParms* parms )
{
	if (!isAllowedToRespondToAiCommands(parms))
		return;

	if( m_manualPack && parms->m_cmdSource == CMD_FROM_AI )
	{
		switch( parms->m_cmd )
		{
			case AICMD_ATTACK_OBJECT:
			case AICMD_FORCE_ATTACK_OBJECT:
			case AICMD_ATTACK_POSITION:
				return;
		}
	}

	if( parms->m_cmd != AICMD_IDLE )
	{
		m_manualDeploy = FALSE;
		m_manualPack = FALSE;
	}

	/*
	//Hack code to allow follow waypoint scripts to be converted to attack follow waypoint scripts
	//for simple-test-map-purposes.
	switch( parms->m_cmd )
	{
		case AICMD_FOLLOW_WAYPOINT_PATH:
			((AICommandParms*)parms)->m_cmd = AICMD_ATTACKFOLLOW_WAYPOINT_PATH;
			break;
		case AICMD_FOLLOW_WAYPOINT_PATH_AS_TEAM:
			((AICommandParms*)parms)->m_cmd = AICMD_ATTACKFOLLOW_WAYPOINT_PATH_AS_TEAM;
			break;
	}
	*/
	AIUpdateInterface::aiDoCommand( parms );
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime DeployStyleAIUpdate::update()
{
	// Suspend deploy/undeploy timers while disabled; only the locomotor runs.
	if (isAiSuspendedByDisable())
		return AIUpdateInterface::update();

	// have to call our parent's isIdle, because we override it to never return true
	// when we have a pending command...
	Object *self = getObject();
	Weapon *weapon = self->getCurrentWeapon();
#if defined(GENERALS_ONLINE_HIGH_FPS_SERVER)
	UnsignedInt now = TheGameLogic->getFrameLegacy();
#else
	UnsignedInt now = TheGameLogic->getFrame();
#endif
	const DeployStyleAIUpdateModuleData *data = getDeployStyleAIUpdateModuleData();

	//Are we attempting to move? If so we can't do it unless we are undeployed.
	Bool isTryingToMove = isWaitingForPath() || getPath();

	//Are we trying to attack something. If so, we need to be in range before we can do so.
	Bool isTryingToAttack = getStateMachine()->isInAttackState();
	Bool isInRange = FALSE;

	//Are we in guard mode? If so, are we idle... idle guarders deploy for fastest response against attackers.
	Bool isInGuardIdleState = getStateMachine()->isInGuardIdleState();

	AIUpdateInterface *ai = self->getAI();

	if( isTryingToAttack && weapon )
	{
		Object *victim = ai->getCurrentVictim();
		if( victim )
		{
			isInRange = weapon->isWithinAttackRange( self, victim );
		}
		else
		{
			const Coord3D *pos = ai->getCurrentVictimPos();
			if( pos )
			{
				isInRange = weapon->isWithinAttackRange( self, pos );
			}
		}
	}

	if( m_frameToWaitForDeploy != 0 && now >= m_frameToWaitForDeploy)
	{
		switch( m_state )
		{
			case DEPLOY:
				setMyState( READY_TO_ATTACK );
				break;
			case UNDEPLOY:
				setMyState( READY_TO_MOVE );
				break;
		}
	}

	// The target left range mid-deploy, so pack up again rather than finish unpacking.
	if( m_state == DEPLOY && isTryingToAttack && !isInRange && !m_manualDeploy )
	{
		if( m_frameToWaitForDeploy != 0 )
		{
			setMyState( UNDEPLOY, TRUE );
		}
		else
		{
			setMyState( UNDEPLOY );
		}
	}

	// A movement path must finish packing before the unit can deploy again.
	if( !m_manualPack && ((!isTryingToMove && (isInRange || isInGuardIdleState)) || m_manualDeploy) )
	{
		switch( m_state )
		{
			case READY_TO_MOVE:
				//We're need to deploy before we can attack.
				if (data->m_turnBeforeUnpacking) {
					// Check if we finished turning before deploying
					if (isWithinAttackAngle()) {
						setMyState(DEPLOY);
					}
					else {
						// stay in READY_TO_MOVE state
						break;
					}
				}
				else { // Deploy right away
					setMyState(DEPLOY);
				}
				break;
			case READY_TO_ATTACK:
				//Let the AI handle attacking.
				break;
			case DEPLOY:
				//We can't start attacking until we are fully deployed.
				break;
			case UNDEPLOY:
				if( m_frameToWaitForDeploy != 0 )
				{
					//Reverse the undeploy at it's current frame!
					setMyState( DEPLOY, TRUE );
				}
				break;
			case ALIGNING_TURRETS:
				//If turrets are aligning, we are able to attack right now.
				setMyState( READY_TO_ATTACK );
				break;
		}
	}
	else if( isTryingToMove || m_manualPack || m_state == ALIGNING_TURRETS )
	{
		switch( m_state )
		{
			case READY_TO_MOVE:
				//We're ready... ai will handle moving.
				break;
			case READY_TO_ATTACK:
			{
				WhichTurretType tur = getWhichTurretForCurWeapon();
				if( tur != TURRET_INVALID )
				{
					if( doTurretsHaveToCenterBeforePacking() )
					{
						//Turrets need to center before we can undeploy.
						setMyState( ALIGNING_TURRETS );
						break;
					}
				}
				//Start undeploying.
				setMyState( UNDEPLOY );
				break;
			}
			case DEPLOY:
				if( m_frameToWaitForDeploy != 0 )
				{
					//Reverse the deploy at it's current frame!
					setMyState( UNDEPLOY, TRUE );
				}
				break;
			case UNDEPLOY:
				break;
			case ALIGNING_TURRETS:
			{
				WhichTurretType tur = getWhichTurretForCurWeapon();
				if( tur == TURRET_INVALID || isTurretInNaturalPosition( tur ) )
				{
					setMyState( UNDEPLOY );
				}
				break;
			}
		}
	}

	switch( m_state )
	{
		case READY_TO_MOVE:
			if( isTryingToMove )
			{
				self->setModelConditionState( MODELCONDITION_MOVING );
			}
			break;

		case READY_TO_ATTACK:
			break;

		case DEPLOY:
			if( data->m_manualDeployAnimations )
			{
#if defined(GENERALS_ONLINE_HIGH_FPS_SERVER)
				if (!TheGameLogic->HasLegacyFrameAdvanced())
				{
					break;
				}
#endif
				UnsignedInt totalFrames = getPackTime();
				UnsignedInt framesLeft = m_frameToWaitForDeploy - now;
				Drawable *draw = self->getDrawable();
				if( draw )
				{
					draw->setAnimationFrame( totalFrames - framesLeft );
				}
			}
			getStateMachine()->setTemporaryState( AI_BUSY, UPDATE_SLEEP_NONE );
			setLocomotorGoalNone();
			break;
		case UNDEPLOY:
			if( data->m_manualDeployAnimations )
			{
#if defined(GENERALS_ONLINE_HIGH_FPS_SERVER)
				if (!TheGameLogic->HasLegacyFrameAdvanced())
				{
					break;
				}
#endif
				UnsignedInt framesLeft = m_frameToWaitForDeploy - now;
				Drawable *draw = self->getDrawable();
				if( draw )
				{
					draw->setAnimationFrame( framesLeft );
				}
			}
			getStateMachine()->setTemporaryState( AI_BUSY, UPDATE_SLEEP_NONE );
			setLocomotorGoalNone();
			break;

		case ALIGNING_TURRETS:
			getStateMachine()->setTemporaryState( AI_BUSY, UPDATE_SLEEP_NONE );
			setLocomotorGoalNone();
			break;
	}

	AIUpdateInterface::update();
	//We can't sleep the deploy AI because any new commands need to be caught and sent
	//into busy state during the update.
	return UPDATE_SLEEP_NONE;

}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime DeployStyleAIUpdate::doLocomotor()
{
	// Attack states can create new movement goals after the deploy update has run.
	if( (m_state != READY_TO_MOVE || m_manualPack) && !isAiSuspendedByDisable() && !isAiInDeadState() )
	{
		PhysicsBehavior *physics = getObject()->getPhysics();
		if( physics )
		{
			physics->scrubVelocity2D( 0 );
		}
		return UPDATE_SLEEP_NONE;
	}

	return AIUpdateInterface::doLocomotor();
}

//-------------------------------------------------------------------------------------------------
/**
 * Deploy or pack up because the player said so, rather than because a target wandered into range.
 * The flag latches: update() consults it so we hold the stance instead of immediately undoing it.
 */
void DeployStyleAIUpdate::toggleManualDeploy()
{
	if( isDeployedOrDeploying() )
	{
		m_manualDeploy = FALSE;
		m_manualPack = TRUE;
		aiIdle( CMD_FROM_PLAYER );

		if( m_state == DEPLOY && m_frameToWaitForDeploy != 0 )
		{
			setMyState( UNDEPLOY, TRUE );
		}
		else if( m_state == READY_TO_ATTACK )
		{
			WhichTurretType tur = getWhichTurretForCurWeapon();
			if( tur != TURRET_INVALID && doTurretsHaveToCenterBeforePacking() )
			{
				setMyState( ALIGNING_TURRETS );
			}
			else
			{
				setMyState( UNDEPLOY );
			}
		}
		return;
	}

	m_manualDeploy = TRUE;
	m_manualPack = FALSE;
	aiIdle( CMD_FROM_PLAYER );

	if( m_state == UNDEPLOY && m_frameToWaitForDeploy != 0 )
	{
		// Reverse a half finished pack up at the frame it reached.
		setMyState( DEPLOY, TRUE );
	}
	else if( m_state == READY_TO_MOVE )
	{
		setMyState( DEPLOY );
	}
}

//-------------------------------------------------------------------------------------------------
void DeployStyleAIUpdate::setMyState( DeployStateTypes stateID, Bool reverseDeploy )
{
	m_state = stateID;
	Object *self = getObject();
#if defined(GENERALS_ONLINE_HIGH_FPS_SERVER)
	UnsignedInt now = TheGameLogic->getFrameLegacy();
#else
	UnsignedInt now = TheGameLogic->getFrame();
#endif
	const DeployStyleAIUpdateModuleData *data = getDeployStyleAIUpdateModuleData();

	switch( stateID )
	{
		case DEPLOY:
		{
			//Tell our object to deploy (so it can continue the same attack later).
			self->clearAndSetModelConditionFlags( MAKE_MODELCONDITION_MASK( MODELCONDITION_PACKING ),
																						 MAKE_MODELCONDITION_MASK( MODELCONDITION_UNPACKING ) );

			if( reverseDeploy )
			{
				//This is a zero to max animation.
				UnsignedInt totalFrames = getUnpackTime();
				UnsignedInt framesLeft = m_frameToWaitForDeploy - now;
				m_frameToWaitForDeploy = now + totalFrames - framesLeft;
				if( data->m_manualDeployAnimations )
				{
					Drawable *draw = self->getDrawable();
					if( draw )
					{
						draw->setAnimationFrame( totalFrames - framesLeft );
					}
				}
			}
			else
			{
				m_frameToWaitForDeploy = getUnpackTime() + now;
			}

			//Play deploy sound
			const ThingTemplate *thing = self->getTemplate();
			const AudioEventRTS* soundToPlayPtr = thing->getPerUnitSound( "Deploy" );
			if( soundToPlayPtr )
			{
				AudioEventRTS soundToPlay = *soundToPlayPtr;
				soundToPlay.setObjectID( self->getID() );
				TheAudio->addAudioEvent( &soundToPlay );
			}

			break;
		}
		case UNDEPLOY:
		{
			//This status will tell the approach AI state to succeed automatically. This prevents
			//twitching on deploy. Make sure we clear it now so when it goes into that state, it'll
			//actually move.
			self->clearStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_DEPLOYED ) );

			//Tell our object to pack up (so it can continue the same move later).
			self->clearAndSetModelConditionFlags( MAKE_MODELCONDITION_MASK2( MODELCONDITION_UNPACKING, MODELCONDITION_DEPLOYED ),
																						 MAKE_MODELCONDITION_MASK( MODELCONDITION_PACKING ) );

			if( reverseDeploy )
			{
				//This is a max to zero animation.
				UnsignedInt totalFrames = getUnpackTime();
				UnsignedInt framesLeft = m_frameToWaitForDeploy - now;
				m_frameToWaitForDeploy = now + totalFrames - framesLeft;
				if( data->m_manualDeployAnimations )
				{
					Drawable *draw = self->getDrawable();
					if( draw )
					{
						draw->setAnimationFrame( framesLeft );
					}
				}
			}
			else
			{
				m_frameToWaitForDeploy = getPackTime() + now;
			}

			if( doTurretsFunctionOnlyWhenDeployed() )
			{
				WhichTurretType tur = getWhichTurretForCurWeapon();
				if( tur != TURRET_INVALID )
				{
					setTurretEnabled( tur, false );
				}
			}

			//Play undeploy sound
			const ThingTemplate *thing = self->getTemplate();
			const AudioEventRTS* soundToPlayPtr = thing->getPerUnitSound( "Undeploy" );
			if( soundToPlayPtr )
			{
				AudioEventRTS soundToPlay = *soundToPlayPtr;
				soundToPlay.setObjectID( self->getID() );
				TheAudio->addAudioEvent( &soundToPlay );
			}

			break;
		}
		case READY_TO_MOVE:
		{
			m_frameToWaitForDeploy = 0;

			self->clearModelConditionFlags( MAKE_MODELCONDITION_MASK( MODELCONDITION_PACKING ) );
			break;
		}
		case READY_TO_ATTACK:
		{
			//This status will tell the approach AI state to succeed automatically. This prevents
			//twitching on deploy.
			self->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_DEPLOYED ) );

			m_frameToWaitForDeploy = 0;

			self->clearAndSetModelConditionFlags( MAKE_MODELCONDITION_MASK( MODELCONDITION_UNPACKING ),
																						 MAKE_MODELCONDITION_MASK( MODELCONDITION_DEPLOYED) );

			if( doTurretsFunctionOnlyWhenDeployed() )
			{
				WhichTurretType tur = getWhichTurretForCurWeapon();
				if( tur != TURRET_INVALID )
				{
					setTurretEnabled( tur, true );
				}
			}

			break;
		}
		case ALIGNING_TURRETS:
		{
			m_frameToWaitForDeploy = 0;
			WhichTurretType tur = getWhichTurretForCurWeapon();
			if( tur != TURRET_INVALID )
			{
				recenterTurret( tur );
			}
			break;
		}
	}

}

// ------------------------------------------------------------------------------------------------
Bool DeployStyleAIUpdate::isWithinAttackAngle(void) const
{
	const Object* self = getObject();
	const Weapon* weapon = self->getCurrentWeapon();
	const AIUpdateInterface* ai = self->getAI();

	Object* victim = ai->getCurrentVictim();
	const Coord3D* pos;
	if (victim)
		pos = victim->getPosition();
	else
		pos = ai->getCurrentVictimPos();

	if (pos == NULL || weapon == NULL)
		return false;

	Real aimDelta = weapon->getTemplate()->getAimDelta();

	// TODO: get limited turret turn angle

	Real relAngle = ThePartitionManager->getRelativeAngle2D(self, pos);
	return abs(relAngle) <= aimDelta;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void DeployStyleAIUpdate::crc( Xfer *xfer )
{
	// extend base class
	AIUpdateInterface::crc(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version
	* 2: Added support for attack move
	* 3: Added improved support for guard, and support for hunt AI
 **/
// ------------------------------------------------------------------------------------------------
void DeployStyleAIUpdate::xfer( Xfer *xfer )
{
  // version
  // Manual stances only persist in saves that allow extensions.
#if RETAIL_COMPATIBLE_XFER_SAVE
  XferVersion currentVersion = 4;
#else
  XferVersion currentVersion = 6;
#endif
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

 // extend base class
	AIUpdateInterface::xfer(xfer);

	if( version >= 4 )
	{
		xfer->xferUser(&m_state, sizeof(m_state));
		xfer->xferUnsignedInt(&m_frameToWaitForDeploy);

		if( version >= 5 )
		{
			xfer->xferBool(&m_manualDeploy);
		}
		if( version >= 6 )
		{
			xfer->xferBool(&m_manualPack);
		}
		else if( xfer->getXferMode() == XFER_LOAD )
		{
			m_manualPack = FALSE;
		}
	}
	else if( xfer->getXferMode() == XFER_LOAD )
	{
		//Good riddance!!!
		Bool obsoleteBool;
		UnsignedInt obsoleteUnsignedInt;
		ObjectID obsoleteObjectID;
		Coord3D obsoleteCoord3D;
   	AICommandParmsStorage	obsoleteAICommandParmsStorage;

		xfer->xferBool( &obsoleteBool );
		xfer->xferUnsignedInt( &obsoleteUnsignedInt );
		xfer->xferUser(&m_state, sizeof(m_state));

		if( version >= 2 )
		{
			xfer->xferObjectID( &obsoleteObjectID );
			xfer->xferObjectID( &obsoleteObjectID );
			xfer->xferCoord3D( &obsoleteCoord3D );
			xfer->xferBool( &obsoleteBool );
			xfer->xferBool( &obsoleteBool );
			xfer->xferBool( &obsoleteBool );
		}
		if( version >= 3 )
		{
			xfer->xferBool( &obsoleteBool );
			xfer->xferBool( &obsoleteBool );
		}

		obsoleteAICommandParmsStorage.doXfer( xfer );

		if( version < 2 )
		{
			obsoleteAICommandParmsStorage.doXfer(xfer);
		}

		//Initialize unit to able to move.
		m_state = READY_TO_MOVE;
	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void DeployStyleAIUpdate::loadPostProcess()
{
 // extend base class
	AIUpdateInterface::loadPostProcess();
}

