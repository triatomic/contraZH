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

// FILE: W3DDeferredLights.h ////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"
#include "WW3D2/dx8compat.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include <vector>

class RenderInfoClass;

// Adds the frame's many small lights to the opaque scene in one instanced draw, from the scene's readable depth.
class W3DDeferredLights
{
public:
	W3DDeferredLights();
	~W3DDeferredLights();

	/// Whether the pass can draw here, loading its shaders on first ask. Lights it cannot take keep their own passes.
	Bool isEnabled();

	/// Hands over the lights for the next render, which lights ground and objects alike.
	void setLights(const std::vector<W3DShaderManager::PixelLight> &lights);

	void render(RenderInfoClass &rinfo);

	void ReleaseResources();	///< drops the shaders and buffers before a device reset; the next draw makes them again

private:
	Bool loadResources();

	std::vector<W3DShaderManager::PixelLight> m_lights;
	DWORD m_vertexShader;
	DWORD m_pixelShader;
	Bool m_loaded;
	Bool m_failed;	///< a render found no way to draw, so the lights go back to their own passes
};

extern W3DDeferredLights *TheW3DDeferredLights;
