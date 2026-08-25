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

// contraZH port: TEMPORARY stubs for UI-layer functions the GeneralsOnline services
// reference. Upstream GO defines these in its rewritten WOL menus
// (WOLBuddyOverlay.cpp, WOLGameSetupMenu.cpp), which are ported in a later phase as
// dual copies under GUICallbacks/Menus/GeneralsOnline/. DELETE this file when those
// copies land - the real definitions replace these.
//
// None of these are reachable until the GeneralsOnline UI is in place: they are
// invoked from lobby/social flows that require a logged-in session.

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameNetwork/GeneralsOnline/NextGenMP_defines.h"
#include "GameNetwork/GeneralsOnline/NGMPGame.h"

// Upstream GO defines this in WOLGameSetupMenu.cpp; it moves there with the dual copy.
NGMPGame* TheNGMPGame = nullptr;

void showNotificationBox(AsciiString nick, UnicodeString message, bool bPlaySound)
{
	// Stub: the notification overlay ships with the GeneralsOnline buddy UI.
}

void updateBuddyInfo(bool bIsAutoRefresh, bool bUseCache)
{
	// Stub: the buddy list UI is not ported yet.
}

void OnKickedFromLobby()
{
	// Stub: the lobby UI is not ported yet.
}
