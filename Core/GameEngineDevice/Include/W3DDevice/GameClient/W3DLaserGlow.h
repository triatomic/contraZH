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
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include <vector>

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
	void appendObjectLights(std::vector<W3DShaderManager::PixelLight> &lights) const;

	void ReleaseResources();	///< drops the shaders before a device reset; the next draw loads them again

private:
	enum { MAX_GLOWS = 64, MAX_GROUP_BEAMS = 7, GROUP_SHADERS = 3 };

	struct Glow
	{
		Vector3 start;
		Vector3 end;
		Real reach;
		Vector3 color;
		const BeamShaderTuning *pulses;
	};

	// Heightmap samples from lo to hi inclusive, so the cells run from lo to hi - 1.
	struct Footprint
	{
		Int loX;
		Int loY;
		Int hiX;
		Int hiY;
	};

	static Footprint findFootprint(WorldHeightMap *map, const Glow &glow);
	static void beamConstants(const Glow &glow, const Vector3 &sceneLight, const Vector4 &pulse, Vector4 *constants);
	static Bool reachesCell(const Glow *const *glows, const Footprint *footprints, Int count, Int x, Int y);
	void drawGlow(WorldHeightMap *map, const Glow &glow, const Vector3 &sceneLight, const Vector4 &pulse, IDirect3DTexture8 *noise);
	Bool drawGroup(WorldHeightMap *map, const Int *members, Int count, const Vector3 &sceneLight, const Vector4 *pulses, IDirect3DTexture8 *noise);
	static Bool drawCells(WorldHeightMap *map, const Glow *const *glows, Int count, const Footprint &box, DWORD shader,
		const Vector4 *constants, Int constantCount, IDirect3DTexture8 *noise);

	Glow m_glows[MAX_GLOWS];
	Int m_count;
	DWORD m_shader;
	DWORD m_groupShaders[GROUP_SHADERS];	///< combine 2, 4 and 7 overlapping glows in one pass, where ps_2_a runs
	Bool m_loaded;
};

extern W3DLaserGlow *TheW3DLaserGlow;
