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

// W3DSkyClouds.cpp ///////////////////////////////////////////////////////////////////////////////
// The HQ sky's cloud shadows, drawn each frame into a map over the ground around the camera
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Lib/BaseType.h"
#include "WWLib/always.h"
#include "W3DDevice/GameClient/W3DSkyClouds.h"
#include "W3DDevice/GameClient/W3DNoiseTexture.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/BaseHeightMap.h"
#include "Common/Debug.h"
#include "Common/GlobalData.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8caps.h"
#include "WW3D2/formconv.h"
#include "WW3D2/shader.h"
#include "WW3D2/texture.h"
#include "WW3D2/vertmaterial.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WW3D2/ww3d.h"

#include <math.h>
#include <vector>

W3DSkyClouds *TheW3DSkyClouds = nullptr;

// CONTRA_SKYCLOUDS bisects faults: 0 keeps the legacy cloud texture, 1 draws the HQ sky's.
static Int Get_Sky_Clouds_Mode()
{
	const char *value = getenv("CONTRA_SKYCLOUDS");
	return (value != nullptr) ? atoi(value) : 1;
}

static const Int SkyCloudsMode = Get_Sky_Clouds_Mode();

enum { MAP_SIZE = 1024, NOISE_SIZE = 512, NOISE_SEED = 11 };

// The map spans a power of two of world units, so zooming rarely changes its texel size.
static const Real MIN_SPAN = 1024.0f;
static const Real MAX_SPAN = 16384.0f;

// How far past the ground on screen the map reaches, and how far from the camera it fits ground at all.
static const Real SPAN_MARGIN = 2.2f;
static const Real MAX_REACH = MAX_SPAN / SPAN_MARGIN;

// How far the shape channels spread around 0.5, one standard deviation, which skyclouds.hlsl's weights undo.
static const Real SHAPE_SPREAD = 0.18f;

// The two shapes' shares of the clouds, and the detail's share at SkyCloudDetail 1.
static const Real SHAPE_A_WEIGHT = 0.65f;
static const Real SHAPE_B_WEIGHT = 0.35f;
static const Real DETAIL_WEIGHT = 0.35f;

// World units the warp bends per SkyCloudBillow, as a share of SkyCloudSize at one standard deviation.
static const Real BILLOW_REACH = 0.25f;

// Lattice cells across the noise, octaves, each octave's gain, and the spread around 0.5.
struct CloudNoiseChannel
{
	Int cells;
	Int octaves;
	Real gain;
	Real spread;
};

// Red and green shape the clouds, and blue and alpha bend them.
static const CloudNoiseChannel Channels[4] =
{
	{ 8, 4, 0.5f, SHAPE_SPREAD },
	{ 8, 3, 0.5f, SHAPE_SPREAD },
	{ 4, 2, 0.5f, 0.2f },
	{ 4, 2, 0.5f, 0.2f }
};

// Each read of the noise: the cells of the channel it reads, its cell as a share of SkyCloudSize, its turn in
// degrees, and at SkyCloudChurn 1 how much faster it drifts than the wind and how many degrees off it.
struct CloudRead
{
	Int cells;
	Real size;
	Real turn;
	Real speedChurn;
	Real turnChurn;
};

// Warp, shape A, shape B and detail, in skyclouds.hlsl's register order. The sizes' irrational ratios keep the sum from repeating.
static const CloudRead Reads[4] =
{
	{ 4, 2.0f, -38.0f, -0.6f, 0.0f },
	{ 8, 1.0f, 0.0f, 0.0f, 0.0f },
	{ 8, 0.618f, 27.0f, 0.5f, 35.0f },
	{ 8, 0.309f, 61.0f, 1.0f, -20.0f }
};

static TextureClass *Build_Cloud_Noise()
{
	std::vector<UnsignedInt> pixels(NOISE_SIZE * NOISE_SIZE, 0);
	std::vector<Real> field(NOISE_SIZE * NOISE_SIZE);

	static const Int shifts[4] = { 16, 8, 0, 24 };
	for (Int channel = 0; channel < 4; channel++)
	{
		const CloudNoiseChannel &layer = Channels[channel];
		W3DNoiseTexture::buildLayer(&field[0], NOISE_SIZE, layer.cells, layer.octaves, layer.gain, 0.0f, NOISE_SEED + channel);
		for (Int i = 0; i < NOISE_SIZE * NOISE_SIZE; i++)
		{
			pixels[i] |= W3DNoiseTexture::toByte(0.5f + layer.spread * field[i]) << shifts[channel];
		}
	}
	return W3DNoiseTexture::createTexture(&pixels[0], NOISE_SIZE);
}

// A read's noise texcoords per world unit: xy along world x, zw along world y.
static void Read_Axes(Int read, Real cloudSize, Real *axes)
{
	const Real scale = 1.0f / (Reads[read].cells * Reads[read].size * cloudSize);
	const Real turn = Reads[read].turn * (PI / 180.0f);
	const Real c = (Real)cos(turn) * scale;
	const Real s = (Real)sin(turn) * scale;
	axes[0] = c;
	axes[1] = s;
	axes[2] = -s;
	axes[3] = c;
}

// Standard deviations above the clouds' mean depth that leave this share of the ground in shadow.
static Real Coverage_Threshold(Real coverage)
{
	if (coverage <= 0.001f)
	{
		return 10.0f;
	}
	if (coverage >= 0.999f)
	{
		return -10.0f;
	}

	// Abramowitz and Stegun 26.2.23, within 4.5e-4 of the normal distribution's tail.
	const Real tail = (coverage < 0.5f) ? coverage : 1.0f - coverage;
	const Real t = (Real)sqrt(-2.0f * log(tail));
	const Real z = t - (2.515517f + t * (0.802853f + t * 0.010328f)) / (1.0f + t * (1.432788f + t * (0.189269f + t * 0.001308f)));
	return (coverage < 0.5f) ? z : -z;
}

// The ground the camera's edge rays reach, as a square around its centre. Near the horizon a ray lands
// far away or never, and where it lands swings with every small turn, so each ray is cut at MAX_REACH
// from the camera. The ground past that wraps the map, and a steady map keeps its clouds steady.
static void Fit_Ground(const CameraClass &camera, Real groundHeight, Real &centerX, Real &centerY, Real &extent)
{
	const Vector3 *corners = camera.Get_Frustum_Corners();
	const Vector3 eye = camera.Get_Position();
	Real minX = eye.X;
	Real maxX = eye.X;
	Real minY = eye.Y;
	Real maxY = eye.Y;

	for (Int i = 0; i < 4; i++)
	{
		const Vector3 &nearPoint = corners[i];
		const Vector3 &farPoint = corners[i + 4];
		Vector3 point = farPoint;
		if (nearPoint.Z > groundHeight && farPoint.Z < groundHeight)
		{
			point = nearPoint + (farPoint - nearPoint) * ((nearPoint.Z - groundHeight) / (nearPoint.Z - farPoint.Z));
		}

		Real dx = point.X - eye.X;
		Real dy = point.Y - eye.Y;
		const Real distance = (Real)sqrt(dx * dx + dy * dy);
		if (distance > MAX_REACH)
		{
			dx *= MAX_REACH / distance;
			dy *= MAX_REACH / distance;
		}
		minX = min(minX, eye.X + dx);
		maxX = max(maxX, eye.X + dx);
		minY = min(minY, eye.Y + dy);
		maxY = max(maxY, eye.Y + dy);
	}

	centerX = 0.5f * (minX + maxX);
	centerY = 0.5f * (minY + maxY);
	extent = 0.5f * max(maxX - minX, maxY - minY);
}

W3DSkyClouds::W3DSkyClouds()
	: m_target(nullptr),
	  m_noise(nullptr),
	  m_shader(0),
	  m_failed(FALSE),
	  m_mapSize(MAP_SIZE),
	  m_updateFrame(0xffffffffu),
	  m_drawnFrame(0),
	  m_drawn(FALSE),
	  m_originX(0.0f),
	  m_originY(0.0f),
	  m_span(MIN_SPAN)
{
	for (Int i = 0; i < READ_COUNT; i++)
	{
		m_travel[i][0] = 0.0;
		m_travel[i][1] = 0.0;
	}
}

W3DSkyClouds::~W3DSkyClouds()
{
	ReleaseResources();
}

void W3DSkyClouds::ReleaseResources()
{
	REF_PTR_RELEASE(m_target);
	REF_PTR_RELEASE(m_noise);

	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device != nullptr && m_shader != 0)
	{
		DX8_DELETE_PIXEL_SHADER(device, m_shader);
	}
	m_shader = 0;
	m_failed = FALSE;
	m_drawn = FALSE;
}

Bool W3DSkyClouds::isActive() const
{
	// Passes drawn before this frame's update, such as reflections, take the last frame's map.
	return m_drawn && SkyCloudsMode != 0 && TheGlobalData->m_useHQSky && WW3D::Get_Frame_Count() - m_drawnFrame <= 2;
}

void W3DSkyClouds::getTextureMatrix(D3DMATRIX &dest, const D3DMATRIX &inverseView) const
{
	D3DMATRIX toMap;
	Set_D3DMATRIX_Identity(toMap);
	toMap.m[0][0] = 1.0f / m_span;
	toMap.m[1][1] = 1.0f / m_span;
	toMap.m[3][0] = -m_originX / m_span;
	toMap.m[3][1] = -m_originY / m_span;
	dest = inverseView * toMap;
}

// Made on first use, once the device can say whether it runs the shader.
Bool W3DSkyClouds::acquire()
{
#if defined(BUILD_WITH_D3D9)
	if (m_failed)
	{
		return FALSE;
	}

	if (m_shader == 0)
	{
		const DX8Caps *caps = DX8Wrapper::Get_Current_Caps();
		if (caps == nullptr || caps->Get_Pixel_Shader_Major_Version() < 2 ||
			FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\skyclouds.pso", nullptr, 0, false, &m_shader)))
		{
			m_shader = 0;
			m_failed = TRUE;
			RENDER_LOG(("W3DSkyClouds: no skyclouds.pso or shader model 2, keeping the legacy clouds"));
			return FALSE;
		}
	}

	if (m_noise == nullptr)
	{
		m_noise = Build_Cloud_Noise();
		if (m_noise == nullptr)
		{
			m_failed = TRUE;
			RENDER_LOG(("W3DSkyClouds: could not make the cloud noise, keeping the legacy clouds"));
			return FALSE;
		}
	}

	if (m_target == nullptr)
	{
		m_target = DX8Wrapper::Create_Render_Target(MAP_SIZE, MAP_SIZE, WW3D_FORMAT_A8R8G8B8);
		if (m_target == nullptr)
		{
			m_failed = TRUE;
			RENDER_LOG(("W3DSkyClouds: no render target, keeping the legacy clouds"));
			return FALSE;
		}

		// The wrapper may shrink the target to the card's limit.
		D3DSURFACE_DESC desc;
		m_target->Peek_D3D_Texture()->GetLevelDesc(0, &desc);
		m_mapSize = (Int)desc.Width;

		m_target->Get_Filter().Set_Min_Filter(TextureFilterClass::FILTER_TYPE_FAST);
		m_target->Get_Filter().Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_FAST);
		m_target->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
		m_target->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_REPEAT);
		m_target->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_REPEAT);
		m_drawn = FALSE;
	}
	return TRUE;
#else
	return FALSE;
#endif
}

// Moves each read along its own drift, kept in its noise texcoords so a long match loses no precision.
void W3DSkyClouds::advance(Real seconds)
{
	const Real cloudSize = max(TheGlobalData->m_skyCloudSize, 1.0f);
	const Real churn = max(TheGlobalData->m_skyCloudChurn, 0.0f);
	for (Int read = 0; read < READ_COUNT; read++)
	{
		const Real speed = TheGlobalData->m_skyCloudWindSpeed * (1.0f + churn * Reads[read].speedChurn);
		const Real heading = (TheGlobalData->m_skyCloudWindAngle + churn * Reads[read].turnChurn) * (PI / 180.0f);
		const Real moveX = speed * (Real)cos(heading) * seconds;
		const Real moveY = speed * (Real)sin(heading) * seconds;

		Real axes[4];
		Read_Axes(read, cloudSize, axes);
		m_travel[read][0] += axes[0] * moveX + axes[2] * moveY;
		m_travel[read][1] += axes[1] * moveX + axes[3] * moveY;
		m_travel[read][0] -= floor(m_travel[read][0]);
		m_travel[read][1] -= floor(m_travel[read][1]);
	}
}

void W3DSkyClouds::update(RenderInfoClass &rinfo, BaseHeightMapRenderObjClass &terrain)
{
#if defined(BUILD_WITH_D3D9)
	const UnsignedInt frame = WW3D::Get_Frame_Count();
	if (frame == m_updateFrame)
	{
		return;
	}
	m_updateFrame = frame;

	// One level of render target nesting is all the wrapper allows.
	if (SkyCloudsMode == 0 || !TheGlobalData->m_useHQSky || DX8Wrapper::Is_Render_To_Texture() || !acquire())
	{
		return;
	}

	advance(WW3D::Get_Logic_Frame_Time_Seconds());

	Real centerX;
	Real centerY;
	Real extent;
	Fit_Ground(rinfo.Camera, terrain.getMinHeight(), centerX, centerY, extent);

	// Whole texels keep panning from moving the clouds across the map's grid, which would make them swim.
	const Real reach = SPAN_MARGIN * extent;
	Real span = MIN_SPAN;
	while (span < reach && span < MAX_SPAN)
	{
		span *= 2.0f;
	}
	const Real texel = span / m_mapSize;
	const Real originX = (Real)floor((centerX - 0.5f * span) / texel) * texel;
	const Real originY = (Real)floor((centerY - 0.5f * span) / texel) * texel;

	const Real cloudSize = max(TheGlobalData->m_skyCloudSize, 1.0f);
	const Real softness = max(TheGlobalData->m_skyCloudSoftness, 0.02f);
	const Real billow = max(TheGlobalData->m_skyCloudBillow, 0.0f) * BILLOW_REACH * cloudSize / Channels[2].spread;

	Vector4 constants[9];
	constants[0].Set(span, billow, 0.5f / softness, 0.0f);

	// The map's corner goes into each read's offset, so the shader works in world units from the corner.
	Real drift[READ_COUNT][2];
	for (Int read = 0; read < READ_COUNT; read++)
	{
		Real axes[4];
		Read_Axes(read, cloudSize, axes);
		constants[1 + read].Set(axes[0], axes[1], axes[2], axes[3]);
		const double u = (double)axes[0] * originX + (double)axes[2] * originY - m_travel[read][0];
		const double v = (double)axes[1] * originX + (double)axes[3] * originY - m_travel[read][1];
		drift[read][0] = (Real)(u - floor(u));
		drift[read][1] = (Real)(v - floor(v));
	}
	constants[5].Set(drift[READ_WARP][0], drift[READ_WARP][1], drift[READ_SHAPE_A][0], drift[READ_SHAPE_A][1]);
	constants[6].Set(drift[READ_SHAPE_B][0], drift[READ_SHAPE_B][1], drift[READ_DETAIL][0], drift[READ_DETAIL][1]);

	// The weights turn the reads into standard deviations of the clouds' depth, less the coverage threshold.
	const Real detail = max(TheGlobalData->m_skyCloudDetail, 0.0f) * DETAIL_WEIGHT;
	const Real toDeviations = 1.0f / (SHAPE_SPREAD * (Real)sqrt(SHAPE_A_WEIGHT * SHAPE_A_WEIGHT + SHAPE_B_WEIGHT * SHAPE_B_WEIGHT + detail * detail));
	constants[7].Set(SHAPE_A_WEIGHT * toDeviations, SHAPE_B_WEIGHT * toDeviations, detail * toDeviations,
		-0.5f * (SHAPE_A_WEIGHT + SHAPE_B_WEIGHT + detail) * toDeviations - Coverage_Threshold(TheGlobalData->m_skyCloudCoverage));

	const Real light = 1.0f - clamp(0.0f, TheGlobalData->m_skyCloudShadowStrength, 1.0f);
	const RGBColor &tint = TheGlobalData->m_skyCloudShadowTint;
	constants[8].Set(tint.red * light, tint.green * light, tint.blue * light, 1.0f);

	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DSurface8 *sceneTarget = nullptr;
	IDirect3DSurface8 *sceneDepth = nullptr;
	if (FAILED(device->GetRenderTarget(0, &sceneTarget)) || sceneTarget == nullptr)
	{
		return;
	}
	device->GetDepthStencilSurface(&sceneDepth);
	IDirect3DSurface8 *mapSurface = m_target->Get_D3D_Surface_Level();

	DX8Wrapper::Set_Texture(0, nullptr);
	DX8Wrapper::Set_Texture(1, nullptr);
	VertexMaterialClass *material = VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(material);
	REF_PTR_RELEASE(material);
	Matrix4x4 identity(true);
	DX8Wrapper::Set_World_Identity();
	DX8Wrapper::Set_View_Identity();
	DX8Wrapper::Set_Transform(D3DTS_PROJECTION, identity);

	ShaderClass replace = ShaderClass::_PresetOpaque2DShader;
	replace.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);
	DX8Wrapper::Set_Shader(replace);
	DX8Wrapper::Apply_Render_State_Changes();

	// The device is read because the wrapper's cache can hold a placeholder after an invalidate.
	// Alpha is written too, since the legacy terrain shaders multiply the ground's alpha by the map's.
	DWORD depthTest = D3DZB_TRUE;
	DWORD stencil = FALSE;
	DWORD colorWrite = 0x0000000f;
	device->GetRenderState(D3DRS_ZENABLE, &depthTest);
	device->GetRenderState(D3DRS_STENCILENABLE, &stencil);
	device->GetRenderState(D3DRS_COLORWRITEENABLE, &colorWrite);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE, 0x0000000f);

	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_TEXCOORDINDEX, 0);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	device->SetTexture(0, m_noise->Peek_D3D_Texture());

	// No depth is bound, and the new target starts with a full viewport.
	if (mapSurface != nullptr && SUCCEEDED(DX8Wrapper::Set_DX8_Render_Target_Surfaces(mapSurface, nullptr)))
	{
		const Real size = (Real)m_mapSize;
		DX8Wrapper::Set_Pixel_Shader(m_shader);
		DX8Wrapper::Set_Pixel_Shader_Constant(0, constants, 9);
		W3DShaderManager::drawClipQuad(W3DShaderManager::getClipToTargetMapping(size, size));

		m_originX = originX;
		m_originY = originY;
		m_span = span;
		m_drawn = TRUE;
		m_drawnFrame = frame;
	}

	DX8Wrapper::Set_Pixel_Shader(0);
	device->SetTexture(0, nullptr);
	DX8Wrapper::Set_Vertex_Buffer(nullptr);
	DX8Wrapper::Set_Index_Buffer(nullptr, 0);
	DX8Wrapper::Set_DX8_Render_Target_Surfaces(sceneTarget, sceneDepth);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, depthTest);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, stencil);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE, colorWrite);
	if (mapSurface != nullptr)
	{
		mapSurface->Release();
	}
	if (sceneDepth != nullptr)
	{
		sceneDepth->Release();
	}
	sceneTarget->Release();

	// Blend and cull are ShaderClass state, so the next shader set restores them in full.
	ShaderClass::Invalidate();
	rinfo.Camera.Apply();

	// The camera's view is deferred, and rivers invalidate the wrapper before drawing, which would drop it.
	DX8Wrapper::Apply_Render_State_Changes();
#else
	(void)rinfo;
	(void)terrain;
#endif
}
