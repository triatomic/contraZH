/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

#include "PreRTS.h"

#include "Common/GameUtility.h"
#include "Common/GlobalData.h"
#include "Common/OptionPreferences.h"
#include "Common/PlayerList.h"
#include "Common/Player.h"
#include "Common/Radar.h"

#include "GameClient/Color.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/InGameUI.h"
#include "GameClient/ParticleSys.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/GhostObject.h"
#include "GameLogic/PartitionManager.h"

namespace rts
{

namespace detail
{
static Bool s_relationshipsChanged = false; ///< basic house colors are stale, see markRelationshipsChanged

static Bool isBasicHouseColorMode()
{
#if RTS_ZEROHOUR
	return TheGlobalData != nullptr && TheGlobalData->m_houseColorMode == HouseColorMode_Basic;
#else
	return false;
#endif
}

static Bool wearsColor(const Player* player, Color color)
{
	// model colors carry a forced opaque alpha, player colors may not
	const Color opaque = color | 0xFF000000;
	return (player->getPlayerColor() | 0xFF000000) == opaque || (player->getPlayerNightColor() | 0xFF000000) == opaque;
}

static void changePlayerCommon(Player* player)
{
	TheParticleSystemManager->setLocalPlayerIndex(player->getPlayerIndex());
	ThePartitionManager->refreshShroudForLocalPlayer();
	TheGhostObjectManager->setLocalPlayerIndex(player->getPlayerIndex());
	TheGameClient->updateFakeDrawables();
	// TheSuperHackers @feature Basic house colors follow whoever is being watched.
	if (isBasicHouseColorMode())
		refreshHouseColors();
	TheRadar->refreshObjects();
	TheInGameUI->deselectAllDrawables();
}

} // namespace detail

bool localPlayerHasRadar()
{
	// Using "local" instead of "observed or local" player because as an observer we prefer
	// the radar to be turned on when observing a player that has no radar.
	const Player* player = ThePlayerList->getLocalPlayer();
	const PlayerIndex index = player->getPlayerIndex();

	if (TheRadar->isRadarForced(index))
		return true;

	if (!TheRadar->isRadarHidden(index) && player->hasRadar())
		return true;

	return false;
}

Player* getObservedOrLocalPlayer()
{
	DEBUG_ASSERTCRASH(TheControlBar != nullptr, ("TheControlBar is null"));
	Player* player = TheControlBar->getObservedPlayer();
	if (player == nullptr)
	{
		DEBUG_ASSERTCRASH(ThePlayerList != nullptr, ("ThePlayerList is null"));
		player = ThePlayerList->getLocalPlayer();
	}
	return player;
}

Player* getObservedOrLocalPlayer_Safe()
{
	Player* player = nullptr;

	if (TheControlBar != nullptr)
		player = TheControlBar->getObservedPlayer();

	if (player == nullptr)
		if (ThePlayerList != nullptr)
			player = ThePlayerList->getLocalPlayer();

	return player;
}

PlayerIndex getObservedOrLocalPlayerIndex_Safe()
{
	if (Player* player = getObservedOrLocalPlayer_Safe())
		return player->getPlayerIndex();

	return 0;
}

void changeLocalPlayer(Player* player)
{
	DEBUG_ASSERTCRASH(player != nullptr, ("Player is null"));

	ThePlayerList->setLocalPlayer(player);
	TheControlBar->setObserverLookAtPlayer(nullptr);
	TheControlBar->setObservedPlayer(nullptr);
	TheControlBar->setControlBarSchemeByPlayer(player);
	TheControlBar->initSpecialPowershortcutBar(player);

	detail::changePlayerCommon(player);
}

void changeObservedPlayer(Player* player)
{
	TheControlBar->setObserverLookAtPlayer(player);

	const Bool canBeginObservePlayer = TheGlobalData->m_enablePlayerObserver && TheGhostObjectManager->trackAllPlayers();
	const Bool canEndObservePlayer = TheControlBar->getObservedPlayer() != nullptr && TheControlBar->getObserverLookAtPlayer() == nullptr;

	if (canBeginObservePlayer || canEndObservePlayer)
	{
		TheControlBar->setObservedPlayer(player);

		Player *becomePlayer = player;
		if (becomePlayer == nullptr)
			becomePlayer = ThePlayerList->findPlayerWithNameKey(TheNameKeyGenerator->nameToKey("ReplayObserver"));
		detail::changePlayerCommon(becomePlayer);
	}
}

Color getHouseColorForLocalView(Color color, const Player* owner)
{
	if (color == 0 || !detail::isBasicHouseColorMode() || ThePlayerList == nullptr)
		return color;

	// An observer watching nobody in particular has no side to color by. A defeated player still
	// has one, so they keep the basic colors.
	const Player* viewer = getObservedOrLocalPlayer_Safe();
	if (viewer == nullptr || viewer->isPlayerObserver())
		return color;

	const Player* wearer = (owner != nullptr && detail::wearsColor(owner, color)) ? owner : nullptr;
	for (Int i = 0; wearer == nullptr && i < ThePlayerList->getPlayerCount(); ++i)
	{
		const Player* player = ThePlayerList->getNthPlayer(i);
		if (player != nullptr && detail::wearsColor(player, color))
			wearer = player;
	}

	// A custom indicator color set by a script or module belongs to no player, so it stays.
	if (wearer == nullptr)
		return color;

	// The same blue, gold and red as Contra's multiplayer colors, so they read as player colors.
	if (wearer == viewer)
		return GameMakeColor(67, 104, 254, 255);

	switch (viewer->getRelationship(wearer->getDefaultTeam()))
	{
		case ALLIES:
			return GameMakeColor(221, 226, 13, 255);
		case ENEMIES:
			return GameMakeColor(255, 0, 0, 255);
		default:
			return color; // civilians and other neutrals keep their own color
	}
}

void refreshHouseColors()
{
	if (TheGameClient != nullptr)
	{
		for (Drawable* draw = TheGameClient->firstDrawable(); draw != nullptr; draw = draw->getNextDrawable())
			draw->refreshIndicatorColor();
	}

	if (TheRadar != nullptr)
		TheRadar->refreshObjectColors();
}

void markRelationshipsChanged()
{
	// Alliances change in the logic (diplomacy, scripts, challenge map setup), often many at once,
	// so only remember it here and repaint once on the client side.
	detail::s_relationshipsChanged = true;
}

void updateHouseColors()
{
	if (!detail::s_relationshipsChanged)
		return;

	detail::s_relationshipsChanged = false;
	if (detail::isBasicHouseColorMode())
		refreshHouseColors();
}

} // namespace rts
