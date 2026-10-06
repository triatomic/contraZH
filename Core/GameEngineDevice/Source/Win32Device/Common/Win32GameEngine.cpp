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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: W3DGameEngine.cpp ////////////////////////////////////////////////////////////////////////
// Author: Colin Day, April 2001
// Description:
//   Implementation of the Win32 game engine, this is the highest level of
//   the game application, it creates all the devices we will use for the game
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>

#include "Win32Device/Common/Win32GameEngine.h"
#include "Common/MessageStream.h"
#include "Common/PerfTimer.h"

#include "GameClient/InGameUI.h"
#include "GameLogic/GameLogic.h"
#include "GameNetwork/LANAPICallbacks.h"

#if defined(GENERALS_ONLINE)
#include "GameNetwork/GeneralsOnline/OnlineServices_Init.h"
#endif

extern DWORD TheMessageTime;
extern HWND ApplicationHWnd;

//-------------------------------------------------------------------------------------------------
// TheSuperHackers @feature new waypoint system (issue #122). While the player holds Alt to plot
// a route, Alt+Tab is the in-game "step the plotting focus to the next selected unit" key -- and
// an accidental task switch would kill the whole plot. A low-level keyboard hook swallows Tab
// while Alt is down (the task switcher never engages) and hands one focus step per physical
// press to the message stream -- but only for as long as the player is actually plotting in a
// running game with input enabled, and only while the game window is the foreground window.
// Every other moment Alt+Tab stays exactly what Windows users expect it to be, so a stuck game
// can still be left the usual way. Releasing Alt first and then pressing Alt+Tab remains the
// deliberate way out of a plotting session.
//-------------------------------------------------------------------------------------------------
static HHOOK s_waypointKeyboardHook = nullptr;

static Bool waypointGuardShouldSwallowTab()
{
	if( TheGameLogic == nullptr || !TheGameLogic->isInGame() )
	{
		return FALSE;
	}

	if( TheInGameUI == nullptr || !TheInGameUI->isInWaypointMode() || !TheInGameUI->getInputEnabled() )
	{
		return FALSE;
	}

	// the hook sees the whole desktop's keys; keep it from eating the player's Alt+Tab in other
	// applications when the game window is not the one they are working in
	return ( GetForegroundWindow() == ApplicationHWnd );
}

static Bool s_waypointTabWasDown = FALSE;

static LRESULT CALLBACK waypointKeyboardHookProc( int code, WPARAM wParam, LPARAM lParam )
{
	if( code >= 0 )
	{
		const KBDLLHOOKSTRUCT *info = (const KBDLLHOOKSTRUCT *)lParam;
		if( info != nullptr && info->vkCode == VK_TAB )
		{
			const Bool isKeyUp = ( info->flags & LLKHF_UP ) != 0;
			const Bool altDown = ( GetAsyncKeyState( VK_MENU ) & 0x8000 ) != 0;

			if( altDown && waypointGuardShouldSwallowTab() )
			{
				// Alt+Tab doubles as the in-game "step the plotting focus to the next selected
				// unit" key: swallow the Tab so the task switcher never engages, and hand one
				// step per physical press to the message stream. The swallow covers the up
				// transition too, so nothing dangles half-delivered.
				if( !isKeyUp && !s_waypointTabWasDown && TheMessageStream != nullptr )
					TheMessageStream->appendMessage( GameMessage::MSG_META_CYCLE_WAYPOINT_FOCUS );

				s_waypointTabWasDown = !isKeyUp;
				return 1;
			}

			// a Tab that reaches the world outside the guard resets the press tracker, so the
			// next plotting Alt+Tab starts a fresh step
			if( !altDown && !isKeyUp )
				s_waypointTabWasDown = FALSE;
		}
	}

	return CallNextHookEx( s_waypointKeyboardHook, code, wParam, lParam );
}

static void installWaypointInputGuard()
{
	if( s_waypointKeyboardHook == nullptr )
	{
		s_waypointKeyboardHook = SetWindowsHookEx( WH_KEYBOARD_LL, waypointKeyboardHookProc,
				GetModuleHandle( nullptr ), 0 );
	}
}

static void removeWaypointInputGuard()
{
	if( s_waypointKeyboardHook != nullptr )
	{
		UnhookWindowsHookEx( s_waypointKeyboardHook );
		s_waypointKeyboardHook = nullptr;
	}
}

//-------------------------------------------------------------------------------------------------
/** Constructor for Win32GameEngine */
//-------------------------------------------------------------------------------------------------
Win32GameEngine::Win32GameEngine()
{
	// Stop blue screen
	m_previousErrorMode = SetErrorMode( SEM_FAILCRITICALERRORS );
}

//-------------------------------------------------------------------------------------------------
/** Destructor for Win32GameEngine */
//-------------------------------------------------------------------------------------------------
Win32GameEngine::~Win32GameEngine()
{
	removeWaypointInputGuard();

	// restore it (this isn't really necessary, but feels good.)
	SetErrorMode( m_previousErrorMode );
}


//-------------------------------------------------------------------------------------------------
/** Initialize the game engine */
//-------------------------------------------------------------------------------------------------
void Win32GameEngine::init()
{

	// extending functionality
	GameEngine::init();

	// TheSuperHackers @feature new waypoint system (issue #122): swallow accidental Alt+Tab
	// while a route is being plotted; see waypointKeyboardHookProc above for the exact scope.
	installWaypointInputGuard();

}

//-------------------------------------------------------------------------------------------------
/** Reset the system */
//-------------------------------------------------------------------------------------------------
void Win32GameEngine::reset()
{

	// extending functionality
	GameEngine::reset();

}

//-------------------------------------------------------------------------------------------------
/** Update the game engine by updating the GameClient and
	* GameLogic singletons. */
//-------------------------------------------------------------------------------------------------
void Win32GameEngine::update()
{


	// call the engine normal update
	GameEngine::update();

	extern HWND ApplicationHWnd;
	if (ApplicationHWnd && ::IsIconic(ApplicationHWnd)) {
		while (ApplicationHWnd && ::IsIconic(ApplicationHWnd)) {
			// We are alt-tabbed out here.  Sleep a bit, & process windows
			// so that we can become un-alt-tabbed out.
			Sleep(5);
			serviceWindowsOS();

			if (TheLAN != nullptr) {
				// BGC - need to update TheLAN so we can process and respond to other
				// people's messages who may not be alt-tabbed out like we are.
				TheLAN->setIsActive(isActive());
				TheLAN->update();
			}

#if defined(GENERALS_ONLINE)
			if (NGMP_OnlineServicesManager::GetInstance() != nullptr)
			{
				NGMP_OnlineServicesManager::GetInstance()->Tick();
			}

			// If we are running a multiplayer game, keep running the logic.
			// GO_CHANGE: If we have an active network session, keep running to prevent disconnecting us from
			// other players during lobby and loading screen where isInMultiplayerGame() returns false
			if (TheGameEngine->getQuitting() || TheGameLogic->isInMultiplayerGame() || (TheNetwork != nullptr)) {
				break; // keep running.
			}
#else
			// If we are running a multiplayer game, keep running the logic.
			// There is code in the client to skip client redraw if we are
			// iconic.  jba.
			if (TheGameEngine->getQuitting() || TheGameLogic->isInInternetGame() || TheGameLogic->isInLanGame()) {
				break; // keep running.
			}
#endif
		}

    // When we are alt-tabbed out... the MilesAudioManager seems to go into a coma sometimes
    // and not regain focus properly when we come back. This seems to wake it up nicely.
    AudioAffect aa = (AudioAffect)0x10;
		TheAudio->setVolume(TheAudio->getVolume( aa ), aa );

	}

	// allow windows to perform regular windows maintenance stuff like msgs
	serviceWindowsOS();

}

//-------------------------------------------------------------------------------------------------
/** This function may be called from within this application to let
  * Microsoft Windows do its message processing and dispatching.  Presumably
	* we would call this at least once each time around the game loop to keep
	* Windows services from backing up */
//-------------------------------------------------------------------------------------------------
void Win32GameEngine::serviceWindowsOS()
{
	MSG msg;
  Int returnValue;

	//
	// see if we have any messages to process, a nullptr window handle tells the
	// OS to look at the main window associated with the calling thread, us!
	//
	while( PeekMessage( &msg, nullptr, 0, 0, PM_NOREMOVE ) )
	{

		// get the message
		returnValue = GetMessage( &msg, nullptr, 0, 0 );

		// this is one possible way to check for quitting conditions as a message
		// of WM_QUIT will cause GetMessage() to return 0
/*
		if( returnValue == 0 )
		{

			setQuitting( true );
			break;

		}
*/

		TheMessageTime = msg.time;
		// translate and dispatch the message
		TranslateMessage( &msg );
		DispatchMessage( &msg );
		TheMessageTime = 0;

	}

}

