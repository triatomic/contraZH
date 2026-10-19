/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

#ifndef RETAIL_COMPATIBLE_CRC
#define RETAIL_COMPATIBLE_CRC (1) // Game is expected to be CRC compatible with retail Generals 1.08, Zero Hour 1.04
#endif

#ifndef USE_DETERMINISTIC_MATH
#define USE_DETERMINISTIC_MATH (1) // Game uses deterministic math for game simulation compatibility among different system architectures in peer to peer networks. Independent of RETAIL_COMPATIBLE_CRC; CMake sets 0 for retail and VC6 builds
#endif

#if defined(__has_include)
#if __has_include("gmath.h")
#define HAS_GAMEMATH (1)
#endif
#endif

#if USE_DETERMINISTIC_MATH && !HAS_GAMEMATH
#error "USE_DETERMINISTIC_MATH needs GameMath's gmath.h on the include path"
#endif
