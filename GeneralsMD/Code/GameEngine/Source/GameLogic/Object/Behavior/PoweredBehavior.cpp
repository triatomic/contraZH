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

// FILE: PoweredBehavior.cpp /////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GameState.h"
#include "Common/Player.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/PoweredBehavior.h"

//-------------------------------------------------------------------------------------------------
PoweredBehaviorModuleData::PoweredBehaviorModuleData()
{
	m_isMobile = TRUE;
	m_disableWeapon = FALSE;
	m_movePenalty = 0.0f;
	m_liftPenalty = 0.0f;
	m_damagePenalty = 0.0f;
	m_rateOfFirePenalty = 0.0f;
	m_rangePenalty = 0.0f;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void PoweredBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "IsMobile",				INI::parseBool,						nullptr, offsetof( PoweredBehaviorModuleData, m_isMobile ) },
		{ "DisableWeapon",	INI::parseBool,						nullptr, offsetof( PoweredBehaviorModuleData, m_disableWeapon ) },
		{ "MovePenalty",		INI::parsePercentToReal,	nullptr, offsetof( PoweredBehaviorModuleData, m_movePenalty ) },
		{ "LiftPenalty",		INI::parsePercentToReal,	nullptr, offsetof( PoweredBehaviorModuleData, m_liftPenalty ) },
		{ "DamagePenalty",	INI::parsePercentToReal,	nullptr, offsetof( PoweredBehaviorModuleData, m_damagePenalty ) },
		{ "RateOfFirePenalty",	INI::parsePercentToReal,	nullptr, offsetof( PoweredBehaviorModuleData, m_rateOfFirePenalty ) },
		{ "RangePenalty",		INI::parsePercentToReal,	nullptr, offsetof( PoweredBehaviorModuleData, m_rangePenalty ) },
		{ "Icon",						INI::parseAsciiString,		nullptr, offsetof( PoweredBehaviorModuleData, m_iconName ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
PoweredBehavior::PoweredBehavior( Thing *thing, const ModuleData* moduleData ) :
	UpdateModule( thing, moduleData ),
	m_powered(TRUE),
	m_clearOldDisable(FALSE)
{
	// the owner is not known yet, so the first tick reads the power state
	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
PoweredBehavior::~PoweredBehavior()
{
}

//-------------------------------------------------------------------------------------------------
static Real weaponPenaltyToScalar( Real penalty )
{
	// Module-fired weapons ignore NO_ATTACK, and a zero rate of fire would divide by zero.
	const Real MIN_WEAPON_SCALAR = 0.01f;
	return penalty > 0.0f ? max( 1.0f - penalty, MIN_WEAPON_SCALAR ) : 1.0f;
}

//-------------------------------------------------------------------------------------------------
Bool PoweredBehavior::onPowerChange( Bool hasPower )
{
	// A load rebuilds the power totals object by object, so they mean nothing until it ends.
	if (TheGameState->isInLoadGame())
	{
		return TRUE;
	}

	if (m_powered == hasPower)
	{
		return TRUE;
	}
	m_powered = hasPower;

	const PoweredBehaviorModuleData *d = getPoweredBehaviorModuleData();
	Object *obj = getObject();
	AIUpdateInterface *ai = obj->getAI();
	Drawable *draw = obj->getDrawable();
	Bool immobile = !d->m_isMobile || d->m_movePenalty >= 1.0f;
	Real speedScalar = (!immobile && d->m_movePenalty > 0.0f) ? 1.0f - d->m_movePenalty : 1.0f;
	Real liftScalar = (d->m_liftPenalty > 0.0f && d->m_liftPenalty < 1.0f) ? 1.0f - d->m_liftPenalty : 1.0f;
	Bool noAttack = d->m_disableWeapon || d->m_damagePenalty >= 1.0f || d->m_rateOfFirePenalty >= 1.0f || d->m_rangePenalty >= 1.0f;

	if (ai)
	{
		ai->applySpeedMultiplier( m_powered ? 1.0f / speedScalar : speedScalar );
		ai->applyLiftMultiplier( m_powered ? 1.0f / liftScalar : liftScalar );
	}

	if (m_powered)
	{
		if (draw)
		{
			draw->clearStatusIcon();
		}
		obj->clearStatus( m_appliedStatus );
		m_appliedStatus.clear();
		obj->setWeaponScalars( 1.0f, 1.0f, 1.0f );
	}
	else
	{
		if (draw && d->m_iconName.isNotEmpty())
		{
			draw->setStatusIcon( d->m_iconName );
		}
		m_appliedStatus.set( OBJECT_STATUS_NO_ATTACK, noAttack );
		m_appliedStatus.set( OBJECT_STATUS_IMMOBILE, immobile );
		obj->setStatus( m_appliedStatus );
		obj->setWeaponScalars( weaponPenaltyToScalar( d->m_damagePenalty ), weaponPenaltyToScalar( d->m_rateOfFirePenalty ), weaponPenaltyToScalar( d->m_rangePenalty ) );
		if (immobile && ai)
		{
			ai->aiIdle( CMD_FROM_AI );
		}
	}
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
void PoweredBehavior::syncToOwner()
{
	const Player *player = getObject()->getControllingPlayer();
	if (player == nullptr)
	{
		return;
	}
	onPowerChange( player->getEnergy()->hasSufficientPower() );
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime PoweredBehavior::update()
{
	syncToOwner();
	return UPDATE_SLEEP_FOREVER;
}

//-------------------------------------------------------------------------------------------------
void PoweredBehavior::onCapture( Player *oldOwner, Player *newOwner )
{
	syncToOwner();
}

//-------------------------------------------------------------------------------------------------
void PoweredBehavior::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
/** Version Info:
	* 1: Initial version
	* 2: No data change, marks saves that cannot hold the KINDOF_POWERED disable */
//-------------------------------------------------------------------------------------------------
void PoweredBehavior::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	m_clearOldDisable = (version < 2);

	UpdateModule::xfer( xfer );

	xfer->xferBool( &m_powered );
	m_appliedStatus.xfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void PoweredBehavior::loadPostProcess()
{
	UpdateModule::loadPostProcess();

	if (m_clearOldDisable)
	{
		getObject()->clearDisabled( DISABLED_UNDERPOWERED );
	}
}
