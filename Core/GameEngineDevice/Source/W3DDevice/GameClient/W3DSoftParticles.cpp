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

// W3DSoftParticles.cpp ///////////////////////////////////////////////////////////////////////////
// Fades particle sprites where they near the scene's depth or the terrain behind them
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Lib/BaseType.h"
#include "WWLib/always.h"
#include "W3DDevice/GameClient/W3DSoftParticles.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DWater.h"
#include "Common/GlobalData.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8caps.h"
#include "WW3D2/formconv.h"
#include "WW3D2/shader.h"
#include "WW3D2/texture.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"

W3DSoftParticles *TheW3DSoftParticles = nullptr;

// CONTRA_SOFTPARTICLES bisects faults: 0 hard, 1 scene depth where readable else terrain, 2 terrain only.
enum { SOFT_PARTICLES_OFF = 0, SOFT_PARTICLES_AUTO = 1, SOFT_PARTICLES_TERRAIN = 2 };

static Int Get_Soft_Particle_Mode()
{
	const char *value = getenv("CONTRA_SOFTPARTICLES");
	return (value != nullptr) ? atoi(value) : SOFT_PARTICLES_AUTO;
}

static const Int SoftParticleMode = Get_Soft_Particle_Mode();

// The sprite's camera-space position comes through this stage, which also holds the surface it fades against.
static const Int SOFT_STAGE = 1;

W3DSoftParticles::W3DSoftParticles()
	: m_depthShader(0),
	  m_heightShader(0),
	  m_loaded(FALSE)
{
	Set_D3DMATRIX_Identity(m_view);
	Set_D3DMATRIX_Identity(m_projection);

	if (SoftParticleMode != SOFT_PARTICLES_OFF)
	{
		SortingRendererClass::Set_Soft_Particle_Hook(this);
	}
}

W3DSoftParticles::~W3DSoftParticles()
{
	if (SortingRendererClass::Peek_Soft_Particle_Hook() == this)
	{
		SortingRendererClass::Set_Soft_Particle_Hook(nullptr);
	}

	ReleaseResources();
}

// The next draw loads them again, so a device made anew gets shaders of its own.
void W3DSoftParticles::ReleaseResources()
{
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device != nullptr)
	{
		DX8_DELETE_PIXEL_SHADER(device, m_depthShader);
		DX8_DELETE_PIXEL_SHADER(device, m_heightShader);
	}
	m_depthShader = 0;
	m_heightShader = 0;
	m_loaded = FALSE;
}

void W3DSoftParticles::beginPass(RenderInfoClass &rinfo)
{
	rinfo.Camera.Apply();
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	device->GetTransform(D3DTS_VIEW, &m_view);
	device->GetTransform(D3DTS_PROJECTION, &m_projection);
}

// Loaded on first use, once the device can say whether it runs them.
Bool W3DSoftParticles::loadShaders()
{
#if defined(BUILD_WITH_D3D9)
	if (!m_loaded)
	{
		m_loaded = TRUE;
		const DX8Caps *caps = DX8Wrapper::Get_Current_Caps();
		if (caps != nullptr && caps->Get_Pixel_Shader_Major_Version() >= 2)
		{
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\softparticledepth.pso", nullptr, 0, false, &m_depthShader)))
			{
				m_depthShader = 0;
			}
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\softparticleheight.pso", nullptr, 0, false, &m_heightShader)))
			{
				m_heightShader = 0;
			}
		}
	}
	return m_depthShader != 0 || m_heightShader != 0;
#else
	return FALSE;
#endif
}

// Only while the scene's depth is the bound one, which leaves out reflections and shadow maps.
// It stays bound as the depth buffer, which INTZ allows while depth writes are off.
Bool W3DSoftParticles::bindSceneDepth()
{
#if defined(BUILD_WITH_D3D9)
	IDirect3DTexture8 *depthTexture = DX8Wrapper::Peek_Scene_Depth_Texture();
	if (m_depthShader == 0 || depthTexture == nullptr || SoftParticleMode != SOFT_PARTICLES_AUTO)
	{
		return FALSE;
	}

	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DSurface8 *bound = nullptr;
	if (FAILED(device->GetDepthStencilSurface(&bound)) || bound == nullptr)
	{
		return FALSE;
	}
	const Bool sceneDepth = (bound == DX8Wrapper::Peek_Scene_Depth_Surface());
	bound->Release();

	// The device is read because the wrapper's cache can hold a placeholder after an invalidate.
	DWORD depthWrites = TRUE;
	device->GetRenderState(D3DRS_ZWRITEENABLE, &depthWrites);
	if (!sceneDepth || depthWrites != FALSE)
	{
		return FALSE;
	}

	D3DSURFACE_DESC desc;
	depthTexture->GetLevelDesc(0, &desc);

	const D3DMATRIX &p = m_projection;
	const Vector4 clipX(p.m[0][0], p.m[1][0], p.m[2][0], p.m[3][0]);
	const Vector4 clipY(p.m[0][1], p.m[1][1], p.m[2][1], p.m[3][1]);
	const Vector4 clipW(p.m[0][3], p.m[1][3], p.m[2][3], p.m[3][3]);
	const Vector4 screenMap = W3DShaderManager::getClipToTargetMapping((Real)desc.Width, (Real)desc.Height);
	const Vector4 linearize(p.m[3][2], p.m[3][3], p.m[2][3], p.m[2][2]);

	DX8Wrapper::Set_Pixel_Shader_Constant(0, &clipX, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(1, &clipY, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(2, &clipW, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(3, &screenMap, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(4, &linearize, 1);

	device->SetTexture(SOFT_STAGE, depthTexture);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_MINFILTER, D3DTEXF_POINT);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_MAGFILTER, D3DTEXF_POINT);
	DX8Wrapper::Set_Pixel_Shader(m_depthShader);
	return TRUE;
#else
	return FALSE;
#endif
}

Bool W3DSoftParticles::bindTerrainHeight()
{
#if defined(BUILD_WITH_D3D9)
	if (m_heightShader == 0 || TheWaterRenderObj == nullptr)
	{
		return FALSE;
	}

	Vector4 heightMap;
	Vector4 heightDecode;
	TextureClass *heightTexture = TheWaterRenderObj->getTerrainHeightTexture(heightMap, heightDecode);
	if (heightTexture == nullptr || heightTexture->Peek_D3D_Texture() == nullptr)
	{
		return FALSE;
	}

	// Camera space back to world, where the terrain heights are.
	D3DMATRIX toWorld;
	float det;
	Invert_D3DMATRIX(toWorld, &det, m_view);
	const Vector4 worldX(toWorld.m[0][0], toWorld.m[1][0], toWorld.m[2][0], toWorld.m[3][0]);
	const Vector4 worldY(toWorld.m[0][1], toWorld.m[1][1], toWorld.m[2][1], toWorld.m[3][1]);
	const Vector4 worldZ(toWorld.m[0][2], toWorld.m[1][2], toWorld.m[2][2], toWorld.m[3][2]);

	DX8Wrapper::Set_Pixel_Shader_Constant(0, &worldX, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(1, &worldY, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(2, &worldZ, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(3, &heightMap, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(4, &heightDecode, 1);

	// Filtering each byte on its own still blends the heights linearly.
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(SOFT_STAGE, heightTexture->Peek_D3D_Texture());
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_Pixel_Shader(m_heightShader);
	return TRUE;
#else
	return FALSE;
#endif
}

bool W3DSoftParticles::Begin(const ShaderClass &shader)
{
	if (!TheGlobalData->m_useSoftParticles || TheGlobalData->m_softParticleDistance <= 0.0f || !loadShaders())
	{
		return false;
	}

	// Cleared through the wrapper, so its record of stage 1 matches the device once End unbinds it.
	DX8Wrapper::Set_Texture(SOFT_STAGE, nullptr);
	DX8Wrapper::Apply_Render_State_Changes();

	if (!bindSceneDepth() && !bindTerrainHeight())
	{
		return false;
	}

	D3DMATRIX identity;
	Set_D3DMATRIX_Identity(identity);
	DX8Wrapper::_Set_DX8_Transform((D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + SOFT_STAGE), identity);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_MIPFILTER, D3DTEXF_NONE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

	// Alpha blending fades alpha alone, adding fades colour too, and multiplying fades towards white.
	const Bool addsColor = shader.Get_Src_Blend_Func() == ShaderClass::SRCBLEND_ONE;
	const Bool multiplies = shader.Get_Dst_Blend_Func() == ShaderClass::DSTBLEND_SRC_COLOR;
	// The sign that makes depth along the view grow away from the camera, for perspective and ortho alike.
	const Real forward = (m_projection.m[2][2] < 0.0f) ? -1.0f : 1.0f;
	const Vector4 params(1.0f / TheGlobalData->m_softParticleDistance, addsColor ? 1.0f : 0.0f, multiplies ? 1.0f : 0.0f, forward);
	DX8Wrapper::Set_Pixel_Shader_Constant(5, &params, 1);
	return true;
}

void W3DSoftParticles::End()
{
	DX8Wrapper::Set_Pixel_Shader(0);
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(SOFT_STAGE, nullptr);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU | SOFT_STAGE);
}
