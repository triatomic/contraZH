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

// W3DStorm.cpp ///////////////////////////////////////////////////////////////////////////////////
// Sandstorms and snowstorms drawn by storm.hlsl, as haze that fills a cylinder and grains inside it
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Lib/BaseType.h"
#include "WWLib/always.h"
#include "W3DDevice/GameClient/W3DStorm.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DWater.h"
#include "W3DDevice/GameClient/BaseHeightMap.h"
#include "Common/GameCommon.h"
#include "Common/GlobalData.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8caps.h"
#include "WW3D2/dx8vertexbuffer.h"
#include "WW3D2/dx8indexbuffer.h"
#include "WW3D2/dx8fvf.h"
#include "WW3D2/formconv.h"
#include "WW3D2/shader.h"
#include "WW3D2/sortingrenderer.h"
#include "WW3D2/texture.h"
#include "WW3D2/vertmaterial.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WW3D2/ww3d.h"

W3DStormManager *TheW3DStorms = nullptr;

// CONTRA_STORMSHADER bisects faults: 0 no storms, 1 storms.
static const Int StormShaderMode = (getenv("CONTRA_STORMSHADER") != nullptr) ? atoi(getenv("CONTRA_STORMSHADER")) : 1;

static const Int NOISE_SIZE = 128;

// Index counts are 16-bit, so one draw holds this many grains and a storm with more draws again.
static const Int GRAINS_PER_DRAW = 10000;
static const Int MAX_GRAIN_DRAWS = 4;

// Grains fill a square this wide around the middle of the view, or the whole storm when it is smaller.
static const Real GRAIN_TILE = 640.0f;

// A grain never draws smaller than this many pixels across.
static const Real MIN_GRAIN_PIXELS = 1.2f;

// The swirl's sines all come round after this many seconds, so the clock wraps there unseen.
static const double SWIRL_PERIOD = 320.0 * 3.14159265358979323846;

// The haze reaches this far past the heights sampled under the storm, for the hills between the samples.
static const Real GROUND_MARGIN = 10.0f;

// A wider storm spreads its height samples too thin to find the hills, so it takes the whole map's heights.
static const Real MAX_SAMPLED_RADIUS = 500.0f;

static const Int HEIGHT_STAGE = 1;
static const Int NOISE_STAGE = 2;

W3DStormManager::W3DStormManager()
	: m_count(0),
	  m_nextHandle(1),
	  m_lastUpdateMs(0),
	  m_loaded(FALSE),
	  m_hazeVertexShader(0),
	  m_hazePixelShader(0),
	  m_grainVertexShader(0),
	  m_grainPixelShader(0),
	  m_noise(nullptr),
	  m_grainVertices(nullptr),
	  m_grainIndices(nullptr)
{
}

W3DStormManager::~W3DStormManager()
{
	ReleaseResources();
}

void W3DStormManager::ReleaseResources()
{
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device != nullptr)
	{
		DX8_DELETE_VERTEX_SHADER(device, m_hazeVertexShader);
		DX8_DELETE_PIXEL_SHADER(device, m_hazePixelShader);
		DX8_DELETE_VERTEX_SHADER(device, m_grainVertexShader);
		DX8_DELETE_PIXEL_SHADER(device, m_grainPixelShader);
	}
	m_hazeVertexShader = 0;
	m_hazePixelShader = 0;
	m_grainVertexShader = 0;
	m_grainPixelShader = 0;
	m_loaded = FALSE;

	if (m_noise != nullptr)
	{
		m_noise->Release();
		m_noise = nullptr;
	}
	REF_PTR_RELEASE(m_grainVertices);
	REF_PTR_RELEASE(m_grainIndices);
}

W3DStormManager::Storm *W3DStormManager::find(Int handle)
{
	for (Int i = 0; i < m_count; i++)
	{
		if (m_storms[i].handle == handle)
		{
			return &m_storms[i];
		}
	}
	return nullptr;
}

Int W3DStormManager::add(const Coord3D &position, const StormShaderInfo &info, UnsignedInt durationMs)
{
	if (m_count == MAX_STORMS)
	{
		return 0;
	}

	Storm &storm = m_storms[m_count++];
	storm.own = info;
	info.resolve(storm.info);
	storm.handle = m_nextHandle++;
	storm.position = position;
	storm.startMs = WW3D::Get_Sync_Time();
	storm.durationMs = durationMs;
	storm.fadeMs = max(storm.info.fadeFrames * MSEC_PER_LOGICFRAME_REAL, 1.0f);
	storm.visible = TRUE;
	storm.shown = 1.0f;
	return storm.handle;
}

void W3DStormManager::move(Int handle, const Coord3D &position)
{
	Storm *storm = find(handle);
	if (storm != nullptr)
	{
		storm->position = position;
	}
}

void W3DStormManager::show(Int handle, Bool visible)
{
	Storm *storm = find(handle);
	if (storm != nullptr)
	{
		storm->visible = visible;
	}
}

void W3DStormManager::remove(Int handle)
{
	Storm *storm = find(handle);
	if (storm == nullptr)
	{
		return;
	}

	// The storm's end moves to one fade from now, unless it already comes sooner.
	const UnsignedInt end = WW3D::Get_Sync_Time() - storm->startMs + (UnsignedInt)storm->fadeMs;
	if (storm->durationMs == 0 || storm->durationMs > end)
	{
		storm->durationMs = max(end, 1u);
	}
}

void W3DStormManager::reset()
{
	m_count = 0;
}

Bool W3DStormManager::update()
{
	const UnsignedInt now = WW3D::Get_Sync_Time();
	const Real elapsedMs = (Real)min(now - m_lastUpdateMs, 1000u);
	m_lastUpdateMs = now;

	Int live = 0;
	for (Int i = 0; i < m_count; i++)
	{
		Storm &storm = m_storms[i];
		if (storm.durationMs != 0 && now - storm.startMs >= storm.durationMs)
		{
			continue;
		}

		storm.own.resolve(storm.info);
		storm.fadeMs = max(storm.info.fadeFrames * MSEC_PER_LOGICFRAME_REAL, 1.0f);

		const Real step = elapsedMs / storm.fadeMs;
		storm.shown = storm.visible ? min(storm.shown + step, 1.0f) : max(storm.shown - step, 0.0f);
		if (live != i)
		{
			m_storms[live] = storm;
		}
		live++;
	}
	m_count = live;
	return m_count > 0;
}

// 0 to 1: the storm builds up over its fade time, dies down over the same before its end, and follows show.
Real W3DStormManager::level(const Storm &storm, UnsignedInt now) const
{
	const Real age = (Real)(now - storm.startMs);
	Real level = min(age / storm.fadeMs, 1.0f);
	if (storm.durationMs != 0)
	{
		level = min(level, ((Real)storm.durationMs - age) / storm.fadeMs);
	}
	level = max(level, 0.0f) * storm.shown;
	return level * level * (3.0f - 2.0f * level);
}

#if defined(BUILD_WITH_D3D9)
// Vertex texture fetch support varies by format even on shader model 3 cards.
static Bool Supports_Vertex_Texture(D3DFORMAT format)
{
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	IDirect3D9 *d3d = nullptr;
	if (device == nullptr || FAILED(device->GetDirect3D(&d3d)))
	{
		return FALSE;
	}
	D3DDEVICE_CREATION_PARAMETERS params;
	D3DDISPLAYMODE mode;
	Bool supported = FALSE;
	if (SUCCEEDED(device->GetCreationParameters(&params)) && SUCCEEDED(d3d->GetAdapterDisplayMode(params.AdapterOrdinal, &mode)))
	{
		supported = SUCCEEDED(d3d->CheckDeviceFormat(params.AdapterOrdinal, params.DeviceType, mode.Format,
			D3DUSAGE_QUERY_VERTEXTEXTURE, D3DRTYPE_TEXTURE, format));
	}
	d3d->Release();
	return supported;
}
#endif

// Loaded on first use, once the device can say whether it runs them.
Bool W3DStormManager::loadShaders()
{
#if defined(BUILD_WITH_D3D9)
	if (!m_loaded)
	{
		m_loaded = TRUE;
		const DX8Caps *caps = DX8Wrapper::Get_Current_Caps();
		if (StormShaderMode != 0 && caps != nullptr && caps->Get_Vertex_Shader_Major_Version() >= 3 &&
			caps->Get_Pixel_Shader_Major_Version() >= 3 && Supports_Vertex_Texture(D3DFMT_A8R8G8B8))
		{
			// The draws keep their fixed-function vertex formats bound, so the declaration only has to be valid.
			DWORD declaration[] =
			{
				D3DVSD_STREAM(0),
				D3DVSD_REG(0, D3DVSDT_FLOAT3),
				D3DVSD_END()
			};
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\stormhaze.vso", declaration, 0, true, &m_hazeVertexShader)))
			{
				m_hazeVertexShader = 0;
			}
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\stormhaze.pso", nullptr, 0, false, &m_hazePixelShader)))
			{
				m_hazePixelShader = 0;
			}
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\stormgrain.vso", declaration, 0, true, &m_grainVertexShader)))
			{
				m_grainVertexShader = 0;
			}
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\stormgrain.pso", nullptr, 0, false, &m_grainPixelShader)))
			{
				m_grainPixelShader = 0;
			}
		}
	}
	return m_hazeVertexShader != 0 && m_hazePixelShader != 0 && m_grainVertexShader != 0 && m_grainPixelShader != 0;
#else
	return FALSE;
#endif
}

static UnsignedInt Hash_Lattice(Int x, Int y, Int seed)
{
	UnsignedInt h = (UnsignedInt)x * 374761393u + (UnsignedInt)y * 668265263u + (UnsignedInt)seed * 2246822519u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}

static Real Hash_Unit(Int x, Int y, Int seed)
{
	return (Hash_Lattice(x, y, seed) & 0xffff) / 65535.0f;
}

// Smooth value noise that tiles across the texture, with this many cells along each side.
static Real Value_Noise(Int x, Int y, Int cells, Int seed)
{
	const Real fx = (Real)(x * cells) / NOISE_SIZE;
	const Real fy = (Real)(y * cells) / NOISE_SIZE;
	const Int x0 = (Int)fx;
	const Int y0 = (Int)fy;
	const Int x1 = (x0 + 1) % cells;
	const Int y1 = (y0 + 1) % cells;
	Real tx = fx - x0;
	Real ty = fy - y0;
	tx = tx * tx * (3.0f - 2.0f * tx);
	ty = ty * ty * (3.0f - 2.0f * ty);

	const Real top = Hash_Unit(x0, y0, seed) + (Hash_Unit(x1, y0, seed) - Hash_Unit(x0, y0, seed)) * tx;
	const Real bottom = Hash_Unit(x0, y1, seed) + (Hash_Unit(x1, y1, seed) - Hash_Unit(x0, y1, seed)) * tx;
	return top + (bottom - top) * ty;
}

// Unrelated noise fields in red, green and blue, four octaves each, so the haze billows at every size.
void W3DStormManager::createNoise()
{
#if defined(BUILD_WITH_D3D9)
	m_noise = DX8Wrapper::_Create_DX8_Texture(NOISE_SIZE, NOISE_SIZE, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1, D3DPOOL_MANAGED, false);
	if (m_noise == nullptr)
	{
		return;
	}

	IDirect3DTexture8 *lockable = DX8Wrapper::_Peek_Lockable_Texture(m_noise);
	D3DLOCKED_RECT locked;
	if (FAILED(lockable->LockRect(0, &locked, nullptr, 0)))
	{
		m_noise->Release();
		m_noise = nullptr;
		return;
	}

	for (Int y = 0; y < NOISE_SIZE; y++)
	{
		UnsignedInt *row = (UnsignedInt *)((UnsignedByte *)locked.pBits + y * locked.Pitch);
		for (Int x = 0; x < NOISE_SIZE; x++)
		{
			UnsignedInt pixel = 0xff000000u;
			for (Int channel = 0; channel < 3; channel++)
			{
				Real value = 0.0f;
				Real weight = 0.5f;
				Int cells = 4;
				for (Int octave = 0; octave < 4; octave++)
				{
					value += weight * Value_Noise(x, y, cells, channel * 4 + octave);
					weight *= 0.5f;
					cells *= 2;
				}
				// The octaves' weights add up to 15/16.
				value *= 16.0f / 15.0f;
				pixel |= (UnsignedInt)(value * 255.0f + 0.5f) << (16 - channel * 8);
			}
			row[x] = pixel;
		}
	}

	lockable->UnlockRect(0);
	DX8Wrapper::_Upload_Lockable_Texture(m_noise);
#endif
}

// Each grain is a quad of four vertices that share a seed. The shader places them from the seed and the time.
Bool W3DStormManager::createGrains()
{
#if defined(BUILD_WITH_D3D9)
	m_grainVertices = NEW_REF(DX8VertexBufferClass, (DX8_FVF_XYZUV2, (unsigned short)(GRAINS_PER_DRAW * 4)));
	m_grainIndices = NEW_REF(DX8IndexBufferClass, ((unsigned short)(GRAINS_PER_DRAW * 6)));

	{
		static const Real corners[4][2] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { -1.0f, 1.0f }, { 1.0f, 1.0f } };
		DX8VertexBufferClass::WriteLockClass lock(m_grainVertices);
		VertexFormatXYZUV2 *vertex = (VertexFormatXYZUV2 *)lock.Get_Vertex_Array();
		for (Int grain = 0; grain < GRAINS_PER_DRAW; grain++)
		{
			for (Int corner = 0; corner < 4; corner++)
			{
				vertex->x = Hash_Unit(grain, 0, 101);
				vertex->y = Hash_Unit(grain, 1, 101);
				vertex->z = Hash_Unit(grain, 2, 101);
				vertex->u1 = corners[corner][0];
				vertex->v1 = corners[corner][1];
				vertex->u2 = Hash_Unit(grain, 3, 101);
				vertex->v2 = Hash_Unit(grain, 4, 101);
				vertex++;
			}
		}
	}

	{
		DX8IndexBufferClass::WriteLockClass lock(m_grainIndices);
		UnsignedShort *index = lock.Get_Index_Array();
		for (Int grain = 0; grain < GRAINS_PER_DRAW; grain++)
		{
			const UnsignedShort base = (UnsignedShort)(grain * 4);
			index[0] = base;
			index[1] = base + 1;
			index[2] = base + 2;
			index[3] = base + 2;
			index[4] = base + 1;
			index[5] = base + 3;
			index += 6;
		}
	}
	return TRUE;
#else
	return FALSE;
#endif
}

#if defined(BUILD_WITH_D3D9)
// What one storm needs for both of its draws this frame.
struct StormDraw
{
	const StormShaderInfo *info;
	Coord3D position;
	Real level;
	Real top;				///< highest the haze reaches
	Real bottom;		///< lowest the haze reaches
	Real rect[4];		///< the storm's place on screen in clip space: left, bottom, right, top
};

// The storm's box on screen, or the whole screen when the box reaches behind the camera. False when none of it shows.
static Bool Screen_Rect(const D3DMATRIX &clip, StormDraw &draw, Real radius)
{
	Real *rect = draw.rect;
	rect[0] = rect[1] = 1.0f;
	rect[2] = rect[3] = -1.0f;

	Bool behind = FALSE;
	for (Int corner = 0; corner < 8; corner++)
	{
		const Real x = draw.position.x + ((corner & 1) ? radius : -radius);
		const Real y = draw.position.y + ((corner & 2) ? radius : -radius);
		const Real z = (corner & 4) ? draw.top : draw.bottom;
		const Real clipX = x * clip.m[0][0] + y * clip.m[1][0] + z * clip.m[2][0] + clip.m[3][0];
		const Real clipY = x * clip.m[0][1] + y * clip.m[1][1] + z * clip.m[2][1] + clip.m[3][1];
		const Real clipW = x * clip.m[0][3] + y * clip.m[1][3] + z * clip.m[2][3] + clip.m[3][3];
		if (clipW < 0.01f)
		{
			behind = TRUE;
			break;
		}
		rect[0] = min(rect[0], clipX / clipW);
		rect[1] = min(rect[1], clipY / clipW);
		rect[2] = max(rect[2], clipX / clipW);
		rect[3] = max(rect[3], clipY / clipW);
	}

	if (behind)
	{
		rect[0] = rect[1] = -1.0f;
		rect[2] = rect[3] = 1.0f;
		return TRUE;
	}

	rect[0] = max(rect[0], -1.0f);
	rect[1] = max(rect[1], -1.0f);
	rect[2] = min(rect[2], 1.0f);
	rect[3] = min(rect[3], 1.0f);
	return rect[0] < rect[2] && rect[1] < rect[3];
}

// World travel per unit of clip-space w along the view ray through a point in clip space.
static Vector3 View_Ray(const D3DMATRIX &projection, const D3DMATRIX &toWorld, Real clipX, Real clipY)
{
	const Real cameraZ = 1.0f / projection.m[2][3];
	const Real cameraX = (clipX - cameraZ * projection.m[2][0]) / projection.m[0][0];
	const Real cameraY = (clipY - cameraZ * projection.m[2][1]) / projection.m[1][1];
	return Vector3(
		cameraX * toWorld.m[0][0] + cameraY * toWorld.m[1][0] + cameraZ * toWorld.m[2][0],
		cameraX * toWorld.m[0][1] + cameraY * toWorld.m[1][1] + cameraZ * toWorld.m[2][1],
		cameraX * toWorld.m[0][2] + cameraY * toWorld.m[1][2] + cameraZ * toWorld.m[2][2]);
}

// The sun and sky on the ground, so a storm darkens at night with everything else.
static Vector3 Storm_Light()
{
	const RGBColor &ambient = TheGlobalData->m_terrainAmbient[0];
	const RGBColor &diffuse = TheGlobalData->m_terrainDiffuse[0];
	return Vector3(min(ambient.red + diffuse.red, 1.0f), min(ambient.green + diffuse.green, 1.0f), min(ambient.blue + diffuse.blue, 1.0f));
}

static void Set_Sampler(IDirect3DDevice8 *device, DWORD sampler, IDirect3DBaseTexture8 *texture, DWORD address, DWORD filter)
{
	device->SetTexture(sampler, texture);
	device->SetSamplerState(sampler, D3DSAMP_ADDRESSU, address);
	device->SetSamplerState(sampler, D3DSAMP_ADDRESSV, address);
	device->SetSamplerState(sampler, D3DSAMP_MINFILTER, filter);
	device->SetSamplerState(sampler, D3DSAMP_MAGFILTER, filter);
	device->SetSamplerState(sampler, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
}

static void Set_Stage(IDirect3DDevice8 *device, Int stage, IDirect3DBaseTexture8 *texture, DWORD address, DWORD filter)
{
	device->SetTexture(stage, texture);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSU, address);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSV, address);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MINFILTER, filter);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MAGFILTER, filter);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MIPFILTER, D3DTEXF_NONE);
}
#endif

void W3DStormManager::render(RenderInfoClass &rinfo)
{
#if defined(BUILD_WITH_D3D9)
	if (!update() || !loadShaders())
	{
		return;
	}
	if (m_noise == nullptr)
	{
		createNoise();
		if (m_noise == nullptr)
		{
			return;
		}
	}
	if (m_grainVertices == nullptr && !createGrains())
	{
		return;
	}

	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	rinfo.Camera.Apply();

	// Apply defers the view to the next state flush, so it is read from the camera, not the device.
	const D3DMATRIX view = To_D3DMATRIX(rinfo.Camera.Get_View_Matrix());
	D3DMATRIX projection;
	device->GetTransform(D3DTS_PROJECTION, &projection);
	if (fabs(projection.m[2][3]) < 1.0e-6f || fabs(projection.m[0][0]) < 1.0e-6f || fabs(projection.m[1][1]) < 1.0e-6f)
	{
		return;
	}
	D3DMATRIX toWorld;
	float det;
	Invert_D3DMATRIX(toWorld, &det, view);
	const D3DMATRIX clip = view * projection;
	const Vector3 eye(toWorld.m[3][0], toWorld.m[3][1], toWorld.m[3][2]);

	// What each storm on screen draws with.
	const UnsignedInt now = WW3D::Get_Sync_Time();
	StormDraw draws[MAX_STORMS];
	Int drawCount = 0;
	for (Int i = 0; i < m_count; i++)
	{
		const Storm &storm = m_storms[i];
		StormDraw &draw = draws[drawCount];
		draw.info = &storm.info;
		draw.position = storm.position;
		draw.level = level(storm, now);
		if (draw.level <= 0.001f || storm.info.radius <= 0.0f || storm.info.height <= 0.0f)
		{
			continue;
		}

		// The haze follows the ground, so it reaches from the lowest ground under the storm to the storm's height over the highest.
		Real lowest = storm.position.z;
		Real highest = storm.position.z;
		if (TheTerrainRenderObject != nullptr && storm.info.radius > MAX_SAMPLED_RADIUS)
		{
			lowest = min(lowest, TheTerrainRenderObject->getMinHeight());
			highest = max(highest, TheTerrainRenderObject->getMaxHeight());
		}
		else if (TheTerrainRenderObject != nullptr)
		{
			for (Int sample = 0; sample < 25; sample++)
			{
				const Real x = storm.position.x + ((sample % 5) * 0.5f - 1.0f) * storm.info.radius;
				const Real y = storm.position.y + ((sample / 5) * 0.5f - 1.0f) * storm.info.radius;
				const Real ground = TheTerrainRenderObject->getHeightMapHeight(x, y, nullptr);
				lowest = min(lowest, ground);
				highest = max(highest, ground);
			}
		}
		draw.bottom = lowest - GROUND_MARGIN;
		draw.top = highest + storm.info.height + GROUND_MARGIN;

		if (Screen_Rect(clip, draw, storm.info.radius))
		{
			drawCount++;
		}
	}
	if (drawCount == 0)
	{
		return;
	}

	// The sorter still holds the particles and translucent meshes, which would draw over the storm.
	SortingRendererClass::Flush();

	// The haze stops at the scene's depth where that is the bound one, which leaves out reflections. Elsewhere it stops at the ground alone.
	IDirect3DTexture8 *depthTexture = DX8Wrapper::Peek_Scene_Depth_Texture();
	if (depthTexture != nullptr)
	{
		IDirect3DSurface8 *bound = nullptr;
		if (FAILED(device->GetDepthStencilSurface(&bound)) || bound == nullptr)
		{
			depthTexture = nullptr;
		}
		else
		{
			if (bound != DX8Wrapper::Peek_Scene_Depth_Surface())
			{
				depthTexture = nullptr;
			}
			bound->Release();
		}
	}
	Vector4 depthMap(0.0f, 0.0f, 0.0f, 0.0f);
	if (depthTexture != nullptr)
	{
		D3DSURFACE_DESC desc;
		depthTexture->GetLevelDesc(0, &desc);
		depthMap = W3DShaderManager::getClipToTargetMapping((Real)desc.Width, (Real)desc.Height);
	}

	Vector4 heightMap(0.0f, 0.0f, 0.0f, 0.0f);
	Vector4 heightDecode(0.0f, 0.0f, 0.0f, 0.0f);
	TextureClass *heightTexture = (TheWaterRenderObj != nullptr) ? TheWaterRenderObj->getTerrainHeightTexture(heightMap, heightDecode) : nullptr;
	IDirect3DBaseTexture8 *heights = (heightTexture != nullptr) ? heightTexture->Peek_D3D_Texture() : nullptr;
	heightDecode.Z = (heights != nullptr) ? 1.0f : 0.0f;

	const double seconds = now / 1000.0;
	const Vector3 light = Storm_Light();

	// Cleared through the wrapper, so its record of the stages matches the device once they are unbound below.
	DX8Wrapper::Set_Texture(0, nullptr);
	DX8Wrapper::Set_Texture(HEIGHT_STAGE, nullptr);
	DX8Wrapper::Set_Texture(NOISE_STAGE, nullptr);
	VertexMaterialClass *material = VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(material);
	REF_PTR_RELEASE(material);
	DX8Wrapper::Set_Shader(ShaderClass::_PresetAlphaSpriteShader);
	DX8Wrapper::Set_Transform(D3DTS_WORLD, Matrix3D(true));

	// No shader restores ZENABLE, so it is put back after.
	DWORD depthTest = D3DZB_TRUE;
	device->GetRenderState(D3DRS_ZENABLE, &depthTest);

	// The haze, one quad per storm over its place on screen.
	{
		DynamicVBAccessClass vbAccess(BUFFER_TYPE_DYNAMIC_DX8, dynamic_fvf_type, drawCount * 4);
		{
			DynamicVBAccessClass::WriteLockClass lock(&vbAccess);
			VertexFormatXYZNDUV2 *verts = lock.Get_Formatted_Vertex_Array();
			for (Int i = 0; i < drawCount; i++)
			{
				const Real *rect = draws[i].rect;
				for (Int corner = 0; corner < 4; corner++)
				{
					const Real clipX = (corner & 1) ? rect[2] : rect[0];
					const Real clipY = (corner & 2) ? rect[3] : rect[1];
					const Vector3 ray = View_Ray(projection, toWorld, clipX, clipY);
					verts->x = clipX;
					verts->y = clipY;
					verts->z = 0.5f;
					verts->nx = ray.X;
					verts->ny = ray.Y;
					verts->nz = ray.Z;
					verts->diffuse = 0xffffffffu;
					verts->u1 = clipX * depthMap.X + depthMap.Z;
					verts->v1 = clipY * depthMap.Y + depthMap.W;
					verts->u2 = 0.0f;
					verts->v2 = 0.0f;
					verts++;
				}
			}
		}

		DynamicIBAccessClass ibAccess(BUFFER_TYPE_DYNAMIC_DX8, drawCount * 6);
		{
			DynamicIBAccessClass::WriteLockClass lock(&ibAccess);
			UnsignedShort *indices = lock.Get_Index_Array();
			for (Int i = 0; i < drawCount; i++)
			{
				const UnsignedShort base = (UnsignedShort)(i * 4);
				indices[i * 6 + 0] = base + 0;
				indices[i * 6 + 1] = base + 1;
				indices[i * 6 + 2] = base + 2;
				indices[i * 6 + 3] = base + 2;
				indices[i * 6 + 4] = base + 1;
				indices[i * 6 + 5] = base + 3;
			}
		}

		DX8Wrapper::Set_Vertex_Buffer(vbAccess);
		DX8Wrapper::Set_Index_Buffer(ibAccess, 0);
		DX8Wrapper::Apply_Render_State_Changes();

		// The fixed-function vertex format stays bound and feeds the shader's inputs.
		device->SetVertexShader(Peek_D3D9_Vertex_Shader(m_hazeVertexShader));
		DX8Wrapper::Set_Pixel_Shader(m_hazePixelShader);
		Set_Stage(device, 0, depthTexture, D3DTADDRESS_CLAMP, D3DTEXF_POINT);
		Set_Stage(device, HEIGHT_STAGE, heights, D3DTADDRESS_CLAMP, D3DTEXF_LINEAR);
		Set_Stage(device, NOISE_STAGE, m_noise, D3DTADDRESS_WRAP, D3DTEXF_LINEAR);

		DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, FALSE);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE, FALSE);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_CULLMODE, D3DCULL_NONE);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, TRUE);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, FALSE);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGENABLE, FALSE);

		const Vector4 eyePosition(eye.X, eye.Y, eye.Z, 0.0f);
		const Vector4 linearize(projection.m[3][2], projection.m[3][3], projection.m[2][3], projection.m[2][2]);
		DX8Wrapper::Set_Pixel_Shader_Constant(0, &eyePosition, 1);
		DX8Wrapper::Set_Pixel_Shader_Constant(3, &linearize, 1);
		DX8Wrapper::Set_Pixel_Shader_Constant(6, &heightMap, 1);

		for (Int i = 0; i < drawCount; i++)
		{
			const StormDraw &draw = draws[i];
			const StormShaderInfo &info = *draw.info;
			const Real noiseScale = 1.0f / max(info.hazeNoiseSize, 1.0f);
			const Real drift = info.windSpeed * noiseScale;

			const Vector4 center(draw.position.x, draw.position.y, draw.position.z, info.radius);
			const Vector4 shape(1.0f / info.height, 1.0f / max(info.edgeFade, 0.01f), draw.top, draw.bottom);
			// The view depth the shader reads is clip-space w, which the projection's sign turns camera z into.
			const Vector4 params(projection.m[2][3], (depthTexture != nullptr) ? 1.0f : 0.0f, 1.0e6f, info.gusts);
			const Vector4 noise(noiseScale, (Real)fmod(seconds * drift * cos(info.windAngle), 1.0), (Real)fmod(seconds * drift * sin(info.windAngle), 1.0),
				info.hazeDensity * 0.01f * draw.level);
			const Vector4 decode(heightDecode.X, heightDecode.Y, heightDecode.Z, draw.position.z);
			const Vector4 color(info.hazeColor.red * light.X, info.hazeColor.green * light.Y, info.hazeColor.blue * light.Z, info.hazeMaxOpacity);
			DX8Wrapper::Set_Pixel_Shader_Constant(1, &center, 1);
			DX8Wrapper::Set_Pixel_Shader_Constant(2, &shape, 1);
			DX8Wrapper::Set_Pixel_Shader_Constant(4, &params, 1);
			DX8Wrapper::Set_Pixel_Shader_Constant(5, &noise, 1);
			DX8Wrapper::Set_Pixel_Shader_Constant(7, &decode, 1);
			DX8Wrapper::Set_Pixel_Shader_Constant(8, &color, 1);
			DX8Wrapper::Draw_Triangles(i * 6, 2, i * 4, 4);
		}

		device->SetTexture(0, nullptr);
		device->SetTexture(HEIGHT_STAGE, nullptr);
		device->SetTexture(NOISE_STAGE, nullptr);
	}

	// The grains, which the scene in front of them hides.
	{
		DX8Wrapper::Set_Vertex_Buffer(m_grainVertices);
		DX8Wrapper::Set_Index_Buffer(m_grainIndices, 0);
		DX8Wrapper::Apply_Render_State_Changes();

		device->SetVertexShader(Peek_D3D9_Vertex_Shader(m_grainVertexShader));
		DX8Wrapper::Set_Pixel_Shader(m_grainPixelShader);
		Set_Sampler(device, D3DVERTEXTEXTURESAMPLER0, heights, D3DTADDRESS_CLAMP, D3DTEXF_LINEAR);
		Set_Sampler(device, D3DVERTEXTEXTURESAMPLER1, m_noise, D3DTADDRESS_WRAP, D3DTEXF_LINEAR);

		DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, D3DZB_TRUE);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE, FALSE);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_CULLMODE, D3DCULL_NONE);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, TRUE);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, FALSE);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGENABLE, FALSE);

		float clipColumns[4][4];
		for (Int column = 0; column < 4; column++)
		{
			for (Int row = 0; row < 4; row++)
			{
				clipColumns[column][row] = clip.m[row][column];
			}
		}
		D3DVIEWPORT8 viewport;
		device->GetViewport(&viewport);
		const Real minHalfSize = 0.5f * MIN_GRAIN_PIXELS * 2.0f / (fabs(projection.m[1][1]) * max((Real)viewport.Height, 1.0f));
		const Vector4 eyePosition(eye.X, eye.Y, eye.Z, 0.0f);
		const Vector3 middleRay = View_Ray(projection, toWorld, 0.0f, 0.0f);
		DX8Wrapper::Set_Vertex_Shader_Constant(0, clipColumns, 4);
		DX8Wrapper::Set_Vertex_Shader_Constant(10, &eyePosition, 1);
		DX8Wrapper::Set_Vertex_Shader_Constant(11, &heightMap, 1);

		for (Int i = 0; i < drawCount; i++)
		{
			const StormDraw &draw = draws[i];
			const StormShaderInfo &info = *draw.info;
			if (info.grainCount <= 0 || info.grainOpacity <= 0.0f)
			{
				continue;
			}

			// The tile sits where the middle of the view meets the storm's half height, pulled inside the storm.
			const Real tile = min(GRAIN_TILE, 2.0f * info.radius);
			Real focusX = eye.X;
			Real focusY = eye.Y;
			if (fabs(middleRay.Z) > 1.0e-6f)
			{
				const Real reach = (draw.position.z + 0.5f * info.height - eye.Z) / middleRay.Z;
				if (reach > 0.0f)
				{
					focusX += middleRay.X * reach;
					focusY += middleRay.Y * reach;
				}
			}
			const Real offX = focusX - draw.position.x;
			const Real offY = focusY - draw.position.y;
			const Real off = sqrt(offX * offX + offY * offY);
			const Real slack = max(info.radius - 0.5f * tile, 0.0f);
			if (off > slack)
			{
				focusX = draw.position.x + offX * slack / off;
				focusY = draw.position.y + offY * slack / off;
			}

			const Real windX = info.windSpeed * cos(info.windAngle);
			const Real windY = info.windSpeed * sin(info.windAngle);
			const Real noiseScale = 1.0f / max(info.hazeNoiseSize, 1.0f);
			// Sand keeps to the ground, where the wind picks it up. Snow fills the storm's height evenly.
			const Real groundHold = (info.type == StormShaderInfo::TYPE_SAND) ? 2.0f : 1.0f;

			const Vector4 center(draw.position.x, draw.position.y, draw.position.z, info.radius);
			const Vector4 box(tile, info.height, 1.0f / max(info.edgeFade, 0.01f), info.grainOpacity * draw.level);
			const Vector4 focus(focusX, focusY, (Real)fmod(seconds, SWIRL_PERIOD), groundHold);
			// The shader multiplies the travel by 2, 3 or 4, so it is kept for a third of the speed and wraps whole.
			const Vector4 travel((Real)fmod(seconds * windX / 3.0, (double)tile), (Real)fmod(seconds * windY / 3.0, (double)tile),
				(Real)fmod(seconds * info.fallSpeed / 3.0 / info.height, 1.0), 0.0f);
			const Vector4 motion(windX, windY, info.fallSpeed, info.grainStreak);
			const Vector4 grain(0.5f * info.grainSize, info.turbulence, minHalfSize, info.gusts);
			const Vector4 decode(heightDecode.X, heightDecode.Y, heightDecode.Z, draw.position.z);
			const Vector4 color(info.grainColor.red * light.X, info.grainColor.green * light.Y, info.grainColor.blue * light.Z, 1.0f);
			const Vector4 gustNoise(noiseScale, (Real)fmod(seconds * windX * noiseScale, 1.0), (Real)fmod(seconds * windY * noiseScale, 1.0), 0.0f);
			DX8Wrapper::Set_Vertex_Shader_Constant(4, &center, 1);
			DX8Wrapper::Set_Vertex_Shader_Constant(5, &box, 1);
			DX8Wrapper::Set_Vertex_Shader_Constant(6, &focus, 1);
			DX8Wrapper::Set_Vertex_Shader_Constant(7, &travel, 1);
			DX8Wrapper::Set_Vertex_Shader_Constant(8, &motion, 1);
			DX8Wrapper::Set_Vertex_Shader_Constant(9, &grain, 1);
			DX8Wrapper::Set_Vertex_Shader_Constant(12, &decode, 1);
			DX8Wrapper::Set_Vertex_Shader_Constant(13, &color, 1);
			DX8Wrapper::Set_Vertex_Shader_Constant(15, &gustNoise, 1);

			Int left = min(info.grainCount, GRAINS_PER_DRAW * MAX_GRAIN_DRAWS);
			for (Int pass = 0; left > 0; pass++)
			{
				const Int grains = min(left, GRAINS_PER_DRAW);
				const Vector4 seedShift(pass * 0.37f, pass * 0.61f, pass * 0.83f, 0.0f);
				DX8Wrapper::Set_Vertex_Shader_Constant(14, &seedShift, 1);
				DX8Wrapper::Draw_Triangles(0, (unsigned short)(grains * 2), 0, (unsigned short)(grains * 4));
				left -= grains;
			}
		}

		device->SetTexture(D3DVERTEXTEXTURESAMPLER0, nullptr);
		device->SetTexture(D3DVERTEXTEXTURESAMPLER1, nullptr);
	}

	device->SetVertexShader(nullptr);
	DX8Wrapper::Set_Pixel_Shader(0);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, depthTest);
	DX8Wrapper::Set_Vertex_Buffer(nullptr);
	DX8Wrapper::Set_Index_Buffer(nullptr, 0);

	// Blend, cull, fog and the depth states are ShaderClass state, so the next shader set restores them.
	ShaderClass::Invalidate();
#endif
}
