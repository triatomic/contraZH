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

// FILE: W3DAmbientOcclusion.h //////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"
#include "WW3D2/dx8compat.h"

class RenderInfoClass;
class Vector4;

// Darkens creases and the ground where objects meet it, from the scene's readable depth. Draws
// over the opaque scene, before water, decals and particles.
class W3DAmbientOcclusion
{
public:
	W3DAmbientOcclusion();
	~W3DAmbientOcclusion();

	void render(RenderInfoClass &rinfo);

	void ReleaseResources();	///< drops the shaders and targets before a device reset; the next draw makes them again

private:
	enum { TARGET_OCCLUSION, TARGET_BLUR, TARGET_COUNT };

	Bool loadShaders();
	Bool acquireTargets(UnsignedInt width, UnsignedInt height);
	void releaseTargets();
	void drawQuad(const Vector4 &clipToTarget);

	IDirect3DTexture8 *m_target[TARGET_COUNT];
	IDirect3DSurface8 *m_targetSurface[TARGET_COUNT];
	DWORD m_occlusionShader;
	DWORD m_blurShader;
	Bool m_loaded;
};

extern W3DAmbientOcclusion *TheW3DAmbientOcclusion;
