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

// W3DSlopeMap.cpp ////////////////////////////////////////////////////////////////////////////////
// Builds the slope maps the derived bumps read, one per texture, through slopemap.hlsl
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Lib/BaseType.h"
#include "WWLib/always.h"
#include "W3DDevice/GameClient/W3DSlopeMap.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "Common/Debug.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8caps.h"
#include "WW3D2/shader.h"
#include "WW3D2/texture.h"
#include "WW3D2/vertmaterial.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WW3D2/ww3d.h"

#include <map>
#include <vector>

#if defined(BUILD_WITH_D3D9)

// CONTRA_SLOPEMAP bisects faults: 0 no slope maps, so no derived bumps, 1 slope maps.
static const Int SlopeMapMode = (getenv("CONTRA_SLOPEMAP") != nullptr) ? atoi(getenv("CONTRA_SLOPEMAP")) : 1;

// Softening keeps a change under 1/SOFTENING, so the scaled slope stays inside the stored range either side of the centre.
static const Real SOFTENING = 6.0f;
static const Real ENCODE_SCALE = 3.0f;

// Eight bits hold this centre exactly, and the specular and road shaders decode with the same value.
static const Real ENCODE_CENTRE = 128.0f / 255.0f;

// Enough builds a frame to cover a new map's textures within a second, few enough not to hitch.
static const Int MAX_BUILDS_PER_FRAME = 32;

// A texture unseen this long lets go of its slope map and the hold on the texture, checked this often.
static const UnsignedInt UNUSED_MS = 30000;
static const UnsignedInt SWEEP_MS = 1000;

struct SlopeEntry
{
	TextureClass *slopeMap;
	IDirect3DTexture8 *builtFrom;	///< held, so a texture loaded in its place cannot take the same address
	Real scaleU;
	Real scaleV;
	UnsignedInt lastUsed;
	Bool queued;
};

// Each key holds a reference, so a freed texture's address never comes back as a stale key.
typedef std::map<TextureClass *, SlopeEntry> SlopeMapCache;
static SlopeMapCache Cache;
static std::vector<TextureClass *> Queue;

static DWORD Shader = 0;
static D3DFORMAT Format = D3DFMT_UNKNOWN;
static Bool Acquired = FALSE;
static Bool Failed = FALSE;
static UnsignedInt UpdateFrame = 0xffffffffu;
static UnsignedInt UpdateTime = 0;
static UnsignedInt SweepTime = 0;

static void Release_Entry(SlopeEntry &entry)
{
	REF_PTR_RELEASE(entry.slopeMap);
	if (entry.builtFrom != nullptr)
	{
		entry.builtFrom->Release();
		entry.builtFrom = nullptr;
	}
}

// Sixteen bits keep distant mips' small slopes from banding, where the card can filter them.
static D3DFORMAT Pick_Format(IDirect3DDevice8 *device)
{
	IDirect3D9 *d3d = nullptr;
	if (FAILED(device->GetDirect3D(&d3d)))
	{
		return D3DFMT_UNKNOWN;
	}
	D3DDEVICE_CREATION_PARAMETERS params;
	D3DDISPLAYMODE mode;
	D3DFORMAT format = D3DFMT_UNKNOWN;
	if (SUCCEEDED(device->GetCreationParameters(&params)) && SUCCEEDED(d3d->GetAdapterDisplayMode(params.AdapterOrdinal, &mode)))
	{
		static const D3DFORMAT candidates[2] = { D3DFMT_G16R16, D3DFMT_A8R8G8B8 };
		for (Int i = 0; i < 2 && format == D3DFMT_UNKNOWN; i++)
		{
			if (d3d->CheckDeviceFormat(params.AdapterOrdinal, params.DeviceType, mode.Format,
				D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_FILTER, D3DRTYPE_TEXTURE, candidates[i]) == D3D_OK)
			{
				format = candidates[i];
			}
		}
	}
	d3d->Release();
	return format;
}

static Bool Acquire(IDirect3DDevice8 *device)
{
	if (!Acquired)
	{
		Acquired = TRUE;
		if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\slopemap.pso", nullptr, 0, false, &Shader)))
		{
			Shader = 0;
		}
		Format = Pick_Format(device);
		Failed = (Shader == 0 || Format == D3DFMT_UNKNOWN);
		if (Failed)
		{
			RENDER_LOG(("W3DSlopeMap: no shader or render target format, so no derived bumps"));
		}
		else
		{
			RENDER_LOG(("W3DSlopeMap: building slope maps as %s", (Format == D3DFMT_G16R16) ? "G16R16" : "A8R8G8B8"));
		}
	}
	return !Failed;
}

static void Drop_Unused()
{
	if (UpdateTime - SweepTime < SWEEP_MS)
	{
		return;
	}
	SweepTime = UpdateTime;

	SlopeMapCache::iterator it = Cache.begin();
	while (it != Cache.end())
	{
		if (!it->second.queued && UpdateTime - it->second.lastUsed > UNUSED_MS)
		{
			Release_Entry(it->second);
			it->first->Release_Ref();
			Cache.erase(it++);
		}
		else
		{
			++it;
		}
	}
}

// Draws every level from the texture's next mip down. A failure leaves the texture flat until it changes.
static void Build(IDirect3DDevice8 *device, TextureClass *texture, SlopeEntry &entry)
{
	IDirect3DTexture8 *source = texture->Peek_D3D_Texture();
	if (entry.builtFrom != nullptr)
	{
		entry.builtFrom->Release();
	}
	entry.builtFrom = source;
	if (source == nullptr)
	{
		return;
	}
	source->AddRef();

	D3DSURFACE_DESC sourceDesc;
	if (FAILED(source->GetLevelDesc(0, &sourceDesc)))
	{
		REF_PTR_RELEASE(entry.slopeMap);
		return;
	}
	const UINT width = max(sourceDesc.Width / 2, (UINT)1);
	const UINT height = max(sourceDesc.Height / 2, (UINT)1);

	IDirect3DTexture8 *target = nullptr;
	if (entry.slopeMap != nullptr)
	{
		D3DSURFACE_DESC desc;
		target = entry.slopeMap->Peek_D3D_Texture();
		if (FAILED(target->GetLevelDesc(0, &desc)) || desc.Width != width || desc.Height != height)
		{
			REF_PTR_RELEASE(entry.slopeMap);
			target = nullptr;
		}
	}
	if (target == nullptr)
	{
		// Cards with conditional non-power-of-two support take such a size only without mips.
		if (FAILED(device->CreateTexture(width, height, 0, D3DUSAGE_RENDERTARGET, Format, D3DPOOL_DEFAULT, &target, nullptr)) &&
			FAILED(device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, Format, D3DPOOL_DEFAULT, &target, nullptr)))
		{
			RENDER_LOG(("W3DSlopeMap: could not make a %ux%u slope map for %s", width, height, (const char *)texture->Get_Texture_Name()));
			return;
		}
		entry.slopeMap = NEW_REF(TextureClass, (target));
		target->Release();
		entry.slopeMap->Get_Filter().Set_Min_Filter(TextureFilterClass::FILTER_TYPE_BEST);
		entry.slopeMap->Get_Filter().Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_BEST);
		entry.slopeMap->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_BEST);
	}
	entry.scaleU = (Real)width / ENCODE_SCALE;
	entry.scaleV = (Real)height / ENCODE_SCALE;

	// The reads and the map repeat or clamp as the texture does.
	const TextureFilterClass::TxtAddrMode addressU = texture->Get_Filter().Get_U_Addr_Mode();
	const TextureFilterClass::TxtAddrMode addressV = texture->Get_Filter().Get_V_Addr_Mode();
	entry.slopeMap->Get_Filter().Set_U_Addr_Mode(addressU);
	entry.slopeMap->Get_Filter().Set_V_Addr_Mode(addressV);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_ADDRESSU,
		(addressU == TextureFilterClass::TEXTURE_ADDRESS_CLAMP) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_ADDRESSV,
		(addressV == TextureFilterClass::TEXTURE_ADDRESS_CLAMP) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);

	DX8Wrapper::Set_DX8_Texture(0, source);
	const DWORD levels = target->GetLevelCount();
	for (DWORD level = 0; level < levels; level++)
	{
		IDirect3DSurface8 *surface = nullptr;
		if (FAILED(target->GetSurfaceLevel(level, &surface)))
		{
			break;
		}
		D3DSURFACE_DESC levelDesc;
		surface->GetDesc(&levelDesc);

		// The new target starts with a full viewport, which the mapping reads.
		if (SUCCEEDED(DX8Wrapper::Set_DX8_Render_Target_Surfaces(surface, nullptr)))
		{
			const Real levelWidth = (Real)levelDesc.Width;
			const Real levelHeight = (Real)levelDesc.Height;
			const Vector4 texel(1.0f / levelWidth, 1.0f / levelHeight, levelWidth / (Real)width, levelHeight / (Real)height);
			DX8Wrapper::Set_Pixel_Shader_Constant(0, &texel, 1);
			W3DShaderManager::drawClipQuad(W3DShaderManager::getClipToTargetMapping(levelWidth, levelHeight));
		}
		surface->Release();
	}
}

#endif

TextureClass *W3DSlopeMap::find(TextureClass *texture, Real &scaleU, Real &scaleV)
{
#if defined(BUILD_WITH_D3D9)
	if (SlopeMapMode == 0 || Failed || texture == nullptr || texture->Peek_D3D_Texture() == nullptr)
	{
		return nullptr;
	}

	SlopeMapCache::iterator it = Cache.find(texture);
	if (it == Cache.end())
	{
		SlopeEntry entry = { nullptr, nullptr, 0.0f, 0.0f, 0, FALSE };
		texture->Add_Ref();
		it = Cache.insert(SlopeMapCache::value_type(texture, entry)).first;
	}

	// A map built from the texture's thumbnail or an earlier reduction serves until the rebuild.
	SlopeEntry &entry = it->second;
	entry.lastUsed = UpdateTime;
	if (entry.builtFrom != texture->Peek_D3D_Texture() && !entry.queued)
	{
		entry.queued = TRUE;
		Queue.push_back(texture);
	}
	scaleU = entry.scaleU;
	scaleV = entry.scaleV;
	return entry.slopeMap;
#else
	(void)texture;
	scaleU = 0.0f;
	scaleV = 0.0f;
	return nullptr;
#endif
}

void W3DSlopeMap::update(RenderInfoClass &rinfo)
{
#if defined(BUILD_WITH_D3D9)
	const UnsignedInt frame = WW3D::Get_Frame_Count();
	if (frame == UpdateFrame)
	{
		return;
	}
	UpdateFrame = frame;
	UpdateTime = timeGetTime();

	Drop_Unused();
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	if (Queue.empty() || device == nullptr)
	{
		return;
	}
	if (!Acquire(device))
	{
		for (size_t i = 0; i < Queue.size(); i++)
		{
			SlopeMapCache::iterator it = Cache.find(Queue[i]);
			if (it != Cache.end())
			{
				it->second.queued = FALSE;
			}
		}
		Queue.clear();
		return;
	}

	IDirect3DSurface8 *sceneTarget = nullptr;
	IDirect3DSurface8 *sceneDepth = nullptr;
	if (FAILED(device->GetRenderTarget(0, &sceneTarget)) || sceneTarget == nullptr)
	{
		return;
	}
	device->GetDepthStencilSurface(&sceneDepth);

	// No stage may still hold a slope map that is about to be drawn into.
	const Int stages = DX8Wrapper::Get_Current_Caps()->Get_Max_Textures_Per_Pass();
	for (Int stage = 0; stage < stages; stage++)
	{
		DX8Wrapper::Set_Texture(stage, nullptr);
	}
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
	for (Int stage = 0; stage < stages; stage++)
	{
		DX8Wrapper::Set_DX8_Texture(stage, nullptr);
	}

	// The device is read because the wrapper's cache can hold a placeholder after an invalidate.
	DWORD depthTest = D3DZB_TRUE;
	DWORD stencil = FALSE;
	DWORD colorWrite = 0x0000000f;
	device->GetRenderState(D3DRS_ZENABLE, &depthTest);
	device->GetRenderState(D3DRS_STENCILENABLE, &stencil);
	device->GetRenderState(D3DRS_COLORWRITEENABLE, &colorWrite);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE, 0x0000000f);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGENABLE, FALSE);

	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_TEXCOORDINDEX, 0);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

	DX8Wrapper::Set_Pixel_Shader(Shader);
	const Vector4 encode(SOFTENING, ENCODE_SCALE, ENCODE_CENTRE, 0.0f);
	DX8Wrapper::Set_Pixel_Shader_Constant(1, &encode, 1);

	size_t next = 0;
	for (Int built = 0; next < Queue.size() && built < MAX_BUILDS_PER_FRAME; next++)
	{
		SlopeMapCache::iterator it = Cache.find(Queue[next]);
		if (it != Cache.end())
		{
			it->second.queued = FALSE;
			Build(device, it->first, it->second);
			built++;
		}
	}
	Queue.erase(Queue.begin(), Queue.begin() + next);

	DX8Wrapper::Set_Pixel_Shader(0);
	DX8Wrapper::Set_DX8_Texture(0, nullptr);
	DX8Wrapper::Set_Vertex_Buffer(nullptr);
	DX8Wrapper::Set_Index_Buffer(nullptr, 0);
	DX8Wrapper::Set_DX8_Render_Target_Surfaces(sceneTarget, sceneDepth);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, depthTest);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, stencil);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE, colorWrite);
	if (sceneDepth != nullptr)
	{
		sceneDepth->Release();
	}
	sceneTarget->Release();

	// Blend, cull and fog are ShaderClass state, so the next shader set restores them in full.
	ShaderClass::Invalidate();
	rinfo.Camera.Apply();
	DX8Wrapper::Apply_Render_State_Changes();
#else
	(void)rinfo;
#endif
}

void W3DSlopeMap::releaseResources()
{
#if defined(BUILD_WITH_D3D9)
	for (SlopeMapCache::iterator it = Cache.begin(); it != Cache.end(); ++it)
	{
		Release_Entry(it->second);
		it->first->Release_Ref();
	}
	Cache.clear();
	Queue.clear();

	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device != nullptr)
	{
		DX8_DELETE_PIXEL_SHADER(device, Shader);
	}
	Shader = 0;
	Format = D3DFMT_UNKNOWN;
	Acquired = FALSE;
	Failed = FALSE;
#endif
}
