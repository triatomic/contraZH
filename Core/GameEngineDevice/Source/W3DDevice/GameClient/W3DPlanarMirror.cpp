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

// W3DPlanarMirror.cpp ////////////////////////////////////////////////////////////////////////////
// Level mirrors on the meshes a model's PlanarMirror keys pick out, drawn by planarmirror.hlsl
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Lib/BaseType.h"
#include "WWLib/always.h"
#include "W3DDevice/GameClient/W3DPlanarMirror.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DWater.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/W3DScene.h"
#include "W3DDevice/GameClient/W3DNoiseTexture.h"
#include "Common/GlobalData.h"
#include "Common/Debug.h"
#include "GameClient/View.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8caps.h"
#include "WW3D2/dx8polygonrenderer.h"
#include "WW3D2/dx8renderer.h"
#include "WW3D2/formconv.h"
#include "WW3D2/matpass.h"
#include "WW3D2/mesh.h"
#include "WW3D2/camera.h"
#include "WW3D2/shader.h"
#include "WW3D2/texture.h"
#include "WW3D2/surfaceclass.h"
#include "WW3D2/ww3d.h"
#include "WWMath/aabox.h"

W3DPlanarMirrorManager *TheW3DPlanarMirrors = nullptr;

// CONTRA_PLANARMIRROR bisects faults: 0 draws mirror meshes plain, 1 shades them.
static const Int PlanarMirrorMode = (getenv("CONTRA_PLANARMIRROR") != nullptr) ? atoi(getenv("CONTRA_PLANARMIRROR")) : 1;

// Mirrors this close in height share a plane.
static const Real PLANE_MERGE = 0.5f;
// A pixel this close to a plane takes its mirror whole, and fades to the skybox over the next PLANE_FADE.
static const Real PLANE_TOLERANCE = 0.75f;
static const Real PLANE_FADE = 1.0f;
// Lifting the clip plane a hair keeps the mirror itself out of the scene it reflects.
static const Real CLIP_LIFT = 0.05f;
// The normal map bends reads a little past a mirror's own patch of screen, so the mirrored scene draws that far around it.
static const Real READ_MARGIN = 0.05f;
// Full frost blurs over this share of the screen's width, and clouds this much of what shows through.
static const Real FROST_RADIUS = 0.012f;
static const Real FROST_CLOUD = 0.55f;
// The frost texture's size, and the world units its crystal grain repeats over.
static const Int FROST_SIZE = 128;
static const Real FROST_GRAIN_TILE = 24.0f;
// Out of every plane's reach.
static const Real NO_PLANE = -1.0e6f;

// Stages and samplers as planarmirror.hlsl reads them. The mirrors take MIRROR_STAGE and the one after.
static const Int NORMAL_STAGE = 3;
static const Int MIRROR_STAGE = 4;
static const Int SCENE_STAGE = 6;
static const DWORD FROST_SAMPLER = 13;

// Vogel spirals for the frost blur, two taps a register, as planarmirror.hlsl reads them from c13 and c16.
static const float FrostSpirals[9][4] =
{
	{ 0.289f, 0.000f, -0.369f, 0.338f }, { 0.056f, -0.643f, 0.465f, 0.606f }, { -0.853f, -0.151f, 0.808f, -0.514f },
	{ 0.204f, 0.000f, -0.261f, 0.239f }, { 0.040f, -0.455f, 0.329f, 0.429f }, { -0.603f, -0.107f, 0.571f, -0.363f },
	{ -0.191f, 0.711f, -0.364f, -0.702f }, { 0.791f, 0.289f, -0.822f, 0.339f }, { 0.396f, -0.847f, 0.293f, 0.934f }
};

// Lays one model's mirror look over its opaque meshes, or draws them as glass, with each polygon group's own texture.
class W3DPlanarMirrorPassClass : public MaterialPassClass
{
public:
	W3DPlanarMirrorPassClass(const PlanarMirrorShaderTuning *own, Bool glass) : m_own(own), m_glass(glass) {}

	virtual void Install_Materials() const override
	{
		TheW3DPlanarMirrors->beginOverlay(m_own, m_glass);
	}

	virtual void UnInstall_Materials() const override
	{
		TheW3DPlanarMirrors->endOverlay();
	}

	virtual void Install_Polygon_Materials(DX8PolygonRendererClass *renderer) const override
	{
		DX8TextureCategoryClass *category = renderer->Get_Texture_Category();
		if (category != nullptr)
		{
			TheW3DPlanarMirrors->setOverlayTexture(category->Peek_Texture(0), category->Get_Shader());
		}
	}

private:
	const PlanarMirrorShaderTuning *m_own;
	Bool m_glass;
};

static MaterialPassClass *Take_Mirror_Pass(MeshClass *mesh, bool sorted, bool &replaces)
{
	Bool glass = FALSE;
	MaterialPassClass *pass = (TheW3DPlanarMirrors != nullptr) ? TheW3DPlanarMirrors->takeMirrorPass(mesh, sorted, glass) : nullptr;
	replaces = (glass != FALSE);
	return pass;
}

// The texture's alpha outlines a mesh that tests or blends by it, and means nothing on the rest.
static Bool Draws_With_Alpha(const ShaderClass &shader)
{
	return shader.Get_Alpha_Test() == ShaderClass::ALPHATEST_ENABLE || shader.Get_Src_Blend_Func() == ShaderClass::SRCBLEND_SRC_ALPHA;
}

static void Set_Camera_Space_Texcoord(Int stage, DWORD source)
{
	D3DMATRIX identity;
	Set_D3DMATRIX_Identity(identity);
	DX8Wrapper::_Set_DX8_Transform((D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + stage), identity);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, source);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3);
}

static void Set_Clamped_Linear(Int stage)
{
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MIPFILTER, D3DTEXF_NONE);
}

W3DPlanarMirrorManager::W3DPlanarMirrorManager()
	: m_mirrorParams(0.0f, 0.0f, 0.0f, 0.0f),
	  m_depth(nullptr),
	  m_sceneCopy(nullptr),
	  m_white(nullptr),
	  m_frost(nullptr),
	  m_camera(nullptr),
	  m_overlayShader(0),
	  m_refractShader(0),
	  m_glassShader(0),
	  m_planeCount(0),
	  m_planeFrame(0),
	  m_loaded(FALSE),
	  m_sceneArmed(FALSE),
	  m_sceneCopied(FALSE),
	  m_overlayGlass(FALSE)
{
	m_tuning.setDefaults();
	for (Int i = 0; i < MAX_PLANES; i++)
	{
		m_mirrors[i] = nullptr;
		m_planeZ[i] = NO_PLANE;
	}
	MeshClass::Mirror_Pass_Hook = Take_Mirror_Pass;
}

W3DPlanarMirrorManager::~W3DPlanarMirrorManager()
{
	MeshClass::Mirror_Pass_Hook = nullptr;
	ReleaseResources();

	for (std::map<const PlanarMirrorShaderTuning *, MaterialPassClass *>::iterator it = m_passes.begin(); it != m_passes.end(); ++it)
	{
		REF_PTR_RELEASE(it->second);
	}
	m_passes.clear();
	REF_PTR_RELEASE(m_white);
	REF_PTR_RELEASE(m_camera);
}

void W3DPlanarMirrorManager::ReleaseResources()
{
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device != nullptr)
	{
		DX8_DELETE_PIXEL_SHADER(device, m_overlayShader);
		DX8_DELETE_PIXEL_SHADER(device, m_refractShader);
		DX8_DELETE_PIXEL_SHADER(device, m_glassShader);
	}
	m_overlayShader = 0;
	m_refractShader = 0;
	m_glassShader = 0;
	releaseTargets();
	SAFE_RELEASE(m_sceneCopy);
	REF_PTR_RELEASE(m_frost);
	m_planeCount = 0;
	m_loaded = FALSE;
}

// Built on first use. Its alpha is a fresh random angle at every texel, read one texel a pixel to turn the blur's taps.
TextureClass *W3DPlanarMirrorManager::findFrostTexture()
{
	if (m_frost == nullptr)
	{
		const Int count = FROST_SIZE * FROST_SIZE;
		std::vector<Real> scatterX(count);
		std::vector<Real> scatterY(count);
		std::vector<Real> patches(count);
		W3DNoiseTexture::buildLayer(&scatterX[0], FROST_SIZE, 16, 3, 0.55f, 0.0f, 71);
		W3DNoiseTexture::buildLayer(&scatterY[0], FROST_SIZE, 16, 3, 0.55f, 0.0f, 72);
		W3DNoiseTexture::buildLayer(&patches[0], FROST_SIZE, 4, 4, 0.6f, 0.0f, 73);

		std::vector<UnsignedInt> pixels(count);
		UnsignedInt seed = 0x9e3779b9u;
		for (Int i = 0; i < count; i++)
		{
			seed = seed * 1664525u + 1013904223u;
			pixels[i] = ((seed >> 24) << 24) |
				(W3DNoiseTexture::toByte(0.5f + 0.18f * scatterX[i]) << 16) |
				(W3DNoiseTexture::toByte(0.5f + 0.18f * scatterY[i]) << 8) |
				W3DNoiseTexture::toByte(0.5f + 0.2f * patches[i]);
		}
		m_frost = W3DNoiseTexture::createTexture(&pixels[0], FROST_SIZE);
	}
	return m_frost;
}

// The water reflection option turns the mirrors off with the water's, and their meshes draw plain.
Bool W3DPlanarMirrorManager::isAvailable()
{
	return PlanarMirrorMode != 0 && TheGlobalData->m_planarMirrorShader && TheGlobalData->m_waterReflections && loadShaders();
}

// Loaded on first use, once the device can say whether it runs them. Both variants need ps_2_a.
Bool W3DPlanarMirrorManager::loadShaders()
{
#if defined(BUILD_WITH_D3D9)
	if (!m_loaded)
	{
		m_loaded = TRUE;
		const DX8Caps *caps = DX8Wrapper::Get_Current_Caps();
		const Bool ps2a = caps != nullptr && caps->Get_Pixel_Shader_Major_Version() >= 2 &&
			(caps->Get_DX8_Caps().PS20Caps.Caps & D3DPS20CAPS_GRADIENTINSTRUCTIONS) != 0 &&
			caps->Get_DX8_Caps().PS20Caps.NumTemps >= 22 && caps->Get_DX8_Caps().PS20Caps.NumInstructionSlots >= 512;
		if (ps2a)
		{
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\planarmirror.pso", nullptr, 0, false, &m_overlayShader)))
			{
				m_overlayShader = 0;
			}
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\planarmirrorrefract.pso", nullptr, 0, false, &m_refractShader)))
			{
				m_refractShader = 0;
			}
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\planarmirrorglass.pso", nullptr, 0, false, &m_glassShader)))
			{
				m_glassShader = 0;
			}
		}
		RENDER_LOG(("W3DPlanarMirror: ps_2_a %s, overlay shader %s, refraction shader %s, glass shader %s", ps2a ? "yes" : "no",
			(m_overlayShader != 0) ? "loaded" : "missing", (m_refractShader != 0) ? "loaded" : "missing", (m_glassShader != 0) ? "loaded" : "missing"));

		// A null texture does not read as white in a pixel shader, so the empty stages get this one.
		if (m_white == nullptr)
		{
			m_white = MSGNEW("TextureClass") TextureClass(1, 1, WW3D_FORMAT_A4R4G4B4, MIP_LEVELS_1);
			SurfaceClass *surface = m_white->Get_Surface_Level();
			int pitch;
			void *bits = surface->Lock(&pitch);
			surface->Draw_Pixel(0, 0, 0xffffffff, surface->Get_Bytes_Per_Pixel(), bits, pitch);
			surface->Unlock();
			REF_PTR_RELEASE(surface);
		}
	}
	return m_overlayShader != 0 && m_refractShader != 0;
#else
	return FALSE;
#endif
}

void W3DPlanarMirrorManager::beginScenePass(Bool fresh)
{
	m_sceneArmed = TRUE;
	if (fresh)
	{
		m_sceneCopied = FALSE;
	}
}

Bool W3DPlanarMirrorManager::suspendScenePass()
{
	const Bool armed = m_sceneArmed;
	m_sceneArmed = FALSE;
	return armed;
}

// A mirror's surface is the top of its bounds.
void W3DPlanarMirrorManager::noteSighting(MeshClass *mesh)
{
	const AABoxClass &box = mesh->Get_Bounding_Box();
	const Real z = box.Center.Z + box.Extent.Z;
	const Real minX = box.Center.X - box.Extent.X;
	const Real maxX = box.Center.X + box.Extent.X;
	const Real minY = box.Center.Y - box.Extent.Y;
	const Real maxY = box.Center.Y + box.Extent.Y;

	for (size_t i = 0; i < m_sightings.size(); i++)
	{
		Sighting &sighting = m_sightings[i];
		if (fabs(sighting.z - z) <= PLANE_MERGE)
		{
			sighting.z = max(sighting.z, z);
			sighting.minX = min(sighting.minX, minX);
			sighting.minY = min(sighting.minY, minY);
			sighting.maxX = max(sighting.maxX, maxX);
			sighting.maxY = max(sighting.maxY, maxY);
			return;
		}
	}

	if (m_sightings.size() < MAX_SIGHTINGS)
	{
		Sighting sighting;
		sighting.z = z;
		sighting.minX = minX;
		sighting.minY = minY;
		sighting.maxX = maxX;
		sighting.maxY = maxY;
		m_sightings.push_back(sighting);
	}
}

MaterialPassClass *W3DPlanarMirrorManager::takeMirrorPass(MeshClass *mesh, Bool sorted, Bool &replaces)
{
	replaces = FALSE;

	// A fading object draws alone through the translucent list, which a pass at depth EQUAL would double.
	if (!m_sceneArmed || mesh->Get_Alpha_Override() != 1.0f || !isAvailable())
	{
		return nullptr;
	}

	noteSighting(mesh);
	if (sorted)
	{
		return nullptr;
	}

	// The settings decide the pass, so one model's meshes all take the same.
	const PlanarMirrorShaderTuning *own = static_cast<const PlanarMirrorShaderTuning *>(mesh->Peek_Shader_Effect_Data());
	const Bool glass = own != nullptr && own->overrideTexture && m_glassShader != 0;
	replaces = glass;
	std::map<const PlanarMirrorShaderTuning *, MaterialPassClass *>::iterator it = m_passes.find(own);
	if (it != m_passes.end())
	{
		return it->second;
	}
	MaterialPassClass *pass = NEW_REF(W3DPlanarMirrorPassClass, (own, glass));
	m_passes[own] = pass;
	return pass;
}

// One copy serves the scene pass. Glass comes after the opaque scene and sorted draws come far to near, so it holds what lies behind the first mirror.
Bool W3DPlanarMirrorManager::takeSceneCopy()
{
	if (!m_sceneCopied)
	{
		m_sceneCopied = TRUE;
		if (!W3DShaderManager::copyRenderTarget(m_sceneCopy))
		{
			SAFE_RELEASE(m_sceneCopy);
		}
	}
	return m_sceneCopy != nullptr;
}

Bool W3DPlanarMirrorManager::ensureTargets(UnsignedInt width, UnsignedInt height)
{
#if defined(BUILD_WITH_D3D9)
	if (m_mirrors[0] != nullptr)
	{
		D3DSURFACE_DESC desc;
		m_mirrors[0]->GetLevelDesc(0, &desc);
		if (desc.Width == width && desc.Height == height)
		{
			return TRUE;
		}
		releaseTargets();
	}

	// The planes share one depth buffer, kept clear of multisampling.
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	for (Int i = 0; i < MAX_PLANES; i++)
	{
		if (FAILED(device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_mirrors[i], nullptr)))
		{
			m_mirrors[i] = nullptr;
		}
	}
	if (FAILED(device->CreateDepthStencilSurface(width, height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &m_depth, nullptr)))
	{
		m_depth = nullptr;
	}
	if (m_mirrors[0] == nullptr || m_mirrors[1] == nullptr || m_depth == nullptr)
	{
		releaseTargets();
		return FALSE;
	}
	return TRUE;
#else
	(void)width;
	(void)height;
	return FALSE;
#endif
}

void W3DPlanarMirrorManager::releaseTargets()
{
	for (Int i = 0; i < MAX_PLANES; i++)
	{
		SAFE_RELEASE(m_mirrors[i]);
	}
	SAFE_RELEASE(m_depth);
}

void W3DPlanarMirrorManager::renderReflections(CameraClass *camera)
{
#if defined(BUILD_WITH_D3D9) && RTS_ZEROHOUR
	m_planeCount = 0;

	// The coming main scene notes its mirrors afresh.
	std::vector<Sighting> sightings;
	sightings.swap(m_sightings);
	if (sightings.empty() || camera == nullptr || TheTacticalView == nullptr || W3DDisplay::m_3DScene == nullptr ||
		DX8Wrapper::Is_Render_To_Texture() || !isAvailable())
	{
		return;
	}

	// The oblique near plane degrades as the view flattens, so a camera looking along the ground keeps the skybox.
	const Matrix3D &transform = camera->Get_Transform();
	const Vector3 eye = transform.Get_Translation();
	const Vector3 forward = -transform.Get_Z_Vector();
	if (forward.Z > -0.09f)
	{
		return;
	}

	// The mirrors nearest where the view looks take the planes.
	Int chosen[MAX_PLANES];
	Real chosenDistance[MAX_PLANES];
	for (size_t i = 0; i < sightings.size(); i++)
	{
		const Sighting &sighting = sightings[i];
		if (eye.Z <= sighting.z + 1.0f)
		{
			continue;
		}

		const Real along = (sighting.z - eye.Z) / forward.Z;
		const Real hitX = eye.X + forward.X * along;
		const Real hitY = eye.Y + forward.Y * along;
		const Real dx = max(max(sighting.minX - hitX, hitX - sighting.maxX), 0.0f);
		const Real dy = max(max(sighting.minY - hitY, hitY - sighting.maxY), 0.0f);
		const Real distance = dx * dx + dy * dy;

		Int slot = m_planeCount;
		while (slot > 0 && distance < chosenDistance[slot - 1])
		{
			if (slot < MAX_PLANES)
			{
				chosen[slot] = chosen[slot - 1];
				chosenDistance[slot] = chosenDistance[slot - 1];
			}
			slot--;
		}
		if (slot < MAX_PLANES)
		{
			chosen[slot] = (Int)i;
			chosenDistance[slot] = distance;
			m_planeCount = min(m_planeCount + 1, (Int)MAX_PLANES);
		}
	}
	if (m_planeCount == 0)
	{
		return;
	}

	IDirect3DSurface8 *target = nullptr;
	if (FAILED(DX8Wrapper::_Get_D3D_Device8()->GetRenderTarget(0, &target)))
	{
		m_planeCount = 0;
		return;
	}
	D3DSURFACE_DESC targetDesc;
	target->GetDesc(&targetDesc);
	target->Release();

	const UnsignedInt width = max(targetDesc.Width / 2, 1u);
	const UnsignedInt height = max(targetDesc.Height / 2, 1u);
	if (!ensureTargets(width, height))
	{
		m_planeCount = 0;
		return;
	}

	if (m_camera == nullptr)
	{
		m_camera = NEW_REF(CameraClass, ());
	}
	for (Int plane = 0; plane < m_planeCount; plane++)
	{
		renderPlane(camera, plane, sightings[chosen[plane]], width, height);
	}
	m_planeFrame = WW3D::Get_Frame_Count();

	// Sampled rather than every frame, so a whole match stays readable.
	static Int passCount = 0;
	if (passCount % 300 == 0 && passCount <= 300 * 15)
	{
		RENDER_LOG(("W3DPlanarMirror: pass %d saw %d heights, mirrored %.1f and %.1f", passCount, (Int)sightings.size(),
			m_planeZ[0], (m_planeCount > 1) ? m_planeZ[1] : NO_PLANE));
	}
	++passCount;
#else
	(void)camera;
#endif
}

void W3DPlanarMirrorManager::renderPlane(CameraClass *camera, Int plane, const Sighting &sighting, UnsignedInt width, UnsignedInt height)
{
#if defined(BUILD_WITH_D3D9) && RTS_ZEROHOUR
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DSurface8 *surface = nullptr;
	if (FAILED(m_mirrors[plane]->GetSurfaceLevel(0, &surface)))
	{
		m_planeZ[plane] = NO_PLANE;
		return;
	}

	WaterRenderObjClass::reflectCamera(m_camera, camera, sighting.z, sighting.z + CLIP_LIFT);

	// Only the mirrors' patch of the view reads the target, so the scene draws there alone, and culls to it.
	Bool bounded = TRUE;
	Vector2 readMin(1.0f, 1.0f);
	Vector2 readMax(0.0f, 0.0f);
	for (Int corner = 0; corner < 4 && bounded; corner++)
	{
		const Vector3 point((corner & 1) ? sighting.maxX : sighting.minX, (corner & 2) ? sighting.maxY : sighting.minY, sighting.z);
		bounded = WaterRenderObjClass::widenReadRect(camera, point, readMin, readMax);
	}
	readMin.Set(max(readMin.X - READ_MARGIN, 0.0f), max(readMin.Y - READ_MARGIN, 0.0f));
	readMax.Set(min(readMax.X + READ_MARGIN, 1.0f), min(readMax.Y + READ_MARGIN, 1.0f));

	for (Int i = 0; i < MAX_PLANES; i++)
	{
		device->SetTexture(MIRROR_STAGE + i, nullptr);
	}
	DX8Wrapper::Set_Render_Target(surface, m_depth);
	surface->Release();
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, FALSE);
	DX8Wrapper::Clear(true, true, Vector3(0.0f, 0.0f, 0.0f), 0.0f, 1.0f, 0);
	if (bounded)
	{
		WaterRenderObjClass::narrowReflectionCamera(m_camera, camera, readMin, readMax);
	}

	WaterRenderObjClass::renderMirroredScene(W3DDisplay::m_3DScene, m_camera, camera, sighting.z, width, height);

	m_planeZ[plane] = sighting.z;
#else
	(void)camera;
	(void)plane;
	(void)sighting;
	(void)width;
	(void)height;
#endif
}

// What every variant shares, over the draw's own applied state and transforms. The mesh texture stays on stage 0.
// The flag is c8's w: an adding mesh for the refraction, a mesh drawn with alpha for glass.
void W3DPlanarMirrorManager::bindShading(const PlanarMirrorShaderTuning *own, DWORD shader, Bool refract, Bool flag)
{
#if defined(BUILD_WITH_D3D9)
	if (own != nullptr)
	{
		own->resolve(m_tuning);
	}
	else
	{
		m_tuning = TheGlobalData->m_planarMirrorTuning;
	}

	// Texture coordinates arrive packed by textured, enabled stage, so stages 1 and 2 hold the white texture.
	// The bare textures are cleared through the wrapper first, so its record of the stages matches the device once they go.
	DX8Wrapper::Set_Texture(1, m_white);
	DX8Wrapper::Set_Texture(2, m_white);
	DX8Wrapper::Set_Texture(NORMAL_STAGE, m_white);
	DX8Wrapper::Set_Texture(MIRROR_STAGE, nullptr);
	DX8Wrapper::Set_Texture(MIRROR_STAGE + 1, nullptr);
	DX8Wrapper::Set_Texture(SCENE_STAGE, nullptr);
	DX8Wrapper::Apply_Render_State_Changes();

	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	D3DMATRIX view;
	D3DMATRIX projection;
	D3DMATRIX toWorld;
	float det;
	device->GetTransform(D3DTS_VIEW, &view);
	device->GetTransform(D3DTS_PROJECTION, &projection);
	Invert_D3DMATRIX(toWorld, &det, view);

	IDirect3DSurface8 *target = nullptr;
	D3DSURFACE_DESC targetDesc;
	targetDesc.Width = 1;
	targetDesc.Height = 1;
	if (SUCCEEDED(device->GetRenderTarget(0, &target)))
	{
		target->GetDesc(&targetDesc);
		target->Release();
	}

	const D3DMATRIX &w = toWorld;
	const D3DMATRIX &p = projection;
	const Vector4 rows[7] =
	{
		Vector4(w.m[0][0], w.m[1][0], w.m[2][0], w.m[3][0]),
		Vector4(w.m[0][1], w.m[1][1], w.m[2][1], w.m[3][1]),
		Vector4(w.m[0][2], w.m[1][2], w.m[2][2], w.m[3][2]),
		Vector4(p.m[0][0], p.m[1][0], p.m[2][0], p.m[3][0]),
		Vector4(p.m[0][1], p.m[1][1], p.m[2][1], p.m[3][1]),
		Vector4(p.m[0][3], p.m[1][3], p.m[2][3], p.m[3][3]),
		W3DShaderManager::getClipToTargetMapping((Real)targetDesc.Width, (Real)targetDesc.Height)
	};
	DX8Wrapper::Set_Pixel_Shader_Constant(0, rows, 7);

	// The planes belong to this frame, and a missing one lies out of every pixel's reach.
	const Int planes = (m_planeFrame == WW3D::Get_Frame_Count()) ? m_planeCount : 0;
	const Vector4 planeHeights((planes > 0) ? m_planeZ[0] : NO_PLANE, (planes > 1) ? m_planeZ[1] : NO_PLANE, 1.0f / PLANE_FADE, PLANE_TOLERANCE);
	DX8Wrapper::Set_Pixel_Shader_Constant(7, &planeHeights, 1);

	const Real reflectivity = WWMath::Clamp(m_tuning.reflectivity);
	m_mirrorParams.Set(reflectivity, 1.0f - reflectivity, 0.0f, flag ? 1.0f : 0.0f);
	DX8Wrapper::Set_Pixel_Shader_Constant(8, &m_mirrorParams, 1);
	const Vector4 tint(m_tuning.tint.red, m_tuning.tint.green, m_tuning.tint.blue, 0.0f);
	DX8Wrapper::Set_Pixel_Shader_Constant(9, &tint, 1);

	// The map's whole light colours the skybox, as on the water.
	const RGBColor &sunDiffuse = TheGlobalData->m_terrainDiffuse[0];
	const RGBColor &ambient = TheGlobalData->m_terrainAmbient[0];
	const Vector4 skyTint(WWMath::Clamp(ambient.red + sunDiffuse.red), WWMath::Clamp(ambient.green + sunDiffuse.green),
		WWMath::Clamp(ambient.blue + sunDiffuse.blue), 0.0f);
	DX8Wrapper::Set_Pixel_Shader_Constant(10, &skyTint, 1);

	// The blur's height in uv matches its width in pixels.
	const Real frost = WWMath::Clamp(m_tuning.frost);
	const Real radius = frost * FROST_RADIUS;
	const Vector4 frostParams(radius, radius * targetDesc.Width / max(targetDesc.Height, 1u), frost * FROST_CLOUD, 1.0f / FROST_GRAIN_TILE);
	const Vector4 frostStep((Real)targetDesc.Width / FROST_SIZE, (Real)targetDesc.Height / FROST_SIZE, 0.0f, 0.0f);
	DX8Wrapper::Set_Pixel_Shader_Constant(11, &frostParams, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(12, &frostStep, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(13, FrostSpirals, 9);

	// A pixel shader alone reads this sampler, past the fixed-function stages.
	TextureClass *frostTexture = findFrostTexture();
	device->SetTexture(FROST_SAMPLER, (frostTexture != nullptr) ? frostTexture->Peek_D3D_Texture() : m_white->Peek_D3D_Texture());
	device->SetSamplerState(FROST_SAMPLER, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	device->SetSamplerState(FROST_SAMPLER, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	device->SetSamplerState(FROST_SAMPLER, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	device->SetSamplerState(FROST_SAMPLER, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	device->SetSamplerState(FROST_SAMPLER, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	IDirect3DTexture8 *white = m_white->Peek_D3D_Texture();
	for (Int i = 0; i < MAX_PLANES; i++)
	{
		device->SetTexture(MIRROR_STAGE + i, (i < planes) ? m_mirrors[i] : white);
		Set_Clamped_Linear(MIRROR_STAGE + i);
	}
	if (refract)
	{
		device->SetTexture(SCENE_STAGE, (m_sceneCopy != nullptr) ? m_sceneCopy : white);
		Set_Clamped_Linear(SCENE_STAGE);
	}

	// The water object comes up with the mirrors, and planarmirror.hlsl reads the skybox on its samplers.
	TheWaterRenderObj->bindSkyboxFaces();

	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_TEXCOORDINDEX, 0);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	Set_Camera_Space_Texcoord(1, D3DTSS_TCI_CAMERASPACENORMAL);
	Set_Camera_Space_Texcoord(2, D3DTSS_TCI_CAMERASPACEPOSITION);
	for (Int stage = 1; stage <= 2; stage++)
	{
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_COLORARG2, D3DTA_CURRENT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
	}
	for (Int stage = NORMAL_STAGE; stage <= SCENE_STAGE; stage++)
	{
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, 0);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	}

	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGENABLE, FALSE);
	DX8Wrapper::Set_Pixel_Shader(shader);
#else
	(void)own;
	(void)shader;
	(void)refract;
	(void)flag;
#endif
}

// A texture without a normal map leaves the mirror unbent.
void W3DPlanarMirrorManager::bindNormalMap(TextureClass *texture)
{
	TextureClass *normalMap = (texture != nullptr) ? W3DShaderManager::findNormalMap(texture) : nullptr;
	DX8Wrapper::Set_Texture(NORMAL_STAGE, (normalMap != nullptr) ? normalMap : m_white);
	m_mirrorParams.Z = (normalMap != nullptr) ? m_tuning.distortion : 0.0f;
	DX8Wrapper::Set_Pixel_Shader_Constant(8, &m_mirrorParams, 1);
}

void W3DPlanarMirrorManager::unbindShading(Bool refract)
{
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	DX8Wrapper::Set_Pixel_Shader(0);
	for (Int i = 0; i < MAX_PLANES; i++)
	{
		device->SetTexture(MIRROR_STAGE + i, nullptr);
	}
	if (refract)
	{
		device->SetTexture(SCENE_STAGE, nullptr);
	}
	DX8Wrapper::Set_Texture(1, nullptr);
	DX8Wrapper::Set_Texture(2, nullptr);
	DX8Wrapper::Set_Texture(NORMAL_STAGE, nullptr);

	for (Int stage = 1; stage <= 2; stage++)
	{
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_COLOROP, D3DTOP_DISABLE);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	}
	for (Int stage = 1; stage <= SCENE_STAGE; stage++)
	{
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU | stage);
	}

	// Z, blend, fog and the first stages' ops are ShaderClass state, so the next shader set restores them in full.
	ShaderClass::Invalidate();
}

// An overlay redraws the mesh at the depth its own draw wrote, so EQUAL limits it to those pixels. Glass draws in place of
// the mesh, over the scene behind it, and writes depth as the mesh would have.
void W3DPlanarMirrorManager::beginOverlay(const PlanarMirrorShaderTuning *own, Bool glass)
{
	// Without a scene copy, glass still covers the mesh's place, over white.
	m_overlayGlass = glass;
	if (glass)
	{
		takeSceneCopy();
	}
	bindShading(own, m_overlayGlass ? m_glassShader : m_overlayShader, m_overlayGlass, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, TRUE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC, m_overlayGlass ? D3DCMP_LESSEQUAL : D3DCMP_EQUAL);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE, m_overlayGlass);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, TRUE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
}

void W3DPlanarMirrorManager::setOverlayTexture(TextureClass *texture, const ShaderClass &shader)
{
	DX8Wrapper::Set_Texture(0, texture);
	m_mirrorParams.W = (m_overlayGlass && Draws_With_Alpha(shader)) ? 1.0f : 0.0f;
	bindNormalMap(texture);
	if (m_overlayGlass)
	{
		DX8Wrapper::Set_DX8_Render_State(D3DRS_CULLMODE, (shader.Get_Cull_Mode() == ShaderClass::CULL_MODE_ENABLE) ? D3DCULL_CW : D3DCULL_NONE);

		// An alpha-tested mesh keeps its hard outline as ShaderClass draws it, and its cut-away texels write no depth.
		const Bool tested = shader.Get_Alpha_Test() == ShaderClass::ALPHATEST_ENABLE;
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, tested);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, !tested);
		if (tested)
		{
			DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAREF, 0x60);
			DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
		}
	}
}

void W3DPlanarMirrorManager::endOverlay()
{
	unbindShading(m_overlayGlass);
	m_overlayGlass = FALSE;
}

// The draw keeps its depth test and writes.
Bool W3DPlanarMirrorManager::beginTranslucent(const ShaderClass &shader, const void *effectData)
{
#if defined(BUILD_WITH_D3D9)
	if (!m_sceneArmed || !isAvailable())
	{
		return FALSE;
	}

	if (!takeSceneCopy())
	{
		return FALSE;
	}

	RenderStateStruct state;
	DX8Wrapper::Get_Render_State(state);
	TextureClass *texture = (state.Textures[0] != nullptr) ? state.Textures[0]->As_TextureClass() : nullptr;

	// Glass blends by its outline. The refraction covers the draw whole, since the shader blends what lies behind in itself.
	const PlanarMirrorShaderTuning *own = static_cast<const PlanarMirrorShaderTuning *>(effectData);
	const Bool glass = own != nullptr && own->overrideTexture && m_glassShader != 0;
	if (glass)
	{
		bindShading(own, m_glassShader, TRUE, Draws_With_Alpha(shader));
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, TRUE);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	}
	else
	{
		const Bool adds = shader.Get_Src_Blend_Func() == ShaderClass::SRCBLEND_ONE && shader.Get_Dst_Blend_Func() == ShaderClass::DSTBLEND_ONE;
		bindShading(own, m_refractShader, TRUE, adds);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, FALSE);
	}
	bindNormalMap(texture);
	return TRUE;
#else
	(void)shader;
	(void)effectData;
	return FALSE;
#endif
}

void W3DPlanarMirrorManager::endTranslucent()
{
	unbindShading(TRUE);
}
