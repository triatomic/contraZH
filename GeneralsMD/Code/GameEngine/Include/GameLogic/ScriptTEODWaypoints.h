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

// FILE: ScriptTEODWaypoints.h ////////////////////////////////////////////////////////////////////
// Saves waypoints that TEOD script actions create, move, link or remove at runtime.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/Snapshot.h"

//-------------------------------------------------------------------------------------------------
class ScriptTEODWaypoints : public Snapshot
{
public:

	ScriptTEODWaypoints();
	virtual ~ScriptTEODWaypoints();

	void reset();

	/// Call after any script change to the waypoints, so saved games store the whole waypoint set.
	void markDirty() { m_dirty = TRUE; }

	/// Returns a waypoint ID that no waypoint, live or removed, has used this game.
	UnsignedInt allocateID();

protected:

	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

private:

	void xferWaypoints( Xfer *xfer );

	Bool m_dirty;
	UnsignedInt m_nextID;		///< 0 until the first allocation scans the map's IDs
};

extern ScriptTEODWaypoints *TheScriptTEODWaypoints;
