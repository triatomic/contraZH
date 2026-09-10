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
	m_unpowered(FALSE),
	m_appliedScalar(1.0f),
	m_appliedLift(1.0f)
{
	// the owner is not known yet, so the first tick reads the power state
	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
PoweredBehavior::~PoweredBehavior()
{
}

//-------------------------------------------------------------------------------------------------
void PoweredBehavior::setPowered( Bool hasPower )
{
	if (m_unpowered == !hasPower)
	{
		return;
	}
	m_unpowered = !hasPower;

	const PoweredBehaviorModuleData *d = getPoweredBehaviorModuleData();
	Object *obj = getObject();
	AIUpdateInterface *ai = obj->getAI();
	Drawable *draw = obj->getDrawable();
	Bool immobile = !d->m_isMobile || d->m_movePenalty >= 1.0f;

	if (m_unpowered)
	{
		if (draw && d->m_iconName.isNotEmpty())
		{
			draw->setStatusIcon( d->m_iconName );
		}

		if (d->m_disableWeapon)
		{
			obj->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_NO_ATTACK ) );
		}

		if (immobile)
		{
			obj->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_IMMOBILE ) );
			if (ai)
			{
				ai->aiIdle( CMD_FROM_AI );
			}
		}
		else if (d->m_movePenalty > 0.0f && ai)
		{
			m_appliedScalar = 1.0f - d->m_movePenalty;
			ai->applySpeedMultiplier( m_appliedScalar );
		}

		if (d->m_liftPenalty > 0.0f && d->m_liftPenalty < 1.0f && ai)
		{
			m_appliedLift = 1.0f - d->m_liftPenalty;
			ai->applyLiftMultiplier( m_appliedLift );
		}
	}
	else
	{
		if (draw)
		{
			draw->clearStatusIcon();
		}
		obj->clearStatus( MAKE_OBJECT_STATUS_MASK2( OBJECT_STATUS_NO_ATTACK, OBJECT_STATUS_IMMOBILE ) );

		if (m_appliedScalar != 1.0f)
		{
			if (ai)
			{
				ai->applySpeedMultiplier( 1.0f / m_appliedScalar );
			}
			m_appliedScalar = 1.0f;
		}

		if (m_appliedLift != 1.0f)
		{
			if (ai)
			{
				ai->applyLiftMultiplier( 1.0f / m_appliedLift );
			}
			m_appliedLift = 1.0f;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void PoweredBehavior::syncToOwner()
{
	const Player *player = getObject()->getControllingPlayer();
	if (player == nullptr)
	{
		return;
	}
	setPowered( player->getEnergy()->hasSufficientPower() );
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

	xfer->xferBool( &m_unpowered );
	xfer->xferReal( &m_appliedScalar );
	xfer->xferReal( &m_appliedLift );
}

//-------------------------------------------------------------------------------------------------
void PoweredBehavior::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
