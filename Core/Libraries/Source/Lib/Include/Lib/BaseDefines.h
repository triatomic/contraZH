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
#if defined(GENONLINE)
#define RETAIL_COMPATIBLE_CRC (0)
#else
#define RETAIL_COMPATIBLE_CRC (1) // Game is expected to be CRC compatible with retail Generals 1.08, Zero Hour 1.04
#endif
#endif

#ifndef RETAIL_COMPATIBLE_DATA
#define RETAIL_COMPATIBLE_DATA (1) // Game and tool data files are expected to be compatible with retail Generals 1.08 and Zero Hour 1.04
#endif

#ifndef USE_DETERMINISTIC_MATH
#if defined(GENONLINE)
#define USE_DETERMINISTIC_MATH (0) // RTS_GENERALS_ONLINE_DETERMINISTIC_MATH turns it on for a build that must match other operating systems
#else
#define USE_DETERMINISTIC_MATH (1) // Game uses deterministic math for game simulation compatibility among different system architectures in peer to peer networks
#endif
#endif

#if defined(__has_include)
#if __has_include(<gmath.h>)
#define HAS_GAMEMATH (1)
#endif
#endif

#if !HAS_GAMEMATH || RETAIL_COMPATIBLE_CRC
#undef USE_DETERMINISTIC_MATH // Unfortunately cannot actually use deterministic math
#endif

#if USE_DETERMINISTIC_MATH && (defined(_M_IX86) || defined(_M_X64) || defined(__SSE2__))
#define USE_SSE_SQRT (1) // IEEE 754 rounds sqrt exactly, so SSE returns the same bits as GameMath's software sqrt
#endif
