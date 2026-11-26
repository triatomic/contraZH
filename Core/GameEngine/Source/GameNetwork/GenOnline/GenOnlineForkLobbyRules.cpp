#include "PreRTS.h"

#include "GameNetwork/GenOnline/GenOnlineForkLobbyRules.h"

#include "Common/GOUIColors.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/GadgetCheckBox.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameNetwork/GUIUtil.h"
#include "GameNetwork/GenOnline/GenOnlineInterfaces.h"

// The Rules tab checkbox, chat command and lobby wording of each LobbyRule, in enum order
struct LobbyRuleControl
{
	const char *window;
	const wchar_t *onText;
	const char *command;
	const wchar_t *help;
	NameKeyType id;
	GameWindow *check;
};

static LobbyRuleControl s_lobbyRules[LOBBY_RULE_COUNT] =
{
	{ "CheckDiplomacyRule", L" is ON: players may change alliances during the match.",
		"diplomacy", L"/diplomacy - Let players change alliances during the match (host only).", NAMEKEY_INVALID, nullptr },
	{ "CheckFreeRadar", L" is ON: every player has radar.",
		"freeradar", L"/freeradar - Give every player radar without a radar source (host only).", NAMEKEY_INVALID, nullptr },
	{ "CheckRevealMap", L" is ON: every player sees the whole map.",
		"revealmap", L"/revealmap - Reveal the whole map to every player (host only).", NAMEKEY_INVALID, nullptr },
	{ "CheckCoopBuildings", L" is ON: allies may finish each other's buildings.",
		"coopbuildings", L"/coopbuildings - Let allies finish each other's buildings (host only).", NAMEKEY_INVALID, nullptr },
	{ "CheckQuickGame", L" is ON: rank requirements are lifted.",
		"quickgame", L"/quickgame - Lift the rank requirements on advanced units and structures (host only).", NAMEKEY_INVALID, nullptr },
	{ "CheckAutoQueue", L" is ON: right click a unit to build it whenever the queue is empty.",
		"autoqueue", L"/autoqueue - Let players right click a unit to build it whenever the queue is empty (host only).", NAMEKEY_INVALID, nullptr },
};

// A hidden announcement carrying one digit per rule, in table order
static const wchar_t *LOBBY_RULES_SYNC = L"[rules] ";

static bool s_rules[LOBBY_RULE_COUNT] = {};
static int64_t s_rulesLobbyID = -1;
static size_t s_syncedMembers = 0;
static LobbyTabs lobbyTabs;
static GameWindow *s_chatList = nullptr;
static GameWindow *s_chatEntry = nullptr;

static GenOnlineLobbyInterface *lobbyInterface()
{
	return GenOnlineServicesManager::GetInterface<GenOnlineLobbyInterface>();
}

// The rules of the current lobby, cleared whenever the lobby changes; null outside a lobby
static bool *currentRules()
{
	GenOnlineLobbyInterface *pLobby = lobbyInterface();
	if (pLobby == nullptr || !pLobby->IsInLobby())
	{
		return nullptr;
	}

	const int64_t lobbyID = pLobby->GetCurrentLobby().lobbyID;
	if (lobbyID != s_rulesLobbyID)
	{
		for (Int i = 0; i < LOBBY_RULE_COUNT; ++i)
		{
			s_rules[i] = false;
		}
		s_rulesLobbyID = lobbyID;
	}
	return s_rules;
}

static void setRuleState(LobbyRule rule, bool on)
{
	bool *rules = currentRules();
	if (rules != nullptr)
	{
		rules[rule] = on;
	}
}

static void clearRules()
{
	for (Int i = 0; i < LOBBY_RULE_COUNT; ++i)
	{
		setRuleState((LobbyRule)i, false);
	}
}

// Quick match lobbies keep their settings fixed
static Bool mayChangeRules(GenOnlineLobbyInterface *pLobby)
{
	return pLobby != nullptr && pLobby->IsHost() && pLobby->CanChangeLobbySettings();
}

static void sendAnnouncement(UnicodeString msg, bool showToHost)
{
	std::shared_ptr<WebSocket> pWS = GenOnlineServicesManager::GetWebSocket();
	if (pWS != nullptr)
	{
		pWS->SendData_LobbyChatMessage(msg, false, true, showToHost);
	}
}

Bool ForkLobbyRuleIsOn(LobbyRule rule)
{
	const bool *rules = currentRules();
	return rules != nullptr && rules[rule];
}

void ForkLobbyRulesInit(GameWindow *parent, GameWindow *chatList, GameWindow *chatEntry)
{
	s_chatList = chatList;
	s_chatEntry = chatEntry;
	s_syncedMembers = 0;
	InitLobbyTabs(lobbyTabs, parent, "GameSpyGameOptionsMenu.wnd");
	for (Int i = 0; i < LOBBY_RULE_COUNT; ++i)
	{
		LobbyRuleControl &rule = s_lobbyRules[i];
		AsciiString fullName;
		fullName.format("GameSpyGameOptionsMenu.wnd:%s", rule.window);
		rule.id = TheNameKeyGenerator->nameToKey(fullName);
		rule.check = TheWindowManager->winGetWindowFromId(parent, rule.id);
		SetLobbyRuleCaption(rule.check, (LobbyRule)i);
	}
}

void ForkLobbyRulesDeinit()
{
	for (Int i = 0; i < LOBBY_RULE_COUNT; ++i)
	{
		s_lobbyRules[i].check = nullptr;
	}
	s_chatList = nullptr;
	s_chatEntry = nullptr;
	lobbyTabs = LobbyTabs();
}

// A refresh echoes each checkbox's message, which the unchanged value turns away
void ForkLobbyRulesRefresh()
{
	GenOnlineLobbyInterface *pLobby = lobbyInterface();
	if (pLobby == nullptr)
	{
		return;
	}

	const Bool mayChange = mayChangeRules(pLobby);
	for (Int i = 0; i < LOBBY_RULE_COUNT; ++i)
	{
		const LobbyRuleControl &rule = s_lobbyRules[i];
		if (rule.check != nullptr)
		{
			GadgetCheckBoxSetChecked(rule.check, ForkLobbyRuleIsOn((LobbyRule)i));
			rule.check->winEnable(mayChange);
		}
	}
}

static void sendRulesSync()
{
	GenOnlineLobbyInterface *pLobby = lobbyInterface();
	if (pLobby == nullptr || !pLobby->IsHost())
	{
		return;
	}

	UnicodeString msg(LOBBY_RULES_SYNC);
	for (Int i = 0; i < LOBBY_RULE_COUNT; ++i)
	{
		msg.concat(ForkLobbyRuleIsOn((LobbyRule)i) ? L"1" : L"0");
	}
	sendAnnouncement(msg, false);
}

void ForkLobbyRulesOnRosterChanged()
{
	GenOnlineLobbyInterface *pLobby = lobbyInterface();
	if (pLobby == nullptr)
	{
		return;
	}

	const size_t members = pLobby->GetCurrentLobby().members.size();
	if (members > s_syncedMembers)
	{
		sendRulesSync();
	}
	s_syncedMembers = members;
}

void ForkLobbyRulesOnHostMigrated()
{
	clearRules();
	ForkLobbyRulesRefresh();
	if (s_chatList != nullptr)
	{
		GadgetListBoxAddEntryText(s_chatList, UnicodeString(L"The lobby rules are OFF. The new host can turn them on from the Rules tab."), GOUIColor(GOUICOLOR_MUTED, GameMakeColor(192, 192, 192, 255)), -1, -1);
	}
}

Bool ForkLobbyRulesOnChatLine(const UnicodeString &line)
{
	const size_t markerLength = wcslen(LOBBY_RULES_SYNC);
	if (wcsncmp(line.str(), LOBBY_RULES_SYNC, markerLength) != 0)
	{
		if (IsLobbyRulesTabShown(lobbyTabs))
		{
			MarkLobbyGameTab(lobbyTabs);
		}
		return FALSE;
	}

	GenOnlineLobbyInterface *pLobby = lobbyInterface();
	const WideChar *flags = line.str() + markerLength;
	if (pLobby == nullptr || pLobby->IsHost() || wcslen(flags) < LOBBY_RULE_COUNT)
	{
		return TRUE;
	}

	for (Int i = 0; i < LOBBY_RULE_COUNT; ++i)
	{
		setRuleState((LobbyRule)i, flags[i] == L'1');
	}
	ForkLobbyRulesRefresh();
	return TRUE;
}

static void setLobbyRule(LobbyRule rule, bool on)
{
	if (!mayChangeRules(lobbyInterface()))
	{
		return;
	}

	setRuleState(rule, on);
	UnicodeString msg = LobbyRuleName(rule);
	msg.concat(on ? s_lobbyRules[rule].onText : L" is OFF.");
	sendAnnouncement(msg, true);
	sendRulesSync();
	ForkLobbyRulesRefresh();
}

// The slash commands flip a rule for the host and explain themselves to anyone else
static void toggleLobbyRule(LobbyRule rule)
{
	GenOnlineLobbyInterface *pLobby = lobbyInterface();
	if (pLobby == nullptr || !pLobby->IsInLobby())
	{
		return;
	}

	if (!mayChangeRules(pLobby))
	{
		if (s_chatList != nullptr)
		{
			UnicodeString msg = LobbyRuleName(rule);
			msg.concat(L": Only the host can change it.");
			GadgetListBoxAddEntryText(s_chatList, msg, GameMakeColor(255, 0, 0, 255), -1, -1);
		}
		return;
	}

	setLobbyRule(rule, !ForkLobbyRuleIsOn(rule));
}

static void showRulesTab(Bool rules)
{
	ShowLobbyTab(lobbyTabs, rules);

	// The chat entry lives on the Game tab
	if (!rules && s_chatEntry != nullptr)
	{
		TheWindowManager->winSetFocus(s_chatEntry);
	}
}

Bool ForkLobbyRulesOnSelected(Int controlID)
{
	if (lobbyTabs.buttonGame != nullptr)
	{
		if (controlID == lobbyTabs.buttonGameID)
		{
			showRulesTab(FALSE);
			return TRUE;
		}
		if (controlID == lobbyTabs.buttonRulesID)
		{
			showRulesTab(TRUE);
			return TRUE;
		}
	}

	for (Int i = 0; i < LOBBY_RULE_COUNT; ++i)
	{
		const LobbyRuleControl &rule = s_lobbyRules[i];
		if (rule.check == nullptr || rule.id != controlID)
		{
			continue;
		}

		if (mayChangeRules(lobbyInterface()))
		{
			const bool on = GadgetCheckBoxIsChecked(rule.check) != FALSE;
			if (on != (ForkLobbyRuleIsOn((LobbyRule)i) != FALSE))
			{
				setLobbyRule((LobbyRule)i, on);
			}
		}
		return TRUE;
	}
	return FALSE;
}

Bool ForkLobbyRulesOnSlashCommand(const AsciiString &token)
{
	for (Int i = 0; i < LOBBY_RULE_COUNT; ++i)
	{
		if (token == s_lobbyRules[i].command)
		{
			toggleLobbyRule((LobbyRule)i);
			return TRUE;
		}
	}
	return FALSE;
}

void ForkLobbyRulesAddHelp(GameWindow *chatList, Color color)
{
	if (chatList == nullptr)
	{
		return;
	}

	for (Int i = 0; i < LOBBY_RULE_COUNT; ++i)
	{
		GadgetListBoxAddEntryText(chatList, UnicodeString(s_lobbyRules[i].help), color, -1, -1);
	}
}
