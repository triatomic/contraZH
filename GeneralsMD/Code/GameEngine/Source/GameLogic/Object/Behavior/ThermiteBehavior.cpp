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

// FILE: ThermiteBehavior.cpp ////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/RandomValue.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/WeaponStatus.h"
#include "GameLogic/Module/ThermiteBehavior.h"

//-------------------------------------------------------------------------------------------------
ThermiteBehaviorModuleData::ThermiteBehaviorModuleData()
{
	m_weaponTemplate = nullptr;
	m_minFrames = 0;
	m_maxFrames = 0;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void ThermiteBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "Weapon",				INI::parseWeaponTemplate,				nullptr, offsetof( ThermiteBehaviorModuleData, m_weaponTemplate ) },
		{ "MinLifetime",	INI::parseDurationUnsignedInt,	nullptr, offsetof( ThermiteBehaviorModuleData, m_minFrames ) },
		{ "MaxLifetime",	INI::parseDurationUnsignedInt,	nullptr, offsetof( ThermiteBehaviorModuleData, m_maxFrames ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
	p.add(UpgradeMuxData::getFieldParse(), offsetof( ThermiteBehaviorModuleData, m_upgradeMuxData ));
}

//-------------------------------------------------------------------------------------------------
ThermiteBehavior::ThermiteBehavior( Thing *thing, const ModuleData* moduleData ) :
	UpdateModule( thing, moduleData ),
	m_weapon(nullptr),
	m_victimID(INVALID_ID),
	m_endFrame(0)
{
	setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
}

//-------------------------------------------------------------------------------------------------
ThermiteBehavior::~ThermiteBehavior()
{
	deleteInstance(m_weapon);
}

//-------------------------------------------------------------------------------------------------
/*static*/ Bool ThermiteBehavior::tryIgnite( Object *projectile, Object *victim )
{
	static NameKeyType key_ThermiteBehavior = NAMEKEY( "ThermiteBehavior" );
	ThermiteBehavior *thermite = (ThermiteBehavior*)projectile->findUpdateModule( key_ThermiteBehavior );
	if (thermite == nullptr)
	{
		return FALSE;
	}

	return thermite->ignite( victim );
}

//-------------------------------------------------------------------------------------------------
Bool ThermiteBehavior::isTriggered() const
{
	const ThermiteBehaviorModuleData *d = getThermiteBehaviorModuleData();
	const Object *obj = getObject();

	UpgradeMaskType activation, conflicting;
	d->m_upgradeMuxData.getUpgradeActivationMasks( activation, conflicting );
	if (!activation.any() && !conflicting.any())
	{
		return TRUE;
	}

	// the projectile is born without upgrades, so look at the player and the launcher too
	UpgradeMaskType keyMask = obj->getObjectCompletedUpgradeMask();
	if (obj->getControllingPlayer())
	{
		keyMask.set( obj->getControllingPlayer()->getCompletedUpgradeMask() );
	}
	const Object *launcher = TheGameLogic->findObjectByID( obj->getProducerID() );
	if (launcher)
	{
		keyMask.set( launcher->getObjectCompletedUpgradeMask() );
	}

	if (!keyMask.testForNone( conflicting ))
	{
		return FALSE;
	}
	if (!activation.any())
	{
		return TRUE;
	}
	return d->m_upgradeMuxData.m_requiresAllTriggers ? keyMask.testForAll( activation ) : keyMask.testForAny( activation );
}

//-------------------------------------------------------------------------------------------------
Bool ThermiteBehavior::ignite( Object *victim )
{
	const ThermiteBehaviorModuleData *d = getThermiteBehaviorModuleData();
	if (d->m_weaponTemplate == nullptr || m_endFrame != 0 || !isTriggered())
	{
		return FALSE;
	}

	Object *obj = getObject();

	m_weapon = TheWeaponStore->allocateNewWeapon( d->m_weaponTemplate, PRIMARY_WEAPON );
	m_weapon->loadAmmoNow( obj );

	UnsignedInt lifetime = GameLogicRandomValue( d->m_minFrames, d->m_maxFrames );
	if (lifetime < 1)
	{
		lifetime = 1;
	}
	m_endFrame = TheGameLogic->getFrame() + lifetime;
	m_victimID = victim ? victim->getID() : INVALID_ID;

	// held stops physics from pulling the burn off its victim
	obj->setDisabled( DISABLED_HELD );
	obj->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_NO_COLLISIONS ) );
	if (obj->getDrawable())
	{
		obj->getDrawable()->setDrawableHidden( true );
	}

	if (victim == nullptr)
	{
		dropToGround();
	}

	setWakeFrame( obj, UPDATE_SLEEP_NONE );
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
void ThermiteBehavior::dropToGround()
{
	Object *obj = getObject();
	m_victimID = INVALID_ID;
	obj->setPositionZ( TheTerrainLogic->getGroundHeight( obj->getPosition()->x, obj->getPosition()->y ) );
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime ThermiteBehavior::update()
{
	if (m_endFrame == 0)
	{
		return UPDATE_SLEEP_FOREVER;
	}

	Object *obj = getObject();
	UnsignedInt now = TheGameLogic->getFrame();
	if (now >= m_endFrame)
	{
		TheGameLogic->destroyObject( obj );
		return UPDATE_SLEEP_FOREVER;
	}

	Object *victim = nullptr;
	if (m_victimID != INVALID_ID)
	{
		victim = TheGameLogic->findObjectByID( m_victimID );
		if (victim == nullptr || victim->isEffectivelyDead() || victim->getContainedBy() != nullptr)
		{
			victim = nullptr;
			dropToGround();
		}
	}

	Bool follows = victim && !victim->isKindOf( KINDOF_IMMOBILE );
	if (follows)
	{
		Coord3D pos;
		victim->getGeometryInfo().getCenterPosition( *victim->getPosition(), pos );
		obj->setPosition( &pos );
	}

	if (m_weapon->getStatus() == READY_TO_FIRE)
	{
		if (victim)
		{
			m_weapon->fireWeapon( obj, victim );
		}
		else
		{
			m_weapon->forceFireWeapon( obj, obj->getPosition() );
		}
	}

	if (follows)
	{
		return UPDATE_SLEEP_NONE;
	}
	return frameToSleepTime( m_weapon->getPossibleNextShotFrame(), m_endFrame );
}

//-------------------------------------------------------------------------------------------------
void ThermiteBehavior::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
/** Version Info:
	* 1: Initial version */
//-------------------------------------------------------------------------------------------------
void ThermiteBehavior::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	UpdateModule::xfer( xfer );

	xfer->xferObjectID( &m_victimID );
	xfer->xferUnsignedInt( &m_endFrame );

	// the weapon only exists once ignited
	if (m_endFrame != 0)
	{
		if (m_weapon == nullptr)
		{
			m_weapon = TheWeaponStore->allocateNewWeapon( getThermiteBehaviorModuleData()->m_weaponTemplate, PRIMARY_WEAPON );
		}
		xfer->xferSnapshot( m_weapon );
	}
}

//-------------------------------------------------------------------------------------------------
void ThermiteBehavior::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
