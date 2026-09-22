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

#include "W3DDevice/GameClient/W3DShadowMap.h"

#include "WW3D2/camera.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8caps.h"
#include "WW3D2/texture.h"
#include "WW3D2/formconv.h"
#include "WWMath/wwmath.h"
#include "Common/Debug.h"

W3DShadowMap* TheW3DShadowMap = nullptr;

// 2048 gives ample texel density at RTS zoom for half the bandwidth of 4096.
static const Int SHADOW_MAP_RESOLUTION = 2048;

// Depth range of the sun frustum. Shallow, because an RTS view covers a bounded
// slab of ground rather than a deep scene.
static const Real SHADOW_NEAR = 1.0f;
static const Real SHADOW_FAR  = 10000.0f;

// Bias expressed in shadow map texels rather than depth units, so it keeps its
// meaning as the fit changes with zoom.
static const Real SHADOW_BIAS_TEXELS = 3.0f;

// Quantising the fitted radius stops small zoom and scroll changes resizing the
// frustum, which would make every shadow edge crawl.
static const Real SHADOW_RADIUS_STEP = 64.0f;
static const Real SHADOW_RADIUS_MIN  = 600.0f;
static const Real SHADOW_RADIUS_MAX  = 2600.0f;

W3DShadowMap::W3DShadowMap()
	: m_depthMode(DEPTH_MODE_NONE),
	  m_resolution(SHADOW_MAP_RESOLUTION),
	  m_colorTarget(nullptr),
	  m_depthTarget(nullptr),
	  m_depthBias(0.0f),
	  m_fittedRadius(0.0f)
{
	m_sunViewProj.Make_Identity();
}

W3DShadowMap::~W3DShadowMap()
{
	ReleaseResources();
}

Bool W3DShadowMap::init()
{
	return ReAcquireResources();
}

void W3DShadowMap::ReleaseResources()
{
	REF_PTR_RELEASE(m_colorTarget);
	REF_PTR_RELEASE(m_depthTarget);
	m_depthMode = DEPTH_MODE_NONE;
}

Bool W3DShadowMap::ReAcquireResources()
{
	ReleaseResources();

	const DX8Caps* caps = DX8Wrapper::Get_Current_Caps();
	if (caps == nullptr)
	{
		return FALSE;
	}

	// Prefer sampling depth directly: the sampler does the compare and its bilinear
	// filter gives 2x2 PCF for one instruction.
	if (caps->Support_Depth_Stencil_Format(WW3D_ZFORMAT_D24S8))
	{
		// D3DUSAGE_DEPTHSTENCIL requires the default pool.
		m_depthTarget = NEW_REF(ZTextureClass,
			(m_resolution, m_resolution, WW3D_ZFORMAT_D24S8, MIP_LEVELS_1,
			 TextureBaseClass::POOL_DEFAULT));

		if (m_depthTarget != nullptr)
		{
			m_depthMode = DEPTH_MODE_HARDWARE;
		}
	}

	if (m_depthMode == DEPTH_MODE_NONE)
	{
		// Fall back to encoding depth into a colour target.
		m_colorTarget = DX8Wrapper::Create_Render_Target(m_resolution, m_resolution,
			WW3D_FORMAT_A8R8G8B8);

		if (m_colorTarget != nullptr)
		{
			m_depthMode = DEPTH_MODE_PACKED;
		}
	}

	if (m_depthMode == DEPTH_MODE_NONE)
	{
		DEBUG_LOG(("W3DShadowMap: no usable shadow map format, falling back to legacy shadows"));
		return FALSE;
	}

	DEBUG_LOG(("W3DShadowMap: %dx%d, %s depth",
		m_resolution, m_resolution,
		m_depthMode == DEPTH_MODE_HARDWARE ? "hardware" : "packed"));

	return TRUE;
}

void W3DShadowMap::updateFrustum(const CameraClass& camera, const Vector3& lightPosWorld)
{
	// The light is stored as a point pushed far along the sun ray, so the direction
	// is simply toward the origin from there.
	Vector3 lightDirection = -lightPosWorld;
	lightDirection.Normalize();

	// Bound the visible ground with a circle rather than a box. A circle is
	// invariant under camera rotation, so orbiting does not resize the frustum and
	// set the shadows crawling.
	const Vector3* corners = camera.Get_Frustum_Corners();

	Vector3 center(0.0f, 0.0f, 0.0f);
	for (Int i = 0; i < 8; ++i)
	{
		center += corners[i];
	}
	center /= 8.0f;

	Real radius = 0.0f;
	for (Int i = 0; i < 8; ++i)
	{
		Real distance = (corners[i] - center).Length();
		if (distance > radius)
		{
			radius = distance;
		}
	}

	// Quantise so a small zoom leaves the extent alone.
	radius = WWMath::Ceil(radius / SHADOW_RADIUS_STEP) * SHADOW_RADIUS_STEP;
	if (radius < SHADOW_RADIUS_MIN) radius = SHADOW_RADIUS_MIN;
	if (radius > SHADOW_RADIUS_MAX) radius = SHADOW_RADIUS_MAX;
	m_fittedRadius = radius;

	computeSunViewProjection(center, radius, lightDirection);

	// A texel is square in the sun's view, but the ground it lands on is stretched
	// along the light by 1/sin(elevation). Without that term a low sun self-shadows
	// whole hillsides while a high one lifts small casters out of their own shadow.
	Real sunElevation = WWMath::Fabs(lightDirection.Z);
	if (sunElevation < 0.15f)
	{
		sunElevation = 0.15f;
	}

	const Real worldPerTexel = (2.0f * radius) / (Real)m_resolution;
	m_depthBias = (SHADOW_BIAS_TEXELS * worldPerTexel) / (sunElevation * (SHADOW_FAR - SHADOW_NEAR));
}

void W3DShadowMap::computeSunViewProjection(const Vector3& center, Real radius,
	const Vector3& lightDirection)
{
	// Look at the fitted centre from far enough back that the whole depth range is
	// in front of the near plane.
	Vector3 eye = center - lightDirection * (SHADOW_FAR * 0.5f);

	Vector3 up(0.0f, 0.0f, 1.0f);
	if (WWMath::Fabs(lightDirection.Z) > 0.99f)
	{
		// Looking nearly straight down, so pick an up vector that is not parallel.
		up.Set(0.0f, 1.0f, 0.0f);
	}

	Vector3 zAxis = lightDirection;
	zAxis.Normalize();

	Vector3 xAxis;
	Vector3::Cross_Product(up, zAxis, &xAxis);
	xAxis.Normalize();

	Vector3 yAxis;
	Vector3::Cross_Product(zAxis, xAxis, &yAxis);

	Matrix4x4 view;
	view[0].Set(xAxis.X, xAxis.Y, xAxis.Z, -Vector3::Dot_Product(xAxis, eye));
	view[1].Set(yAxis.X, yAxis.Y, yAxis.Z, -Vector3::Dot_Product(yAxis, eye));
	view[2].Set(zAxis.X, zAxis.Y, zAxis.Z, -Vector3::Dot_Product(zAxis, eye));
	view[3].Set(0.0f, 0.0f, 0.0f, 1.0f);

	// Orthographic projection into the [0,1] depth range D3D expects.
	const Real invRange = 1.0f / (SHADOW_FAR - SHADOW_NEAR);

	Matrix4x4 projection;
	projection.Make_Identity();
	projection[0][0] = 1.0f / radius;
	projection[1][1] = 1.0f / radius;
	projection[2][2] = invRange;
	projection[2][3] = -SHADOW_NEAR * invRange;

	m_sunViewProj = projection * view;

	// Snap the fitted centre to a whole texel. Without this, sub-texel drift while
	// scrolling makes every shadow edge shimmer, which in an RTS is constant.
	Vector4 origin(center.X, center.Y, center.Z, 1.0f);
	Vector4 projected = m_sunViewProj * origin;

	if (WWMath::Fabs(projected.W) > WWMATH_EPSILON)
	{
		const Real halfMap = 0.5f * (Real)m_resolution;

		Real x = (projected.X / projected.W) * halfMap;
		Real y = (projected.Y / projected.W) * halfMap;

		Real offsetX = (WWMath::Floor(x + 0.5f) - x) / halfMap;
		Real offsetY = (WWMath::Floor(y + 0.5f) - y) / halfMap;

		// Exact as a post-projection translation because the projection is
		// orthographic, and it applies identically to the depth pass and the
		// lookups since both use this matrix.
		m_sunViewProj[0][3] += offsetX;
		m_sunViewProj[1][3] += offsetY;
	}
}
