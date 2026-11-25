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

#pragma once

#include "GameLogic/AIPathfind.h"

// Counters behind the once-a-second PFSTAT log line
struct PathfindStats
{
	UnsignedInt droppedQueue;      ///< requests refused by a full queue
	UnsignedInt searches;          ///< searches started
	UnsignedInt cappedSearches;    ///< searches that returned a partial path at their cap
	UnsignedInt hierOk;            ///< coarse searches that found a corridor
	UnsignedInt hierFail;          ///< coarse searches that failed
	UnsignedInt cells;             ///< cell records released by searches
	UnsignedInt maxCellsOneSearch; ///< largest single search
	UnsignedInt zoneOverflow;      ///< cells that ran past the zone table
	UnsignedInt rawZoneHighWater;  ///< raw zone count of the last zone calculation
	UnsignedInt byType[PFS_SEARCH_COUNT];
	UnsignedInt events[PFE_EVENT_COUNT];
};
extern PathfindStats s_pathfindStats;
