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

#include "Lib/BaseType.h"
#include "WWLib/always.h"
#include "W3DDevice/GameClient/W3DDistanceFog.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "Common/GlobalData.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8caps.h"
#include "WW3D2/shader.h"
#include "WW3D2/vertmaterial.h"
#include "WW3D2/camera.h"

W3DDistanceFog *TheW3DDistanceFog = nullptr;

W3DDistanceFog::W3DDistanceFog() : m_sceneCopy(nullptr), m_shader(0), m_loaded(FALSE)
{
}

W3DDistanceFog::~W3DDistanceFog()
{
	ReleaseResources();
}

void W3DDistanceFog::ReleaseResources()
{
	if (m_sceneCopy != nullptr)
	{
		m_sceneCopy->Release();
		m_sceneCopy = nullptr;
	}
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device != nullptr)
	{
		DX8_DELETE_PIXEL_SHADER(device, m_shader);
	}
	m_shader = 0;
	m_loaded = FALSE;
}

void W3DDistanceFog::render(CameraClass &camera)
{
#if defined(BUILD_WITH_D3D9) && RTS_ZEROHOUR
	const GlobalData *data = TheGlobalData;
	IDirect3DTexture8 *depth = DX8Wrapper::Peek_Scene_Depth_Texture();
	if (data == nullptr || !data->m_distanceFog || !(data->m_distanceFogStrength > 0.0f) ||
		!(data->m_distanceFogEnd > max(data->m_distanceFogStart, 0.0f)) || depth == nullptr)
	{
		return;
	}
	if (!m_loaded)
	{
		m_loaded = TRUE;
		const DX8Caps *caps = DX8Wrapper::Get_Current_Caps();
		if (caps != nullptr && caps->Get_Pixel_Shader_Major_Version() >= 2)
		{
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\distancefog.pso", nullptr, 0, false, &m_shader)))
			{
				m_shader = 0;
			}
		}
	}
	if (m_shader == 0)
	{
		return;
	}

	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DSurface8 *bound = nullptr;
	if (FAILED(device->GetDepthStencilSurface(&bound)) || bound == nullptr)
	{
		return;
	}
	if (bound != DX8Wrapper::Peek_Scene_Depth_Surface())
	{
		bound->Release();
		return;
	}

	camera.Apply();
	DX8Wrapper::Apply_Render_State_Changes();
	D3DMATRIX projection;
	device->GetTransform(D3DTS_PROJECTION, &projection);
	if (!W3DShaderManager::copyRenderTarget(m_sceneCopy))
	{
		bound->Release();
		return;
	}
	D3DSURFACE_DESC sceneDesc;
	D3DSURFACE_DESC depthDesc;
	m_sceneCopy->GetLevelDesc(0, &sceneDesc);
	depth->GetLevelDesc(0, &depthDesc);
	if (sceneDesc.Width != depthDesc.Width || sceneDesc.Height != depthDesc.Height)
	{
		bound->Release();
		return;
	}

	static const Vector3 presets[] = {
		Vector3(0.65f, 0.72f, 0.75f),
		Vector3(0.78f, 0.64f, 0.43f),
		Vector3(0.28f, 0.60f, 0.68f),
		Vector3(0.24f, 0.28f, 0.34f),
		Vector3(0.48f, 0.43f, 0.25f)
	};
	Int preset = data->m_distanceFogPreset;
	Vector3 color(data->m_distanceFogColor.red, data->m_distanceFogColor.green, data->m_distanceFogColor.blue);
	if (preset >= 0 && preset < 5)
	{
		color = presets[preset];
	}
	const RGBColor &ambient = data->m_terrainAmbient[0];
	const RGBColor &sun = data->m_terrainDiffuse[0];
	const Vector3 light(min(max(ambient.red + sun.red, 0.0f), 1.0f),
		min(max(ambient.green + sun.green, 0.0f), 1.0f), min(max(ambient.blue + sun.blue, 0.0f), 1.0f));
	const Real influence = preset == 0 ? 1.0f : WWMath::Clamp(data->m_distanceFogSunInfluence, 0.0f, 1.0f);
	color = color * (1.0f - influence) + light * influence;
	const Vector4 fogColor(color.X, color.Y, color.Z, 0.0f);
	const Vector4 fog(max(data->m_distanceFogStart, 0.0f),
		1.0f / max(data->m_distanceFogEnd - max(data->m_distanceFogStart, 0.0f), 1.0f),
		WWMath::Clamp(data->m_distanceFogStrength, 0.0f, 1.0f), 0.0f);
	const Vector4 linearize(projection.m[3][2], projection.m[3][3], projection.m[2][3], projection.m[2][2]);
	const Vector4 screenMap = W3DShaderManager::getClipToTargetMapping((Real)sceneDesc.Width, (Real)sceneDesc.Height);

	// D3D9 cannot sample the depth surface while it is attached.
	if (FAILED(device->SetDepthStencilSurface(nullptr)))
	{
		bound->Release();
		return;
	}

	DX8Wrapper::Set_Texture(0, nullptr);
	DX8Wrapper::Set_Texture(1, nullptr);
	VertexMaterialClass *material = VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(material);
	REF_PTR_RELEASE(material);
	DX8Wrapper::Set_World_Identity();
	DX8Wrapper::Set_View_Identity();
	DX8Wrapper::Set_Transform(D3DTS_PROJECTION, Matrix4x4(true));
	ShaderClass replace = ShaderClass::_PresetOpaque2DShader;
	replace.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);
	DX8Wrapper::Set_Shader(replace);
	DX8Wrapper::Apply_Render_State_Changes();

	DWORD depthTest = D3DZB_TRUE;
	DWORD stencil = FALSE;
	device->GetRenderState(D3DRS_ZENABLE, &depthTest);
	device->GetRenderState(D3DRS_STENCILENABLE, &stencil);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGENABLE, FALSE);
	for (Int stage = 0; stage < 2; stage++)
	{
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MINFILTER, D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MAGFILTER, D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MIPFILTER, D3DTEXF_NONE);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	}
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_TEXCOORDINDEX, 0);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

	device->SetTexture(0, m_sceneCopy);
	device->SetTexture(1, depth);
	DX8Wrapper::Set_Pixel_Shader(m_shader);
	DX8Wrapper::Set_Pixel_Shader_Constant(0, &linearize, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(1, &fog, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(2, &fogColor, 1);
	W3DShaderManager::drawClipQuad(screenMap);

	DX8Wrapper::Set_Pixel_Shader(0);
	device->SetTexture(0, nullptr);
	device->SetTexture(1, nullptr);
	device->SetDepthStencilSurface(bound);
	bound->Release();
	DX8Wrapper::Set_Vertex_Buffer(nullptr);
	DX8Wrapper::Set_Index_Buffer(nullptr, 0);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, depthTest);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, stencil);
	ShaderClass::Invalidate();
	camera.Apply();
	DX8Wrapper::Apply_Render_State_Changes();
#else
	(void)camera;
#endif
}
