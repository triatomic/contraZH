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

// Counts each frame's draw calls by the vertex and pixel shader bound at the time, one text line per pair.
// While enabled it hooks the D3D9 device's draw and shader calls, so every caller is counted.
class ShaderDrawCounterClass
{
public:
	enum { MAX_LINES = 64, MAX_LINE_LENGTH = 96 };

	static void Set_Enabled(bool enabled);
	static bool Is_Enabled();

	// Publishes the frame's lines and starts counting the next one.
	static void End_Frame();
	static void Release_Device();

	static int Get_Line_Count();
	static const char * Get_Line(int index);
};
