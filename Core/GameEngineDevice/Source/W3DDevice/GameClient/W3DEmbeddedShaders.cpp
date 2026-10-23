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

// W3DEmbeddedShaders.cpp /////////////////////////////////////////////////////////////////////////
// Looks up the shaders the build links into the executable
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "W3DDevice/GameClient/W3DEmbeddedShaders.h"

#include <ctype.h>

// Matches the generator's bytewise order on lowercase names, with either slash as a backslash.
static Int compareShaderNames(const char *a, const char *b)
{
	for (;;)
	{
		Int ca = tolower((unsigned char)*a++);
		Int cb = tolower((unsigned char)*b++);
		if (ca == '/')
		{
			ca = '\\';
		}
		if (cb == '/')
		{
			cb = '\\';
		}
		if (ca != cb || ca == 0)
		{
			return ca - cb;
		}
	}
}

Bool Find_Embedded_Shader(const char *path, const void *&data, DWORD &size)
{
	Int low = 0;
	Int high = (Int)TheEmbeddedShaderCount - 1;
	while (low <= high)
	{
		Int mid = (low + high) / 2;
		Int order = compareShaderNames(path, TheEmbeddedShaders[mid].name);
		if (order == 0)
		{
			data = TheEmbeddedShaders[mid].data;
			size = TheEmbeddedShaders[mid].size;
			return true;
		}
		if (order < 0)
		{
			high = mid - 1;
		}
		else
		{
			low = mid + 1;
		}
	}
	return false;
}
