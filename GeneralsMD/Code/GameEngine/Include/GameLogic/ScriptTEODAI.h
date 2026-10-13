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

// FILE: ScriptTEODAI.h ///////////////////////////////////////////////////////////////////////////
// Per-player AI build settings that TEOD script actions change at runtime.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/Snapshot.h"

#include <map>
#include <vector>

class Object;
class ThingTemplate;

//-------------------------------------------------------------------------------------------------
/** AI state that TEOD scripts set. Everything is off or empty until a script asks for it, so the
	stock AI runs unchanged on maps that do not use these actions. */
//-------------------------------------------------------------------------------------------------
class ScriptTEODAI : public Snapshot
{
public:

	ScriptTEODAI();
	virtual ~ScriptTEODAI();

	void reset();

	// Base defense structures the skirmish AI picks from when building along its defense circle.
	void addBaseDefense(Int playerIndex, const AsciiString &templateName);
	void removeBaseDefense(Int playerIndex, const AsciiString &templateName);
	const std::vector<AsciiString> &getBaseDefenses(Int playerIndex) const;

	// Build specific building picks the unbuilt build list entry closest to an idle dozer.
	Bool getClosestDozerPriority(Int playerIndex) const;
	void setClosestDozerPriority(Int playerIndex, Bool enable);

	// Particle cannon beams that chase object types instead of sweeping an S curve.
	void setCannonTeleportMode(Int playerIndex, Bool teleport);
	void setCannonTargets(const Object *cannon, const Coord3D *firePos, const std::vector<const ThingTemplate *> &targets);
	void clearCannonTargets(const Object *cannon);

	/// Called by ParticleUplinkCannonUpdate each firing frame. Returns FALSE to keep the stock beam path.
	Bool steerParticleCannon(const Object *cannon, UnsignedInt startAttackFrame, Real fastDrivingSpeed, Coord3D *beamPos);

protected:

	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

private:

	struct CannonTrack
	{
		UnsignedInt startFrame;		///< m_startAttackFrame of the firing these targets belong to
		Bool started;
		ObjectID victimID;
		Coord3D victimPos;
		std::vector<const ThingTemplate *> targets;
	};

	typedef std::map<ObjectID, CannonTrack> CannonTrackMap;

	std::vector<AsciiString> m_baseDefenses[MAX_PLAYER_COUNT];
	Bool m_closestDozerPriority[MAX_PLAYER_COUNT];
	Bool m_cannonTeleport[MAX_PLAYER_COUNT];
	CannonTrackMap m_cannonTracks;
};

extern ScriptTEODAI *TheScriptTEODAI;
