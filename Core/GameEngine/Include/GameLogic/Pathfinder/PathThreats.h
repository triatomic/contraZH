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

// Team route threat helpers that the pathfinder search and Path smoothing share.

#pragma once

#if RTS_ZEROHOUR
#include "GameLogic/Pathfinder/Path.h"

enum ThreatClearance { THREAT_BUFFER, THREAT_FIRING_RANGE, THREAT_SOFT_COST };

// The fine search and path smoothing share the same threat costs.
extern const TeamRouteThreats *s_routeThreats;

Real exposedLength(const TeamRouteThreats &threats, const Coord3D &from, const Coord3D &to);
Bool canCrossThreats(const TeamRouteThreats &threats, const Coord3D &from, const Coord3D &to, ThreatClearance clearance);
#endif
