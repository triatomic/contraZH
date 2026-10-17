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

// Counts each frame's draw calls by bound vertex and pixel shader, hooking the D3D9 device's draw and shader calls while enabled.
class ShaderDrawCounterClass
{
public:
	enum { MAX_LINES = 64, MAX_LINE_LENGTH = 96 };

	// Stays off in a D3D8 build, which has no shaders to count.
	static void Set_Enabled(bool enabled);
	static bool Is_Enabled();

	// Publishes the frame's lines and starts counting the next one.
	static void End_Frame();
	static void Release_Device();

	// A reset unbinds the shaders without going through the hooks.
	static void Device_Reset();

	static int Get_Line_Count();
	static const char * Get_Line(int index);
};
