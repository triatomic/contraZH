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

#pragma once

#include "Lib/BaseType.h"

// For miscellaneous game utility functions.

class Player;
typedef Int PlayerIndex;
typedef Int Color;

namespace rts
{

bool localPlayerHasRadar();
Player* getObservedOrLocalPlayer(); ///< Get the current observed or local player. Is never null.
Player* getObservedOrLocalPlayer_Safe(); ///< Get the current observed or local player. Is never null, except when the application does not have players.
PlayerIndex getObservedOrLocalPlayerIndex_Safe(); ///< Get the current observed or local player index. Returns 0 when the application does not have players.

void changeLocalPlayer(Player* player); //< Change local player during game. Must not pass null.
void changeObservedPlayer(Player* player); ///< Change observed player during game. Can pass null: is identical to passing the "ReplayObserver" player.

// TheSuperHackers @feature HouseColorMode (Options.ini). The house color to draw for `color` as the
// observed or local player sees it. `owner` is the player normally drawn in that color, when known;
// any other player wearing it, such as a disguise, is found by the color itself. Client side only:
// the result must never reach the game logic.
Color getHouseColorForLocalView(Color color, const Player* owner);
void refreshHouseColors(); ///< Recolor every model and radar blip after HouseColorMode or the viewing player changed.
void markRelationshipsChanged(); ///< Cheap, safe from the logic: basic house colors repaint on the next client frame.
void updateHouseColors(); ///< Once per client frame: does the repaint markRelationshipsChanged asked for.

} // namespace rts
