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
		{ "Icon",						INI::parseAsciiString,		nullptr, offsetof( PoweredBehaviorModuleData, m_iconName ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
PoweredBehavior::PoweredBehavior( Thing *thing, const ModuleData* moduleData ) :
	UpdateModule( thing, moduleData ),
	m_powered(TRUE)
{
	// the owner is not known yet, so the first tick reads the power state
	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
PoweredBehavior::~PoweredBehavior()
{
}

//-------------------------------------------------------------------------------------------------
Bool PoweredBehavior::onPowerChange( Bool hasPower )
{
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
	}
	else
	{
		if (draw && d->m_iconName.isNotEmpty())
		{
			draw->setStatusIcon( d->m_iconName );
		}
		m_appliedStatus.set( OBJECT_STATUS_NO_ATTACK, d->m_disableWeapon );
		m_appliedStatus.set( OBJECT_STATUS_IMMOBILE, immobile );
		obj->setStatus( m_appliedStatus );
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
	* 1: Initial version */
//-------------------------------------------------------------------------------------------------
void PoweredBehavior::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	UpdateModule::xfer( xfer );

	xfer->xferBool( &m_powered );
	m_appliedStatus.xfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void PoweredBehavior::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
