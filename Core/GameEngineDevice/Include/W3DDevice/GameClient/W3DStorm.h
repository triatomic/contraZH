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

// FILE: W3DStorm.h /////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"
#include "Common/GameType.h"
#include "GameClient/StormShader.h"
#include "WW3D2/dx8compat.h"

class RenderInfoClass;
class DX8VertexBufferClass;
class DX8IndexBufferClass;

// Sandstorms and snowstorms that stand on the ground, for FXList's Storm and W3DStormDraw.
class W3DStormManager
{
public:
	W3DStormManager();
	~W3DStormManager();

	/// Starts a storm and returns its handle, or 0 when it cannot start. A duration of 0 lasts until remove.
	Int add(const Coord3D &position, const StormShaderInfo &info, UnsignedInt durationMs);
	void move(Int handle, const Coord3D &position);
	void show(Int handle, Bool visible);	///< a hidden storm dies down and stays, and builds up again when shown
	void remove(Int handle);	///< the storm dies down over its fade time, then goes
	void reset();	///< drops every storm at once
	Bool update();	///< ages the storms and drops the ones that ran out, and tells whether any are left
	void render(RenderInfoClass &rinfo);
	void ReleaseResources();	///< drops the shaders, buffers and noise before a device reset; render makes them again

private:
	enum { MAX_STORMS = 8 };

	struct Storm
	{
		Int handle;
		Coord3D position;
		StormShaderInfo own;	///< the entry's settings, unset where it leaves GameData.ini's
		StormShaderInfo info;	///< resolved again every update, so GameData.ini edits show on a running storm
		UnsignedInt startMs;
		UnsignedInt durationMs;	///< 0 while the storm has no end
		Real fadeMs;
		Bool visible;
		Real shown;	///< 0 to 1, following visible over the fade time
	};

	Storm *find(Int handle);
	Bool loadShaders();
	void createNoise();
	Bool createGrains();
	Real level(const Storm &storm, UnsignedInt now) const;

	Storm m_storms[MAX_STORMS];
	Int m_count;
	Int m_nextHandle;
	UnsignedInt m_lastUpdateMs;
	Bool m_loaded;
	DWORD m_hazeVertexShader;
	DWORD m_hazePixelShader;
	DWORD m_grainVertexShader;
	DWORD m_grainPixelShader;
	IDirect3DTexture8 *m_noise;
	DX8VertexBufferClass *m_grainVertices;
	DX8IndexBufferClass *m_grainIndices;
};

extern W3DStormManager *TheW3DStorms;
