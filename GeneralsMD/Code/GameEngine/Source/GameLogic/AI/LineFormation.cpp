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

// LineFormation.cpp
// Drag-to-face formation slots, see LineFormation.h.

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/LineFormation.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Lib/trig.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"

namespace
{
struct Ranked
{
	Int index;
	Real key1;
	Real key2;
	UnsignedInt id;
};

// key1 descending, then key2 descending, then id ascending: a total order, so std::sort gives one answer.
Bool rankedBefore( const Ranked &a, const Ranked &b )
{
	if( a.key1 != b.key1 )
	{
		return a.key1 > b.key1;
	}
	if( a.key2 != b.key2 )
	{
		return a.key2 > b.key2;
	}
	return a.id < b.id;
}
} // namespace

//-------------------------------------------------------------------------------------------------
UnsignedInt parseFormationStyle( const char *order, const char *wheels, const char *speed )
{
	UnsignedInt style = 0;
	if( order && stricmp( order, "Nearest" ) == 0 )
	{
		style |= FORMATION_STYLE_NEAREST;
	}
	if( wheels && stricmp( wheels, "TurnInPlace" ) == 0 )
	{
		style |= FORMATION_STYLE_WHEELS_TURN_IN_PLACE;
	}
	if( speed && stricmp( speed, "Own" ) == 0 )
	{
		style |= FORMATION_STYLE_SPEED_OWN;
	}
	else if( speed && stricmp( speed, "ArriveTogether" ) == 0 )
	{
		style |= FORMATION_STYLE_SPEED_ARRIVE;
	}
	else
	{
		style |= FORMATION_STYLE_SPEED_MARCH;	// the family's formation move (C&C3, ZH Ctrl+F)
	}
	return style;
}

//-------------------------------------------------------------------------------------------------
Real formationUnitSpeed( UnsignedInt style, Real distance, Real ownSpeed, Real slowest, Real arrivalTime )
{
	switch( style & FORMATION_STYLE_SPEED_MASK )
	{
		case FORMATION_STYLE_SPEED_MARCH:
			return slowest > 0.0f && slowest < ownSpeed ? slowest : 0.0f;
		case FORMATION_STYLE_SPEED_ARRIVE:
		{
			if( arrivalTime <= 0.0f || ownSpeed <= 0.0f )
			{
				return 0.0f;
			}
			Real speed = distance / arrivalTime;
			const Real floor = ownSpeed * FORMATION_ARRIVE_MIN_SPEED;
			if( speed < floor )
			{
				speed = floor;
			}
			return speed < ownSpeed ? speed : 0.0f;
		}
		default:
			return 0.0f;
	}
}

//-------------------------------------------------------------------------------------------------
Int lineFormationRows( Real dragDistance, Int count )
{
	if( count <= 1 || dragDistance <= FORMATION_ROW_START )
	{
		return 1;
	}
	const Int rows = 1 + (Int)((dragDistance - FORMATION_ROW_START) / FORMATION_ROW_STEP);
	return rows < count ? rows : count;
}

//-------------------------------------------------------------------------------------------------
// The Hungarian assignment uses a fixed iteration order to keep peers in sync.
static void assignMinimumCost( const std::vector<double> &cost, Int n, std::vector<Int> &result )
{
	const double INF = 1e300;
	std::vector<double> u( n + 1, 0.0 ), v( n + 1, 0.0 ), minv( n + 1 );
	std::vector<Int> p( n + 1, 0 ), way( n + 1, 0 );
	std::vector<char> used( n + 1 );
	for( Int i = 1; i <= n; ++i )
	{
		p[0] = i;
		Int j0 = 0;
		for( Int j = 0; j <= n; ++j )
		{
			minv[j] = INF;
			used[j] = FALSE;
		}
		do
		{
			used[j0] = TRUE;
			const Int i0 = p[j0];
			double delta = INF;
			Int j1 = 0;
			for( Int j = 1; j <= n; ++j )
			{
				if( used[j] )
				{
					continue;
				}
				const double cur = cost[(i0 - 1) * n + (j - 1)] - u[i0] - v[j];
				if( cur < minv[j] )
				{
					minv[j] = cur;
					way[j] = j0;
				}
				if( minv[j] < delta )
				{
					delta = minv[j];
					j1 = j;
				}
			}
			for( Int j = 0; j <= n; ++j )
			{
				if( used[j] )
				{
					u[p[j]] += delta;
					v[j] -= delta;
				}
				else
				{
					minv[j] -= delta;
				}
			}
			j0 = j1;
		} while( p[j0] != 0 );
		do
		{
			const Int j1 = way[j0];
			p[j0] = p[j1];
			j0 = j1;
		} while( j0 != 0 );
	}
	result.assign( n, 0 );
	for( Int j = 1; j <= n; ++j )
	{
		result[p[j] - 1] = j - 1;
	}
}

//-------------------------------------------------------------------------------------------------
// Toughness bands restrict slot choices before minimum-distance assignment.
void computeLineFormation( const Coord3D &center, Real angle, Int rows, const FormationMember *members, Int count,
	Coord3D *slots, UnsignedInt style )
{
	if( count <= 0 )
	{
		return;
	}
	if( rows < 1 )
	{
		rows = 1;
	}
	if( rows > count )
	{
		rows = count;
	}
	const Int cols = (count + rows - 1) / rows;
	rows = (count + cols - 1) / cols;	// no empty rows
	const Bool nearest = (style & FORMATION_STYLE_NEAREST) != 0;

	const Real fx = Cos( angle );
	const Real fy = Sin( angle );
	const Real lx = -fy;	// lateral: to the left of the facing
	const Real ly = fx;

	// Toughness order: toughest first; among equals, whoever is already further forward.
	std::vector<Ranked> order( count );
	for( Int i = 0; i < count; ++i )
	{
		const Real ox = members[i].pos.x - center.x;
		const Real oy = members[i].pos.y - center.y;
		order[i].index = i;
		order[i].key1 = members[i].toughness;
		order[i].key2 = ox * fx + oy * fy;
		order[i].id = members[i].id;
	}
	std::sort( order.begin(), order.end(), rankedBefore );

	// Row sizes: as the toughness order fills them; Nearest may put anyone anywhere, so every row fits the largest.
	Real largest = 0.0f;
	for( Int i = 0; i < count; ++i )
	{
		if( members[i].radius > largest )
		{
			largest = members[i].radius;
		}
	}
	std::vector<Real> rowRadius( rows, nearest ? largest : 0.0f );
	if( !nearest )
	{
		for( Int k = 0; k < count; ++k )
		{
			const Int r = k / cols;
			const Real radius = members[order[k].index].radius;
			if( radius > rowRadius[r] )
			{
				rowRadius[r] = radius;
			}
		}
	}

	// Row depths: neighbouring rows are their largest radii apart, plus the gap; centred on the chord point.
	std::vector<Real> rowForward( rows, 0.0f );
	Real depth = 0.0f;
	for( Int r = 1; r < rows; ++r )
	{
		depth += rowRadius[r - 1] + rowRadius[r] + FORMATION_GAP;
		rowForward[r] = -depth;
	}
	for( Int r = 0; r < rows; ++r )
	{
		rowForward[r] += depth * 0.5f;
	}

	// The slots, front row first, each row left to right and centred.
	std::vector<Coord3D> slotPos( count );
	std::vector<Int> slotRow( count );
	for( Int r = 0, s = 0; r < rows; ++r )
	{
		const Int n = (r < rows - 1) ? cols : count - (rows - 1) * cols;
		const Real spacing = 2.0f * rowRadius[r] + FORMATION_GAP;
		for( Int j = 0; j < n; ++j, ++s )
		{
			const Real lateral = ((n - 1) * 0.5f - j) * spacing;
			slotPos[s].x = center.x + fx * rowForward[r] + lx * lateral;
			slotPos[s].y = center.y + fy * rowForward[r] + ly * lateral;
			slotPos[s].z = center.z;
			slotRow[s] = r;
		}
	}

	// The rows each unit may take: its band's (ToughFront), any (Nearest).
	std::vector<Int> firstRow( count, 0 ), lastRow( count, rows - 1 );
	if( !nearest )
	{
		for( Int k = 0; k < count; )
		{
			const Real bandTop = members[order[k].index].toughness;
			Int end = k + 1;
			while( end < count && members[order[end].index].toughness >= bandTop * (1.0f - FORMATION_BAND_WIDTH) )
			{
				++end;
			}
			for( Int b = k; b < end; ++b )
			{
				firstRow[order[b].index] = k / cols;
				lastRow[order[b].index] = (end - 1) / cols;
			}
			k = end;
		}
	}

	// Members in ObjectID order, so the result does not depend on the order the caller lists them in.
	std::vector<Ranked> byId( count );
	for( Int i = 0; i < count; ++i )
	{
		byId[i].index = i;
		byId[i].key1 = 0.0f;
		byId[i].key2 = 0.0f;
		byId[i].id = members[i].id;
	}
	std::sort( byId.begin(), byId.end(), rankedBefore );

	// Cost: the distance to the slot; a row outside the unit's band costs more than any distance can.
	const double OUTSIDE_BAND = 1.0e7;
	std::vector<double> cost( (size_t)count * count );
	for( Int a = 0; a < count; ++a )
	{
		const FormationMember &m = members[byId[a].index];
		for( Int s = 0; s < count; ++s )
		{
			const double dx = slotPos[s].x - m.pos.x;
			const double dy = slotPos[s].y - m.pos.y;
			double c = sqrt( dx * dx + dy * dy );
			const Int r = slotRow[s];
			if( r < firstRow[byId[a].index] )
			{
				c += OUTSIDE_BAND * (firstRow[byId[a].index] - r);
			}
			else if( r > lastRow[byId[a].index] )
			{
				c += OUTSIDE_BAND * (r - lastRow[byId[a].index]);
			}
			cost[(size_t)a * count + s] = c;
		}
	}
	std::vector<Int> assigned;
	assignMinimumCost( cost, count, assigned );
	for( Int a = 0; a < count; ++a )
	{
		slots[byId[a].index] = slotPos[assigned[a]];
	}
}
//-------------------------------------------------------------------------------------------------
Real formationApproachDistance( Real radius )
{
	const Real d = radius * FORMATION_APPROACH_RADII;
	return d > FORMATION_APPROACH_MIN ? d : FORMATION_APPROACH_MIN;
}

//-------------------------------------------------------------------------------------------------
Coord3D formationApproachPoint( const Coord3D &slot, Real angle, Real approachDistance )
{
	Coord3D p = slot;
	p.x -= Cos( angle ) * approachDistance;
	p.y -= Sin( angle ) * approachDistance;
	return p;
}

//-------------------------------------------------------------------------------------------------
Bool needsFormationApproach( const Coord3D &unit, const Coord3D &slot, Real angle, Real approachDistance )
{
	const Real dx = slot.x - unit.x;
	const Real dy = slot.y - unit.y;
	const Real ahead = dx * Cos( angle ) + dy * Sin( angle );	// how far the slot lies ahead of the unit, along the facing
	const Real dist = sqrtf( dx * dx + dy * dy );
	return ahead < approachDistance || ahead < dist * FORMATION_APPROACH_CONE_COS;
}

//-------------------------------------------------------------------------------------------------
Bool canMoveExactlyToSlot( const Coord3D &slot, const Coord3D &adjusted )
{
	const Real dx = adjusted.x - slot.x;
	const Real dy = adjusted.y - slot.y;
	return dx * dx + dy * dy < PATHFIND_CELL_SIZE_F * PATHFIND_CELL_SIZE_F;
}

//-------------------------------------------------------------------------------------------------
Bool isLineFormationMover( const Object *obj )
{
	return obj
		&& !obj->isDisabledByType( DISABLED_HELD )
		&& !obj->isKindOf( KINDOF_IMMOBILE )
		&& obj->getAI() != nullptr;
}

//-------------------------------------------------------------------------------------------------
FormationMember makeFormationMember( const Object *obj )
{
	FormationMember m;
	m.id = (UnsignedInt)obj->getID();
	m.pos = *obj->getPosition();
	m.radius = obj->getGeometryInfo().getBoundingCircleRadius();
	const BodyModuleInterface *body = obj->getBodyModule();
	m.toughness = body ? body->getMaxHealth() : 0.0f;
	return m;
}
