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

// Raw CRT transcendentals bypass GameMath; game logic sources compile C4995 as an error.

#pragma once

#include <math.h>
#include <cmath>
#include "WWMath/wwmath.h"
#include "WWMath/matrix4.h"

#if defined(_MSC_VER)
#pragma deprecated(sin, cos, tan, asin, acos, atan, atan2, pow)
#pragma deprecated(sinf, cosf, tanf, asinf, acosf, atanf, atan2f, powf)
#endif
