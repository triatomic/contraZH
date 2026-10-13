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

// FILE: ScriptActionsCLPWaypoints.cpp ////////////////////////////////////////////////////////////
// CLP_AI script actions that create, move, link and remove waypoints at runtime.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/Team.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptActions.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/ScriptCLP.h"
#include "GameLogic/ScriptCLPWaypoints.h"
#include "GameLogic/TerrainLogic.h"

#include <map>
#include <vector>

ScriptCLPWaypoints *TheScriptCLPWaypoints = nullptr;

//-------------------------------------------------------------------------------------------------
static void waypointWarning(const char *format, const AsciiString &name)
{
	AsciiString msg;
	msg.format(format, name.str());
	DEBUG_LOG(("%s", msg.str()));
	TheScriptEngine->AppendDebugMessage(msg, FALSE);
}

//-------------------------------------------------------------------------------------------------
static Waypoint *findWaypoint(const AsciiString &name)
{
	Waypoint *way = TheTerrainLogic->getWaypointByName(name);
	if (way == nullptr)
	{
		waypointWarning("***WARNING - Waypoint '%s' not found.***", name);
	}
	return way;
}

//-------------------------------------------------------------------------------------------------
/** Adds a link unless it exists; reports a waypoint that has no free link slot. */
//-------------------------------------------------------------------------------------------------
static void linkWaypoints(Waypoint *from, Waypoint *to)
{
	if (from->hasLink(to))
	{
		return;
	}
	if (!from->addLink(to))
	{
		AsciiString msg;
		msg.format("***WARNING - Waypoint '%s' already has %d links, cannot link it to '%s'.***",
			from->getName().str(), (Int)Waypoint::MAX_LINKS, to->getName().str());
		DEBUG_LOG(("%s", msg.str()));
		TheScriptEngine->AppendDebugMessage(msg, FALSE);
	}
}

//-------------------------------------------------------------------------------------------------
/** Creates a waypoint unless the name is empty or already taken. */
//-------------------------------------------------------------------------------------------------
static void createWaypoint(const AsciiString &name, const Coord3D *pos, const AsciiString &label1,
	const AsciiString &label2, const AsciiString &label3, Bool biDirectional)
{
	if (name.isEmpty())
	{
		TheScriptEngine->AppendDebugMessage("***WARNING - Cannot create a waypoint without a name.***", FALSE);
		return;
	}
	if (TheTerrainLogic->getWaypointByName(name))
	{
		waypointWarning("***WARNING - Waypoint '%s' already exists, not created.***", name);
		return;
	}

	WaypointID id = (WaypointID)TheScriptCLPWaypoints->allocateID();
	Waypoint *way = newInstance(Waypoint)(id, name, pos, label1, label2, label3, biDirectional);
	TheTerrainLogic->insertWaypoint(way);
	TheScriptCLPWaypoints->markDirty();
}

//-------------------------------------------------------------------------------------------------
/** Reference point for the objectType actions: the AI base center, else the player's average object position. */
//-------------------------------------------------------------------------------------------------
static Bool getCurrentPlayerPosition(Coord3D *pos)
{
	Player *player = TheScriptEngine->getCurrentPlayer();
	if (player == nullptr)
	{
		return FALSE;
	}
	if (player->getAiBaseCenter(pos))
	{
		return TRUE;
	}

	pos->zero();
	Int count = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->getControllingPlayer() == player)
		{
			pos->add(*obj->getPosition());
			++count;
		}
	}
	if (count == 0)
	{
		return FALSE;
	}
	pos->scale(1.0f / count);
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
static Object *selectObjectForCurrentPlayer(const AsciiString &objectType, Int selectionMode)
{
	Coord3D origin;
	if (!getCurrentPlayerPosition(&origin))
	{
		return nullptr;
	}
	return clpSelectObject(&origin, nullptr, objectType, selectionMode);
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doCreateWaypoint(const AsciiString& waypointName, Coord3D location, const AsciiString& label1, const AsciiString& label2, const AsciiString& label3, Bool biDirectional, Bool snapToGrid)
{
	if (snapToGrid)
	{
		location.z = TheTerrainLogic->getGroundHeight(location.x, location.y);
	}
	createWaypoint(waypointName, &location, label1, label2, label3, biDirectional);
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doCreateWaypointAtUnit(const AsciiString& waypointName, const AsciiString& unitName, const AsciiString& label1, const AsciiString& label2, const AsciiString& label3, Bool biDirectional)
{
	Object *obj = TheScriptEngine->getUnitNamed(unitName);
	if (obj == nullptr)
	{
		return;
	}
	createWaypoint(waypointName, obj->getPosition(), label1, label2, label3, biDirectional);
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doCreateWaypointAtTeam(const AsciiString& waypointName, const AsciiString& teamName, const AsciiString& label1, const AsciiString& label2, const AsciiString& label3, Bool biDirectional)
{
	Team *team = TheScriptEngine->getTeamNamed(teamName);
	if (team == nullptr)
	{
		return;
	}
	const Coord3D *pos = team->getEstimateTeamPosition();
	if (pos == nullptr)
	{
		return;
	}
	createWaypoint(waypointName, pos, label1, label2, label3, biDirectional);
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doCreateWaypointAtType(const AsciiString& waypointName, const AsciiString& objectType, const AsciiString& label1, const AsciiString& label2, const AsciiString& label3, Bool biDirectional, Int selectionMode)
{
	Object *obj = selectObjectForCurrentPlayer(objectType, selectionMode);
	if (obj == nullptr)
	{
		return;
	}
	createWaypoint(waypointName, obj->getPosition(), label1, label2, label3, biDirectional);
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doRemoveWaypoint(const AsciiString& waypointName)
{
	Waypoint *way = findWaypoint(waypointName);
	if (way == nullptr)
	{
		return;
	}
	TheTerrainLogic->removeWaypoint(way);
	TheScriptCLPWaypoints->markDirty();
}

//-------------------------------------------------------------------------------------------------
/** Links source to target, and back when source is bi-directional, as map loading does. */
//-------------------------------------------------------------------------------------------------
void ScriptActions::doConnectWaypoint(const AsciiString& waypointSource, const AsciiString& waypointTarget)
{
	Waypoint *source = findWaypoint(waypointSource);
	Waypoint *target = findWaypoint(waypointTarget);
	if (source == nullptr || target == nullptr || source == target)
	{
		return;
	}

	linkWaypoints(source, target);
	if (source->getBiDirectional())
	{
		linkWaypoints(target, source);
	}
	TheScriptCLPWaypoints->markDirty();
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doDisconnectWaypoint(const AsciiString& waypointSource, const AsciiString& waypointTarget)
{
	Waypoint *source = findWaypoint(waypointSource);
	Waypoint *target = findWaypoint(waypointTarget);
	if (source == nullptr || target == nullptr || source == target)
	{
		return;
	}

	source->removeLink(target);
	target->removeLink(source);
	TheScriptCLPWaypoints->markDirty();
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doSetWaypointPathLabels(const AsciiString& waypointName, const AsciiString& label1, const AsciiString& label2, const AsciiString& label3)
{
	Waypoint *way = findWaypoint(waypointName);
	if (way == nullptr)
	{
		return;
	}
	way->setPathLabels(label1, label2, label3);
	TheScriptCLPWaypoints->markDirty();
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doClearWaypointPathLabels(const AsciiString& waypointName, Bool clear1, Bool clear2, Bool clear3)
{
	Waypoint *way = findWaypoint(waypointName);
	if (way == nullptr)
	{
		return;
	}

	AsciiString label1 = clear1 ? AsciiString::TheEmptyString : way->getPathLabel1();
	AsciiString label2 = clear2 ? AsciiString::TheEmptyString : way->getPathLabel2();
	AsciiString label3 = clear3 ? AsciiString::TheEmptyString : way->getPathLabel3();
	way->setPathLabels(label1, label2, label3);
	TheScriptCLPWaypoints->markDirty();
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doRelocateWaypoint(const AsciiString& waypointName, Coord3D location, Bool snapToGrid)
{
	Waypoint *way = findWaypoint(waypointName);
	if (way == nullptr)
	{
		return;
	}
	if (snapToGrid)
	{
		location.z = TheTerrainLogic->getGroundHeight(location.x, location.y);
	}
	way->setLocation(&location);
	TheScriptCLPWaypoints->markDirty();
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doRelocateWaypointUnit(const AsciiString& waypointName, const AsciiString& unitName)
{
	Waypoint *way = findWaypoint(waypointName);
	if (way == nullptr)
	{
		return;
	}
	Object *obj = TheScriptEngine->getUnitNamed(unitName);
	if (obj == nullptr)
	{
		return;
	}
	way->setLocation(obj->getPosition());
	TheScriptCLPWaypoints->markDirty();
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doRelocateWaypointTeam(const AsciiString& waypointName, const AsciiString& teamName)
{
	Waypoint *way = findWaypoint(waypointName);
	if (way == nullptr)
	{
		return;
	}
	Team *team = TheScriptEngine->getTeamNamed(teamName);
	if (team == nullptr)
	{
		return;
	}
	const Coord3D *pos = team->getEstimateTeamPosition();
	if (pos == nullptr)
	{
		return;
	}
	way->setLocation(pos);
	TheScriptCLPWaypoints->markDirty();
}

//-------------------------------------------------------------------------------------------------
void ScriptActions::doRelocateWaypointType(const AsciiString& waypointName, const AsciiString& objectType, Int selectionMode)
{
	Waypoint *way = findWaypoint(waypointName);
	if (way == nullptr)
	{
		return;
	}
	Object *obj = selectObjectForCurrentPlayer(objectType, selectionMode);
	if (obj == nullptr)
	{
		return;
	}
	way->setLocation(obj->getPosition());
	TheScriptCLPWaypoints->markDirty();
}

//-------------------------------------------------------------------------------------------------
/** Adds or drops the reverse links that map loading creates for a bi-directional waypoint. */
//-------------------------------------------------------------------------------------------------
void ScriptActions::doSetWaypointBiDirectional(const AsciiString& waypointName, Bool biDirectional)
{
	Waypoint *way = findWaypoint(waypointName);
	if (way == nullptr)
	{
		return;
	}

	way->setBiDirectional(biDirectional);
	for (Int i = 0; i < way->getNumLinks(); ++i)
	{
		Waypoint *target = way->getLink(i);
		if (biDirectional)
		{
			linkWaypoints(target, way);
		}
		else if (!target->getBiDirectional())
		{
			target->removeLink(way);
		}
	}
	TheScriptCLPWaypoints->markDirty();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ScriptCLPWaypoints::ScriptCLPWaypoints()
{
	reset();
}

//-------------------------------------------------------------------------------------------------
ScriptCLPWaypoints::~ScriptCLPWaypoints()
{
}

//-------------------------------------------------------------------------------------------------
void ScriptCLPWaypoints::reset()
{
	m_dirty = FALSE;
	m_nextID = 0;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt ScriptCLPWaypoints::allocateID()
{
	if (m_nextID == 0)
	{
		m_nextID = 1;
		Waypoint *way;
		for (way = TheTerrainLogic->getFirstWaypoint(); way; way = way->getNext())
		{
			if ((UnsignedInt)way->getID() >= m_nextID)
			{
				m_nextID = (UnsignedInt)way->getID() + 1;
			}
		}
		for (way = TheTerrainLogic->getFirstRemovedWaypoint(); way; way = way->getNext())
		{
			if ((UnsignedInt)way->getID() >= m_nextID)
			{
				m_nextID = (UnsignedInt)way->getID() + 1;
			}
		}
	}
	return m_nextID++;
}

//-------------------------------------------------------------------------------------------------
void ScriptCLPWaypoints::crc( Xfer *xfer )
{
}

//-------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
//-------------------------------------------------------------------------------------------------
void ScriptCLPWaypoints::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	xfer->xferUnsignedInt( &m_nextID );
	xfer->xferBool( &m_dirty );
	if( m_dirty )
	{
		xferWaypoints( xfer );
	}
}

//-------------------------------------------------------------------------------------------------
namespace
{
	struct SavedWaypoint
	{
		UnsignedInt id;
		AsciiString name;
		Coord3D location;
		AsciiString label1;
		AsciiString label2;
		AsciiString label3;
		Bool biDirectional;
		Bool removed;
		std::vector<UnsignedInt> links;
	};
}

//-------------------------------------------------------------------------------------------------
static void collectSavedWaypoints(Waypoint *first, Bool removed, std::vector<SavedWaypoint> &saved)
{
	for (Waypoint *way = first; way; way = way->getNext())
	{
		SavedWaypoint s;
		s.id = (UnsignedInt)way->getID();
		s.name = way->getName();
		s.location = *way->getLocation();
		s.label1 = way->getPathLabel1();
		s.label2 = way->getPathLabel2();
		s.label3 = way->getPathLabel3();
		s.biDirectional = way->getBiDirectional();
		s.removed = removed;
		for (Int i = 0; i < way->getNumLinks(); ++i)
		{
			s.links.push_back((UnsignedInt)way->getLink(i)->getID());
		}
		saved.push_back(s);
	}
}

//-------------------------------------------------------------------------------------------------
/** Updates map waypoints in place, creates the rest, and retires map waypoints the save lacks. */
//-------------------------------------------------------------------------------------------------
static void applySavedWaypoints(const std::vector<SavedWaypoint> &saved)
{
	typedef std::map<UnsignedInt, Waypoint *> WaypointMap;
	WaypointMap existing;
	Waypoint *way;
	for (way = TheTerrainLogic->getFirstWaypoint(); way; way = way->getNext())
	{
		existing[(UnsignedInt)way->getID()] = way;
	}
	for (way = TheTerrainLogic->getFirstRemovedWaypoint(); way; way = way->getNext())
	{
		existing[(UnsignedInt)way->getID()] = way;
	}

	std::vector<Waypoint *> active;
	std::vector<Waypoint *> removed;
	std::vector<Waypoint *> ordered;
	WaypointMap byID;
	size_t i;
	for (i = 0; i < saved.size(); ++i)
	{
		const SavedWaypoint &s = saved[i];
		WaypointMap::iterator it = existing.find(s.id);
		if (it != existing.end())
		{
			way = it->second;
			existing.erase(it);
			way->setLocation(&s.location);
			way->setPathLabels(s.label1, s.label2, s.label3);
			way->setBiDirectional(s.biDirectional);
			way->clearLinks();
		}
		else
		{
			way = newInstance(Waypoint)((WaypointID)s.id, s.name, &s.location, s.label1, s.label2, s.label3, s.biDirectional);
		}
		ordered.push_back(way);
		byID[s.id] = way;
		if (s.removed)
		{
			removed.push_back(way);
		}
		else
		{
			active.push_back(way);
		}
	}

	for (WaypointMap::iterator it = existing.begin(); it != existing.end(); ++it)
	{
		removed.push_back(it->second);
	}
	TheTerrainLogic->setWaypointLists(active, removed);

	for (i = 0; i < saved.size(); ++i)
	{
		for (size_t j = 0; j < saved[i].links.size(); ++j)
		{
			WaypointMap::iterator it = byID.find(saved[i].links[j]);
			if (it != byID.end())
			{
				ordered[i]->addLink(it->second);
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Saves every waypoint, live then removed, with its links by ID. */
//-------------------------------------------------------------------------------------------------
void ScriptCLPWaypoints::xferWaypoints( Xfer *xfer )
{
	std::vector<SavedWaypoint> saved;
	if( xfer->getXferMode() != XFER_LOAD )
	{
		collectSavedWaypoints( TheTerrainLogic->getFirstWaypoint(), FALSE, saved );
		collectSavedWaypoints( TheTerrainLogic->getFirstRemovedWaypoint(), TRUE, saved );
	}

	UnsignedInt count = (UnsignedInt)saved.size();
	xfer->xferUnsignedInt( &count );
	if( xfer->getXferMode() == XFER_LOAD )
	{
		saved.resize( count );
	}

	for( UnsignedInt i = 0; i < count; ++i )
	{
		SavedWaypoint &s = saved[ i ];
		xfer->xferUnsignedInt( &s.id );
		xfer->xferAsciiString( &s.name );
		xfer->xferCoord3D( &s.location );
		xfer->xferAsciiString( &s.label1 );
		xfer->xferAsciiString( &s.label2 );
		xfer->xferAsciiString( &s.label3 );
		xfer->xferBool( &s.biDirectional );
		xfer->xferBool( &s.removed );

		UnsignedByte numLinks = (UnsignedByte)s.links.size();
		xfer->xferUnsignedByte( &numLinks );
		s.links.resize( numLinks );
		for( UnsignedByte j = 0; j < numLinks; ++j )
		{
			xfer->xferUnsignedInt( &s.links[ j ] );
		}
	}

	if( xfer->getXferMode() == XFER_LOAD )
	{
		applySavedWaypoints( saved );
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptCLPWaypoints::loadPostProcess()
{
}
