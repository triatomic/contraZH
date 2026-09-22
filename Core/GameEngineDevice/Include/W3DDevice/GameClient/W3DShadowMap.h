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

// Multiplies the sun's shadow into an object after its own passes. Objects light
// fixed function, so this receives without reproducing their lighting in a shader.
class W3DShadowReceiveMaterialPassClass : public MaterialPassClass
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

	// True once the depth pass has filled the map, until clearDepth is called.
	Bool hasDepth() const { return m_hasDepth; }
	void clearDepth() { m_hasDepth = FALSE; }

	// Fits the sun frustum to the ground the camera can see, for this frame.
	void updateFrustum(const CameraClass& camera, const Vector3& lightPosWorld);

	// Fills the map with caster depth. Binds its own render target, then restores the
	// back buffer and the camera in rinfo.
	void renderDepthPass(RenderInfoClass& rinfo);

	// True if a caster with these bounds can reach the fitted area of the map.
	Bool isCasterInRange(const SphereClass& bounds) const;

	// Why candidates were kept or dropped in the current depth pass, for the debug log.
	struct CasterStats
	{
		Int disabled;
		Int hidden;
		Int shrouded;
		Int outOfRange;
		Int drawn;
	};
	CasterStats& getCasterStats() { return m_casterStats; }

	// Takes the darkness from the legacy shadow colour, which multiplies the ground.
	void setShadowColor(UnsignedInt argb);

	// Binds the map to a texture stage for a receiving pixel shader. The shader reads
	// the sun clip position from that stage's texcoord and its parameters from c0.
	// Fails when the map holds no depth to receive.
	Bool bindReceiver(Int stage) const;
	void unbindReceiver(Int stage) const;

	// The pass objects push to receive the shadow, or null when nothing can receive it.
	MaterialPassClass* getReceivePass();

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
	Real           m_shadowStrength;
	Bool           m_hasDepth;
	CasterStats    m_casterStats;

	W3DShadowReceiveMaterialPassClass m_receivePass;
};

extern W3DShadowMap* TheW3DShadowMap;
