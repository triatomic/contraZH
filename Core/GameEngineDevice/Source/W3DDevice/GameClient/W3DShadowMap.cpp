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

#include <stdlib.h>

#include "WW3D2/camera.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8caps.h"
#include "WW3D2/dx8renderer.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/sortingrenderer.h"
#include "WW3D2/texture.h"
#include "WW3D2/formconv.h"
#include "WW3D2/ww3d.h"
#include "WWMath/sphere.h"
#include "WWMath/wwmath.h"
#include "Common/Debug.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/BaseHeightMap.h"

#if RTS_ZEROHOUR
#include "Lib/BaseType.h"
#include "Common/GlobalData.h"
#include "GameClient/View.h"
#include "W3DDevice/GameClient/W3DShadow.h"
#include "W3DDevice/GameClient/W3DVolumetricShadow.h"
#include "W3DDevice/GameClient/W3DProjectedShadow.h"
#endif

W3DShadowMap* TheW3DShadowMap = nullptr;

// 2048 gives ample texel density at RTS zoom for half the bandwidth of 4096.
static const Int SHADOW_MAP_RESOLUTION = 2048;

// Depth range of the sun frustum. Shallow, because an RTS view covers a bounded
// slab of ground rather than a deep scene.
static const Real SHADOW_NEAR = 1.0f;
static const Real SHADOW_FAR  = 10000.0f;

// Biases in shadow map texels rather than depth units, so they keep their meaning as the
// fit changes with zoom. Casters carry most of it, scaled by their slope in the depth
// pass, because steep surfaces are what shadow themselves. Receivers keep a hair, since
// any more detaches every shadow from its caster's base.
static const Real SHADOW_CASTER_BIAS_TEXELS   = 1.0f;
static const Real SHADOW_CASTER_SLOPE_BIAS    = 2.0f;
static const Real SHADOW_RECEIVER_BIAS_TEXELS = 0.5f;

// Spacing of the receivers' 3x3 filter taps. The edge softens across roughly twice this
// plus one texel, so larger values blur and smaller ones step.
static const Real SHADOW_FILTER_SPACING_TEXELS = 0.33f;

// Quantising the fitted radius stops small zoom and scroll changes resizing the
// frustum, which would make every shadow edge crawl.
static const Real SHADOW_RADIUS_STEP = 64.0f;
static const Real SHADOW_RADIUS_MIN  = 600.0f;
static const Real SHADOW_RADIUS_MAX  = 2600.0f;

// The cull camera is perspective, because that is all CameraClass culls with. Placed
// this far back along the sun ray its frustum is close enough to the ortho box.
static const Real SHADOW_CULL_DISTANCE = 100000.0f;

// Bisects shadow map faults without a rebuild. CONTRA_SHADOWMAP=0 turns the map off and
// 1 fills it without letting anything receive it.
enum { SHADOW_DEBUG_OFF = 0, SHADOW_DEBUG_DEPTH_ONLY = 1, SHADOW_DEBUG_FULL = 2 };

static Int Get_Shadow_Debug_Mode()
{
	const char *value = getenv("CONTRA_SHADOWMAP");
	return (value != nullptr) ? atoi(value) : SHADOW_DEBUG_FULL;
}

static const Int ShadowDebugMode = Get_Shadow_Debug_Mode();

W3DShadowMap::W3DShadowMap()
	: m_depthMode(DEPTH_MODE_NONE),
	  m_resolution(SHADOW_MAP_RESOLUTION),
	  m_colorTarget(nullptr),
	  m_depthTarget(nullptr),
	  m_cullCamera(nullptr),
	  m_fittedCenter(0.0f, 0.0f, 0.0f),
	  m_lightDirection(0.0f, 0.0f, -1.0f),
	  m_depthBias(0.0f),
	  m_casterDepthBias(0.0f),
	  m_fittedRadius(0.0f),
	  m_shadowStrength(0.0f),
	  m_hasDepth(FALSE)
{
	memset(&m_casterStats, 0, sizeof(m_casterStats));
	m_sunView.Make_Identity();
	m_sunProjection.Make_Identity();
	m_sunViewProj.Make_Identity();
	m_cullCamera = NEW_REF(CameraClass, ());
}

W3DShadowMap::~W3DShadowMap()
{
	ReleaseResources();
	REF_PTR_RELEASE(m_cullCamera);
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
	m_hasDepth = FALSE;
}

Bool W3DShadowMap::ReAcquireResources()
{
	ReleaseResources();

	DEBUG_LOG(("W3DShadowMap: debug mode %d", ShadowDebugMode));
	if (ShadowDebugMode == SHADOW_DEBUG_OFF)
	{
		return FALSE;
	}

	const DX8Caps* caps = DX8Wrapper::Get_Current_Caps();
	if (caps == nullptr)
	{
		return FALSE;
	}

	// Checked against the device caps because the engine's chipset table files every
	// NVIDIA card since 2002 under GeForce4.
	if (caps->Get_Vertex_Shader_Major_Version() < 2 || caps->Get_Pixel_Shader_Major_Version() < 2)
	{
		DEBUG_LOG(("W3DShadowMap: shader model 2 unavailable, falling back to legacy shadows"));
		return FALSE;
	}

	// A colour target is needed either way. The wrapper cannot bind a depth surface on
	// its own, so the hardware path masks colour writes rather than skipping the target.
	m_colorTarget = DX8Wrapper::Create_Render_Target(m_resolution, m_resolution,
		WW3D_FORMAT_A8R8G8B8);

	if (m_colorTarget == nullptr)
	{
		DEBUG_LOG(("W3DShadowMap: no usable render target, falling back to legacy shadows"));
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
		// Fall back to encoding depth into the colour target.
		m_depthMode = DEPTH_MODE_PACKED;
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

	// Fit to the terrain the camera can see. The frustum's own corners include the far
	// plane, thousands of units away and below the ground, which drags the fit off the map.
	AABoxClass visibleBox;
	if (TheTerrainRenderObject == nullptr ||
		!TheTerrainRenderObject->getMaximumVisibleBox(camera.Get_Frustum(), &visibleBox, TRUE))
	{
		return;
	}

	// Bounded with a circle rather than the box itself, so orbiting the camera does
	// not reshape the frustum and set the shadows crawling.
	const Vector3 center = visibleBox.Center;
	Real radius = visibleBox.Extent.Length();

	// Quantise so a small zoom leaves the extent alone.
	radius = WWMath::Ceil(radius / SHADOW_RADIUS_STEP) * SHADOW_RADIUS_STEP;
	if (radius < SHADOW_RADIUS_MIN) radius = SHADOW_RADIUS_MIN;
	if (radius > SHADOW_RADIUS_MAX) radius = SHADOW_RADIUS_MAX;
	m_fittedRadius = radius;
	m_fittedCenter = center;
	m_lightDirection = lightDirection;

	computeSunViewProjection(center, radius, lightDirection);
	updateCullCamera(center, radius, lightDirection);

	const Real worldPerTexel = (2.0f * radius) / (Real)m_resolution;
	const Real depthRange = SHADOW_FAR - SHADOW_NEAR;
	m_depthBias = (SHADOW_RECEIVER_BIAS_TEXELS * worldPerTexel) / depthRange;
	m_casterDepthBias = (SHADOW_CASTER_BIAS_TEXELS * worldPerTexel) / depthRange;
}

Real W3DShadowMap::getCasterSlopeBias()
{
	return SHADOW_CASTER_SLOPE_BIAS;
}

Bool W3DShadowMap::isCasterInRange(const SphereClass& bounds) const
{
	// The map is a box along the light, so a caster reaches it when its bounds come
	// within the fitted radius of the light ray through the fitted centre.
	Vector3 offset = bounds.Center - m_fittedCenter;
	offset -= m_lightDirection * Vector3::Dot_Product(offset, m_lightDirection);

	const Real reach = m_fittedRadius + bounds.Radius;
	return offset.Length2() <= reach * reach;
}

void W3DShadowMap::setShadowColor(UnsignedInt argb)
{
	const Real multiplier = (Real)((argb >> 8) & 0xff) / 255.0f;
	m_shadowStrength = 1.0f - multiplier;
}

Bool W3DShadowMap::bindReceiver(Int stage) const
{
	if (!m_hasDepth || ShadowDebugMode == SHADOW_DEBUG_DEPTH_ONLY)
	{
		return FALSE;
	}

	TextureBaseClass *texture = (m_depthMode == DEPTH_MODE_HARDWARE)
		? (TextureBaseClass *)m_depthTarget : (TextureBaseClass *)m_colorTarget;

	// Bound through the wrapper so its texture cache stays true for whatever draws next,
	// since a receiver pass can run between two ordinary passes with no invalidate. The
	// first call forces the change through when the map is already the cached texture,
	// and it is applied now because applying a texture also applies its own sampler state.
	DX8Wrapper::Set_Texture(stage, nullptr);
	DX8Wrapper::Set_Texture(stage, texture);
	DX8Wrapper::Apply_Render_State_Changes();

	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MIPFILTER, D3DTEXF_NONE);

	// Linear filtering on a depth texture is what gives the hardware path its PCF. The
	// packed path filters by hand, since blending packed channels corrupts the depth.
	const DWORD filter = (m_depthMode == DEPTH_MODE_HARDWARE) ? D3DTEXF_LINEAR : D3DTEXF_POINT;
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MINFILTER, filter);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MAGFILTER, filter);

	// Camera space back to world, into the sun's clip space, then onto the map. The
	// half texel lines D3D9's texel centres up with the pixels the depth pass wrote.
	// The view is read from the device, because every invalidate zeroes the wrapper's copy
	// and receivers drawn after the terrain would otherwise invert a zero matrix.
	D3DMATRIX view;
	DX8Wrapper::_Get_D3D_Device8()->GetTransform(D3DTS_VIEW, &view);

	D3DMATRIX inverseView;
	float det;
	Invert_D3DMATRIX(inverseView, &det, view);

	const float halfTexel = 0.5f / (float)m_resolution;

	D3DMATRIX toMap;
	Set_D3DMATRIX_Identity(toMap);
	toMap.m[0][0] = 0.5f;
	toMap.m[1][1] = -0.5f;
	toMap.m[3][0] = 0.5f + halfTexel;
	toMap.m[3][1] = 0.5f + halfTexel;

	D3DMATRIX textureTransform = (inverseView * To_D3DMATRIX(m_sunViewProj)) * toMap;
	DX8Wrapper::_Set_DX8_Transform((D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + stage), textureTransform);

	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT4);

	Vector4 params(1.0f / (Real)m_resolution, m_depthBias, m_shadowStrength, SHADOW_FILTER_SPACING_TEXELS);
	DX8Wrapper::Set_Pixel_Shader_Constant(0, &params, 1);

	return TRUE;
}

void W3DShadowMap::unbindReceiver(Int stage) const
{
	DX8Wrapper::Set_Texture(stage, nullptr);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU | stage);
}

MaterialPassClass* W3DShadowMap::getReceivePass()
{
	if (!m_hasDepth || W3DShaderManager::getShaderPasses(W3DShaderManager::ST_SHADOW_MULTIPLY) == 0)
	{
		return nullptr;
	}
	return &m_receivePass;
}

void W3DShadowReceiveMaterialPassClass::Install_Materials() const
{
	W3DShaderManager::setShader(W3DShaderManager::ST_SHADOW_MULTIPLY, 0);
}

void W3DShadowReceiveMaterialPassClass::UnInstall_Materials() const
{
	W3DShaderManager::resetShader(W3DShaderManager::ST_SHADOW_MULTIPLY);
}

// The caster lists live in the Zero Hour shadow managers, so Generals keeps its legacy
// shadows and this pass does nothing there.
void W3DShadowMap::renderDepthPass(RenderInfoClass& rinfo)
{
#if RTS_ZEROHOUR
	if (!isAvailable() || W3DShaderManager::getShaderPasses(W3DShaderManager::ST_SHADOW_DEPTH) == 0)
	{
		return;
	}

	// One level of render target nesting is all the wrapper allows, so this pass must
	// not run inside another one.
	if (DX8Wrapper::Is_Render_To_Texture())
	{
		return;
	}

	DX8Wrapper::Set_Render_Target_With_Z(m_colorTarget, m_depthTarget);
	DX8Wrapper::Clear(true, true, Vector3(1.0f, 1.0f, 1.0f), 1.0f);

	// Stencil is outside ShaderClass, so it is the one piece of inherited state that
	// could reject casters. Read from the device because the wrapper's cache holds a
	// sentinel after every invalidate, and restoring that would switch stencil on.
	DWORD stencilEnable = FALSE;
	DX8Wrapper::_Get_D3D_Device8()->GetRenderState(D3DRS_STENCILENABLE, &stencilEnable);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, FALSE);

	// Vertex processing stays fixed function, so the device applies each caster's
	// world transform. The mesh renderer sets it per mesh after the pass installs,
	// where a vertex shader could not see it.
	DX8Wrapper::Set_Transform(D3DTS_VIEW, m_sunView);
	DX8Wrapper::Set_Transform(D3DTS_PROJECTION, m_sunProjection);

	// A mesh with a sort level would otherwise go onto the static sort list and be
	// drawn a second time in the main scene.
	const bool staticSortLists = WW3D::Are_Static_Sort_Lists_Enabled();
	WW3D::Enable_Static_Sort_Lists(false);

	// Casters draw with their own textures and shaders so cutouts keep their shape. The
	// depth shader overrides the rest of each shader's state for as long as it is set.
	RenderInfoClass sunInfo(*m_cullCamera);
	W3DShaderManager::setShader(W3DShaderManager::ST_SHADOW_DEPTH, 0);

	memset(&m_casterStats, 0, sizeof(m_casterStats));

	if (TheW3DVolumetricShadowManager != nullptr)
	{
		TheW3DVolumetricShadowManager->renderShadowMapCasters(sunInfo);
	}

	if (TheW3DProjectedShadowManager != nullptr)
	{
		TheW3DProjectedShadowManager->renderShadowMapCasters(sunInfo);
	}

	// Sampled rather than every pass, so a whole match stays readable.
	static Int passCount = 0;
	if (passCount % 300 == 0 && passCount <= 300 * 15)
	{
		DEBUG_LOG(("W3DShadowMap: pass %d drew %d, dropped %d disabled %d hidden %d shrouded %d out of range, centre (%.0f, %.0f, %.0f) radius %.0f",
			passCount, m_casterStats.drawn, m_casterStats.disabled, m_casterStats.hidden,
			m_casterStats.shrouded, m_casterStats.outOfRange,
			m_fittedCenter.X, m_fittedCenter.Y, m_fittedCenter.Z, m_fittedRadius));
	}
	++passCount;

	TheDX8MeshRenderer.Flush();

	// Sorted meshes, often foliage, were deferred rather than drawn. Flushed here they
	// draw into the map with the sun's matrices, and not into the main view later.
	SortingRendererClass::Flush();

	W3DShaderManager::resetShader(W3DShaderManager::ST_SHADOW_DEPTH);

	WW3D::Enable_Static_Sort_Lists(staticSortLists);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, stencilEnable);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE, 0x0000000f);
	DX8Wrapper::Set_Render_Target((IDirect3DSurface8 *)nullptr);
	DX8Wrapper::Invalidate_Cached_Render_States();

	// Restores the viewport, view and projection the scene was rendering with.
	rinfo.Camera.Apply();

	// Depth-only debugging leaves the map unreceived and the legacy shadows in place.
	m_hasDepth = (ShadowDebugMode != SHADOW_DEBUG_DEPTH_ONLY);
#else
	(void)rinfo;
#endif
}

void W3DShadowMap::updateCullCamera(const Vector3& center, Real radius, const Vector3& lightDirection)
{
	// Matches the axes computeSunViewProjection builds, with X flipped because a camera
	// looks down its own -Z and must stay right-handed.
	Vector3 up(0.0f, 0.0f, 1.0f);
	if (WWMath::Fabs(lightDirection.Z) > 0.99f)
	{
		up.Set(0.0f, 1.0f, 0.0f);
	}

	Vector3 xAxis;
	Vector3::Cross_Product(up, lightDirection, &xAxis);
	xAxis.Normalize();

	Vector3 yAxis;
	Vector3::Cross_Product(lightDirection, xAxis, &yAxis);

	const Vector3 position = center - lightDirection * (SHADOW_FAR * 0.5f + SHADOW_CULL_DISTANCE);

	Matrix3D transform(
		-xAxis.X, yAxis.X, -lightDirection.X, position.X,
		-xAxis.Y, yAxis.Y, -lightDirection.Y, position.Y,
		-xAxis.Z, yAxis.Z, -lightDirection.Z, position.Z);

	m_cullCamera->Set_Transform(transform);
	m_cullCamera->Set_Clip_Planes(SHADOW_CULL_DISTANCE + SHADOW_NEAR, SHADOW_CULL_DISTANCE + SHADOW_FAR);

	// Wide enough at the near plane to cover the corners of the square map.
	const Real halfExtent = (radius * 1.5f) / SHADOW_CULL_DISTANCE;
	m_cullCamera->Set_View_Plane(Vector2(-halfExtent, -halfExtent), Vector2(halfExtent, halfExtent));
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

	m_sunView[0].Set(xAxis.X, xAxis.Y, xAxis.Z, -Vector3::Dot_Product(xAxis, eye));
	m_sunView[1].Set(yAxis.X, yAxis.Y, yAxis.Z, -Vector3::Dot_Product(yAxis, eye));
	m_sunView[2].Set(zAxis.X, zAxis.Y, zAxis.Z, -Vector3::Dot_Product(zAxis, eye));
	m_sunView[3].Set(0.0f, 0.0f, 0.0f, 1.0f);

	// Orthographic projection into the [0,1] depth range D3D expects.
	const Real invRange = 1.0f / (SHADOW_FAR - SHADOW_NEAR);

	m_sunProjection.Make_Identity();
	m_sunProjection[0][0] = 1.0f / radius;
	m_sunProjection[1][1] = 1.0f / radius;
	m_sunProjection[2][2] = invRange;
	m_sunProjection[2][3] = -SHADOW_NEAR * invRange;

	// Snap the world origin to a whole texel so the texel grid stays fixed to the world.
	// Without this, sub-texel drift while scrolling makes every shadow edge shimmer. The
	// fitted centre cannot be the snap point, because it always projects to zero.
	Matrix4x4 unsnapped = m_sunProjection * m_sunView;
	Vector4 projected = unsnapped * Vector4(0.0f, 0.0f, 0.0f, 1.0f);

	const Real halfMap = 0.5f * (Real)m_resolution;

	Real x = projected.X * halfMap;
	Real y = projected.Y * halfMap;

	// Exact in the projection because it is orthographic and the view's last row is
	// (0,0,0,1), so the offset lands unchanged on every projected point.
	m_sunProjection[0][3] += (WWMath::Floor(x + 0.5f) - x) / halfMap;
	m_sunProjection[1][3] += (WWMath::Floor(y + 0.5f) - y) / halfMap;

	m_sunViewProj = m_sunProjection * m_sunView;
}
