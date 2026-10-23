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
struct BeamShaderTuning;
struct DisruptionShaderInfo;

// Fades particle sprites near the surface behind them, shades flames as fire, electricity as arcs, beams as lasers and cryo effects as ice, and draws the heat haze behind flames and the disruption behind jammers.
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

	/// Whether laser beams and streaks get the laser effect at all, loading its shaders on first ask.
	Bool laserEnabled();

	/// Whether cryo beams, streaks and sprites get the cryo effect at all, loading its shaders on first ask.
	Bool cryoEnabled();

	/// The noise laser pulses run on, with pulse x = travel so far, y = world to noise scale, z = swing, which is 0 with laser shading off or no settings.
	IDirect3DTexture8 *getLaserPulse(const BeamShaderTuning *pulses, Vector4 &pulse);

	/// Copies the scene for the haze pass. False leaves the haze undrawn.
	Bool beginHaze();

	/// Whether anything gets the disruption effect at all, loading its shaders on first ask.
	Bool disruptionEnabled();

	/// Copies the scene for the disruption pass, whose draws bind only until endDisruption. False leaves them undrawn.
	Bool beginDisruption();
	void endDisruption();

	/// The effect data is a flame's FlameShaderTuning, a beam's or mesh's BeamShaderTuning or with EFFECT_DISRUPT the draw's DisruptionShaderInfo, and null takes GameData.ini's.
	virtual bool Begin(const ShaderClass &shader, unsigned effects, const void *effectData) override;
	virtual void End() override;
	virtual bool Can_Disrupt() override;

	void ReleaseResources();	///< drops the shaders and textures before a device reset; the next draw makes them again

private:
	Bool loadShaders();
	void createNoise();
	Vector4 setClipConstants(Real width, Real height);
	void setWorldConstants(Int firstRegister);
	Bool bindSceneDepth(DWORD shader);
	Bool bindTerrainHeight(DWORD shader);
	void bindFlame(const FlameShaderTuning &tuning, Bool beam, Bool mesh);
	void bindElectric(const BeamShaderTuning &tuning, Bool beam, Bool mesh);
	void bindLaser(const BeamShaderTuning &tuning);
	void bindCryo(const BeamShaderTuning &tuning, Bool beam);
	Bool bindHaze(const ShaderClass &shader, const FlameShaderTuning &tuning);
	Bool bindDisruption(const ShaderClass &shader, const DisruptionShaderInfo &info);
	void bindSceneCopy();

	DWORD m_depthShader;
	DWORD m_heightShader;
	DWORD m_flameDepthShader;
	DWORD m_flameHeightShader;
	DWORD m_flameShader;
	DWORD m_electricDepthShader;
	DWORD m_electricHeightShader;
	DWORD m_electricShader;
	DWORD m_laserDepthShader;
	DWORD m_laserHeightShader;
	DWORD m_laserShader;
	DWORD m_cryoDepthShader;
	DWORD m_cryoHeightShader;
	DWORD m_cryoShader;
	DWORD m_cryoBeamDepthShader;
	DWORD m_cryoBeamHeightShader;
	DWORD m_cryoBeamShader;
	DWORD m_hazeShader;
	DWORD m_disruptionShaders[3];	///< one per DisruptionShaderInfo::Shape
	IDirect3DTexture8 *m_noise;
	IDirect3DTexture8 *m_sceneCopy;
	Bool m_loaded;
	Bool m_disrupting;	///< between beginDisruption and endDisruption, while the scene copy is the disruption's
	unsigned m_bound;		///< effects the current Begin bound, for End to undo
	D3DMATRIX m_view;
	D3DMATRIX m_projection;
	D3DMATRIX m_toWorld;
};

extern W3DSoftParticles *TheW3DSoftParticles;
