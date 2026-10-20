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
// Description: Campaign screen listing Campaign.ini campaigns by tab and their unlocked missions
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
#include "GameClient/GadgetPushButton.h"
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

enum { MAX_CAMPAIGN_TABS = 6 };

// window ids -------------------------------------------------------------------------------------
static NameKeyType parentID = NAMEKEY_INVALID;
static NameKeyType listboxCampaignsID = NAMEKEY_INVALID;
static NameKeyType listboxChaptersID = NAMEKEY_INVALID;
static NameKeyType buttonPlayID = NAMEKEY_INVALID;
static NameKeyType buttonBackID = NAMEKEY_INVALID;
static NameKeyType radioEasyID = NAMEKEY_INVALID;
static NameKeyType radioMediumID = NAMEKEY_INVALID;
static NameKeyType radioHardID = NAMEKEY_INVALID;
static NameKeyType buttonTabID[MAX_CAMPAIGN_TABS];

// window pointers --------------------------------------------------------------------------------
static GameWindow *parent = nullptr;
static GameWindow *listboxCampaigns = nullptr;
static GameWindow *listboxChapters = nullptr;
static GameWindow *buttonPlay = nullptr;
static GameWindow *buttonBack = nullptr;
static GameWindow *mapWindow = nullptr;
static GameWindow *buttonTab[MAX_CAMPAIGN_TABS];

// Campaigns sharing a CampaignGroupLabel share a tab, in Campaign.ini order
struct CampaignTab
{
	AsciiString label;
	std::vector< Campaign* > campaigns;
};
static std::vector< CampaignTab > tabs;
static Int activeTab = 0;

static GameDifficulty difficulty = DIFFICULTY_NORMAL;
static Bool isShuttingDown = FALSE;
static Bool gameStarting = FALSE;
static Bool justEntered = FALSE;
static Int initialGadgetDelay = 2;

static const Color textColor = GameMakeColor( 255, 255, 255, 255 );
static const Color lockedColor = GameMakeColor( 110, 110, 110, 255 );

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

static UnicodeString fetchLabel( const AsciiString &label, const AsciiString &fallback )
{
	Bool exists = FALSE;
	UnicodeString text = TheGameText->fetch( label, &exists );
	if( !exists )
	{
		text.translate( fallback );
	}
	return text;
}

static void buildTabs()
{
	tabs.clear();

	// Challenge campaigns need a general picked in the challenge menu, so they are left out
	const std::list< Campaign* > &campaigns = TheCampaignManager->getCampaignList();
	for( std::list< Campaign* >::const_iterator it = campaigns.begin(); it != campaigns.end(); ++it )
	{
		Campaign *campaign = *it;
		if( campaign->isChallengeCampaign() || campaign->m_missions.empty() )
		{
			continue;
		}

		AsciiString label = campaign->m_groupLabel;
		if( label.isEmpty() )
		{
			label = "GUI:Campaign";
		}
		size_t tab = 0;
		while( tab < tabs.size() && tabs[tab].label.compareNoCase( label ) != 0 )
		{
			++tab;
		}
		if( tab == tabs.size() )
		{
			tabs.push_back( CampaignTab() );
			tabs.back().label = label;
		}
		tabs[tab].campaigns.push_back( campaign );
	}
}

static void layoutTabs()
{
	// One tab is no choice, so the row stays hidden; more than fit share a plain Campaign tab
	Int shown = (Int)tabs.size();
	if( shown > MAX_CAMPAIGN_TABS )
	{
		CampaignTab &last = tabs[MAX_CAMPAIGN_TABS - 1];
		last.label = "GUI:Campaign";
		for( size_t tab = MAX_CAMPAIGN_TABS; tab < tabs.size(); ++tab )
		{
			last.campaigns.insert( last.campaigns.end(), tabs[tab].campaigns.begin(), tabs[tab].campaigns.end() );
		}
		tabs.resize( MAX_CAMPAIGN_TABS );
		shown = MAX_CAMPAIGN_TABS;
	}
	if( shown < 2 )
	{
		shown = 0;
	}

	Int x = 0, y = 0, width = 0, height = 0, right = 0;
	if( buttonTab[0] )
	{
		buttonTab[0]->winGetPosition( &x, &y );
		buttonTab[0]->winGetSize( &width, &height );
		right = x + width;
	}
	if( buttonBack )
	{
		Int backX = 0, backY = 0, backWidth = 0, backHeight = 0;
		buttonBack->winGetPosition( &backX, &backY );
		buttonBack->winGetSize( &backWidth, &backHeight );
		right = backX + backWidth;
	}
	const Int gap = 4;
	if( shown > 0 && ( right - x - gap * ( shown - 1 ) ) / shown < width )
	{
		width = ( right - x - gap * ( shown - 1 ) ) / shown;
	}

	for( Int tab = 0; tab < MAX_CAMPAIGN_TABS; ++tab )
	{
		if( buttonTab[tab] == nullptr )
		{
			continue;
		}
		if( tab >= shown )
		{
			buttonTab[tab]->winHide( TRUE );
			continue;
		}
		buttonTab[tab]->winSetPosition( x + tab * ( width + gap ), y );
		buttonTab[tab]->winSetSize( width, height );
		GadgetButtonSetText( buttonTab[tab], fetchLabel( tabs[tab].label, tabs[tab].label ) );
		buttonTab[tab]->winHide( FALSE );
	}
}

static void populateCampaigns()
{
	GadgetListBoxReset( listboxCampaigns );
	for( Int tab = 0; tab < (Int)tabs.size() && tab < MAX_CAMPAIGN_TABS; ++tab )
	{
		if( buttonTab[tab] )
		{
			buttonTab[tab]->winEnable( tab != activeTab );
		}
	}
	if( activeTab < 0 || activeTab >= (Int)tabs.size() )
	{
		return;
	}

	const std::vector< Campaign* > &campaigns = tabs[activeTab].campaigns;
	for( size_t i = 0; i < campaigns.size(); ++i )
	{
		Campaign *campaign = campaigns[i];
		Int row = GadgetListBoxAddEntryText( listboxCampaigns, fetchLabel( campaign->m_campaignNameLabel, campaign->m_name ), textColor, -1 );
		GadgetListBoxSetItemData( listboxCampaigns, campaign, row );
	}
}

static void showMapPreview( const Mission *mission )
{
	if( mapWindow == nullptr )
	{
		return;
	}
	const MapMetaData *map = ( mission && TheMapCache ) ? TheMapCache->findMap( mission->m_mapName ) : nullptr;
	Image *image = map ? getMapPreviewImage( mission->m_mapName ) : nullptr;
	mapWindow->winSetUserData( (void *)map );
	if( image )
	{
		mapWindow->winSetStatus( WIN_STATUS_IMAGE );
		mapWindow->winSetEnabledImage( 0, image );
	}
	else
	{
		mapWindow->winClearStatus( WIN_STATUS_IMAGE );
	}
}

static void populateChapters( Campaign *campaign )
{
	GadgetListBoxReset( listboxChapters );
	if( campaign == nullptr )
	{
		showMapPreview( nullptr );
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

	// The default row is the first open mission still to be won, else the last open one
	Int select = 0;
	Int lastUnlocked = 0;
	Bool foundNext = FALSE;
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

		Bool unlocked = TheCampaignManager->isMissionUnlocked( campaign, *it );
		UnicodeString text;
		text.format( L"%d. %s", number, title.str() );
		Int row = GadgetListBoxAddEntryText( listboxChapters, text, unlocked ? textColor : lockedColor, -1 );
		GadgetListBoxSetItemData( listboxChapters, *it, row );
		if( unlocked )
		{
			lastUnlocked = row;
			if( !foundNext && !TheCampaignManager->isMissionCompleted( campaign, *it ) )
			{
				select = row;
				foundNext = TRUE;
			}
		}
	}
	if( !foundNext )
	{
		select = lastUnlocked;
	}
	GadgetListBoxSetSelected( listboxChapters, select );
}

static void updatePlayButton()
{
	Campaign *campaign = getSelectedCampaign();
	Mission *mission = getSelectedMission();
	showMapPreview( mission );
	if( buttonPlay )
	{
		buttonPlay->winEnable( TheCampaignManager->isMissionUnlocked( campaign, mission ) );
	}
}

static void selectTab( Int tab )
{
	if( tab < 0 || tab >= (Int)tabs.size() )
	{
		return;
	}
	activeTab = tab;

	// Selecting the campaign fills the chapter list through GLM_SELECTED
	populateCampaigns();
	GadgetListBoxSetSelected( listboxCampaigns, 0 );
}

static void selectCurrentCampaign()
{
	// Coming back from a mission reopens its campaign so the next chapter is the default row
	Campaign *current = TheCampaignManager->getCurrentCampaign();
	for( size_t tab = 0; tab < tabs.size(); ++tab )
	{
		const std::vector< Campaign* > &campaigns = tabs[tab].campaigns;
		for( size_t i = 0; i < campaigns.size(); ++i )
		{
			if( campaigns[i] == current )
			{
				selectTab( (Int)tab );
				GadgetListBoxSetSelected( listboxCampaigns, (Int)i );
				return;
			}
		}
	}
	selectTab( 0 );
}

static void startChapter()
{
	Campaign *campaign = getSelectedCampaign();
	Mission *mission = getSelectedMission();
	if( gameStarting || campaign == nullptr || mission == nullptr )
	{
		return;
	}
	if( !TheCampaignManager->isMissionUnlocked( campaign, mission ) )
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
	buttonPlay = TheWindowManager->winGetWindowFromId( parent, buttonPlayID );
	buttonBack = TheWindowManager->winGetWindowFromId( parent, buttonBackID );
	mapWindow = TheWindowManager->winGetWindowFromId( parent, TheNameKeyGenerator->nameToKey( "CampaignChapters.wnd:MapWindow" ) );
	for( Int tab = 0; tab < MAX_CAMPAIGN_TABS; ++tab )
	{
		AsciiString name;
		name.format( "CampaignChapters.wnd:ButtonTab%d", tab );
		buttonTabID[tab] = TheNameKeyGenerator->nameToKey( name );
		buttonTab[tab] = TheWindowManager->winGetWindowFromId( parent, buttonTabID[tab] );
	}

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

	buildTabs();
	layoutTabs();
	selectCurrentCampaign();

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
			updatePlayButton();
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
			else
			{
				for( Int tab = 0; tab < MAX_CAMPAIGN_TABS; ++tab )
				{
					if( controlID == buttonTabID[tab] )
					{
						selectTab( tab );
						break;
					}
				}
			}
			break;
		}

		default:
			return MSG_IGNORED;
	}

	return MSG_HANDLED;
}
