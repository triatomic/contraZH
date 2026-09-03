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

// FILE: TornadoUpdate.h /////////////////////////////////////////////////////////////////////////
// Desc: Pulls nearby objects toward this object, lifts and spins them, damages them, and drops
//       them when the effect fades. The module never moves its own object, so a drifting tornado
//       is simply an object with its own locomotor.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __TORNADO_UPDATE_H_
#define __TORNADO_UPDATE_H_

#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Damage.h"

//-------------------------------------------------------------------------------------------------
class TornadoUpdateModuleData : public UpdateModuleData
{
public:

	TornadoUpdateModuleData();

	Real				m_radius;					///< how far from this object victims are grabbed
	Real				m_pullForce;				///< inward speed toward the center, per frame
	Real				m_liftForce;				///< how fast victims climb toward MaxLiftHeight, per frame
	Real				m_spinForce;				///< orbit speed around the center; negative orbits the other way
	Real				m_yawRate;					///< how fast a victim spins about its own axis
	Real				m_maxLiftHeight;			///< above this height over the tornado's ground, lift stops
	Real				m_maxVictimSpeed;			///< speed cap on victims, 0 disables the cap
	Real				m_massReference;			///< victims heavier than this spin slower, 0 disables scaling
	Real				m_releaseSpeed;				///< horizontal speed kept when a victim is let go
	KindOfMaskType		m_requiredKindOf;			///< a victim must be at least one of these
	KindOfMaskType		m_forbiddenKindOf;			///< a victim must be none of these
	Int					m_targetsMask;				///< ALLIES, ENEMIES or NEUTRALS
	Bool				m_affectAirborne;			///< also grab airborne targets
	UnsignedInt			m_rampUpFrames;				///< time to reach full strength
	UnsignedInt			m_fullStrengthFrames;		///< time at full strength, 0 lasts until told to stop
	UnsignedInt			m_rampDownFrames;			///< time to fade from full strength to nothing
	Real				m_damagePerSecond;			///< damage rate at full strength
	Real				m_damageRadius;				///< 0 uses Radius
	UnsignedInt			m_damagePulseFrames;		///< time between damage pulses, 0 disables damage
	DamageType			m_damageType;
	DeathType			m_deathType;
	Bool				m_killObjectWhenDone;		///< destroy this object once the effect has faded

	static void buildFieldParse(MultiIniFieldParse& p);
};

//-------------------------------------------------------------------------------------------------
class TornadoUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( TornadoUpdate, "TornadoUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( TornadoUpdate, TornadoUpdateModuleData )

public:

	TornadoUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update( void );

	/// Start fading out now; safe to call repeatedly and on an already fading tornado.
	void beginRampDown( void );

protected:

	Real computeStrength( UnsignedInt now ) const;
	Int buildRelationshipFlags( void ) const;
	Bool canAffect( const Object *obj ) const;
	void captureVictim( Object *obj );
	void holdVictim( Object *obj, const Coord3D *center, Real groundZ, Real strength );
	void releaseVictim( Object *obj );
	void releaseAll( void );
	void doDamagePulse( const Coord3D *center, Real strength );

	ObjectIDVector	m_victims;				///< who we are currently holding, sorted by id
	UnsignedInt		m_startFrame;			///< frame the effect began, valid once m_started is set
	UnsignedInt		m_rampDownStartFrame;	///< frame the fade out began, valid once m_rampingDown is set
	Real			m_rampDownStartStrength;///< strength when the fade out began
	UnsignedInt		m_nextDamagePulseFrame;
	Bool			m_started;
	Bool			m_rampingDown;
	Bool			m_done;
};

#endif
