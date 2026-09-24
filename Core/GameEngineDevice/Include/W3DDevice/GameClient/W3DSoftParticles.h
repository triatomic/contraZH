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

// FILE: W3DSoftParticles.h /////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"
#include "WW3D2/sortingrenderer.h"
#include "WW3D2/dx8compat.h"

class RenderInfoClass;

// Fades particle sprites where they near the surface behind them. With the scene's depth readable
// that is anything already drawn; otherwise it is the terrain alone.
class W3DSoftParticles : public SoftParticleHookClass
{
public:
	W3DSoftParticles();
	virtual ~W3DSoftParticles() override;

	/// Takes this pass's camera, before its particles draw. They draw with an identity view.
	void beginPass(RenderInfoClass &rinfo);

	virtual bool Begin(const ShaderClass &shader) override;
	virtual void End() override;

	void ReleaseResources();	///< drops the shaders before a device reset; the next draw reloads them

private:
	Bool loadShaders();
	Bool bindSceneDepth();
	Bool bindTerrainHeight();

	DWORD m_depthShader;
	DWORD m_heightShader;
	Bool m_loaded;
	D3DMATRIX m_view;
	D3DMATRIX m_projection;
};

extern W3DSoftParticles *TheW3DSoftParticles;
