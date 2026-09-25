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

// FILE: W3DLaserGlow.h ///////////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"
#include "WW3D2/dx8compat.h"
#include "WWMath/vector3.h"

class RenderInfoClass;
class Vector4;
class WorldHeightMap;
struct BeamShaderTuning;

// Lights the terrain along laser beams per pixel, with a light shaped like the beam. Direct3D 9 only;
// without it W3DLaserDraw lights the ground with a strip of dynamic lights.
class W3DLaserGlow
{
public:
	W3DLaserGlow();
	~W3DLaserGlow();

	/// Whether beams light the ground here, loading the shader on first ask.
	Bool isEnabled();

	/// Lights the ground within reach of the beam this frame. The color carries the light's intensity.
	/// The light pulses with the beam's laser settings, or holds steady with none.
	void add(const Vector3 &start, const Vector3 &end, Real reach, const Vector3 &color, const BeamShaderTuning *pulses);

	/// Draws this frame's glows over the terrain, then forgets them.
	void render(RenderInfoClass &rinfo);

	void ReleaseResources();	///< drops the shader before a device reset; the next draw loads it again

private:
	enum { MAX_GLOWS = 64 };

	struct Glow
	{
		Vector3 start;
		Vector3 end;
		Real reach;
		Vector3 color;
		const BeamShaderTuning *pulses;
	};

	void drawGlow(WorldHeightMap *map, const Glow &glow, const Vector3 &sceneLight, const Vector4 &pulse, IDirect3DTexture8 *noise);

	Glow m_glows[MAX_GLOWS];
	Int m_count;
	DWORD m_shader;
	Bool m_loaded;
};

extern W3DLaserGlow *TheW3DLaserGlow;
