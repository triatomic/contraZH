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

// FILE: W3DShockwave.h /////////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"
#include "Common/GameType.h"
#include "WW3D2/dx8compat.h"

class RenderInfoClass;

// Expanding rings that bend the finished scene, for the blast of large explosions.
class W3DShockwaveManager
{
public:
	W3DShockwaveManager();
	~W3DShockwaveManager();

	/// radius is how far the ring travels, width its thickness and strength how far it bends the scene, all in world units
	void add(const Coord3D &position, Real radius, Real width, Real strength, UnsignedInt durationMs);
	/// draws the live rings over the scene, after the particles
	void render(RenderInfoClass &rinfo);
	void ReleaseResources();	///< drops the scene copy before a device reset; render recreates it

private:
	enum { MAX_SHOCKWAVES = 16 };

	struct Shockwave
	{
		Coord3D position;
		Real radius;
		Real width;
		Real strength;
		UnsignedInt startMs;
		UnsignedInt durationMs;
	};

	Bool copyScene();

	Shockwave m_shockwaves[MAX_SHOCKWAVES];
	Int m_count;
	IDirect3DTexture8 *m_sceneCopy;
	DWORD m_shader;
	Bool m_loaded;
};

extern W3DShockwaveManager *TheW3DShockwaves;
