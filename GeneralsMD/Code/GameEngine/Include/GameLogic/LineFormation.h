/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

// The preview and the logic share these formation rules.

#pragma once

#include "Lib/BaseType.h"

class Object;

struct FormationMember
{
	UnsignedInt id;			///< ObjectID: last tie-break, so the result does not depend on the members' order
	Coord3D pos;				///< where the unit is now
	Real radius;				///< bounding circle
	Real toughness;			///< max health: tougher units take the front rows
};

/// Each order carries its style so every peer computes the same formation.
enum FormationStyle CPP_11(: UnsignedInt)
{
	FORMATION_STYLE_NEAREST							= 0x01,	///< FormationOrder = Nearest: slots by distance only (default ToughFront)
	FORMATION_STYLE_WHEELS_TURN_IN_PLACE	= 0x02,	///< FormationWheels = TurnInPlace: no drive-in (default DriveIn)
	FORMATION_STYLE_SPEED_MASK					= 0x0C,	///< FormationSpeed, one of:
	FORMATION_STYLE_SPEED_OWN						= 0x00,	///< Own: every unit at its own speed
	FORMATION_STYLE_SPEED_MARCH					= 0x04,	///< MarchTogether (default): ground units at the slowest one's speed (as C&C3, ZH Ctrl+F)
	FORMATION_STYLE_SPEED_ARRIVE				= 0x08,	///< ArriveTogether: each its own speed, so all reach their slots together
};

/// Options.ini values (nullptr when the key is absent) to style flags; unknown values keep the default.
UnsignedInt parseFormationStyle( const char *order, const char *wheels, const char *speed );

/// ArriveTogether: no unit slower than this share of its own speed (one near its slot arrives early instead).
static const Real FORMATION_ARRIVE_MIN_SPEED = 0.4f;

/// Zero leaves speed unchanged; arrivalTime is the longest distance divided by own speed.
Real formationUnitSpeed( UnsignedInt style, Real distance, Real ownSpeed, Real slowest, Real arrivalTime );

/// Drag distance beyond which each further step adds a row (world units).
static const Real FORMATION_ROW_START = 40.0f;
static const Real FORMATION_ROW_STEP = 30.0f;
/// Free space kept between neighbours.
static const Real FORMATION_GAP = 5.0f;
/// Units within this fraction of the toughest health can swap rows.
static const Real FORMATION_BAND_WIDTH = 0.15f;

/// Rows for a drag of this length (world units, from the chord point): 1, then one more per step, at most one per unit.
Int lineFormationRows( Real dragDistance, Int count );

/// slots[i] belongs to members[i]; ObjectID breaks ties regardless of member order.
void computeLineFormation( const Coord3D &center, Real angle, Int rows, const FormationMember *members, Int count,
	Coord3D *slots, UnsignedInt style = 0 );

/// Wheeled units approach from behind so their turning pivots do not displace the final slot.
static const Real FORMATION_APPROACH_RADII = 2.5f;
static const Real FORMATION_APPROACH_MIN = 25.0f;
/// Already coming in from behind within this cone (cos 30 degrees): no approach point needed.
static const Real FORMATION_APPROACH_CONE_COS = 0.8660254f;

Real formationApproachDistance( Real radius );

/// The point behind the slot the unit drives in through.
Coord3D formationApproachPoint( const Coord3D &slot, Real angle, Real approachDistance );

/// Units already behind the slot inside the approach cone need no extra waypoint.
Bool needsFormationApproach( const Coord3D &unit, const Coord3D &slot, Real angle, Real approachDistance );

/// Use exact movement only when the pathfinder keeps the destination within one cell.
Bool canMoveExactlyToSlot( const Coord3D &slot, const Coord3D &adjusted );

/// The members that move (the same filter as a normal group move).
Bool isLineFormationMover( const Object *obj );

/// The formation data of a mover.
FormationMember makeFormationMember( const Object *obj );
