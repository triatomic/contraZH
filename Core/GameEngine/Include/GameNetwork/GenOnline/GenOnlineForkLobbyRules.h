#pragma once

// The fork's lobby rules on the GenOnline lobby's Rules tab. The host keeps them on its own client and shares them through a hidden chat announcement.

#include "GameClient/Color.h"
#include "GameNetwork/LobbyRules.h"

class AsciiString;
class GameWindow;
class UnicodeString;

// Finds the Rules tab and its checkboxes in the lobby layout. Every window may be null.
void ForkLobbyRulesInit(GameWindow *parent, GameWindow *chatList, GameWindow *chatEntry);
void ForkLobbyRulesDeinit();

// Shows the rule state on the checkboxes and enables them for a host who may change them.
void ForkLobbyRulesRefresh();

// The host repeats the rules when the roster grows, because a newcomer's Rules tab starts blank.
void ForkLobbyRulesOnRosterChanged();

// The rules left with the previous host, so they reset to OFF.
void ForkLobbyRulesOnHostMigrated();

// TRUE when the line is the host's hidden sync, which the chat never shows.
Bool ForkLobbyRulesOnChatLine(const UnicodeString &line);

// TRUE when the control is a tab button or a rule checkbox.
Bool ForkLobbyRulesOnSelected(Int controlID);

// TRUE when the lower-case token is a rule command.
Bool ForkLobbyRulesOnSlashCommand(const AsciiString &token);

void ForkLobbyRulesAddHelp(GameWindow *chatList, Color color);

// The rule state of the current lobby. FALSE outside a lobby.
Bool ForkLobbyRuleIsOn(LobbyRule rule);
