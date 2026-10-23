// FILE: FireWeaponAdvancedUpdate.h /////////////////////////////////////////////////////////////////////////
// Desc: expanded FireWeaponUpdate with more Parameters
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __FIRE_WEAPON_ADVANCED_UPDATE_H_
#define __FIRE_WEAPON_ADVANCED_UPDATE_H_

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/UpgradeModule.h"
#include "GameLogic/Weapon.h"
#include <GameClient/RadiusDecal.h>

//-------------------------------------------------------------------------------------------------
class FireWeaponAdvancedUpdateModuleData : public UpdateModuleData
{
public:
	UpgradeMuxData m_upgradeMuxData;
	const WeaponTemplate* m_weaponTemplate;
  UnsignedInt m_initialDelayFrames;
	UnsignedInt m_exclusiveWeaponDelay;	///< If non-zero, any other weapon having fired this recently will keep us from doing anything
	std::vector<Coord2D> m_scatterTargets;
	Real m_scatterTargetScalar;
	Real m_fireHeight;
	Real m_lockOnRadius;
	bool m_scatterOnLockedStructuresMajorRadius;
	Real m_fireOffsetXMin;
	Real m_fireOffsetXMax;
	Real m_fireOffsetYMin;
	Real m_fireOffsetYMax;
	RadiusDecalTemplate m_decalTemplate;
	Real m_decalRadius;
	UnsignedInt m_decalDuration;
	Real m_scatterRadius;

	FireWeaponAdvancedUpdateModuleData();

	static void parseScatterTarget(INI* ini, void* instance, void* /*store*/, const void* /*userData*/);
	static void buildFieldParse(MultiIniFieldParse& p);

private:

};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class FireWeaponAdvancedUpdate : public UpdateModule, public UpgradeMux
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( FireWeaponAdvancedUpdate, "FireWeaponAdvancedUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( FireWeaponAdvancedUpdate, FireWeaponAdvancedUpdateModuleData )

public:

	FireWeaponAdvancedUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	static Int getInterfaceMask() { return UpdateModule::getInterfaceMask() | MODULEINTERFACE_UPGRADE; }

	virtual UpgradeModuleInterface* getUpgrade() { return this; }

	virtual UpdateSleepTime update();

protected:

	// firing is gated on the live mask, so the upgrade itself only drives FX and removal
	virtual void upgradeImplementation()
	{
	}

	virtual void getUpgradeActivationMasks(UpgradeMaskType& activation, UpgradeMaskType& conflicting) const
	{
		getFireWeaponAdvancedUpdateModuleData()->m_upgradeMuxData.getUpgradeActivationMasks(activation, conflicting);
	}

	virtual void performUpgradeFX()
	{
		getFireWeaponAdvancedUpdateModuleData()->m_upgradeMuxData.performUpgradeFX(getObject());
	}

	virtual void processUpgradeRemoval()
	{
		getFireWeaponAdvancedUpdateModuleData()->m_upgradeMuxData.muxDataProcessUpgradeRemoval(getObject());
	}

	virtual Bool requiresAllActivationUpgrades() const
	{
		return getFireWeaponAdvancedUpdateModuleData()->m_upgradeMuxData.m_requiresAllTriggers;
	}

	virtual Bool isSubObjectsUpgrade() { return false; }

	Bool isOkayToFire();
	Coord3D getNextTargetPos();
	void adjustFireHeight(Object* targetLock, Coord3D* targetPos);

	Weapon* m_weapon;
  UnsignedInt m_initialDelayFrame;
	Coord3D m_initialPosition;
	bool m_initialized;
	RadiusDecal										m_deliveryDecal;
	UnsignedInt m_radiusDecalRemoveFrame;
};

#endif

