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

#pragma once

// The rules a Generals Online host keeps on its own client, the service having no field for them
enum LobbyRule
{
	LOBBY_RULE_DIPLOMACY,
	LOBBY_RULE_FREE_RADAR,
	LOBBY_RULE_REVEAL_MAP,
	LOBBY_RULE_COOP_BUILDINGS,
	LOBBY_RULE_QUICK_GAME,
	LOBBY_RULE_COUNT
};
