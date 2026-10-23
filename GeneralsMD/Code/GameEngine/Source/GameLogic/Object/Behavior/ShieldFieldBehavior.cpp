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

// FILE: ShieldFieldBehavior.cpp //////////////////////////////////////////////////////////////////
// Desc:   A projected shield: a sphere around the object that stops enemy projectiles and
//         instant-hit shots at its surface.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GlobalData.h"
#include "Common/Player.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/ShieldFieldBehavior.h"

// Every live field, so a shot can ask them all; construction order keeps the walk deterministic.
static std::vector<ShieldFieldBehavior*> s_fields;

//-------------------------------------------------------------------------------------------------
ShieldFieldBehaviorModuleData::ShieldFieldBehaviorModuleData()
{
	m_radius = 0.0f;
	m_coverageAngle = PI;
	m_blockProjectiles = TRUE;
	m_blockBeams = TRUE;
	m_damageSelf = FALSE;
	m_hitFX = nullptr;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void ShieldFieldBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "Radius",						INI::parseReal,				nullptr, offsetof( ShieldFieldBehaviorModuleData, m_radius ) },
		{ "CoverageAngle",		INI::parseAngleReal,	nullptr, offsetof( ShieldFieldBehaviorModuleData, m_coverageAngle ) },
		{ "BlockProjectiles",	INI::parseBool,				nullptr, offsetof( ShieldFieldBehaviorModuleData, m_blockProjectiles ) },
		{ "BlockBeams",				INI::parseBool,				nullptr, offsetof( ShieldFieldBehaviorModuleData, m_blockBeams ) },
		{ "DamageSelf",				INI::parseBool,				nullptr, offsetof( ShieldFieldBehaviorModuleData, m_damageSelf ) },
		{ "HitFX",						INI::parseFXList,			nullptr, offsetof( ShieldFieldBehaviorModuleData, m_hitFX ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
	p.add(UpgradeMuxData::getFieldParse(), offsetof( ShieldFieldBehaviorModuleData, m_upgradeMuxData ));
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ShieldFieldBehavior::ShieldFieldBehavior( Thing *thing, const ModuleData* moduleData ) :
	UpdateModule( thing, moduleData )
{
	DEBUG_ASSERTCRASH( getShieldFieldBehaviorModuleData()->m_radius > 0.0f, ("ShieldFieldBehavior on '%s' has no Radius", getObject()->getTemplate()->getName().str()) );

	s_fields.push_back( this );
}

//-------------------------------------------------------------------------------------------------
ShieldFieldBehavior::~ShieldFieldBehavior()
{
	for( size_t i = 0; i < s_fields.size(); ++i )
	{
		if( s_fields[i] == this )
		{
			s_fields.erase( s_fields.begin() + i );
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime ShieldFieldBehavior::update()
{
	const ShieldFieldBehaviorModuleData *d = getShieldFieldBehaviorModuleData();
	if( !d->m_blockProjectiles )
	{
		return UPDATE_SLEEP_FOREVER;
	}
	if( !isUp() )
	{
		// an upgrade-gated field sleeps until upgradeImplementation wakes it
		UpgradeMaskType activation, conflicting;
		getUpgradeActivationMasks( activation, conflicting );
		return (activation.any() && !isAlreadyUpgraded()) ? UPDATE_SLEEP_FOREVER : UPDATE_SLEEP_NONE;
	}

	// Projectiles about to enter next frame are caught too, so a fast one cannot step over the
	// surface or hit its target before this update runs. The margin is one frame of flight.
	const Real stepMargin = 100.0f;

	Object *me = getObject();
	PartitionFilterRelationship relationship( me, PartitionFilterRelationship::ALLOW_ENEMIES | PartitionFilterRelationship::ALLOW_NEUTRAL );
	PartitionFilterAlive filterAlive;
	PartitionFilterAcceptByKindOf filterKindOf( MAKE_KINDOF_MASK( KINDOF_PROJECTILE ), KINDOFMASK_NONE );
	PartitionFilter *filters[] = { &relationship, &filterAlive, &filterKindOf, nullptr };

	ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( me->getPosition(), d->m_radius + stepMargin, FROM_CENTER_3D, filters );
	MemoryPoolObjectHolder hold( iter );
	for( Object *other = iter->first(); other; other = iter->next() )
	{
		if( other->isDestroyed() )
		{
			continue;
		}

		ProjectileUpdateInterface *pui = other->getProjectileUpdateInterface();
		if( pui && pui->projectileIsLingering() )
		{
			continue;
		}

		// A shot fired inside the sphere never crossed its surface. With no launch point on record
		// the projectile's own position stands in for the coverage test only.
		Coord3D origin;
		const Bool originKnown = pui && pui->projectileGetLaunchPos( origin );
		if( originKnown && isInside( &origin ) )
		{
			continue;
		}
		if( !covers( originKnown ? &origin : other->getPosition() ) )
		{
			continue;
		}

		Coord3D hit = *other->getPosition();
		if( !isInside( &hit ) )
		{
			const PhysicsBehavior *physics = other->getPhysics();
			if( physics == nullptr )
			{
				continue;
			}
			Coord3D next = hit;
			next.add( *physics->getVelocity() );
			Real t;
			if( !findEntry( &hit, &next, &t ) )
			{
				continue;
			}
			hit.x += (next.x - hit.x) * t;
			hit.y += (next.y - hit.y) * t;
			hit.z += (next.z - hit.z) * t;
		}

		stopProjectile( other, &hit );
	}

	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
/*static*/ ShieldFieldBehavior* ShieldFieldBehavior::findBlockingField( const Object* shooter, const Coord3D* from, const Coord3D* to, Coord3D* hitPos )
{
	ShieldFieldBehavior *best = nullptr;
	Real bestT = 0.0f;
	for( size_t i = 0; i < s_fields.size(); ++i )
	{
		ShieldFieldBehavior *field = s_fields[i];

		// A sphere farther from the shot's start than the shot's length plus the radius cannot touch it; (a+b)^2 <= 2(a^2+b^2) keeps this sqrt-free.
		const Coord3D *center = field->getObject()->getPosition();
		Real radius = field->getShieldFieldBehaviorModuleData()->m_radius;
		Real fx = from->x - center->x;
		Real fy = from->y - center->y;
		Real fz = from->z - center->z;
		Real sx = to->x - from->x;
		Real sy = to->y - from->y;
		Real sz = to->z - from->z;
		if( fx * fx + fy * fy + fz * fz > 2.0f * (sx * sx + sy * sy + sz * sz + radius * radius) )
		{
			continue;
		}

		if( !field->getShieldFieldBehaviorModuleData()->m_blockBeams || !field->isUp() || !field->blocksShotsFrom( shooter ) || !field->covers( from ) )
		{
			continue;
		}

		Real t;
		if( !field->findEntry( from, to, &t ) )
		{
			continue;
		}

		// Nearest surface wins; the lower object ID settles an exact tie the same way on every machine.
		if( best == nullptr || t < bestT || (t == bestT && field->getObject()->getID() < best->getObject()->getID()) )
		{
			best = field;
			bestT = t;
		}
	}

	if( best )
	{
		hitPos->x = from->x + (to->x - from->x) * bestT;
		hitPos->y = from->y + (to->y - from->y) * bestT;
		hitPos->z = from->z + (to->z - from->z) * bestT;
	}
	return best;
}

//-------------------------------------------------------------------------------------------------
void ShieldFieldBehavior::absorbShot( const WeaponTemplate* weapon, const WeaponBonus& bonus, ObjectID sourceID, const Coord3D* at )
{
	const ShieldFieldBehaviorModuleData *d = getShieldFieldBehaviorModuleData();

	FXList::doFXPos( d->m_hitFX, at );

	if( !d->m_damageSelf || weapon == nullptr )
	{
		return;
	}

	DamageInfo damageInfo;
	damageInfo.in.m_damageType = weapon->getDamageType();
	damageInfo.in.m_deathType = weapon->getDeathType();
	damageInfo.in.m_sourceID = sourceID;
	damageInfo.in.m_amount = weapon->getPrimaryDamage( bonus );
	const Object *source = TheGameLogic->findObjectByID( sourceID );
	if( source )
	{
		damageInfo.in.m_sourceTemplate = source->getTemplate();
		if( source->getControllingPlayer() )
		{
			damageInfo.in.m_sourcePlayerMask = source->getControllingPlayer()->getPlayerMask();
		}
	}
	getObject()->attemptDamage( &damageInfo );
}

//-------------------------------------------------------------------------------------------------
Bool ShieldFieldBehavior::isUp() const
{
	// Only states that cut the object's systems drop the field; being held, carried or unmanned does not.
	DisabledMaskType systemsDown = MAKE_DISABLED_MASK5( DISABLED_DEFAULT, DISABLED_HACKED, DISABLED_EMP, DISABLED_UNDERPOWERED, DISABLED_SUBDUED );
	systemsDown.set( MAKE_DISABLED_MASK3( DISABLED_PARALYZED, DISABLED_SCRIPT_UNDERPOWERED, DISABLED_FROZEN ) );

	const Object *me = getObject();
	if( me->isEffectivelyDead() || me->getDisabledFlags().testForAny( systemsDown ) || me->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
	{
		return FALSE;
	}

	// tested live rather than latched, so ConflictsWith can switch the field off again
	UpgradeMaskType maskToCheck = me->getObjectCompletedUpgradeMask();
	const Player *owner = me->getControllingPlayer();
	if( owner )
	{
		maskToCheck.set( owner->getCompletedUpgradeMask() );
	}
	return testUpgradeConditions( maskToCheck );
}

//-------------------------------------------------------------------------------------------------
Bool ShieldFieldBehavior::blocksShotsFrom( const Object* shooter ) const
{
	if( shooter == nullptr || shooter == getObject() )
	{
		return FALSE;
	}
	return getObject()->getRelationship( shooter ) != ALLIES;
}

//-------------------------------------------------------------------------------------------------
Bool ShieldFieldBehavior::isInside( const Coord3D* pos ) const
{
	const Coord3D *center = getObject()->getPosition();
	Real dx = pos->x - center->x;
	Real dy = pos->y - center->y;
	Real dz = pos->z - center->z;
	Real radius = getShieldFieldBehaviorModuleData()->m_radius;
	return dx * dx + dy * dy + dz * dz <= radius * radius;
}

//-------------------------------------------------------------------------------------------------
Bool ShieldFieldBehavior::covers( const Coord3D* origin ) const
{
	Real coverageAngle = getShieldFieldBehaviorModuleData()->m_coverageAngle;
	if( coverageAngle >= PI )
	{
		return TRUE;
	}

	const Coord3D *center = getObject()->getPosition();
	Coord3D toOrigin;
	toOrigin.x = origin->x - center->x;
	toOrigin.y = origin->y - center->y;
	toOrigin.z = 0.0f;
	Real len = toOrigin.length();
	if( len < 0.01f )
	{
		return TRUE;
	}

	const Coord3D *facing = getObject()->getUnitDirectionVector2D();
	Real cosAngle = (facing->x * toOrigin.x + facing->y * toOrigin.y) / len;
	return cosAngle >= WWMath::Cos( coverageAngle );
}

//-------------------------------------------------------------------------------------------------
// Entry parameter of the from-to segment into the sphere; a segment that starts inside never enters.
Bool ShieldFieldBehavior::findEntry( const Coord3D* from, const Coord3D* to, Real* t ) const
{
	const Coord3D *center = getObject()->getPosition();
	Real radius = getShieldFieldBehaviorModuleData()->m_radius;

	Coord3D f;
	f.x = from->x - center->x;
	f.y = from->y - center->y;
	f.z = from->z - center->z;
	Real c = f.x * f.x + f.y * f.y + f.z * f.z - radius * radius;
	if( c <= 0.0f )
	{
		return FALSE;
	}

	Coord3D dir;
	dir.x = to->x - from->x;
	dir.y = to->y - from->y;
	dir.z = to->z - from->z;
	Real a = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
	if( a <= 0.0f )
	{
		return FALSE;
	}

	Real b = 2.0f * (f.x * dir.x + f.y * dir.y + f.z * dir.z);
	Real discriminant = b * b - 4.0f * a * c;
	if( discriminant < 0.0f )
	{
		return FALSE;
	}

	*t = (-b - WWMath::Sqrt( discriminant )) / (2.0f * a);
	return *t >= 0.0f && *t <= 1.0f;
}

//-------------------------------------------------------------------------------------------------
// The projectile vanishes without detonating or dying, so its warhead and die modules stay quiet.
void ShieldFieldBehavior::stopProjectile( Object* projectile, const Coord3D* at )
{
	const WeaponTemplate *weapon = nullptr;
	ObjectID launcherID = INVALID_ID;
	ProjectileUpdateInterface *pui = projectile->getProjectileUpdateInterface();
	if( pui )
	{
		weapon = pui->projectileGetDetonationWeapon();
		launcherID = pui->projectileGetLauncherID();
	}

	// The bonus a detonation would have used: the projectile's own flags plus the launcher's.
	WeaponBonus bonus;
	bonus.clear();
	if( weapon )
	{
		WeaponBonusConditionFlags flags = projectile->getWeaponBonusCondition();
		const Object *launcher = TheGameLogic->findObjectByID( launcherID );
		if( launcher )
		{
			flags.set( launcher->getWeaponBonusCondition() );
		}
		if( TheGlobalData->m_weaponBonusSet )
		{
			TheGlobalData->m_weaponBonusSet->appendBonuses( flags, bonus );
		}
		if( weapon->getExtraBonus() )
		{
			weapon->getExtraBonus()->appendBonuses( flags, bonus );
		}
	}

	absorbShot( weapon, bonus, launcherID != INVALID_ID ? launcherID : projectile->getID(), at );
	TheGameLogic->destroyObject( projectile );
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void ShieldFieldBehavior::crc( Xfer *xfer )
{
	// extend base class
	UpdateModule::crc( xfer );

	// extend upgrade mux
	UpgradeMux::upgradeMuxCRC( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void ShieldFieldBehavior::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// extend upgrade mux
	UpgradeMux::upgradeMuxXfer( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void ShieldFieldBehavior::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

	// extend upgrade mux
	UpgradeMux::upgradeMuxLoadPostProcess();

}
