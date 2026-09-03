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

// FILE: TeleportSelfSpecialPower.cpp ///////////////////////////////////////////////////////////////
// Desc:   Special power update module that teleports its own object to a clicked position.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file int the GameEngine

#include "Common/GameAudio.h"
#include "Common/Xfer.h"
#include "Common/ThingTemplate.h"
#include "Common/DisabledTypes.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/TeleportSelfSpecialPower.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
TeleportSelfSpecialPowerModuleData::TeleportSelfSpecialPowerModuleData( void )
{
	m_specialPowerTemplate = nullptr;
	m_maxRange = 0.0f;
	m_teleportDelayFrames = 0;
	m_recoverDurationFrames = 0;
	m_sourceFX = nullptr;
	m_targetFX = nullptr;
	m_recoverEndFX = nullptr;
	m_tintStatus = TINT_STATUS_INVALID;
	m_recoverCondition = MODELCONDITION_INVALID;
	m_opacityStart = 1.0f;
	m_opacityEnd = 1.0f;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void TeleportSelfSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "SpecialPowerTemplate", INI::parseSpecialPowerTemplate, NULL, offsetof(TeleportSelfSpecialPowerModuleData, m_specialPowerTemplate) },
		{ "MaxTeleportRange", INI::parseReal, NULL, offsetof(TeleportSelfSpecialPowerModuleData, m_maxRange) },
		{ "TeleportDelay", INI::parseDurationUnsignedInt, NULL, offsetof(TeleportSelfSpecialPowerModuleData, m_teleportDelayFrames) },
		{ "RecoverDuration", INI::parseDurationUnsignedInt, NULL, offsetof(TeleportSelfSpecialPowerModuleData, m_recoverDurationFrames) },
		{ "TeleportStartFX", INI::parseFXList, NULL, offsetof(TeleportSelfSpecialPowerModuleData, m_sourceFX) },
		{ "TeleportTargetFX", INI::parseFXList, NULL, offsetof(TeleportSelfSpecialPowerModuleData, m_targetFX) },
		{ "TeleportRecoverEndFX", INI::parseFXList, NULL, offsetof(TeleportSelfSpecialPowerModuleData, m_recoverEndFX) },
		{ "TeleportRecoverSoundAmbient", INI::parseAudioEventRTS, NULL, offsetof(TeleportSelfSpecialPowerModuleData, m_recoverSoundLoop) },
		{ "TeleportRecoverTint", TintStatusFlags::parseSingleBitFromINI, NULL, offsetof(TeleportSelfSpecialPowerModuleData, m_tintStatus) },
		{ "TeleportRecoverCondition", ModelConditionFlags::parseSingleBitFromINI, NULL, offsetof(TeleportSelfSpecialPowerModuleData, m_recoverCondition) },
		{ "TeleportRecoverOpacityStart", INI::parsePercentToReal, NULL, offsetof(TeleportSelfSpecialPowerModuleData, m_opacityStart) },
		{ "TeleportRecoverOpacityEnd", INI::parsePercentToReal, NULL, offsetof(TeleportSelfSpecialPowerModuleData, m_opacityEnd) },
		{ 0, 0, 0, 0 }
	};
	p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
TeleportSelfSpecialPower::TeleportSelfSpecialPower( Thing *thing, const ModuleData* moduleData ) : SpecialPowerUpdateModule( thing, moduleData )
{
	m_specialPowerModule = nullptr;
	m_destLocation.zero();
	m_teleportFrame = 0;
	m_recoverUntilFrame = 0;
	m_active = FALSE;
	m_isRecovering = FALSE;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
TeleportSelfSpecialPower::~TeleportSelfSpecialPower( void )
{
}

//-------------------------------------------------------------------------------------------------
// Cache the paired SpecialPowerModule (recharge/cost/timer) so we can trigger it later.
//-------------------------------------------------------------------------------------------------
void TeleportSelfSpecialPower::onObjectCreated()
{
	const TeleportSelfSpecialPowerModuleData *data = getTeleportSelfSpecialPowerModuleData();
	Object *obj = getObject();

	if( !data->m_specialPowerTemplate )
	{
		DEBUG_CRASH( ("%s object's TeleportSelfSpecialPower lacks access to the SpecialPowerTemplate. Needs to be specified in ini.", obj->getTemplate()->getName().str() ) );
		return;
	}

	m_specialPowerModule = obj->getSpecialPowerModule( data->m_specialPowerTemplate );
}

//-------------------------------------------------------------------------------------------------
// Snap the clicked point onto a spot the object may legally stand on.
//-------------------------------------------------------------------------------------------------
Bool TeleportSelfSpecialPower::validateDestination( Coord3D *destination )
{
	Object *obj = getObject();
	AIUpdateInterface *ai = obj->getAIUpdateInterface();

	TheAI->pathfinder()->adjustDestination( obj, ai->getLocomotorSet(), destination );
	destination->z = TheTerrainLogic->getGroundHeight( destination->x, destination->y );

	PathfindLayerEnum layer = TheTerrainLogic->getLayerForDestination( destination );

	return TheAI->pathfinder()->validMovementPosition( obj->getCrusherLevel() > 0, layer,
		ai->getLocomotorSet(), obj->getRequiredBridgeHeight(), destination );
}

//-------------------------------------------------------------------------------------------------
// The clicked position arrives here as targetPos. Every failure path returns before the power is
// triggered, so a rejected click costs the player neither money nor cooldown.
//-------------------------------------------------------------------------------------------------
Bool TeleportSelfSpecialPower::initiateIntentToDoSpecialPower(const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions )
{
	if( m_specialPowerModule == nullptr || m_specialPowerModule->getSpecialPowerTemplate() != specialPowerTemplate )
	{
		// Make sure our modules are connected.
		return FALSE;
	}

	if( targetPos == nullptr )
	{
		return FALSE;
	}

	if( m_active || m_isRecovering )
	{
		return FALSE;
	}

	Object *obj = getObject();

	// Both pathfinder checks need a locomotor set, so without an AI there is no way to tell whether
	// a destination is legal.
	if( obj->getAIUpdateInterface() == nullptr )
	{
		return FALSE;
	}

	// Teleporting a passenger out of its transport is nonsense, and our recovery disable would
	// propagate to the container's other riders.
	if( obj->getContainedBy() != nullptr )
	{
		return FALSE;
	}

	const TeleportSelfSpecialPowerModuleData *data = getTeleportSelfSpecialPowerModuleData();

	if( data->m_maxRange > 0.0f )
	{
		Coord3D dir;
		Real distSq = ThePartitionManager->getDistanceSquared( obj, targetPos, FROM_CENTER_2D, &dir );
		if( distSq > data->m_maxRange * data->m_maxRange )
		{
			return FALSE;
		}
	}

	Coord3D destination = *targetPos;
	if( !validateDestination( &destination ) )
	{
		return FALSE;
	}

	// triggerSpecialPower bails out before starting the recharge when the cost is unaffordable,
	// which would leave us teleporting for free, so refuse the order here instead.
	Int cost = specialPowerTemplate->getCost();
	if( cost > 0 )
	{
		const Player *player = obj->getControllingPlayer();
		if( player != nullptr && player->getMoney()->countMoney() < (UnsignedInt)cost )
		{
			return FALSE;
		}
	}

	m_destLocation.set( destination );
	m_active = TRUE;

	// Charge the cost and start the recharge. This must happen before any disable of ours exists,
	// since a disable pauses the countdown.
	m_specialPowerModule->markSpecialPowerTriggered( &m_destLocation );

	// Arm the teleport. setWakeFrame is ignored from within update(), so it has to happen here.
	m_teleportFrame = TheGameLogic->getFrame() + data->m_teleportDelayFrames;
	setWakeFrame( getObject(), data->m_teleportDelayFrames > 0 ? UPDATE_SLEEP( data->m_teleportDelayFrames ) : UPDATE_SLEEP_NONE );

	return TRUE;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void TeleportSelfSpecialPower::doTeleport()
{
	const TeleportSelfSpecialPowerModuleData *data = getTeleportSelfSpecialPowerModuleData();
	Object *obj = getObject();
	AIUpdateInterface *ai = obj->getAIUpdateInterface();

	FXList::doFXObj( data->m_sourceFX, obj );

	Drawable *draw = obj->getDrawable();
	if( draw )
	{
		// Must happen before the move, or the tread marks stretch from here to the destination.
		draw->reactToTeleport();
	}

	if( ai )
	{
		ai->aiIdle( CMD_FROM_AI );
		ai->destroyPath();
	}

	obj->setPosition( &m_destLocation );
	TheAI->pathfinder()->updatePos( obj, &m_destLocation );

	FXList::doFXObj( data->m_targetFX, obj );

	if( data->m_recoverDurationFrames == 0 )
	{
		return;
	}

	UnsignedInt now = TheGameLogic->getFrame();
	m_recoverUntilFrame = now + data->m_recoverDurationFrames;
	m_isRecovering = TRUE;

	obj->setDisabledUntil( DISABLED_TELEPORT_RECOVER, m_recoverUntilFrame );

	applyRecoverEffects( now );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bool TeleportSelfSpecialPower::usesOpacityRamp() const
{
	const TeleportSelfSpecialPowerModuleData *data = getTeleportSelfSpecialPowerModuleData();

	return data->m_opacityStart < 1.0f || data->m_opacityEnd < 1.0f;
}

//-------------------------------------------------------------------------------------------------
// Opacity for a point in the recovery, clamped so a zero-length recovery cannot divide by zero.
//-------------------------------------------------------------------------------------------------
Real TeleportSelfSpecialPower::getRecoverOpacity( UnsignedInt now ) const
{
	const TeleportSelfSpecialPowerModuleData *data = getTeleportSelfSpecialPowerModuleData();

	Real progress = 1.0f;
	if( data->m_recoverDurationFrames > 0 )
	{
		UnsignedInt startFrame = m_recoverUntilFrame - data->m_recoverDurationFrames;
		progress = clamp( 0.0f, INT_TO_REAL( now - startFrame ) / INT_TO_REAL( data->m_recoverDurationFrames ), 1.0f );
	}

	return WWMath::Lerp( data->m_opacityStart, data->m_opacityEnd, progress );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void TeleportSelfSpecialPower::applyRecoverEffects( UnsignedInt now )
{
	const TeleportSelfSpecialPowerModuleData *data = getTeleportSelfSpecialPowerModuleData();
	Object *obj = getObject();

	if( data->m_recoverCondition > MODELCONDITION_INVALID )
	{
		obj->setModelConditionState( data->m_recoverCondition );
	}

	m_recoverSoundLoop = data->m_recoverSoundLoop;
	m_recoverSoundLoop.setObjectID( obj->getID() );
	m_recoverSoundLoop.setPlayingHandle( TheAudio->addAudioEvent( &m_recoverSoundLoop ) );

	Drawable *draw = obj->getDrawable();
	if( draw )
	{
		if( data->m_tintStatus > TINT_STATUS_INVALID && data->m_tintStatus < TINT_STATUS_COUNT )
		{
			draw->setTintStatus( data->m_tintStatus );
		}

		// Takes the frame so a load resuming partway through picks up the ramp where it left off.
		if( usesOpacityRamp() )
		{
			draw->setDrawableOpacity( getRecoverOpacity( now ) );
		}
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void TeleportSelfSpecialPower::removeRecoverEffects()
{
	const TeleportSelfSpecialPowerModuleData *data = getTeleportSelfSpecialPowerModuleData();
	Object *obj = getObject();

	if( data->m_recoverCondition > MODELCONDITION_INVALID )
	{
		obj->clearModelConditionState( data->m_recoverCondition );
	}

	TheAudio->removeAudioEvent( m_recoverSoundLoop.getPlayingHandle() );

	Drawable *draw = obj->getDrawable();
	if( draw )
	{
		if( data->m_tintStatus > TINT_STATUS_INVALID && data->m_tintStatus < TINT_STATUS_COUNT )
		{
			draw->clearTintStatus( data->m_tintStatus );
		}

		// Only restore what we actually took, so we don't stomp a stealth or chrono fade.
		if( usesOpacityRamp() )
		{
			draw->setDrawableOpacity( 1.0f );
		}
	}

	FXList::doFXObj( data->m_recoverEndFX, obj );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpdateSleepTime TeleportSelfSpecialPower::update()
{
	const TeleportSelfSpecialPowerModuleData *data = getTeleportSelfSpecialPowerModuleData();
	UnsignedInt now = TheGameLogic->getFrame();

	if( m_isRecovering )
	{
		if( now < m_recoverUntilFrame )
		{
			// Without a ramp to animate there is nothing to do until the recovery ends.
			if( !usesOpacityRamp() )
			{
				return UPDATE_SLEEP( m_recoverUntilFrame - now );
			}

			Drawable *draw = getObject()->getDrawable();
			if( draw )
			{
				draw->setDrawableOpacity( getRecoverOpacity( now ) );
			}
			return UPDATE_SLEEP_NONE;
		}

		// The engine clears the disable itself once it expires, so we only undo our own effects.
		removeRecoverEffects();
		m_isRecovering = FALSE;
		return UPDATE_SLEEP_FOREVER;
	}

	if( !m_active || now < m_teleportFrame )
	{
		return UPDATE_SLEEP_FOREVER;
	}

	// Everything was validated when the order was given, but a delay gives the world time to
	// change underneath us, so none of it can be trusted at the moment we actually fire.
	Object *obj = getObject();
	Coord3D destination = m_destLocation;

	// Our own recovery disable cannot be set yet, so any disable at all means something else
	// stopped us, and an EMP'd unit should not get to escape.
	if( obj->isEffectivelyDead() || obj->getContainedBy() != nullptr || obj->isDisabled()
			|| obj->getAIUpdateInterface() == nullptr
			|| !validateDestination( &destination ) )
	{
		m_active = FALSE;
		return UPDATE_SLEEP_FOREVER;
	}

	m_destLocation.set( destination );

	doTeleport();
	m_active = FALSE;

	if( !m_isRecovering )
	{
		return UPDATE_SLEEP_FOREVER;
	}

	return usesOpacityRamp() ? UPDATE_SLEEP_NONE : UPDATE_SLEEP( data->m_recoverDurationFrames );
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void TeleportSelfSpecialPower::crc( Xfer *xfer )
{
	// extend base class
	SpecialPowerUpdateModule::crc( xfer );
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void TeleportSelfSpecialPower::xfer( Xfer *xfer )
{
	const XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	SpecialPowerUpdateModule::xfer( xfer );

	// m_specialPowerModule is re-resolved in onObjectCreated
	xfer->xferCoord3D( &m_destLocation );
	xfer->xferUnsignedInt( &m_teleportFrame );
	xfer->xferUnsignedInt( &m_recoverUntilFrame );
	xfer->xferBool( &m_active );
	xfer->xferBool( &m_isRecovering );
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void TeleportSelfSpecialPower::loadPostProcess( void )
{
	// extend base class
	SpecialPowerUpdateModule::loadPostProcess();

	// A playing handle is a client side index that does not survive the save, so the ambient loop
	// and the visuals have to be issued again rather than restored.
	// Do not call setWakeFrame() here, and do not re-apply the disable: UpdateModule::xfer has
	// already restored the wake frame, and Object xfers the disable itself.
	if( m_isRecovering )
	{
		applyRecoverEffects( TheGameLogic->getFrame() );
	}
}
