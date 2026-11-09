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

// FILE: ShieldFieldBehavior.h ////////////////////////////////////////////////////////////////////
// Desc:   A projected shield: a sphere around the object that stops enemy projectiles and
//         instant-hit shots at its surface. Projectiles are caught by the per-frame scan here,
//         instant-hit weapons ask findBlockingField before they deal damage.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/UpgradeModule.h"

class FXList;
class WeaponTemplate;
class WeaponBonus;

//-------------------------------------------------------------------------------------------------
class ShieldFieldBehaviorModuleData : public UpdateModuleData
{
public:
	UpgradeMuxData m_upgradeMuxData;
	Real m_radius;							///< sphere radius around the object's position
	Real m_coverageAngle;				///< radians each side of the facing a shot's origin must lie in; PI covers all round
	Bool m_blockProjectiles;
	Bool m_blockBeams;					///< weapons without a ProjectileObject: lasers, tracer guns
	Bool m_damageSelf;					///< a stopped shot damages this object through its armor
	const FXList* m_hitFX;			///< played where a shot stops

	ShieldFieldBehaviorModuleData();

	static void buildFieldParse(MultiIniFieldParse& p);
};

//-------------------------------------------------------------------------------------------------
class ShieldFieldBehavior : public UpdateModule, public UpgradeMux
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ShieldFieldBehavior, "ShieldFieldBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ShieldFieldBehavior, ShieldFieldBehaviorModuleData )

public:

	ShieldFieldBehavior( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	static Int getInterfaceMask() { return UpdateModule::getInterfaceMask() | MODULEINTERFACE_UPGRADE; }

	virtual UpgradeModuleInterface* getUpgrade() override { return this; }

	virtual UpdateSleepTime update() override;

	/// The field whose surface a shot from shooter crosses first on its way from from to to, with the crossing point.
	static ShieldFieldBehavior* findBlockingField( const Object* shooter, const Coord3D* from, const Coord3D* to, Coord3D* hitPos );

	/// Plays the hit FX at the stop point and, with DamageSelf and a weapon, lands the shot's damage on the object.
	void absorbShot( const WeaponTemplate* weapon, const WeaponBonus& bonus, ObjectID sourceID, const Coord3D* at );

protected:

	// the field is gated on the live upgrade mask; the upgrade only wakes a sleeping scan
	virtual void upgradeImplementation() override
	{
		setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
	}

	virtual void getUpgradeActivationMasks(UpgradeMaskType& activation, UpgradeMaskType& conflicting) const override
	{
		getShieldFieldBehaviorModuleData()->m_upgradeMuxData.getUpgradeActivationMasks(activation, conflicting);
	}

	virtual void performUpgradeFX() override
	{
		getShieldFieldBehaviorModuleData()->m_upgradeMuxData.performUpgradeFX(getObject());
	}

	virtual void processUpgradeRemoval() override
	{
		getShieldFieldBehaviorModuleData()->m_upgradeMuxData.muxDataProcessUpgradeRemoval(getObject());
	}

	virtual Bool requiresAllActivationUpgrades() const override
	{
		return getShieldFieldBehaviorModuleData()->m_upgradeMuxData.m_requiresAllTriggers;
	}

	virtual Bool isSubObjectsUpgrade() override { return false; }

private:

	Bool isUp() const;
	Bool blocksShotsFrom( const Object* shooter ) const;
	Bool isInside( const Coord3D* pos ) const;
	Bool covers( const Coord3D* origin ) const;
	Bool findEntry( const Coord3D* from, const Coord3D* to, Real* t ) const;
	void stopProjectile( Object* projectile, const Coord3D* at );

};
