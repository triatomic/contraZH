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

// A single directional shadow map cast by the sun.
//
// Runs alongside the legacy stencil volume and decal shadows and never at the same
// time, so a hard volume shadow can never meet a filtered map shadow.

#pragma once

#include "WWLib/always.h"
#include "WWMath/matrix4.h"
#include "WWMath/vector3.h"
#include "WW3D2/matpass.h"

class CameraClass;
class RenderInfoClass;
class SphereClass;
class TextureClass;
class ZTextureClass;

// Installs the depth state for one caster. The mesh renderer re-applies material
// state per object, so it has to be installed per pass rather than once.
class W3DShadowDepthMaterialPassClass : public MaterialPassClass
{
public:

	virtual void Install_Materials() const override;
	virtual void UnInstall_Materials() const override;
};

class W3DShadowMap
{
public:

	// How the depth ends up in a texture. The hardware path samples a depth surface
	// directly and gets PCF from the sampler; the packed path encodes depth into
	// RGBA8 for devices and wrappers that cannot sample depth.
	enum DepthMode
	{
		DEPTH_MODE_NONE = 0,
		DEPTH_MODE_HARDWARE,
		DEPTH_MODE_PACKED
	};

	W3DShadowMap();
	~W3DShadowMap();

	Bool init();
	void ReleaseResources();
	Bool ReAcquireResources();

	Bool isAvailable() const { return m_depthMode != DEPTH_MODE_NONE; }
	DepthMode getDepthMode() const { return m_depthMode; }

	// Fits the sun frustum to the ground the camera can see, for this frame.
	void updateFrustum(const CameraClass& camera, const Vector3& lightPosWorld);

	// Fills the map with caster depth. Binds its own render target, then restores the
	// back buffer and the camera in rinfo.
	void renderDepthPass(RenderInfoClass& rinfo);

	// True if a caster with these bounds can reach the fitted area of the map.
	Bool isCasterInRange(const SphereClass& bounds) const;

	const Matrix4x4& getSunViewProjection() const { return m_sunViewProj; }
	const Matrix4x4& getSunProjection() const { return m_sunProjection; }
	TextureClass* peekColorTarget() const { return m_colorTarget; }
	ZTextureClass* peekDepthTarget() const { return m_depthTarget; }
	Int getResolution() const { return m_resolution; }

	// Depth bias in light-space units, recomputed with the fit each frame.
	Real getDepthBias() const { return m_depthBias; }

protected:

	void computeSunViewProjection(const Vector3& center, Real radius, const Vector3& lightDirection);
	void updateCullCamera(const Vector3& center, Real radius, const Vector3& lightDirection);

	DepthMode      m_depthMode;
	Int            m_resolution;
	TextureClass*  m_colorTarget;
	ZTextureClass* m_depthTarget;

	// The mesh renderer culls against a camera, so the depth pass needs one that
	// contains the sun's box. It never reaches the device.
	CameraClass*   m_cullCamera;

	Matrix4x4      m_sunView;
	Matrix4x4      m_sunProjection;
	Matrix4x4      m_sunViewProj;
	Vector3        m_fittedCenter;
	Vector3        m_lightDirection;
	Real           m_depthBias;
	Real           m_fittedRadius;

	W3DShadowDepthMaterialPassClass m_depthPass;
};

extern W3DShadowMap* TheW3DShadowMap;
