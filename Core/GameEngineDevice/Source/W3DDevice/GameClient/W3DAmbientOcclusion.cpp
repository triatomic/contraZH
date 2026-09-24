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

// W3DAmbientOcclusion.cpp ////////////////////////////////////////////////////////////////////////
// Screen-space ambient occlusion from the scene's INTZ depth
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Lib/BaseType.h"
#include "WWLib/always.h"
#include "W3DDevice/GameClient/W3DAmbientOcclusion.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "Common/GlobalData.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8vertexbuffer.h"
#include "WW3D2/dx8indexbuffer.h"
#include "WW3D2/dx8fvf.h"
#include "WW3D2/formconv.h"
#include "WW3D2/shader.h"
#include "WW3D2/vertmaterial.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"

W3DAmbientOcclusion *TheW3DAmbientOcclusion = nullptr;

// CONTRA_SSAO bisects faults: 0 off, 1 on.
static Int Get_Ambient_Occlusion_Mode()
{
	const char *value = getenv("CONTRA_SSAO");
	return (value != nullptr) ? atoi(value) : 1;
}

static const Int AmbientOcclusionMode = Get_Ambient_Occlusion_Mode();

enum { SPIRAL_SAMPLES = 12, SPIRAL_TURNS = 7 };

// A sample's facing must pass this cosine before it occludes, so curved and tessellated surfaces don't shade themselves.
static const Real OCCLUSION_BIAS = 0.1f;

// The disc never spans more than this much of the screen, which keeps close-ups from thrashing the texture cache.
static const Real MAX_RADIUS_UV = 0.1f;

// The blur stops at depth steps larger than this fraction of the pixel's depth.
static const Real BLUR_DEPTH_TOLERANCE = 0.1f;

W3DAmbientOcclusion::W3DAmbientOcclusion()
	: m_occlusionShader(0),
	  m_blurShader(0),
	  m_loaded(FALSE)
{
	for (Int i = 0; i < TARGET_COUNT; i++)
	{
		m_target[i] = nullptr;
		m_targetSurface[i] = nullptr;
	}
}

W3DAmbientOcclusion::~W3DAmbientOcclusion()
{
	ReleaseResources();
}

void W3DAmbientOcclusion::ReleaseResources()
{
	releaseTargets();

	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device != nullptr)
	{
		DX8_DELETE_PIXEL_SHADER(device, m_occlusionShader);
		DX8_DELETE_PIXEL_SHADER(device, m_blurShader);
	}
	m_occlusionShader = 0;
	m_blurShader = 0;
	m_loaded = FALSE;
}

void W3DAmbientOcclusion::releaseTargets()
{
	for (Int i = 0; i < TARGET_COUNT; i++)
	{
		if (m_targetSurface[i] != nullptr)
		{
			m_targetSurface[i]->Release();
			m_targetSurface[i] = nullptr;
		}
		if (m_target[i] != nullptr)
		{
			m_target[i]->Release();
			m_target[i] = nullptr;
		}
	}
}

// Loaded on first use, once the device can say whether it runs them.
Bool W3DAmbientOcclusion::loadShaders()
{
#if defined(BUILD_WITH_D3D9)
	if (!m_loaded)
	{
		m_loaded = TRUE;
		if (W3DShaderManager::supportsPixelShader2a())
		{
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\ambientocclusion.pso", nullptr, 0, false, &m_occlusionShader)))
			{
				m_occlusionShader = 0;
			}
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\ambientocclusionblur.pso", nullptr, 0, false, &m_blurShader)))
			{
				m_blurShader = 0;
			}
		}
	}
	return m_occlusionShader != 0 && m_blurShader != 0;
#else
	return FALSE;
#endif
}

Bool W3DAmbientOcclusion::acquireTargets(UnsignedInt width, UnsignedInt height)
{
#if defined(BUILD_WITH_D3D9)
	if (m_target[0] != nullptr)
	{
		D3DSURFACE_DESC desc;
		m_target[0]->GetLevelDesc(0, &desc);
		if (desc.Width == width && desc.Height == height)
		{
			return TRUE;
		}
		releaseTargets();
	}

	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	for (Int i = 0; i < TARGET_COUNT; i++)
	{
		if (FAILED(device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_target[i], nullptr)) ||
			FAILED(m_target[i]->GetSurfaceLevel(0, &m_targetSurface[i])))
		{
			releaseTargets();
			return FALSE;
		}
	}
	return TRUE;
#else
	(void)width;
	(void)height;
	return FALSE;
#endif
}

// A clip-space quad over the viewport whose uv lands on the texel centres of scene-sized targets.
void W3DAmbientOcclusion::drawQuad(const Vector4 &clipToTarget)
{
	static const Real cornerX[4] = { -1.0f, 1.0f, -1.0f, 1.0f };
	static const Real cornerY[4] = { 1.0f, 1.0f, -1.0f, -1.0f };

	DynamicVBAccessClass vbAccess(BUFFER_TYPE_DYNAMIC_DX8, dynamic_fvf_type, 4);
	{
		DynamicVBAccessClass::WriteLockClass lock(&vbAccess);
		VertexFormatXYZNDUV2 *verts = lock.Get_Formatted_Vertex_Array();
		for (Int i = 0; i < 4; i++)
		{
			verts[i].x = cornerX[i];
			verts[i].y = cornerY[i];
			verts[i].z = 0.0f;
			verts[i].nx = 0.0f;
			verts[i].ny = 0.0f;
			verts[i].nz = 0.0f;
			verts[i].diffuse = 0xffffffff;
			verts[i].u1 = cornerX[i] * clipToTarget.X + clipToTarget.Z;
			verts[i].v1 = cornerY[i] * clipToTarget.Y + clipToTarget.W;
			verts[i].u2 = 0.0f;
			verts[i].v2 = 0.0f;
		}
	}

	DynamicIBAccessClass ibAccess(BUFFER_TYPE_DYNAMIC_DX8, 6);
	{
		DynamicIBAccessClass::WriteLockClass lock(&ibAccess);
		UnsignedShort *indices = lock.Get_Index_Array();
		indices[0] = 0;
		indices[1] = 1;
		indices[2] = 2;
		indices[3] = 2;
		indices[4] = 1;
		indices[5] = 3;
	}

	DX8Wrapper::Set_Vertex_Buffer(vbAccess);
	DX8Wrapper::Set_Index_Buffer(ibAccess, 0);
	DX8Wrapper::Draw_Triangles(0, 2, 0, 4);
}

void W3DAmbientOcclusion::render(RenderInfoClass &rinfo)
{
#if defined(BUILD_WITH_D3D9)
	if (AmbientOcclusionMode == 0 || !TheGlobalData->m_useAmbientOcclusion ||
		TheGlobalData->m_ambientOcclusionRadius <= 0.0f || TheGlobalData->m_ambientOcclusionStrength <= 0.0f)
	{
		return;
	}

	IDirect3DTexture8 *depthTexture = DX8Wrapper::Peek_Scene_Depth_Texture();
	if (depthTexture == nullptr || !loadShaders())
	{
		return;
	}

	// Only while the scene's depth is the bound one, which leaves out reflections and shadow maps.
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DSurface8 *sceneDepth = nullptr;
	IDirect3DSurface8 *sceneTarget = nullptr;
	if (FAILED(device->GetDepthStencilSurface(&sceneDepth)) || sceneDepth == nullptr)
	{
		return;
	}
	if (sceneDepth != DX8Wrapper::Peek_Scene_Depth_Surface() || FAILED(device->GetRenderTarget(0, &sceneTarget)) || sceneTarget == nullptr)
	{
		sceneDepth->Release();
		return;
	}

	// The targets share the depth's pixel grid, so one uv reads both.
	D3DSURFACE_DESC depthDesc;
	D3DSURFACE_DESC targetDesc;
	depthTexture->GetLevelDesc(0, &depthDesc);
	sceneTarget->GetDesc(&targetDesc);
	if (targetDesc.Width != depthDesc.Width || targetDesc.Height != depthDesc.Height ||
		targetDesc.MultiSampleType != D3DMULTISAMPLE_NONE || !acquireTargets(depthDesc.Width, depthDesc.Height))
	{
		sceneTarget->Release();
		sceneDepth->Release();
		return;
	}

	const Real width = (Real)depthDesc.Width;
	const Real height = (Real)depthDesc.Height;

	rinfo.Camera.Apply();
	D3DMATRIX projection;
	D3DVIEWPORT8 cameraViewport;
	device->GetTransform(D3DTS_PROJECTION, &projection);
	device->GetViewport(&cameraViewport);
	const Vector4 cameraMap = W3DShaderManager::getClipToTargetMapping(width, height);

	// Texel uv to the camera's clip space, then back through the projection to camera space.
	D3DMATRIX uvToClip;
	Set_D3DMATRIX_Identity(uvToClip);
	uvToClip.m[0][0] = 1.0f / cameraMap.X;
	uvToClip.m[1][1] = 1.0f / cameraMap.Y;
	uvToClip.m[3][0] = -cameraMap.Z / cameraMap.X;
	uvToClip.m[3][1] = -cameraMap.W / cameraMap.Y;
	D3DMATRIX unproject;
	float det;
	Invert_D3DMATRIX(unproject, &det, projection);
	const D3DMATRIX toView = uvToClip * unproject;

	const D3DMATRIX &p = projection;
	const Real radius = TheGlobalData->m_ambientOcclusionRadius;
	const Vector4 radiusConstant(radius * p.m[0][0] * cameraMap.X, radius * p.m[1][1] * fabs(cameraMap.Y), p.m[2][3], p.m[3][3]);
	const Vector4 params(1.0f / (radius * radius), OCCLUSION_BIAS, TheGlobalData->m_ambientOcclusionStrength * 2.0f / (Real)SPIRAL_SAMPLES, MAX_RADIUS_UV);
	const Vector4 targetSize(width, height, 0.0f, 0.0f);
	const Vector4 linearize(p.m[3][2], p.m[3][3], p.m[2][3], p.m[2][2]);

	// Samples spiral outwards, crowding near the centre where contact shading is strongest.
	Vector4 spiral[SPIRAL_SAMPLES / 2];
	for (Int i = 0; i < SPIRAL_SAMPLES / 2; i++)
	{
		Real offset[4];
		for (Int j = 0; j < 2; j++)
		{
			const Real along = ((Real)(i * 2 + j) + 0.5f) / (Real)SPIRAL_SAMPLES;
			const Real angle = along * (Real)SPIRAL_TURNS * 2.0f * PI;
			offset[j * 2 + 0] = along * cos(angle);
			offset[j * 2 + 1] = along * sin(angle);
		}
		spiral[i].Set(offset[0], offset[1], offset[2], offset[3]);
	}

	// Depth reads stay point sampled; filtering the occlusion would blur it across depth edges.
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
	ShaderClass multiply = ShaderClass::_PresetMultiplicative2DShader;
	multiply.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);

	DX8Wrapper::Set_Shader(replace);
	DX8Wrapper::Apply_Render_State_Changes();
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, FALSE);

	// The device is read because the wrapper's cache can hold a placeholder after an invalidate.
	DWORD stencil = FALSE;
	device->GetRenderState(D3DRS_STENCILENABLE, &stencil);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, FALSE);
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

	// No depth is bound while the passes read it, and each new target starts with a full viewport.
	device->SetTexture(0, depthTexture);
	if (SUCCEEDED(DX8Wrapper::Set_DX8_Render_Target_Surfaces(m_targetSurface[TARGET_OCCLUSION], nullptr)))
	{
		const Vector4 fullMap = W3DShaderManager::getClipToTargetMapping(width, height);
		DX8Wrapper::Set_Pixel_Shader(m_occlusionShader);
		DX8Wrapper::Set_Pixel_Shader_Constant(0, &toView, 4);
		DX8Wrapper::Set_Pixel_Shader_Constant(4, &radiusConstant, 1);
		DX8Wrapper::Set_Pixel_Shader_Constant(5, &params, 1);
		DX8Wrapper::Set_Pixel_Shader_Constant(6, &targetSize, 1);
		DX8Wrapper::Set_Pixel_Shader_Constant(7, spiral, SPIRAL_SAMPLES / 2);
		drawQuad(fullMap);

		if (SUCCEEDED(DX8Wrapper::Set_DX8_Render_Target_Surfaces(m_targetSurface[TARGET_BLUR], nullptr)))
		{
			const Vector4 across(1.0f / width, 0.0f, 1.0f / BLUR_DEPTH_TOLERANCE, 0.0f);
			DX8Wrapper::Set_Pixel_Shader(m_blurShader);
			DX8Wrapper::Set_Pixel_Shader_Constant(0, &linearize, 1);
			DX8Wrapper::Set_Pixel_Shader_Constant(1, &across, 1);
			device->SetTexture(1, m_target[TARGET_OCCLUSION]);
			drawQuad(fullMap);

			// The last pass blurs down and lays the result over the camera's part of the scene.
			if (SUCCEEDED(DX8Wrapper::Set_DX8_Render_Target_Surfaces(sceneTarget, nullptr)))
			{
				DX8Wrapper::Set_Viewport(&cameraViewport);
				DX8Wrapper::Set_Shader(TheGlobalData->m_ambientOcclusionDebug ? replace : multiply);
				DX8Wrapper::Apply_Render_State_Changes();

				const Vector4 down(0.0f, 1.0f / height, 1.0f / BLUR_DEPTH_TOLERANCE, 0.0f);
				DX8Wrapper::Set_Pixel_Shader_Constant(1, &down, 1);
				device->SetTexture(1, m_target[TARGET_BLUR]);
				drawQuad(cameraMap);
			}
		}
	}

	DX8Wrapper::Set_Pixel_Shader(0);
	device->SetTexture(0, nullptr);
	device->SetTexture(1, nullptr);
	DX8Wrapper::Set_Vertex_Buffer(nullptr);
	DX8Wrapper::Set_Index_Buffer(nullptr, 0);
	DX8Wrapper::Set_DX8_Render_Target_Surfaces(sceneTarget, sceneDepth);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, TRUE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, stencil);
	sceneTarget->Release();
	sceneDepth->Release();

	// Blend and cull are ShaderClass state, so the next shader set restores them in full.
	ShaderClass::Invalidate();
	rinfo.Camera.Apply();

	// The camera's view is deferred, and rivers invalidate the wrapper before drawing, which would drop it.
	DX8Wrapper::Apply_Render_State_Changes();
#else
	(void)rinfo;
#endif
}
