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

// Formation arrival and displaced-slot rules.

#pragma once

#include "Lib/BaseType.h"

/// A pathless unit can still be between approach legs, so it must also be idle.
Bool shouldFaceOnArrival( UnsignedInt now, UnsignedInt orderFrame, Bool isMoving, Bool hasPath, Bool isIdle );

/// A later order (any frame after the facing order) cancels the pending facing; the move it came with does not.
Bool doesCommandCancelFacing( UnsignedInt now, UnsignedInt orderFrame );

// Allied make-way commands preserve the slot for a later return.

static const UnsignedInt FORMATION_RETURN_DELAY = 30;	///< wait in logic frames so the other unit can pass
static const Real FORMATION_RETURN_MIN = 3.0f;				///< closer than this: only face again
static const Real FORMATION_RETURN_MAX = 150.0f;			///< further (fought or chased meanwhile): forget the slot
static const Int FORMATION_MAX_RETURNS = 3;						///< per formation order: no endless pushing back and forth

/// Player and script orders cancel the slot after the frame that assigned it.
Bool doesCommandKeepFormationSlot( UnsignedInt now, UnsignedInt slotFrame, Bool isMakeWay, Bool isFromAI );

enum FormationReturn CPP_11(: Int)
{
	FORMATION_RETURN_MOVE,		///< go back to the slot (and face)
	FORMATION_RETURN_FACE,		///< still on it: face again
	FORMATION_RETURN_FORGET,	///< too far, or pushed too often: stay, forget the slot
};

FormationReturn decideFormationReturn( Real distToSlot, Int returnsSoFar );
