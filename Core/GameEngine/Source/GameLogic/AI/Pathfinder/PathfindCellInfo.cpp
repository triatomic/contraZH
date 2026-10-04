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

#include "GameLogic/Pathfinder/PathfindCell.h"
#include "GameLogic/Pathfinder/PathfindCellInfo.h"

// TheSuperHackers @feature simonx645 16/11/2026 The pool's size, set when it is made: contraZH's 100,000 records, or
// 150,000 with "pool" (GameLogic/PathTuning.h). A search that runs out of records fails, so the size changes routes.
static UnsignedInt CELL_INFOS_TO_ALLOCATE = 100000;

PathfindCellInfo *PathfindCellInfo::s_infoArray = nullptr;
PathfindCellInfo *PathfindCellInfo::s_firstFree = nullptr;
Int PathfindCellInfo::s_freeCount = 0;

#if RETAIL_COMPATIBLE_PATHFINDING
void PathfindCellInfo::forceCleanPathFindCellInfos()
{
	for (Int i = 0; i < CELL_INFOS_TO_ALLOCATE - 1; i++) {
		s_infoArray[i].m_nextOpen = nullptr;
		s_infoArray[i].m_prevOpen = nullptr;
		s_infoArray[i].m_open = FALSE;
		s_infoArray[i].m_closed = FALSE;
	}
}
#endif

/**
 * Allocates a pool of pathfind cell infos.
 */
void PathfindCellInfo::allocateCellInfos()
{
	releaseCellInfos();
	CELL_INFOS_TO_ALLOCATE = PathTuning::Pool ? PathTuning::POOL_CELLS : 100000;
	s_infoArray = MSGNEW("PathfindCellInfo") PathfindCellInfo[CELL_INFOS_TO_ALLOCATE];	// pool[]ify
	s_infoArray[CELL_INFOS_TO_ALLOCATE-1].m_pathParent = nullptr;
	s_infoArray[CELL_INFOS_TO_ALLOCATE-1].m_isFree = true;
	s_firstFree = s_infoArray;
	for (Int i=0; i<CELL_INFOS_TO_ALLOCATE-1; i++) {
		s_infoArray[i].m_pathParent = &s_infoArray[i+1];
		s_infoArray[i].m_isFree = true;
	}
	s_freeCount = CELL_INFOS_TO_ALLOCATE;
}

/**
 * Releases a pool of pathfind cell infos.
 */
void PathfindCellInfo::releaseCellInfos()
{
	if (s_infoArray==nullptr) {
		return; // haven't allocated any yet.
	}
	Int count=0;
	while (s_firstFree) {
		count++;
		DEBUG_ASSERTCRASH(s_firstFree->m_isFree, ("Should be freed."));
		s_firstFree = s_firstFree->m_pathParent;
	}
	DEBUG_ASSERTCRASH(count==CELL_INFOS_TO_ALLOCATE, ("Error - Allocated cellinfos."));
	delete[] s_infoArray;
	s_infoArray = nullptr;
	s_firstFree = nullptr;
	s_freeCount = 0;
}

/**
 * Gets a pathfindcellinfo.
 */
PathfindCellInfo *PathfindCellInfo::getACellInfo(PathfindCell *cell,const ICoord2D &pos)
{
	PathfindCellInfo *info = s_firstFree;
	if (s_firstFree) {
		DEBUG_ASSERTCRASH(s_firstFree->m_isFree, ("Should be freed."));
		s_firstFree = s_firstFree->m_pathParent;
		s_freeCount--;
		info->m_isFree = false;  // Just allocated it.
		info->m_cell = cell;
		info->m_pos = pos;

		info->m_nextOpen = nullptr;
		info->m_prevOpen = nullptr;
		info->m_pathParent = nullptr;
		info->m_costSoFar = 0;
		info->m_totalCost = 0;
		info->m_open = 0;
		info->m_closed = 0;
		info->m_obstacleID = INVALID_ID;
		info->m_goalUnitID = INVALID_ID;
		info->m_posUnitID = INVALID_ID;
		info->m_goalAircraftID = INVALID_ID;
		info->m_obstacleIsFence = false;
		info->m_obstacleIsTransparent = false;
		info->m_blockedByAlly = false;
	}
	return info;
}

/**
 * Returns a pathfindcellinfo.
 */
void PathfindCellInfo::releaseACellInfo(PathfindCellInfo *theInfo)
{
	DEBUG_ASSERTCRASH(!theInfo->m_isFree, ("Shouldn't be free."));
	//@ todo -fix this assert on usa04.  jba.
	//DEBUG_ASSERTCRASH(theInfo->m_obstacleID==0, ("Shouldn't be obstacle."));
	theInfo->m_cell = nullptr;
	theInfo->m_pathParent = s_firstFree;
	s_firstFree = theInfo;
	s_firstFree->m_isFree = true;
	s_freeCount++;
}

Int PathfindCellInfo::recycleUnusedCellInfos()
{
	if (s_infoArray == nullptr) {
		return 0;
	}
	Int inUse = (Int)CELL_INFOS_TO_ALLOCATE - s_freeCount;
	if (inUse < (Int)(CELL_INFOS_TO_ALLOCATE / 4)) {
		return 0;
	}

	Int freeBefore = s_freeCount;
	for (UnsignedInt i = 0; i < CELL_INFOS_TO_ALLOCATE; i++) {
		PathfindCellInfo *info = &s_infoArray[i];
		if (info->m_isFree || info->m_open || info->m_closed || info->m_prevOpen || info->m_nextOpen) {
			continue;
		}
		if (info->m_obstacleID != INVALID_ID || info->m_posUnitID != INVALID_ID
			|| info->m_goalUnitID != INVALID_ID || info->m_goalAircraftID != INVALID_ID) {
			continue;
		}

		if (info->m_cell) {
			info->m_cell->releaseInfo();
		}
	}
	return s_freeCount - freeBefore;
}
