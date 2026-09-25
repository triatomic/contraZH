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
class Vector4;
struct FlameShaderTuning;

// Fades particle sprites near the surface behind them, shades flames as fire and electricity as arcs, and draws the heat haze behind flames.
class W3DSoftParticles : public SoftParticleHookClass
{
public:
	W3DSoftParticles();
	virtual ~W3DSoftParticles() override;

	/// Takes this pass's camera, before its particles draw. They draw with an identity view.
	void beginPass(RenderInfoClass &rinfo);

	/// Whether flame systems get the flame effect at all, loading its shaders on first ask.
	Bool flameEnabled();

	/// Whether electric systems get the electric effect at all, loading its shaders on first ask.
	Bool electricEnabled();

	/// Copies the scene for the haze pass. False leaves the haze undrawn.
	Bool beginHaze();

	/// The effect data is the ParticleSystemTemplate with flame settings of its own, or null.
	virtual bool Begin(const ShaderClass &shader, unsigned effects, const void *effectData) override;
	virtual void End() override;

	void ReleaseResources();	///< drops the shaders and textures before a device reset; the next draw makes them again

private:
	Bool loadShaders();
	void createNoise();
	Vector4 setClipConstants(Real width, Real height);
	void setWorldConstants(Int firstRegister);
	Bool bindSceneDepth(DWORD shader);
	Bool bindTerrainHeight(DWORD shader);
	void bindFlame(const FlameShaderTuning &tuning);
	void bindElectric();
	Bool bindHaze(const ShaderClass &shader, const FlameShaderTuning &tuning);

	DWORD m_depthShader;
	DWORD m_heightShader;
	DWORD m_flameDepthShader;
	DWORD m_flameHeightShader;
	DWORD m_flameShader;
	DWORD m_electricDepthShader;
	DWORD m_electricHeightShader;
	DWORD m_electricShader;
	DWORD m_hazeShader;
	IDirect3DTexture8 *m_noise;
	IDirect3DTexture8 *m_sceneCopy;
	Bool m_loaded;
	unsigned m_bound;		///< effects the current Begin bound, for End to undo
	D3DMATRIX m_view;
	D3DMATRIX m_projection;
	D3DMATRIX m_toWorld;
};

extern W3DSoftParticles *TheW3DSoftParticles;
