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

// FILE: GUIUtil.h //////////////////////////////////////////////////////
// Author: Matthew D. Campbell, Sept 2002

#pragma once

#include "GameNetwork/LobbyRules.h"

class GameWindow;
class GameInfo;

void ShowUnderlyingGUIElements( Bool show, const char *layoutFilename, const char *parentName,
															 const char **gadgetsToHide, const char **perPlayerGadgetsToHide );

void PopulateColorComboBox(Int comboBox, GameWindow *comboArray[], GameInfo *myGame, Bool isObserver = FALSE);
void PopulatePlayerTemplateComboBox(Int comboBox, GameWindow *comboArray[], GameInfo *myGame, Bool allowObservers );
void PopulateTeamComboBox(Int comboBox, GameWindow *comboArray[], GameInfo *myGame, Bool isObserver = FALSE);
void PopulateStartingCashComboBox(GameWindow *comboBox, GameInfo *myGame);

void EnableSlotListUpdates( Bool val );
Bool AreSlotListUpdatesEnabled();

void UpdateSlotList( GameInfo *myGame, GameWindow *comboPlayer[],
										GameWindow *comboColor[], GameWindow *comboPlayerTemplate[],
										GameWindow *comboTeam[], GameWindow *buttonAccept[],
										GameWindow *buttonStart, GameWindow *buttonMapStartPosition[] );

void EnableAcceptControls(Bool Enabled, GameInfo *myGame, GameWindow *comboPlayer[],
										GameWindow *comboColor[], GameWindow *comboPlayerTemplate[],
										GameWindow *comboTeam[], GameWindow *buttonAccept[], GameWindow *buttonStart,
										GameWindow *buttonMapStartPosition[], Int slotNum = -1);

// Game and Rules tabs of the Skirmish and LAN lobbies; every pointer is null when the layout lacks them
struct LobbyTabs
{
	NameKeyType buttonGameID;
	NameKeyType buttonRulesID;
	GameWindow *buttonGame;
	GameWindow *buttonRules;
	GameWindow *panelGame;
	GameWindow *panelRules;
};

void InitLobbyTabs( LobbyTabs &tabs, GameWindow *parent, const char *layoutFilename );
void ShowLobbyTab( LobbyTabs &tabs, Bool rules );
Bool IsLobbyRulesTabShown( const LobbyTabs &tabs );
void MarkLobbyGameTab( LobbyTabs &tabs );

UnicodeString LobbyRuleName( LobbyRule rule ); ///< The caption of a Rules page checkbox, the same in every lobby
void SetLobbyRuleCaption( GameWindow *check, LobbyRule rule ); ///< Caption and tooltip; a null check is left alone
