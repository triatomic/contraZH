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

///////////////////////////////////////////////////////////////////////////////////////
// FILE: CampaignChaptersMenu.cpp
// Description: Starts any mission of a Campaign.ini campaign
///////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GameEngine.h"
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/OptionPreferences.h"
#include "Common/RandomValue.h"
#include "GameClient/CampaignManager.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetRadioButton.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GameWindowTransitions.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/MapUtil.h"
#include "GameClient/Shell.h"
#include "GameClient/WindowLayout.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/ScriptEngine.h"

// window ids -------------------------------------------------------------------------------------
static NameKeyType parentID = NAMEKEY_INVALID;
static NameKeyType listboxCampaignsID = NAMEKEY_INVALID;
static NameKeyType listboxChaptersID = NAMEKEY_INVALID;
static NameKeyType buttonPlayID = NAMEKEY_INVALID;
static NameKeyType buttonBackID = NAMEKEY_INVALID;
static NameKeyType radioEasyID = NAMEKEY_INVALID;
static NameKeyType radioMediumID = NAMEKEY_INVALID;
static NameKeyType radioHardID = NAMEKEY_INVALID;

// window pointers --------------------------------------------------------------------------------
static GameWindow *parent = nullptr;
static GameWindow *listboxCampaigns = nullptr;
static GameWindow *listboxChapters = nullptr;
static GameWindow *buttonBack = nullptr;

static GameDifficulty difficulty = DIFFICULTY_NORMAL;
static Bool isShuttingDown = FALSE;
static Bool gameStarting = FALSE;
static Bool justEntered = FALSE;
static Int initialGadgetDelay = 2;

static const Color textColor = GameMakeColor( 255, 255, 255, 255 );

static Campaign *getSelectedCampaign()
{
	Int row = -1;
	GadgetListBoxGetSelected( listboxCampaigns, &row );
	if( row < 0 )
	{
		return nullptr;
	}
	return (Campaign *)GadgetListBoxGetItemData( listboxCampaigns, row );
}

static Mission *getSelectedMission()
{
	Int row = -1;
	GadgetListBoxGetSelected( listboxChapters, &row );
	if( row < 0 )
	{
		return nullptr;
	}
	return (Mission *)GadgetListBoxGetItemData( listboxChapters, row );
}

static void populateCampaigns()
{
	GadgetListBoxReset( listboxCampaigns );

	// Challenge campaigns need a general picked in the challenge menu, so they are left out
	const std::list< Campaign* > &campaigns = TheCampaignManager->getCampaignList();
	for( std::list< Campaign* >::const_iterator it = campaigns.begin(); it != campaigns.end(); ++it )
	{
		Campaign *campaign = *it;
		if( campaign->isChallengeCampaign() || campaign->m_missions.empty() )
		{
			continue;
		}

		Bool exists = FALSE;
		UnicodeString name = TheGameText->fetch( campaign->m_campaignNameLabel, &exists );
		if( !exists )
		{
			name.translate( campaign->m_name );
		}
		Int row = GadgetListBoxAddEntryText( listboxCampaigns, name, textColor, -1 );
		GadgetListBoxSetItemData( listboxCampaigns, campaign, row );
	}
}

static void populateChapters( Campaign *campaign )
{
	GadgetListBoxReset( listboxChapters );
	if( campaign == nullptr )
	{
		return;
	}

	// Missions are listed in NextMission order from FirstMission, then any the chain never reaches
	std::list< Mission* > ordered;
	Mission *mission = campaign->getNextMission( nullptr );
	while( mission && std::find( ordered.begin(), ordered.end(), mission ) == ordered.end() )
	{
		ordered.push_back( mission );
		mission = campaign->getNextMission( mission );
	}
	for( Campaign::MissionListIt it = campaign->m_missions.begin(); it != campaign->m_missions.end(); ++it )
	{
		if( std::find( ordered.begin(), ordered.end(), *it ) == ordered.end() )
		{
			ordered.push_back( *it );
		}
	}

	Int number = 1;
	for( std::list< Mission* >::iterator it = ordered.begin(); it != ordered.end(); ++it, ++number )
	{
		UnicodeString title;
		const MapMetaData *map = TheMapCache ? TheMapCache->findMap( (*it)->m_mapName ) : nullptr;
		if( map && !map->m_displayName.isEmpty() )
		{
			title = map->m_displayName;
		}
		else
		{
			title.translate( (*it)->m_name );
		}

		UnicodeString text;
		text.format( L"%d. %s", number, title.str() );
		Int row = GadgetListBoxAddEntryText( listboxChapters, text, textColor, -1 );
		GadgetListBoxSetItemData( listboxChapters, *it, row );
	}
	GadgetListBoxSetSelected( listboxChapters, 0 );
}

static void startChapter()
{
	Campaign *campaign = getSelectedCampaign();
	Mission *mission = getSelectedMission();
	if( gameStarting || campaign == nullptr || mission == nullptr )
	{
		return;
	}

	TheCampaignManager->setCampaignAndMission( campaign->m_name, mission->m_name );
	if( TheCampaignManager->getCurrentMission() != mission )
	{
		TheCampaignManager->setCampaign( AsciiString::TheEmptyString );
		return;
	}

	OptionPreferences pref;
	pref.setCampaignDifficulty( difficulty );
	pref.write();
	TheScriptEngine->setGlobalDifficulty( difficulty );
	TheCampaignManager->setGameDifficulty( difficulty );
	TheWritableGlobalData->m_pendingFile = mission->m_mapName;

	if( TheGameLogic->isInGame() )
	{
		TheGameLogic->clearGameData();
	}

	GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_NEW_GAME );
	msg->appendIntegerArgument( GAME_SINGLE_PLAYER );
	msg->appendIntegerArgument( difficulty );
	msg->appendIntegerArgument( TheCampaignManager->getRankPoints() );
	InitRandom( 0 );
	gameStarting = TRUE;
}

//-------------------------------------------------------------------------------------------------
void CampaignChaptersMenuInit( WindowLayout *layout, void *userData )
{
	TheShell->showShellMap( TRUE );

	parentID = TheNameKeyGenerator->nameToKey( "CampaignChapters.wnd:ParentCampaignChapters" );
	listboxCampaignsID = TheNameKeyGenerator->nameToKey( "CampaignChapters.wnd:ListboxCampaigns" );
	listboxChaptersID = TheNameKeyGenerator->nameToKey( "CampaignChapters.wnd:ListboxChapters" );
	buttonPlayID = TheNameKeyGenerator->nameToKey( "CampaignChapters.wnd:ButtonPlay" );
	buttonBackID = TheNameKeyGenerator->nameToKey( "CampaignChapters.wnd:ButtonBack" );
	radioEasyID = TheNameKeyGenerator->nameToKey( "CampaignChapters.wnd:RadioButtonEasy" );
	radioMediumID = TheNameKeyGenerator->nameToKey( "CampaignChapters.wnd:RadioButtonMedium" );
	radioHardID = TheNameKeyGenerator->nameToKey( "CampaignChapters.wnd:RadioButtonHard" );

	parent = TheWindowManager->winGetWindowFromId( nullptr, parentID );
	listboxCampaigns = TheWindowManager->winGetWindowFromId( parent, listboxCampaignsID );
	listboxChapters = TheWindowManager->winGetWindowFromId( parent, listboxChaptersID );
	buttonBack = TheWindowManager->winGetWindowFromId( parent, buttonBackID );

	OptionPreferences pref;
	difficulty = (GameDifficulty)pref.getCampaignDifficulty();
	NameKeyType radioID = radioMediumID;
	if( difficulty == DIFFICULTY_EASY )
	{
		radioID = radioEasyID;
	}
	else if( difficulty == DIFFICULTY_HARD )
	{
		radioID = radioHardID;
	}
	else
	{
		difficulty = DIFFICULTY_NORMAL;
	}
	GameWindow *radio = TheWindowManager->winGetWindowFromId( parent, radioID );
	if( radio )
	{
		GadgetRadioSetSelection( radio, FALSE );
	}

	// Selecting the first campaign fills the chapter list through GLM_SELECTED
	populateCampaigns();
	GadgetListBoxSetSelected( listboxCampaigns, 0 );

	layout->hide( FALSE );
	TheWindowManager->winSetFocus( parent );

	justEntered = TRUE;
	initialGadgetDelay = 2;
	GameWindow *win = TheWindowManager->winGetWindowFromId( nullptr, TheNameKeyGenerator->nameToKey( "CampaignChapters.wnd:GadgetParent" ) );
	if( win )
	{
		win->winHide( TRUE );
	}
	isShuttingDown = FALSE;
	gameStarting = FALSE;
}

//-------------------------------------------------------------------------------------------------
void CampaignChaptersMenuShutdown( WindowLayout *layout, void *userData )
{
	Bool popImmediate = *(Bool *)userData;
	if( popImmediate )
	{
		layout->hide( TRUE );
		TheShell->shutdownComplete( layout );
		return;
	}

	TheTransitionHandler->reverse( "CampaignChaptersMenuFade" );
	isShuttingDown = TRUE;
}

//-------------------------------------------------------------------------------------------------
void CampaignChaptersMenuUpdate( WindowLayout *layout, void *userData )
{
	if( justEntered )
	{
		if( initialGadgetDelay == 1 )
		{
			TheTransitionHandler->remove( "MainMenuDefaultMenuLogoFade" );
			TheTransitionHandler->setGroup( "CampaignChaptersMenuFade" );
			initialGadgetDelay = 2;
			justEntered = FALSE;
		}
		else
		{
			initialGadgetDelay--;
		}
	}

	if( isShuttingDown && TheShell->isAnimFinished() && TheTransitionHandler->isFinished() )
	{
		TheShell->shutdownComplete( layout );
	}
}

//-------------------------------------------------------------------------------------------------
WindowMsgHandledType CampaignChaptersMenuInput( GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2 )
{
	if( msg == GWM_CHAR && mData1 == KEY_ESC )
	{
		if( buttonBack && BitIsSet( mData2, KEY_STATE_UP ) )
		{
			TheWindowManager->winSendSystemMsg( window, GBM_SELECTED, (WindowMsgData)buttonBack, buttonBackID );
		}
		return MSG_HANDLED;
	}
	return MSG_IGNORED;
}

//-------------------------------------------------------------------------------------------------
WindowMsgHandledType CampaignChaptersMenuSystem( GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2 )
{
	switch( msg )
	{
		case GWM_INPUT_FOCUS:
		{
			if( mData1 == TRUE )
			{
				*(Bool *)mData2 = TRUE;
			}
			return MSG_HANDLED;
		}

		case GLM_SELECTED:
		{
			GameWindow *control = (GameWindow *)mData1;
			if( control->winGetWindowId() == listboxCampaignsID )
			{
				populateChapters( getSelectedCampaign() );
			}
			break;
		}

		case GLM_DOUBLE_CLICKED:
		{
			GameWindow *control = (GameWindow *)mData1;
			if( control->winGetWindowId() == listboxChaptersID )
			{
				startChapter();
			}
			break;
		}

		case GBM_SELECTED:
		{
			GameWindow *control = (GameWindow *)mData1;
			Int controlID = control->winGetWindowId();
			if( controlID == buttonPlayID )
			{
				startChapter();
			}
			else if( controlID == buttonBackID )
			{
				TheShell->pop();
			}
			else if( controlID == radioEasyID )
			{
				difficulty = DIFFICULTY_EASY;
			}
			else if( controlID == radioMediumID )
			{
				difficulty = DIFFICULTY_NORMAL;
			}
			else if( controlID == radioHardID )
			{
				difficulty = DIFFICULTY_HARD;
			}
			break;
		}

		default:
			return MSG_IGNORED;
	}

	return MSG_HANDLED;
}
