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
// Fades particle sprites where they near the scene's depth or the terrain behind them, shades
// flame sprites as fire and electric sprites as arcs, and draws the heat haze behind flames
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Lib/BaseType.h"
#include "WWLib/always.h"
#include "W3DDevice/GameClient/W3DSoftParticles.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DWater.h"
#include "Common/GlobalData.h"
#include "GameClient/ParticleSys.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8caps.h"
#include "WW3D2/formconv.h"
#include "WW3D2/shader.h"
#include "WW3D2/texture.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WW3D2/ww3d.h"

W3DSoftParticles *TheW3DSoftParticles = nullptr;

// CONTRA_SOFTPARTICLES bisects faults: 0 hard, 1 scene depth where readable else terrain, 2 terrain only.
enum { SOFT_PARTICLES_OFF = 0, SOFT_PARTICLES_AUTO = 1, SOFT_PARTICLES_TERRAIN = 2 };

// CONTRA_FLAMESHADER bisects faults: 0 plain flames, 1 flame shading, 2 flame shading and heat haze.
enum { FLAME_SHADER_OFF = 0, FLAME_SHADER_SPRITES = 1, FLAME_SHADER_HAZE = 2 };

static Int Get_Env_Mode(const char *name, Int fallback)
{
	const char *value = getenv(name);
	return (value != nullptr) ? atoi(value) : fallback;
}

static const Int SoftParticleMode = Get_Env_Mode("CONTRA_SOFTPARTICLES", SOFT_PARTICLES_AUTO);
static const Int FlameShaderMode = Get_Env_Mode("CONTRA_FLAMESHADER", FLAME_SHADER_HAZE);

// CONTRA_ELECTRICSHADER bisects faults: 0 plain electric sprites, 1 electric shading.
static const Int ElectricShaderMode = Get_Env_Mode("CONTRA_ELECTRICSHADER", 1);

// The sprite's camera-space position comes through this stage, which also holds the surface it fades against.
static const Int SOFT_STAGE = 1;
static const Int NOISE_STAGE = 2;
static const Int SCENE_STAGE = 3;

static const Int NOISE_SIZE = 64;

W3DSoftParticles::W3DSoftParticles()
	: m_depthShader(0),
	  m_heightShader(0),
	  m_flameDepthShader(0),
	  m_flameHeightShader(0),
	  m_flameShader(0),
	  m_electricDepthShader(0),
	  m_electricHeightShader(0),
	  m_electricShader(0),
	  m_hazeShader(0),
	  m_noise(nullptr),
	  m_sceneCopy(nullptr),
	  m_loaded(FALSE),
	  m_bound(0)
{
	Set_D3DMATRIX_Identity(m_view);
	Set_D3DMATRIX_Identity(m_projection);
	Set_D3DMATRIX_Identity(m_toWorld);

	if (SoftParticleMode != SOFT_PARTICLES_OFF || FlameShaderMode != FLAME_SHADER_OFF || ElectricShaderMode != 0)
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

// The next draw makes them again, so a device made anew gets its own.
void W3DSoftParticles::ReleaseResources()
{
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device != nullptr)
	{
		DX8_DELETE_PIXEL_SHADER(device, m_depthShader);
		DX8_DELETE_PIXEL_SHADER(device, m_heightShader);
		DX8_DELETE_PIXEL_SHADER(device, m_flameDepthShader);
		DX8_DELETE_PIXEL_SHADER(device, m_flameHeightShader);
		DX8_DELETE_PIXEL_SHADER(device, m_flameShader);
		DX8_DELETE_PIXEL_SHADER(device, m_electricDepthShader);
		DX8_DELETE_PIXEL_SHADER(device, m_electricHeightShader);
		DX8_DELETE_PIXEL_SHADER(device, m_electricShader);
		DX8_DELETE_PIXEL_SHADER(device, m_hazeShader);
	}
	m_depthShader = 0;
	m_heightShader = 0;
	m_flameDepthShader = 0;
	m_flameHeightShader = 0;
	m_flameShader = 0;
	m_electricDepthShader = 0;
	m_electricHeightShader = 0;
	m_electricShader = 0;
	m_hazeShader = 0;

	if (m_noise != nullptr)
	{
		m_noise->Release();
		m_noise = nullptr;
	}
	if (m_sceneCopy != nullptr)
	{
		m_sceneCopy->Release();
		m_sceneCopy = nullptr;
	}
	m_loaded = FALSE;
}

void W3DSoftParticles::beginPass(RenderInfoClass &rinfo)
{
	rinfo.Camera.Apply();
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	device->GetTransform(D3DTS_VIEW, &m_view);
	device->GetTransform(D3DTS_PROJECTION, &m_projection);

	float det;
	Invert_D3DMATRIX(m_toWorld, &det, m_view);
}

Bool W3DSoftParticles::flameEnabled()
{
	if (FlameShaderMode == FLAME_SHADER_OFF || !TheGlobalData->m_useFlameShaders)
	{
		return FALSE;
	}
	loadShaders();
	return m_flameShader != 0 && m_noise != nullptr;
}

Bool W3DSoftParticles::electricEnabled()
{
	if (ElectricShaderMode == 0 || !TheGlobalData->m_useElectricShaders)
	{
		return FALSE;
	}
	loadShaders();
	return m_electricShader != 0 && m_noise != nullptr;
}

static void Load_Pixel_Shader(const char *path, DWORD &shader)
{
	if (FAILED(W3DShaderManager::LoadAndCreateD3DShader(path, nullptr, 0, false, &shader)))
	{
		shader = 0;
	}
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
			Load_Pixel_Shader("shaders\\softparticledepth.pso", m_depthShader);
			Load_Pixel_Shader("shaders\\softparticleheight.pso", m_heightShader);
			if (FlameShaderMode != FLAME_SHADER_OFF)
			{
				Load_Pixel_Shader("shaders\\softparticleflamedepth.pso", m_flameDepthShader);
				Load_Pixel_Shader("shaders\\softparticleflameheight.pso", m_flameHeightShader);
				Load_Pixel_Shader("shaders\\particleflame.pso", m_flameShader);
				Load_Pixel_Shader("shaders\\heathaze.pso", m_hazeShader);
			}
			if (ElectricShaderMode != 0)
			{
				Load_Pixel_Shader("shaders\\softparticleelectricdepth.pso", m_electricDepthShader);
				Load_Pixel_Shader("shaders\\softparticleelectricheight.pso", m_electricHeightShader);
				Load_Pixel_Shader("shaders\\particleelectric.pso", m_electricShader);
			}
			if (FlameShaderMode != FLAME_SHADER_OFF || ElectricShaderMode != 0)
			{
				createNoise();
			}
		}
	}
	return m_depthShader != 0 || m_heightShader != 0 || m_flameShader != 0 || m_electricShader != 0;
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

	const Real v00 = (Hash_Lattice(x0, y0, seed) & 0xffff) / 65535.0f;
	const Real v10 = (Hash_Lattice(x1, y0, seed) & 0xffff) / 65535.0f;
	const Real v01 = (Hash_Lattice(x0, y1, seed) & 0xffff) / 65535.0f;
	const Real v11 = (Hash_Lattice(x1, y1, seed) & 0xffff) / 65535.0f;
	const Real top = v00 + (v10 - v00) * tx;
	const Real bottom = v01 + (v11 - v01) * tx;
	return top + (bottom - top) * ty;
}

// Three unrelated noise fields, one per colour channel, two octaves each.
void W3DSoftParticles::createNoise()
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
				const Real value = 0.65f * Value_Noise(x, y, 8, channel) + 0.35f * Value_Noise(x, y, 16, channel + 3);
				pixel |= (UnsignedInt)(value * 255.0f + 0.5f) << (16 - channel * 8);
			}
			row[x] = pixel;
		}
	}

	lockable->UnlockRect(0);
	DX8Wrapper::_Upload_Lockable_Texture(m_noise);
#endif
}

// Camera space to clip space, then to a texture of the given size, at c0 to c3. Returns the mapping at c3.
Vector4 W3DSoftParticles::setClipConstants(Real width, Real height)
{
	const D3DMATRIX &p = m_projection;
	const Vector4 clipX(p.m[0][0], p.m[1][0], p.m[2][0], p.m[3][0]);
	const Vector4 clipY(p.m[0][1], p.m[1][1], p.m[2][1], p.m[3][1]);
	const Vector4 clipW(p.m[0][3], p.m[1][3], p.m[2][3], p.m[3][3]);
	const Vector4 screenMap = W3DShaderManager::getClipToTargetMapping(width, height);

	DX8Wrapper::Set_Pixel_Shader_Constant(0, &clipX, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(1, &clipY, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(2, &clipW, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(3, &screenMap, 1);
	return screenMap;
}

// Camera space back to world, one row per register from the first.
void W3DSoftParticles::setWorldConstants(Int firstRegister)
{
	const D3DMATRIX &w = m_toWorld;
	const Vector4 worldX(w.m[0][0], w.m[1][0], w.m[2][0], w.m[3][0]);
	const Vector4 worldY(w.m[0][1], w.m[1][1], w.m[2][1], w.m[3][1]);
	const Vector4 worldZ(w.m[0][2], w.m[1][2], w.m[2][2], w.m[3][2]);
	DX8Wrapper::Set_Pixel_Shader_Constant(firstRegister, &worldX, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(firstRegister + 1, &worldY, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(firstRegister + 2, &worldZ, 1);
}

// Only while the scene's depth is the bound one, which leaves out reflections and shadow maps.
// It stays bound as the depth buffer, which INTZ allows while depth writes are off.
Bool W3DSoftParticles::bindSceneDepth(DWORD shader)
{
#if defined(BUILD_WITH_D3D9)
	IDirect3DTexture8 *depthTexture = DX8Wrapper::Peek_Scene_Depth_Texture();
	if (shader == 0 || depthTexture == nullptr || SoftParticleMode != SOFT_PARTICLES_AUTO)
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
	const Vector4 linearize(p.m[3][2], p.m[3][3], p.m[2][3], p.m[2][2]);

	setClipConstants((Real)desc.Width, (Real)desc.Height);
	DX8Wrapper::Set_Pixel_Shader_Constant(4, &linearize, 1);

	device->SetTexture(SOFT_STAGE, depthTexture);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_MINFILTER, D3DTEXF_POINT);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_MAGFILTER, D3DTEXF_POINT);
	DX8Wrapper::Set_Pixel_Shader(shader);
	return TRUE;
#else
	return FALSE;
#endif
}

Bool W3DSoftParticles::bindTerrainHeight(DWORD shader)
{
#if defined(BUILD_WITH_D3D9)
	if (shader == 0 || TheWaterRenderObj == nullptr)
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
	setWorldConstants(0);
	DX8Wrapper::Set_Pixel_Shader_Constant(3, &heightMap, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(4, &heightDecode, 1);

	// Filtering each byte on its own still blends the heights linearly.
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(SOFT_STAGE, heightTexture->Peek_D3D_Texture());
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_Pixel_Shader(shader);
	return TRUE;
#else
	return FALSE;
#endif
}

// The shaders scroll this times 1, 1.3 or 1.4, all whole tiles at 10, so the wrap is seamless for any rate.
static Real Noise_Rise(Real tilesPerSecond)
{
	return (Real)fmod(WW3D::Get_Sync_Time() / 1000.0 * tilesPerSecond, 10.0);
}

static void Bind_Noise(IDirect3DTexture8 *noise)
{
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(NOISE_STAGE, noise);
	DX8Wrapper::Set_DX8_Texture_Stage_State(NOISE_STAGE, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(NOISE_STAGE, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(NOISE_STAGE, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(NOISE_STAGE, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(NOISE_STAGE, D3DTSS_MIPFILTER, D3DTEXF_NONE);
}

// The sprite's camera-space position reaches the pixel shader as TEXCOORD1.
static void Bind_Camera_Position()
{
	D3DMATRIX identity;
	Set_D3DMATRIX_Identity(identity);
	DX8Wrapper::_Set_DX8_Transform((D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + SOFT_STAGE), identity);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_MIPFILTER, D3DTEXF_NONE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
}

// World units across a noise tile, as the scale from world units to tiles.
static Real Noise_Scale(Real size)
{
	return 1.0f / ((size > 0.01f) ? size : 0.01f);
}

void W3DSoftParticles::bindFlame(const FlameShaderTuning &tuning)
{
	const Vector4 flame(Noise_Rise(tuning.rise), tuning.warp, Noise_Scale(tuning.noiseSize), tuning.heat);
	const Vector4 shape(tuning.flicker, tuning.breakup, 0.0f, 0.0f);
	DX8Wrapper::Set_Pixel_Shader_Constant(6, &flame, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(10, &shape, 1);
	setWorldConstants(7);
	Bind_Noise(m_noise);
}

// The field jumps to a fresh spot ElectricRate times a second, so arcs crackle rather than drift.
void W3DSoftParticles::bindElectric()
{
	const Real rate = TheGlobalData->m_electricRate;
	const Int jump = (rate > 0.0f) ? (Int)fmod(WW3D::Get_Sync_Time() / 1000.0 * rate, 65536.0) : 0;
	const Real offsetX = (Hash_Lattice(jump, 0, 7) & 0xffff) / 65536.0f;
	const Real offsetY = (Hash_Lattice(jump, 1, 7) & 0xffff) / 65536.0f;

	// The default camera's distance to the ground it looks at.
	const Real pitch = DEG_TO_RADF(max(TheGlobalData->m_cameraPitch, 10.0f));
	const Real cameraDistance = TheGlobalData->m_cameraHeight / sinf(pitch);

	const Vector4 electric(offsetX, offsetY, Noise_Scale(TheGlobalData->m_electricNoiseSize), TheGlobalData->m_electricJitter);
	const Vector4 shape(TheGlobalData->m_electricFlicker, TheGlobalData->m_electricArcSharpness, TheGlobalData->m_electricArcs, cameraDistance);
	DX8Wrapper::Set_Pixel_Shader_Constant(6, &electric, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(7, &shape, 1);
	Bind_Noise(m_noise);
}

Bool W3DSoftParticles::beginHaze()
{
	if (FlameShaderMode != FLAME_SHADER_HAZE || !TheGlobalData->m_useHeatEffects || !flameEnabled() || m_hazeShader == 0)
	{
		return FALSE;
	}
	return W3DShaderManager::copyRenderTarget(m_sceneCopy);
}

// The flame's shader tells the mask whether alpha counts. Fog is off because the scene copy already carries it.
Bool W3DSoftParticles::bindHaze(const ShaderClass &shader, const FlameShaderTuning &tuning)
{
#if defined(BUILD_WITH_D3D9)
	if (m_hazeShader == 0 || m_noise == nullptr || m_sceneCopy == nullptr)
	{
		return FALSE;
	}

	DX8Wrapper::Set_Texture(SOFT_STAGE, nullptr);
	DX8Wrapper::Set_Texture(NOISE_STAGE, nullptr);
	DX8Wrapper::Set_Texture(SCENE_STAGE, nullptr);
	DX8Wrapper::Apply_Render_State_Changes();

	D3DSURFACE_DESC desc;
	m_sceneCopy->GetLevelDesc(0, &desc);
	const Vector4 screenMap = setClipConstants((Real)desc.Width, (Real)desc.Height);
	setWorldConstants(7);

	// A world unit at unit depth, in scene uv. The shader divides by the sprite's depth.
	const Real bend = tuning.hazeBend * fabs(m_projection.m[0][0] * screenMap.X);
	const Vector4 haze(Noise_Rise(tuning.hazeRise), bend, Noise_Scale(tuning.hazeNoiseSize), tuning.hazeMask);
	DX8Wrapper::Set_Pixel_Shader_Constant(4, &haze, 1);

	const Bool addsColor = shader.Get_Src_Blend_Func() == ShaderClass::SRCBLEND_ONE;
	const Vector4 params(0.0f, addsColor ? 1.0f : 0.0f, 0.0f, 0.0f);
	DX8Wrapper::Set_Pixel_Shader_Constant(5, &params, 1);

	Bind_Noise(m_noise);

	DX8Wrapper::_Get_D3D_Device8()->SetTexture(SCENE_STAGE, m_sceneCopy);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SCENE_STAGE, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SCENE_STAGE, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SCENE_STAGE, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SCENE_STAGE, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SCENE_STAGE, D3DTSS_MIPFILTER, D3DTEXF_NONE);

	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, TRUE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGENABLE, FALSE);
	Bind_Camera_Position();
	DX8Wrapper::Set_Pixel_Shader(m_hazeShader);
	m_bound = EFFECT_HAZE;
	return TRUE;
#else
	return FALSE;
#endif
}

bool W3DSoftParticles::Begin(const ShaderClass &shader, unsigned effects, const void *effectData)
{
	m_bound = 0;
	FlameShaderTuning tuning;
	if ((effects & (EFFECT_FLAME | EFFECT_HAZE)) != 0)
	{
		ParticleSystemTemplate::resolveFlameTuning(static_cast<const ParticleSystemTemplate *>(effectData), tuning);
	}

	if ((effects & EFFECT_HAZE) != 0)
	{
		return bindHaze(shader, tuning) != FALSE;
	}

	const Bool soft = (effects & EFFECT_SOFT) != 0 && SoftParticleMode != SOFT_PARTICLES_OFF &&
		TheGlobalData->m_useSoftParticles && TheGlobalData->m_softParticleDistance > 0.0f;
	const Bool flame = (effects & EFFECT_FLAME) != 0 && flameEnabled();
	const Bool electric = !flame && (effects & EFFECT_ELECTRIC) != 0 && electricEnabled();
	if ((!soft && !flame && !electric) || !loadShaders())
	{
		return false;
	}

	// Cleared through the wrapper, so its record of the stages matches the device once End unbinds them.
	DX8Wrapper::Set_Texture(SOFT_STAGE, nullptr);
	if (flame || electric)
	{
		DX8Wrapper::Set_Texture(NOISE_STAGE, nullptr);
	}
	DX8Wrapper::Apply_Render_State_Changes();

	DWORD depthShader = m_depthShader;
	DWORD heightShader = m_heightShader;
	DWORD shadedShader = 0;
	if (flame)
	{
		depthShader = m_flameDepthShader;
		heightShader = m_flameHeightShader;
		shadedShader = m_flameShader;
	}
	else if (electric)
	{
		depthShader = m_electricDepthShader;
		heightShader = m_electricHeightShader;
		shadedShader = m_electricShader;
	}

	Bool bound = soft && (bindSceneDepth(depthShader) || bindTerrainHeight(heightShader));
	// Without the variants that also fade, shaded sprites keep their shading and lose the fade.
	if (!bound && shadedShader != 0)
	{
		DX8Wrapper::Set_Pixel_Shader(shadedShader);
		bound = TRUE;
	}
	if (!bound)
	{
		return false;
	}

	if (flame)
	{
		bindFlame(tuning);
		m_bound = EFFECT_FLAME;
	}
	else if (electric)
	{
		bindElectric();
		m_bound = EFFECT_ELECTRIC;
	}
	Bind_Camera_Position();

	// Alpha blending fades alpha alone, adding fades colour too, and multiplying fades towards white.
	const Bool addsColor = shader.Get_Src_Blend_Func() == ShaderClass::SRCBLEND_ONE;
	const Bool multiplies = shader.Get_Dst_Blend_Func() == ShaderClass::DSTBLEND_SRC_COLOR;
	// The sign that makes depth along the view grow away from the camera, for perspective and ortho alike.
	const Real forward = (m_projection.m[2][2] < 0.0f) ? -1.0f : 1.0f;
	const Real fadeRate = soft ? 1.0f / TheGlobalData->m_softParticleDistance : 0.0f;
	const Vector4 params(fadeRate, addsColor ? 1.0f : 0.0f, multiplies ? 1.0f : 0.0f, forward);
	DX8Wrapper::Set_Pixel_Shader_Constant(5, &params, 1);
	return true;
}

void W3DSoftParticles::End()
{
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	DX8Wrapper::Set_Pixel_Shader(0);
	device->SetTexture(SOFT_STAGE, nullptr);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SOFT_STAGE, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU | SOFT_STAGE);

	if (m_bound != 0)
	{
		device->SetTexture(NOISE_STAGE, nullptr);
	}
	if ((m_bound & EFFECT_HAZE) != 0)
	{
		device->SetTexture(SCENE_STAGE, nullptr);

		// Blend and fog are ShaderClass state, so the next shader set restores them.
		ShaderClass::Invalidate();
	}
	m_bound = 0;
}
