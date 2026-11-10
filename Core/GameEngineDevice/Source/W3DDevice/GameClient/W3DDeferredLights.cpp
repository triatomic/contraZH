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

// W3DDeferredLights.cpp //////////////////////////////////////////////////////////////////////////
// Point lights added to the opaque scene from its INTZ depth, all in one instanced draw
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Lib/BaseType.h"
#include "WWLib/always.h"
#include "W3DDevice/GameClient/W3DDeferredLights.h"
#include "W3DDevice/GameClient/W3DLaserGlow.h"
#include "Common/GlobalData.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8caps.h"
#include "WW3D2/dx8vertexbuffer.h"
#include "WW3D2/dx8indexbuffer.h"
#include "WW3D2/formconv.h"
#include "WW3D2/shader.h"
#include "WW3D2/vertmaterial.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WWLib/WWCommon.h"
#include <stddef.h>

W3DDeferredLights *TheW3DDeferredLights = nullptr;

// CONTRA_DEFERREDLIGHTS bisects faults: 0 leaves every light to its own passes, 1 draws the small ones here.
static Int Get_Deferred_Lights_Mode()
{
	const char *value = getenv("CONTRA_DEFERREDLIGHTS");
	return (value != nullptr) ? atoi(value) : 1;
}

static const Int DeferredLightsMode = Get_Deferred_Lights_Mode();

#if defined(BUILD_WITH_D3D9)

enum { LIGHT_BUFFER_COUNT = 4096 };

// One light as the vertex shader reads it from the instance stream.
struct LightInstance
{
	Vector4 place;	///< camera space position, and the outer radius
	Vector4 color;	///< colour over the scene's light, and 1 / (outer - inner radius)
	Real ambient;	///< ambient as a fraction of the colour
};

static IDirect3DVertexDeclaration9 *LightDeclaration = nullptr;
static IDirect3DVertexBuffer9 *LightInstances = nullptr;
static DX8VertexBufferClass *LightQuad = nullptr;
static DX8IndexBufferClass *LightQuadIndices = nullptr;

#endif

W3DDeferredLights::W3DDeferredLights()
	: m_vertexShader(0),
	  m_pixelShader(0),
	  m_loaded(FALSE),
	  m_failed(FALSE)
{
}

W3DDeferredLights::~W3DDeferredLights()
{
	ReleaseResources();
}

void W3DDeferredLights::ReleaseResources()
{
#if defined(BUILD_WITH_D3D9)
	REF_PTR_RELEASE(LightQuad);
	REF_PTR_RELEASE(LightQuadIndices);
	SAFE_RELEASE(LightInstances);
	SAFE_RELEASE(LightDeclaration);

	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device != nullptr)
	{
		DX8_DELETE_VERTEX_SHADER(device, m_vertexShader);
		DX8_DELETE_PIXEL_SHADER(device, m_pixelShader);
	}
#endif
	m_vertexShader = 0;
	m_pixelShader = 0;
	m_loaded = FALSE;
	m_failed = FALSE;
}

// Loaded on first use, once the device can say whether it runs them.
Bool W3DDeferredLights::loadResources()
{
#if defined(BUILD_WITH_D3D9)
	if (!m_loaded)
	{
		m_loaded = TRUE;

		// Instancing needs what the particle billboards need.
		const DX8Caps *caps = DX8Wrapper::Get_Current_Caps();
		if (caps == nullptr || !W3DShaderManager::supportsPixelShader2a() || caps->Get_Vertex_Shader_Major_Version() < 3 ||
			caps->Get_DX8_Caps().MaxStreams < 2 || !(caps->Get_DX8_Caps().DevCaps2 & D3DDEVCAPS2_STREAMOFFSET) ||
			!(caps->Get_DX8_Caps().DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) || !DX8Wrapper::Is_Hardware_Vertex_Processing())
		{
			return FALSE;
		}

		// The pass sets its own declaration, so the one loaded here only has to be valid.
		DWORD declaration[] =
		{
			D3DVSD_STREAM(0),
			D3DVSD_REG(0, D3DVSDT_FLOAT3),
			D3DVSD_END()
		};
		if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\deferredlights.vso", declaration, 0, true, &m_vertexShader)))
		{
			m_vertexShader = 0;
		}
		if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\deferredlights.pso", nullptr, 0, false, &m_pixelShader)))
		{
			m_pixelShader = 0;
		}

		IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
		const D3DVERTEXELEMENT9 elements[] =
		{
			{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
			{ 1, (WORD)offsetof(LightInstance, place), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 4 },
			{ 1, (WORD)offsetof(LightInstance, color), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 5 },
			{ 1, (WORD)offsetof(LightInstance, ambient), D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 6 },
			D3DDECL_END()
		};
		if (FAILED(device->CreateVertexDeclaration(elements, &LightDeclaration)))
		{
			LightDeclaration = nullptr;
		}
		if (FAILED(device->CreateVertexBuffer(LIGHT_BUFFER_COUNT * sizeof(LightInstance), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0,
			D3DPOOL_DEFAULT, &LightInstances, nullptr)))
		{
			LightInstances = nullptr;
		}

		const Vector3 positions[] =
		{
			Vector3(-0.5f, 0.5f, 0.0f), Vector3(-0.5f, -0.5f, 0.0f),
			Vector3(0.5f, -0.5f, 0.0f), Vector3(0.5f, 0.5f, 0.0f)
		};
		const Vector2 uvs[] =
		{
			Vector2(0.0f, 0.0f), Vector2(0.0f, 1.0f), Vector2(1.0f, 1.0f), Vector2(1.0f, 0.0f)
		};
		LightQuad = NEW_REF(DX8VertexBufferClass, (positions, uvs, 4));
		LightQuadIndices = NEW_REF(DX8IndexBufferClass, (6));
		{
			DX8IndexBufferClass::WriteLockClass lock(LightQuadIndices);
			unsigned short *indices = lock.Get_Index_Array();
			indices[0] = 0;
			indices[1] = 1;
			indices[2] = 2;
			indices[3] = 0;
			indices[4] = 2;
			indices[5] = 3;
		}
	}
	return m_vertexShader != 0 && m_pixelShader != 0 && LightDeclaration != nullptr && LightInstances != nullptr &&
		LightQuad != nullptr && LightQuadIndices != nullptr;
#else
	return FALSE;
#endif
}

Bool W3DDeferredLights::isEnabled()
{
#if defined(BUILD_WITH_D3D9)
	return DeferredLightsMode != 0 && !m_failed && DX8Wrapper::Peek_Scene_Depth_Texture() != nullptr && loadResources();
#else
	return FALSE;
#endif
}

void W3DDeferredLights::setLights(const std::vector<W3DShaderManager::PixelLight> &lights)
{
	m_lights = lights;
}

void W3DDeferredLights::render(RenderInfoClass &rinfo)
{
#if defined(BUILD_WITH_D3D9)
	if (m_lights.empty())
	{
		return;
	}
	IDirect3DTexture8 *depthTexture = DX8Wrapper::Peek_Scene_Depth_Texture();
	if (!isEnabled())
	{
		m_lights.clear();
		return;
	}

	// Only while the scene's depth is the bound one. A reflection drawn first leaves the lights for the scene.
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

	// A target the depth texture does not match pixel for pixel sends the lights back to their own passes.
	D3DSURFACE_DESC depthDesc;
	D3DSURFACE_DESC targetDesc;
	depthTexture->GetLevelDesc(0, &depthDesc);
	sceneTarget->GetDesc(&targetDesc);
	if (targetDesc.Width != depthDesc.Width || targetDesc.Height != depthDesc.Height || targetDesc.MultiSampleType != D3DMULTISAMPLE_NONE)
	{
		m_failed = TRUE;
		m_lights.clear();
		sceneTarget->Release();
		sceneDepth->Release();
		return;
	}

	rinfo.Camera.Apply();
	DX8Wrapper::Apply_Render_State_Changes();
	D3DMATRIX view;
	D3DMATRIX projection;
	D3DVIEWPORT8 cameraViewport;
	DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, view);
	DX8Wrapper::_Get_DX8_Transform(D3DTS_PROJECTION, projection);
	DX8Wrapper::Get_Viewport(cameraViewport);
	const Vector4 depthMap = W3DShaderManager::getClipToTargetMapping((Real)depthDesc.Width, (Real)depthDesc.Height);

	// Depth texture uv to the camera's clip space, then back through the projection to camera space.
	D3DMATRIX uvToClip;
	Set_D3DMATRIX_Identity(uvToClip);
	uvToClip.m[0][0] = 1.0f / depthMap.X;
	uvToClip.m[1][1] = 1.0f / depthMap.Y;
	uvToClip.m[3][0] = -depthMap.Z / depthMap.X;
	uvToClip.m[3][1] = -depthMap.W / depthMap.Y;
	D3DMATRIX unproject;
	float det;
	Invert_D3DMATRIX(unproject, &det, projection);
	const D3DMATRIX toView = uvToClip * unproject;

	Vector4 vertexConstants[5];
	for (Int column = 0; column < 4; column++)
	{
		vertexConstants[column].Set(projection.m[0][column], projection.m[1][column], projection.m[2][column], projection.m[3][column]);
	}
	vertexConstants[4] = depthMap;

	// The light multiplies the pixel's own colour, which the scene's light already dimmed, so that light is divided out.
	const Vector3 sceneLight = W3DLaserGlow::getSceneLight();
	static std::vector<LightInstance> instances;
	instances.resize(m_lights.size());
	for (size_t i = 0; i < m_lights.size(); i++)
	{
		const W3DShaderManager::PixelLight &light = m_lights[i];
		const Vector3 &p = light.position;
		LightInstance &instance = instances[i];
		instance.place.Set(
			p.X * view.m[0][0] + p.Y * view.m[1][0] + p.Z * view.m[2][0] + view.m[3][0],
			p.X * view.m[0][1] + p.Y * view.m[1][1] + p.Z * view.m[2][1] + view.m[3][1],
			p.X * view.m[0][2] + p.Y * view.m[1][2] + p.Z * view.m[2][2] + view.m[3][2],
			light.outerRadius);
		instance.color.Set(light.diffuse.X / sceneLight.X, light.diffuse.Y / sceneLight.Y, light.diffuse.Z / sceneLight.Z,
			1.0f / max(light.outerRadius - light.innerRadius, 0.001f));
		instance.ambient = light.ambientScale;
	}
	m_lights.clear();

	DX8Wrapper::Set_Texture(0, nullptr);
	DX8Wrapper::Set_Texture(1, nullptr);
	VertexMaterialClass *material = VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(material);
	REF_PTR_RELEASE(material);
	ShaderClass shader = ShaderClass::_PresetOpaque2DShader;
	shader.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);
	DX8Wrapper::Set_Shader(shader);
	DX8Wrapper::Set_Vertex_Buffer(LightQuad);
	DX8Wrapper::Set_Index_Buffer(LightQuadIndices, 0);
	DX8Wrapper::Apply_Render_State_Changes();

	// The device is read because the wrapper's cache can hold a placeholder after an invalidate.
	DWORD depthTest = D3DZB_TRUE;
	DWORD stencil = FALSE;
	device->GetRenderState(D3DRS_ZENABLE, &depthTest);
	device->GetRenderState(D3DRS_STENCILENABLE, &stencil);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGENABLE, FALSE);

	// Lights what is already drawn: dest * (1 + light).
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, TRUE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, D3DBLEND_ONE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_BLENDOP, D3DBLENDOP_ADD);

	// Depth reads stay point sampled, so a pixel takes its own surface and not a blend across an edge.
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_POINT);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

	// No depth is bound while the pass reads it, and the new binding starts with a full viewport.
	if (SUCCEEDED(DX8Wrapper::Set_DX8_Render_Target_Surfaces(sceneTarget, nullptr)))
	{
		DX8Wrapper::Set_Viewport(&cameraViewport);
		device->SetTexture(0, depthTexture);
		DX8Wrapper::Set_Pixel_Shader(m_pixelShader);
		DX8Wrapper::Set_Pixel_Shader_Constant(0, &toView, 4);
		DX8Wrapper::Set_Vertex_Shader_Constant(0, vertexConstants, 5);

		size_t drawn = 0;
		while (drawn < instances.size() && !m_failed)
		{
			const unsigned count = (unsigned)min(instances.size() - drawn, (size_t)LIGHT_BUFFER_COUNT);
			void *data = nullptr;
			if (FAILED(LightInstances->Lock(0, count * sizeof(LightInstance), &data, D3DLOCK_DISCARD)))
			{
				m_failed = TRUE;
				break;
			}
			memcpy(data, &instances[drawn], count * sizeof(LightInstance));
			LightInstances->Unlock();

			DX8Wrapper::Begin_Instanced_Drawing(LightDeclaration, Peek_D3D9_Vertex_Shader(m_vertexShader));
			DX8Wrapper::Draw_Instanced_Triangles(0, 2, 0, 4, LightInstances, 0, sizeof(LightInstance), count);
			DX8Wrapper::End_Instanced_Drawing();
			drawn += count;
		}
	}

	DX8Wrapper::Set_Pixel_Shader(0);
	device->SetTexture(0, nullptr);
	DX8Wrapper::Set_Vertex_Buffer(nullptr);
	DX8Wrapper::Set_Index_Buffer(nullptr, 0);
	DX8Wrapper::Set_DX8_Render_Target_Surfaces(sceneTarget, sceneDepth);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, depthTest);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, stencil);
	sceneTarget->Release();
	sceneDepth->Release();

	// Blend, alpha test, fog and cull are ShaderClass state, so the next shader set restores them in full.
	ShaderClass::Invalidate();
	rinfo.Camera.Apply();

	// The camera's view is deferred, and rivers invalidate the wrapper before drawing, which would drop it.
	DX8Wrapper::Apply_Render_State_Changes();
#else
	(void)rinfo;
#endif
}
