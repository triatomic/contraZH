/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

#include "../StdAfx.h"
#include "MaterialPreviewPane.h"

#include "WWMath/aabox.h"		// highlighted sub-object outline box
#include "WW3D2/assetmgr.h"
#include "WW3D2/camera.h"
#include "WW3D2/coltest.h"	// ray-cast for the Blender-style light placement
#include "WW3D2/dx8renderer.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/light.h"
#include "WWMath/matrix3.h"	// Matrix3x3 for the pan basis
#include "WWMath/quat.h"		// Build_Matrix3 / Build_Quaternion / Trackball
#include "WW3D2/hanim.h"	// animation playback (Set_Animation_Asset)
#include "WWLib/rcfile.h"	// ResourceFileClass (embedded Light.w3d gizmo)
#include <vector>	// scratch vertex buffer for the selection-tint overlay
#include "WWMath/wwmath.h"	// DEG_TO_RADF
#include "WW3D2/mesh.h"
#include "WW3D2/meshmdl.h"
#include "WW3D2/rendobj.h"
#include "WW3D2/scene.h"
#include "WW3D2/shader.h"
#include "WW3D2/vertmaterial.h"
#include "WW3D2/w3d_file.h"
#include "WW3D2/ww3d.h"
#include "../GraphicView.h"	// backface-tint toggle + colour (shared with main viewport)
#include "../Utils.h"		// GetCurrentDocument
#include "../W3DDarkMode.h"
#include "../W3DViewDoc.h"

#include "W3dMaterialData.h"

BEGIN_MESSAGE_MAP(CMaterialPreviewPane, CWnd)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

CMaterialPreviewPane::CMaterialPreviewPane()
	: m_SwapChain(nullptr),
	  m_DepthBuffer(nullptr),
	  m_Scene(nullptr),
	  m_Camera(nullptr),
	  m_Light(nullptr),
	  m_RenderObj(nullptr),
	  m_LightMesh(nullptr),
	  m_LightMeshInScene(false),
	  m_LightMeshScale(1.0F),
	  m_LightPlacementMode(LIGHT_PER_FACE),
	  m_SphereCenter(0, 0, 0),
	  m_CameraDistance(10.0F),
	  m_MinZoomAdjust(0.0F),
	  m_bMouseDown(FALSE),
	  m_bRMouseDown(FALSE),
	  m_bMMouseDown(FALSE),
	  m_AltOnMDown(false),
	  m_GroundVisible(false),
	  m_GroundZ(0.0F),
	  m_TintHighlight(false),
	  m_Anim(nullptr),
	  m_AnimPlaying(false),
	  m_AnimFrame(0.0F),
	  m_SizeDirty(false),
	  m_LastRenderTicks(0)
{
	m_Rotation.Make_Identity();
	m_LastPoint.x = m_LastPoint.y = 0;
	m_MButtonDownPoint.x = m_MButtonDownPoint.y = 0;
}

CMaterialPreviewPane::~CMaterialPreviewPane()
{
}

BOOL
CMaterialPreviewPane::Create(CWnd *parent, const RECT &rect, UINT id)
{
	return CWnd::Create(nullptr, "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, rect, parent, id);
}

int
CMaterialPreviewPane::OnCreate(LPCREATESTRUCT create_struct)
{
	if (CWnd::OnCreate(create_struct) == -1) {
		return -1;
	}

	m_Scene = new SimpleSceneClass;
	m_Scene->Set_Ambient_Light(Vector3(0.5F, 0.5F, 0.5F));

	m_Camera = new CameraClass;

	// Mirror the main viewer's default scene light (see CW3DViewDoc::InitScene).
	m_Light = new LightClass;
	m_Light->Set_Position(Vector3(0, 5000, 3000));
	m_Light->Set_Intensity(1.0F);
	m_Light->Set_Force_Visible(true);
	m_Light->Set_Flag(LightClass::NEAR_ATTENUATION, false);
	m_Light->Set_Far_Attenuation_Range(1000000, 1000000);
	m_Light->Set_Ambient(Vector3(0, 0, 0));
	m_Light->Set_Diffuse(Vector3(1, 1, 1));
	m_Light->Set_Specular(Vector3(1, 1, 1));
	m_Scene->Add_Render_Object(m_Light);

	Create_Swap_Chain();
	Reset_Camera_To_Sphere(SphereClass(Vector3(0, 0, 0), 3.0F));	// sane default until a model loads

	// W3DView does not otherwise use the cleanup hook (only WorldBuilder does),
	// so the single slot is free for the preview pane.
	DX8Wrapper::SetCleanupHook(this);
	return 0;
}

void
CMaterialPreviewPane::ReleaseResources()
{
	Free_Swap_Chain();
}

void
CMaterialPreviewPane::ReAcquireResources()
{
	// Lazy: Render() recreates the swap chain when it is missing.
}

void
CMaterialPreviewPane::OnDestroy()
{
	DX8Wrapper::SetCleanupHook(nullptr);

	if (m_RenderObj != nullptr) {
		m_Scene->Remove_Render_Object(m_RenderObj);
		m_RenderObj->Release_Ref();
		m_RenderObj = nullptr;
	}

	if (m_Anim != nullptr) {
		m_Anim->Release_Ref();
		m_Anim = nullptr;
	}

	if (m_LightMesh != nullptr) {
		if (m_LightMeshInScene && m_Scene != nullptr) {
			m_Scene->Remove_Render_Object(m_LightMesh);
			m_LightMeshInScene = false;
		}
		m_LightMesh->Release_Ref();
		m_LightMesh = nullptr;
	}

	if (m_Light != nullptr) {
		m_Scene->Remove_Render_Object(m_Light);
		m_Light->Release_Ref();
		m_Light = nullptr;
	}

	if (m_Camera != nullptr) {
		m_Camera->Release_Ref();
		m_Camera = nullptr;
	}

	if (m_Scene != nullptr) {
		m_Scene->Release_Ref();
		m_Scene = nullptr;
	}

	Free_Swap_Chain();
	CWnd::OnDestroy();
}

void
CMaterialPreviewPane::Free_Swap_Chain()
{
	if (m_DepthBuffer != nullptr) {
		m_DepthBuffer->Release();
		m_DepthBuffer = nullptr;
	}

	if (m_SwapChain != nullptr) {
		m_SwapChain->Release();
		m_SwapChain = nullptr;
	}
}

void
CMaterialPreviewPane::Create_Swap_Chain()
{
	Free_Swap_Chain();

	CRect rect;
	GetClientRect(&rect);
	if (rect.Width() < 1 || rect.Height() < 1) {
		return;
	}

	m_SwapChain = DX8Wrapper::Create_Additional_Swap_Chain(m_hWnd);

	// The device's default depth buffer is sized to the main view and may be
	// multisampled; either mismatch makes SetRenderTarget fail silently. Pair
	// the swap chain with a pane-sized non-MSAA depth buffer instead.
	if (m_SwapChain != nullptr) {
		IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
		if (device != nullptr) {
			D3DFORMAT zformat = D3DFMT_D24S8;
			IDirect3DSurface8 *default_z = nullptr;
			if (SUCCEEDED(device->GetDepthStencilSurface(&default_z)) && default_z != nullptr) {
				D3DSURFACE_DESC desc;
				if (SUCCEEDED(default_z->GetDesc(&desc))) {
					zformat = desc.Format;
				}
				default_z->Release();
			}
#if defined(BUILD_WITH_D3D9)
			device->CreateDepthStencilSurface(rect.Width(), rect.Height(),
				zformat, D3DMULTISAMPLE_NONE, 0, FALSE, &m_DepthBuffer, nullptr);
#else
			device->CreateDepthStencilSurface(rect.Width(), rect.Height(),
				zformat, D3DMULTISAMPLE_NONE, &m_DepthBuffer);
#endif
		}
	}

	// Never keep a swap chain without its matching depth buffer: pairing the
	// pane's back buffer with the device's default (main-view-sized) depth
	// makes SetRenderTarget fail silently, and the preview would then clear
	// and draw straight into the MAIN view's back buffer.
	if (m_SwapChain != nullptr && m_DepthBuffer == nullptr) {
		Free_Swap_Chain();
	}

	if (m_Camera != nullptr) {
		m_Camera->Set_Aspect_Ratio((float)rect.Width() / (float)rect.Height());
	}
}

namespace
{

// Recursively give every MeshClass under a render object a private copy of its
// MeshModelClass (which is shared with the cached prototype by default), so the
// live preview can mutate materials without affecting the shared asset.
void Make_Preview_Meshes_Unique(RenderObjClass *obj)
{
	if (obj == nullptr) {
		return;
	}
	if (obj->Class_ID() == RenderObjClass::CLASSID_MESH) {
		((MeshClass *)obj)->Make_Unique(true);	// force clone of the mesh model
	}
	int count = obj->Get_Num_Sub_Objects();
	for (int i = 0; i < count; i++) {
		RenderObjClass *child = obj->Get_Sub_Object(i);
		if (child != nullptr) {
			Make_Preview_Meshes_Unique(child);
			child->Release_Ref();	// Get_Sub_Object adds a ref
		}
	}
}

} // anonymous namespace

bool
CMaterialPreviewPane::LoadModel(const char *name)
{
	if (m_RenderObj != nullptr) {
		m_Scene->Remove_Render_Object(m_RenderObj);
		m_RenderObj->Release_Ref();
		m_RenderObj = nullptr;
	}

	RenderObjClass *render_obj = WW3DAssetManager::Get_Instance()->Create_Render_Obj(name);
	if (render_obj == nullptr) {
		return false;
	}

	render_obj->Set_Transform(Matrix3D(1));
	m_Scene->Add_Render_Object(render_obj);
	m_RenderObj = render_obj;

	// Mesh instances share their MeshModelClass with the cached asset-manager
	// prototype by default. The live material preview mutates the model, so give
	// this preview's meshes private copies first — otherwise edits would leak
	// into the shared prototype (and the main viewport) and could not be undone
	// by reloading. See ApplyLiveMaterials.
	Make_Preview_Meshes_Unique(render_obj);

	// Only re-frame the camera when a *different* object is loaded. Reloading
	// the same model (e.g. the post-save asset refresh after a material edit)
	// keeps the user's current orbit and zoom so the view doesn't jump.
	bool same_model = (m_LoadedName == name);
	SphereClass sphere = render_obj->Get_Bounding_Sphere();
	if (!same_model) {
		Reset_Camera_To_Sphere(sphere);
		// Ground default: sit at the new object's bounding-box bottom. A reload
		// of the same model (post-save refresh) keeps the user's height.
		m_GroundZ = CGraphicView::Ground_Default_Z(render_obj);
	} else {
		m_ViewedSphere = sphere;
	}
	m_LoadedName = name;
	return true;
}

// Height default + slider bounds come from the shared derivation so the preview
// and the main viewport agree exactly (see CGraphicView).
bool
CMaterialPreviewPane::Get_Ground_Z_Range(float &z_min, float &z_max) const
{
	if (m_RenderObj == nullptr) {
		return false;
	}
	CGraphicView::Ground_Z_Range(m_RenderObj, z_min, z_max);
	return true;
}

void
CMaterialPreviewPane::Reset_Ground_To_Object()
{
	m_GroundZ = CGraphicView::Ground_Default_Z(m_RenderObj);
}

void
CMaterialPreviewPane::Set_Animation_Asset(HAnimClass *anim)
{
	if (anim != nullptr) {
		anim->Add_Ref();
	}
	if (m_Anim != nullptr) {
		m_Anim->Release_Ref();
	}
	m_Anim = anim;
	m_AnimFrame = 0.0F;

	if (m_RenderObj != nullptr) {
		if (m_Anim != nullptr) {
			m_RenderObj->Set_Animation(m_Anim, 0.0F, RenderObjClass::ANIM_MODE_MANUAL);
		} else {
			m_AnimPlaying = false;
			m_RenderObj->Set_Animation();	// back to the base pose
		}
	}
}

void
CMaterialPreviewPane::Seek_Animation(float frame)
{
	if (m_Anim == nullptr) {
		return;
	}
	float last = (float)(m_Anim->Get_Num_Frames() - 1);
	if (last < 0.0F) {
		last = 0.0F;
	}
	m_AnimFrame = (frame < 0.0F) ? 0.0F : (frame > last) ? last : frame;
	if (m_RenderObj != nullptr) {
		m_RenderObj->Set_Animation(m_Anim, m_AnimFrame, RenderObjClass::ANIM_MODE_MANUAL);
	}
}

void
CMaterialPreviewPane::UnloadModel()
{
	if (m_RenderObj != nullptr) {
		if (m_Scene != nullptr) {
			m_Scene->Remove_Render_Object(m_RenderObj);
		}
		m_RenderObj->Release_Ref();
		m_RenderObj = nullptr;
	}
	// Deliberately keep m_LoadedName: a reload of the same model (unload then
	// load during the post-save refresh) must preserve the camera. A genuine
	// mesh switch loads a different name and re-frames.
}

//////////////////////////////////////////////////////////////////////////////
//	Live material apply
//////////////////////////////////////////////////////////////////////////////

namespace
{

// Mesh names in the document are "Container.MeshName"; a render object's mesh
// reports either that full name or just the trailing "MeshName". Match on the
// part after the last '.', case-insensitively, so both forms line up.
const char *Bare_Mesh_Name(const char *name)
{
	if (name == nullptr) {
		return "";
	}
	const char *dot = strrchr(name, '.');
	return (dot != nullptr) ? dot + 1 : name;
}

bool Mesh_Names_Match(const char *a, const char *b)
{
	return _stricmp(Bare_Mesh_Name(a), Bare_Mesh_Name(b)) == 0;
}

// Live-preview the U/V clamp (texture-address) bits. Clamp is a property of the
// TextureClass filter, which is set once at load from the W3DTEXTURE_CLAMP_U/V
// bits and shared (by pointer) with every other mesh using the same texture.
// Mutating that shared filter in place would flip tiling on unrelated models and
// leak past the viewer, so instead we make a private copy of the texture: a new
// TextureClass built from the SAME underlying D3D surface (no re-decode, just an
// AddRef) but with its own filter. We copy the current filter, override only the
// address mode from the edited attributes, and swap the copy into this one pass/
// stage. Returns true if the texture pointer was replaced (so the caller can
// re-register the mesh's render category, which is keyed by texture+shader).
//
// Only the address mode is live-previewable this way. The other W3DTEXTURE_* bits
// (No-LOD/mip count, alpha-bitmap, hint) change how the texture is DECODED at
// load, so they can't be applied without a reload and stay save-only.
bool Apply_Clamp_To_Texture_Stage(MeshModelClass *model, int pass, int stage,
	const W3dMaterialViewer::TextureData &tex_data)
{
	TextureClass *live = model->Peek_Single_Texture(pass, stage);
	if (live == nullptr) {
		return false;
	}

	const bool want_u_clamp = (tex_data.attributes & W3DTEXTURE_CLAMP_U) != 0;
	const bool want_v_clamp = (tex_data.attributes & W3DTEXTURE_CLAMP_V) != 0;
	const bool have_u_clamp =
		live->Get_Filter().Get_U_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP;
	const bool have_v_clamp =
		live->Get_Filter().Get_V_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP;
	if (want_u_clamp == have_u_clamp && want_v_clamp == have_v_clamp) {
		return false;	// nothing to change; keep the shared texture as-is
	}

	IDirect3DBaseTexture8 *d3d = live->Peek_D3D_Base_Texture();
	if (d3d == nullptr) {
		return false;	// surface not resident yet (async load) — retried next edit
	}

	// New object over the shared surface. The ctor AddRefs the D3D texture, so it
	// owns an independent ref and outlives the original.
	TextureClass *edit = new TextureClass(d3d);
	edit->Get_Filter() = live->Get_Filter();	// inherit filter/mip settings, then...
	edit->Get_Filter().Set_U_Addr_Mode(want_u_clamp
		? TextureFilterClass::TEXTURE_ADDRESS_CLAMP : TextureFilterClass::TEXTURE_ADDRESS_REPEAT);
	edit->Get_Filter().Set_V_Addr_Mode(want_v_clamp
		? TextureFilterClass::TEXTURE_ADDRESS_CLAMP : TextureFilterClass::TEXTURE_ADDRESS_REPEAT);

	model->Set_Single_Texture(edit, pass, stage);	// takes its own ref
	edit->Release_Ref();
	return true;
}

// Stamp one mesh's edited vertex materials + shaders onto the live model. The
// model was already made private (Make_Preview_Meshes_Unique) so mutating it is
// safe; the vertex material is still cloned so Replace_VertexMaterial has a
// distinct new pointer to swap in and update the DX8 render category.
void Apply_Mesh_Materials(MeshClass *mesh, const W3dMaterialViewer::MeshMaterialData &data)
{
	MeshModelClass *model = mesh->Peek_Model();
	if (model == nullptr) {
		return;
	}

	bool shader_changed = false;

	int passes = model->Get_Pass_Count();
	for (int pass = 0; pass < passes && pass < (int)data.passes.size(); pass++) {
		const W3dMaterialViewer::PassData &pass_data = data.passes[pass];

		// --- Vertex material: colors / opacity / shininess ---
		// Replace_VertexMaterial swaps the material everywhere it is used (single
		// slot or per-vertex array) AND updates the live DX8 render category, so
		// the change is visible without re-registering. We clone the current one,
		// edit the clone, and swap it in (never touching the shared original).
		int vmi = pass_data.vertexMaterialIndex;
		if (vmi >= 0 && vmi < (int)data.vertexMaterials.size()) {
			const W3dMaterialViewer::VertexMaterialData &vm = data.vertexMaterials[vmi];
			VertexMaterialClass *live = model->Peek_Single_Material(pass);
			if (live != nullptr) {
				VertexMaterialClass *edit = live->Clone();
				edit->Set_Ambient(vm.ambient[0] / 255.0F, vm.ambient[1] / 255.0F, vm.ambient[2] / 255.0F);
				edit->Set_Diffuse(vm.diffuse[0] / 255.0F, vm.diffuse[1] / 255.0F, vm.diffuse[2] / 255.0F);
				edit->Set_Specular(vm.specular[0] / 255.0F, vm.specular[1] / 255.0F, vm.specular[2] / 255.0F);
				edit->Set_Emissive(vm.emissive[0] / 255.0F, vm.emissive[1] / 255.0F, vm.emissive[2] / 255.0F);
				edit->Set_Opacity(vm.opacity);
				edit->Set_Shininess(vm.shininess);

				// --- Stage 0/1 UV mapping type + mapper args ---
				// Rebuild the mappers exactly as the loader does: clear both
				// stages, then let the engine's own factory recreate them from
				// the edited mapping-type bits (in attributes) and args strings.
				// A change to plain UV leaves the cleared (null) mapper.
				edit->Set_Mapper(nullptr, 0);
				edit->Set_Mapper(nullptr, 1);
				W3dVertexMaterialStruct vmat_struct;
				::memset(&vmat_struct, 0, sizeof(vmat_struct));
				vmat_struct.Attributes = vm.attributes;
				// Parse_Mapping_Args takes non-const char*; copy the args strings.
				std::vector<char> args0(vm.mapperArgs0.begin(), vm.mapperArgs0.end());
				args0.push_back('\0');
				std::vector<char> args1(vm.mapperArgs1.begin(), vm.mapperArgs1.end());
				args1.push_back('\0');
				edit->Parse_Mapping_Args(vmat_struct,
					vm.mapperArgs0.empty() ? nullptr : args0.data(),
					vm.mapperArgs1.empty() ? nullptr : args1.data());

				model->Replace_VertexMaterial(live, edit);
				edit->Release_Ref();	// Replace_VertexMaterial took its own ref
			}
		}

		// --- Shader: blend / depth / gradients / alpha test ---
		int si = pass_data.shaderIndex;
		if (si >= 0 && si < (int)data.shaders.size()) {
			const W3dMaterialViewer::ShaderData &sh = data.shaders[si];
			// Start from the live shader so bits we don't manage are preserved,
			// then overwrite the managed fields (their W3D chunk values map 1:1
			// to the ShaderClass enums).
			ShaderClass shader = model->Get_Single_Shader(pass);
			shader.Set_Src_Blend_Func((ShaderClass::SrcBlendFuncType)sh.srcBlend);
			shader.Set_Dst_Blend_Func((ShaderClass::DstBlendFuncType)sh.destBlend);
			shader.Set_Depth_Compare((ShaderClass::DepthCompareType)sh.depthCompare);
			shader.Set_Depth_Mask((ShaderClass::DepthMaskType)sh.depthMask);
			shader.Set_Alpha_Test((ShaderClass::AlphaTestType)sh.alphaTest);
			shader.Set_Primary_Gradient((ShaderClass::PriGradientType)sh.priGradient);
			shader.Set_Secondary_Gradient((ShaderClass::SecGradientType)sh.secGradient);
			// Detail color/alpha funcs are not live-previewed (the runtime shader
			// exposes only the post-detail fields); they still save to the file.
			model->Set_Single_Shader(shader, pass);
			shader_changed = true;
		}

		// --- Texture: U/V clamp (address) mode, per stage ---
		for (int stage = 0; stage < 2; stage++) {
			int ti = pass_data.stageTextureIndex[stage];
			if (ti >= 0 && ti < (int)data.textures.size()) {
				if (Apply_Clamp_To_Texture_Stage(model, pass, stage, data.textures[ti])) {
					shader_changed = true;	// texture swap re-buckets the render category
				}
			}
		}
	}

	// A shader change re-buckets which DX8 render category the mesh belongs to
	// (categories are keyed by texture+shader), so rebuild its registration.
	// Material changes above already updated their category in place.
	if (shader_changed) {
		TheDX8MeshRenderer.Unregister_Mesh_Type(model);
		TheDX8MeshRenderer.Register_Mesh_Type(model);
	}
}

// Recursively visit every MeshClass under a render object, matching it to a
// document mesh by name and stamping the edits.
void Apply_To_Render_Obj(RenderObjClass *obj, const W3dMaterialViewer::MaterialDocument &document)
{
	if (obj == nullptr) {
		return;
	}

	if (obj->Class_ID() == RenderObjClass::CLASSID_MESH) {
		const char *obj_name = obj->Get_Name();
		for (const W3dMaterialViewer::MeshMaterialData &mesh_data : document.meshes) {
			if (Mesh_Names_Match(obj_name, mesh_data.meshName.c_str())) {
				Apply_Mesh_Materials((MeshClass *)obj, mesh_data);
				break;
			}
		}
	}

	int count = obj->Get_Num_Sub_Objects();
	for (int i = 0; i < count; i++) {
		RenderObjClass *child = obj->Get_Sub_Object(i);
		if (child != nullptr) {
			Apply_To_Render_Obj(child, document);
			child->Release_Ref();	// Get_Sub_Object adds a ref
		}
	}
}

} // anonymous namespace

void
CMaterialPreviewPane::ApplyLiveMaterials(const W3dMaterialViewer::MaterialDocument &document)
{
	if (m_RenderObj == nullptr) {
		return;
	}
	Apply_To_Render_Obj(m_RenderObj, document);
}

// Frames the camera on `sphere` — same model as CGraphicView::Reset_Camera_To_
// Display_Sphere: distance = 3*radius, looking down +X at the centre.
void
CMaterialPreviewPane::Reset_Camera_To_Sphere(const SphereClass &sphere)
{
	if (m_Camera == nullptr) {
		return;
	}

	m_CameraDistance = sphere.Radius * 3.0F;
	m_CameraDistance = (m_CameraDistance < 1.0F) ? 1.0F : m_CameraDistance;

	Matrix3D transform(1);
	transform.Look_At(sphere.Center + Vector3(m_CameraDistance, 0, 0), sphere.Center, 0);

	m_SphereCenter = sphere.Center;
	m_MinZoomAdjust = m_CameraDistance / 190.0F;
	m_Rotation = Build_Quaternion(transform);
	m_Camera->Set_Transform(transform);

	float max_dist = m_CameraDistance * 60.0F;
	float min_dist = max(0.2F, m_MinZoomAdjust / 2);
	m_Camera->Set_Clip_Planes(min_dist, max_dist);

	m_ViewedSphere = sphere;

	// Mirror the main viewport (Reset_Camera_To_Display_Sphere): place the
	// scene light above the object and keep the Ctrl gizmo with it, rescaled
	// to the framed object.
	if (m_Light != nullptr) {
		Matrix3D light_tm(1);
		light_tm.Set_Translation(sphere.Center);
		light_tm.Translate(Vector3(0, 0, 0.7F * m_CameraDistance));
		m_Light->Set_Transform(light_tm);
		if (m_LightMesh != nullptr) {
			m_LightMesh->Set_Transform(light_tm);
			m_LightMesh->Scale(m_CameraDistance / (14.0F * m_LightMeshScale));
			m_LightMeshScale = m_CameraDistance / 14.0F;
		}
	}
}

void
CMaterialPreviewPane::Set_Highlight_Mesh(const char *name)
{
	m_HighlightMeshName = (name != nullptr) ? name : "";
}

// Draws the highlighted sub-object's bounding box as a magenta wireframe
// (matching the main viewport's sub-object selection colour), always on top.
// Same overlay recipe as the main view's bone/selection overlays: runs inside
// the render bracket after the scene, so view/projection are already set.
void
CMaterialPreviewPane::Render_Highlight_Outline()
{
	if (m_HighlightMeshName.empty() || m_RenderObj == nullptr) {
		return;
	}

	// Find the sub-object: full "Container.Mesh" name first, then short name.
	RenderObjClass *sub = nullptr;
	int count = m_RenderObj->Get_Num_Sub_Objects();
	for (int i = 0; i < count && sub == nullptr; i++) {
		RenderObjClass *candidate = m_RenderObj->Get_Sub_Object(i);
		if (candidate == nullptr) {
			continue;
		}
		const char *full = candidate->Get_Name();
		const char *dot = ::strchr(full, '.');
		const char *short_name = (dot != nullptr) ? dot + 1 : full;
		if (::_stricmp(full, m_HighlightMeshName.c_str()) == 0
				|| ::_stricmp(short_name, m_HighlightMeshName.c_str()) == 0) {
			sub = candidate;
		} else {
			candidate->Release_Ref();
		}
	}
	if (sub == nullptr) {
		return;
	}

	// Tint mode: paint the mesh's own triangles with the selection colour
	// instead of the box; falls through to the box when tinting isn't possible.
	if (m_TintHighlight && Render_Highlight_Tint(sub)) {
		sub->Release_Ref();
		return;
	}

	AABoxClass box;
	sub->Get_Obj_Space_Bounding_Box(box);
	Matrix3D tm = sub->Get_Transform();
	sub->Release_Ref();

	Vector3 corners[8];
	for (int i = 0; i < 8; i++) {
		Vector3 local(
			box.Center.X + ((i & 1) ? box.Extent.X : -box.Extent.X),
			box.Center.Y + ((i & 2) ? box.Extent.Y : -box.Extent.Y),
			box.Center.Z + ((i & 4) ? box.Extent.Z : -box.Extent.Z));
		Matrix3D::Transform_Vector(tm, local, &corners[i]);
	}

	ShaderClass shader = ShaderClass::_PresetOpaqueSolidShader;
	shader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);
	shader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
	DX8Wrapper::Set_Shader(shader);
	DX8Wrapper::Set_Texture(0, nullptr);

	VertexMaterialClass *vm = VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vm);
	if (vm != nullptr) {
		vm->Release_Ref();
	}

	Matrix3D identity(true);
	DX8Wrapper::Set_Transform(D3DTS_WORLD, identity);
	DX8Wrapper::Apply_Render_State_Changes();

	IDirect3DDevice8 *dev = DX8Wrapper::_Get_D3D_Device8();
	if (dev == nullptr) {
		return;
	}

	struct OutlineVertex { float x, y, z; DWORD color; };
	const DWORD color = 0xFFFF00FF;	// magenta = selected sub-object (main view)
	static const int EDGES[12][2] = {
		{ 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 },	// X-aligned edges
		{ 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 },	// Y-aligned edges
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },	// Z-aligned edges
	};
	OutlineVertex verts[24];
	for (int e = 0; e < 12; e++) {
		for (int p = 0; p < 2; p++) {
			const Vector3 &c = corners[EDGES[e][p]];
			OutlineVertex &v = verts[e * 2 + p];
			v.x = c.X;
			v.y = c.Y;
			v.z = c.Z;
			v.color = color;
		}
	}

	DX8_SET_FVF(dev, D3DFVF_XYZ | D3DFVF_DIFFUSE);
	dev->DrawPrimitiveUP(D3DPT_LINELIST, 12, verts, sizeof(OutlineVertex));
	DX8Wrapper::Invalidate_Cached_Render_States();
}

// Re-draws the selected sub-mesh's triangles as a translucent overlay in the
// selection colour. Runs after the scene inside the render bracket: the mesh
// was just drawn at these exact positions, so with PASS_LEQUAL and no depth
// write the tint lands precisely on its visible surfaces (hidden parts stay
// hidden). Model-space vertices are drawn under the mesh's world transform;
// the polygon array (TriIndex = three 16-bit indices) doubles directly as the
// D3DFMT_INDEX16 buffer. Skins deform outside the model-space array, so they
// (and anything over 64k vertices) report false and keep the box outline.
bool
CMaterialPreviewPane::Render_Highlight_Tint(RenderObjClass *sub)
{
	if (sub == nullptr || sub->Class_ID() != RenderObjClass::CLASSID_MESH) {
		return false;
	}
	MeshClass *mesh = (MeshClass *)sub;
	MeshModelClass *model = mesh->Peek_Model();
	if (model == nullptr || model->Get_Flag(MeshGeometryClass::SKIN)) {
		return false;
	}

	int vcount = model->Get_Vertex_Count();
	int pcount = model->Get_Polygon_Count();
	const TriIndex *polys = model->Get_Polygon_Array();
	const Vector3 *verts = model->Get_Vertex_Array();
	if (vcount <= 0 || vcount > 0xFFFF || pcount <= 0
			|| polys == nullptr || verts == nullptr) {
		return false;
	}

	ShaderClass shader = ShaderClass::_PresetAlphaSolidShader;
	shader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
	DX8Wrapper::Set_Shader(shader);
	DX8Wrapper::Set_Texture(0, nullptr);

	VertexMaterialClass *vm = VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vm);
	if (vm != nullptr) {
		vm->Release_Ref();
	}

	DX8Wrapper::Set_Transform(D3DTS_WORLD, mesh->Get_Transform());
	DX8Wrapper::Apply_Render_State_Changes();

	IDirect3DDevice8 *dev = DX8Wrapper::_Get_D3D_Device8();
	if (dev == nullptr) {
		return false;
	}

	struct TintVertex { float x, y, z; DWORD color; };
	const DWORD color = 0x60FF00FF;	// selection magenta (matches the box) at ~38% alpha
	std::vector<TintVertex> tinted(vcount);
	for (int i = 0; i < vcount; i++) {
		tinted[i].x = verts[i].X;
		tinted[i].y = verts[i].Y;
		tinted[i].z = verts[i].Z;
		tinted[i].color = color;
	}

	DX8_SET_FVF(dev, D3DFVF_XYZ | D3DFVF_DIFFUSE);
	dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, vcount, pcount,
		polys, D3DFMT_INDEX16, &tinted[0], sizeof(TintVertex));
	DX8Wrapper::Invalidate_Cached_Render_States();
	return true;
}

// Draws the ground plane + blob shadow; the drawing itself is shared with the
// main viewport (CGraphicView::Render_Ground, Ground menu), which documents the
// draw-before-the-scene ordering that makes glow land on the plane.
void
CMaterialPreviewPane::Render_Ground_Plane()
{
	if (!m_GroundVisible || m_Camera == nullptr || m_Scene == nullptr) {
		return;
	}
	CGraphicView::Render_Ground(m_Camera, m_RenderObj, m_ViewedSphere, m_GroundZ,
		m_Scene->Get_Ambient_Light(), m_Light);
}

// Adds/removes the light gizmo mesh so it is visible exactly while Ctrl is
// held (called from OnMouseMove, like the main viewport). The mesh comes from
// the LIGHT prototype (embedded Light.w3d resource), created lazily.
void
CMaterialPreviewPane::Sync_Light_Mesh(bool visible)
{
	if (visible && m_LightMesh == nullptr && m_Light != nullptr) {
		WW3DAssetManager *assets = WW3DAssetManager::Get_Instance();
		// The main view loads Light.w3d at startup; ensure it in case that changes.
		if (assets->Render_Obj_Exists("LIGHT") == false) {
			ResourceFileClass light_mesh_file(nullptr, "Light.w3d");
			assets->Load_3D_Assets(light_mesh_file);
		}
		m_LightMesh = assets->Create_Render_Obj("LIGHT");
		if (m_LightMesh != nullptr) {
			m_LightMesh->Set_Transform(m_Light->Get_Transform());
			m_LightMesh->Scale(m_CameraDistance / 14.0F);
			m_LightMeshScale = m_CameraDistance / 14.0F;
		}
	}

	if (m_LightMesh == nullptr || m_Scene == nullptr) {
		return;
	}

	if (visible && !m_LightMeshInScene) {
		m_LightMesh->Set_Transform(m_Light->Get_Transform());
		m_Scene->Add_Render_Object(m_LightMesh);
		m_LightMeshInScene = true;
	} else if (!visible && m_LightMeshInScene) {
		m_Scene->Remove_Render_Object(m_LightMesh);
		m_LightMeshInScene = false;
	}
}

// Ctrl+left-drag, Free Roam mode: trackball-rotate the scene light about the
// orbit centre (same math as CGraphicView's light rotation).
void
CMaterialPreviewPane::Rotate_Light(CPoint point)
{
	if (m_Light == nullptr || m_Camera == nullptr) {
		return;
	}

	RECT rect;
	GetClientRect(&rect);
	float midPointX = float(rect.right >> 1);
	float midPointY = float(rect.bottom >> 1);
	if (midPointX == 0.0F || midPointY == 0.0F) {
		return;
	}

	float lastPointX = ((float)m_LastPoint.x - midPointX) / midPointX;
	float lastPointY = (midPointY - (float)m_LastPoint.y) / midPointY;
	float pointX = ((float)point.x - midPointX) / midPointX;
	float pointY = (midPointY - (float)point.y) / midPointY;

	Quaternion mouse_motion = Inverse(::Trackball(lastPointX, lastPointY, pointX, pointY, 0.8F));
	Quaternion camera = Build_Quaternion(m_Camera->Get_Transform());
	Quaternion cur_light = Build_Quaternion(m_Light->Get_Transform());

	Quaternion light_orientation = camera;
	light_orientation = light_orientation * mouse_motion;
	light_orientation = light_orientation * Inverse(camera);
	light_orientation = light_orientation * cur_light;
	light_orientation.Normalize();

	Vector3 to_center;
	Matrix3D matrix = m_Light->Get_Transform();
	Matrix3D::Inverse_Transform_Vector(matrix, m_SphereCenter, &to_center);

	Matrix3D light_tm(light_orientation, m_SphereCenter);
	light_tm.Translate(-to_center);

	if (m_LightMesh != nullptr) {
		m_LightMesh->Set_Transform(light_tm);
	}
	m_Light->Set_Transform(light_tm);
}

// Ctrl+left click/drag, Per Face mode: Blender-style light placement. Casts a pick ray
// through the cursor at the displayed object; on a hit the light moves onto
// the hit point's surface normal (keeping its current stand-off distance), so
// the spot under the cursor becomes the most-lit point. A miss leaves the
// light where it is.
void
CMaterialPreviewPane::Position_Light_At_Cursor(CPoint point)
{
	if (m_Light == nullptr || m_Camera == nullptr || m_RenderObj == nullptr) {
		return;
	}

	RECT rect;
	GetClientRect(&rect);
	if (rect.right <= 0 || rect.bottom <= 0) {
		return;
	}

	// Cursor -> world-space pick ray through the view plane.
	Vector2 device;
	device.X = 2.0F * (float)point.x / (float)rect.right - 1.0F;
	device.Y = 1.0F - 2.0F * (float)point.y / (float)rect.bottom;

	Vector3 on_plane;
	m_Camera->Un_Project(on_plane, device);
	Vector3 start = m_Camera->Get_Transform().Get_Translation();
	Vector3 dir = on_plane - start;
	dir.Normalize();
	Vector3 end = start + dir * (m_CameraDistance * 60.0F);	// out to the far clip

	CastResultStruct result;
	result.ComputeContactPoint = true;
	LineSegClass ray(start, end);
	RayCollisionTestClass raytest(ray, &result, COLL_TYPE_ALL, true /*translucent*/, false);
	m_RenderObj->Cast_Ray(raytest);

	if (result.StartBad || result.Fraction >= 1.0F) {
		return;
	}

	Vector3 contact = start + (end - start) * result.Fraction;
	Vector3 normal = result.Normal;
	if (normal.Length2() < 0.0001F) {
		return;
	}
	normal.Normalize();

	// Keep the light's current stand-off from the surface (Ctrl+right-drag
	// adjusts it), with a floor so it never sits inside small details.
	float offset = (m_Light->Get_Position() - contact).Length();
	float min_offset = m_ViewedSphere.Radius * 0.25F;
	if (offset < min_offset) {
		offset = min_offset;
	}

	// Camera-style Look_At: -Z faces the contact point, so the existing
	// distance adjust (translate along local Z) still moves to/from the surface.
	Matrix3D light_tm(1);
	light_tm.Look_At(contact + normal * offset, contact, 0);

	if (m_LightMesh != nullptr) {
		m_LightMesh->Set_Transform(light_tm);
	}
	m_Light->Set_Transform(light_tm);
}

// Ctrl+right-drag: move the light along its view axis, refusing to enter the
// object's bounding sphere (same rule as CGraphicView).
void
CMaterialPreviewPane::Adjust_Light_Distance(int deltaY)
{
	if (m_Light == nullptr || deltaY == 0) {
		return;
	}

	RECT rect;
	GetClientRect(&rect);
	if (rect.bottom <= rect.top) {
		return;
	}

	float deltay = (float)deltaY / (float)(rect.bottom - rect.top);
	float adjustment = deltay * (m_ViewedSphere.Radius * 3.0F);

	Matrix3D transform = m_Light->Get_Transform();
	transform.Translate(Vector3(0, 0, adjustment));

	float distance = (transform.Get_Translation() - m_ViewedSphere.Center).Length();
	if (distance > m_ViewedSphere.Radius) {
		if (m_LightMesh != nullptr) {
			m_LightMesh->Set_Transform(transform);
		}
		m_Light->Set_Transform(transform);
	}
}

void
CMaterialPreviewPane::Get_Camera_State(Matrix3D &transform, Vector3 &center,
	Quaternion &rotation, float &distance, float &minZoomAdjust) const
{
	transform = (m_Camera != nullptr) ? m_Camera->Get_Transform() : Matrix3D(1);
	center = m_SphereCenter;
	rotation = m_Rotation;
	distance = m_CameraDistance;
	minZoomAdjust = m_MinZoomAdjust;
}

void
CMaterialPreviewPane::Set_Camera_State(const Matrix3D &transform, const Vector3 &center,
	const Quaternion &rotation, float distance, float minZoomAdjust)
{
	m_SphereCenter = center;
	m_Rotation = rotation;
	m_CameraDistance = distance;
	m_MinZoomAdjust = minZoomAdjust;
	if (m_Camera != nullptr) {
		m_Camera->Set_Transform(transform);
	}
}

// 3ds Max-style orbit (mirrors CGraphicView::Orbit_Camera): pitch about the
// camera-local X and yaw about world Z through m_SphereCenter, both scaled to
// the viewport so a full sweep is 360/180 degrees.
void
CMaterialPreviewPane::Orbit_Camera(int deltaX, int deltaY)
{
	if (m_Camera == nullptr) {
		return;
	}

	RECT rect;
	GetClientRect(&rect);
	const float viewW = float(rect.right  > 1 ? rect.right  : 1);
	const float viewH = float(rect.bottom > 1 ? rect.bottom : 1);

	const float yawRad = DEG_TO_RADF( (float)deltaX / viewW * 360.0f );
	float pitchRad = DEG_TO_RADF( -(float)deltaY / viewH * 180.0f );

	// Respect the shared "Invert Movement (Y)" preference (View menu / persisted
	// in the profile), read live from the main viewport so a toggle applies here too.
	CW3DViewDoc *doc = GetCurrentDocument();
	CGraphicView *mainView = (doc != nullptr) ? doc->GetGraphicView() : nullptr;
	if (mainView != nullptr && mainView->Is_Invert_Camera_Y()) {
		pitchRad = -pitchRad;
	}

	Matrix3D transform = m_Camera->Get_Transform();

	// Pitch: local X rotation about the camera-space image of the orbit centre.
	{
		Matrix3D inverseMatrix;
		transform.Get_Orthogonal_Inverse(inverseMatrix);
		Vector3 to_object;
		inverseMatrix.mulVector3(m_SphereCenter, to_object);

		transform.Translate(to_object);
		transform.Rotate_X(pitchRad);
		transform.Translate(-to_object);
	}

	// Yaw: world-space Z rotation about the orbit centre (no roll).
	transform.Set_Translation(transform.Get_Translation() - m_SphereCenter);
	transform.Pre_Rotate_Z(yawRad);
	transform.Set_Translation(transform.Get_Translation() + m_SphereCenter);

	// Keep the accumulator in sync so pan uses the right basis.
	m_Rotation = Build_Quaternion(transform);
	m_Camera->Set_Transform(transform);
}

// Confines the cursor to the pane's client rect during a drag.
void
CMaterialPreviewPane::Clip_Cursor_To_View()
{
	RECT rect;
	GetClientRect(&rect);
	CPoint tl(rect.left, rect.top), br(rect.right, rect.bottom);
	ClientToScreen(&tl);
	ClientToScreen(&br);
	RECT screen = { tl.x, tl.y, br.x, br.y };
	::ClipCursor(&screen);
}

// Wraps the cursor to the opposite edge when it reaches a border, so orbit/pan
// drags are effectively infinite.
bool
CMaterialPreviewPane::Wrap_Cursor_In_View(CPoint &point)
{
	const int EDGE_MARGIN = 4;

	RECT rect;
	GetClientRect(&rect);

	const int w = rect.right - rect.left;
	const int h = rect.bottom - rect.top;
	if (w <= 2 * EDGE_MARGIN || h <= 2 * EDGE_MARGIN) {
		return false;
	}

	int newX = point.x;
	int newY = point.y;

	if (point.x <= rect.left + EDGE_MARGIN) {
		newX = rect.right - EDGE_MARGIN - 1;
	} else if (point.x >= rect.right - EDGE_MARGIN - 1) {
		newX = rect.left + EDGE_MARGIN + 1;
	}

	if (point.y <= rect.top + EDGE_MARGIN) {
		newY = rect.bottom - EDGE_MARGIN - 1;
	} else if (point.y >= rect.bottom - EDGE_MARGIN - 1) {
		newY = rect.top + EDGE_MARGIN + 1;
	}

	if (newX == point.x && newY == point.y) {
		return false;
	}

	CPoint screen(newX, newY);
	ClientToScreen(&screen);
	::SetCursorPos(screen.x, screen.y);

	point.x = newX;
	point.y = newY;
	m_LastPoint = point;
	return true;
}

void
CMaterialPreviewPane::Render()
{
	if (!IsWindowVisible()) {
		return;
	}

	// Never touch the render target while the device is lost or pending reset;
	// the main view's Begin_Render handles the reset and our cleanup hook
	// releases the swap chain for it.
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device == nullptr || device->TestCooperativeLevel() != D3D_OK) {
		Free_Swap_Chain();
		return;
	}

	// Recreate lazily, at most once per rendered frame. Recreating directly
	// in OnSize churns swap chains and depth surfaces many times per frame
	// during a drag-resize of the viewer window, which pressures the shared
	// device's video memory and destabilizes the main view.
	if (m_SwapChain == nullptr || m_SizeDirty) {
		Create_Swap_Chain();
		m_SizeDirty = false;
		if (m_SwapChain == nullptr) {
			return;
		}
	}

	// Create_Swap_Chain guarantees the pane-sized depth buffer exists whenever
	// the swap chain does; never render against the device default depth.
	if (m_DepthBuffer == nullptr) {
		Free_Swap_Chain();
		return;
	}

	// Match the main viewport's per-theme default background.
	float bg = W3DDarkMode::IsDark() ? W3DVIEW_BG_DARK_F : W3DVIEW_BG_LIGHT_F;

	IDirect3DSurface8 *back_buffer = nullptr;
	m_SwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &back_buffer);
	if (back_buffer == nullptr) {
		Free_Swap_Chain();
		return;
	}
	DX8Wrapper::Set_Render_Target(back_buffer, m_DepthBuffer);
	back_buffer->Release();

	// Advance WW3D's global sync clock by the real elapsed time so animated UV
	// mappers (linear-offset scrolling, grid, rotate, ...) keep moving. Their
	// texture matrices are driven by WW3D::Get_Sync_Time(), which only advances
	// via WW3D::Sync(). While the viewer is open the main viewport is paused and
	// no longer calls Sync, so without this the scrolling UVs freeze.
	{
		DWORD now = ::GetTickCount();
		DWORD elapsed = (m_LastRenderTicks != 0) ? (now - m_LastRenderTicks) : 0;
		m_LastRenderTicks = now;
		// Clamp the first/stalled frame so a long pause doesn't jump the UVs.
		if (elapsed > 250) {
			elapsed = 250;
		}
		if (elapsed > 0) {
			WW3D::Update_Logic_Frame_Time((float)elapsed);
			WW3D::Sync(true);
		}

		// Advance and (re)apply the animation pose. Applying every frame (not
		// only while playing) keeps the pose after a post-save reload swapped in
		// a fresh render object.
		if (m_Anim != nullptr && m_RenderObj != nullptr) {
			int frames = m_Anim->Get_Num_Frames();
			if (m_AnimPlaying && frames > 1 && elapsed > 0) {
				m_AnimFrame += m_Anim->Get_Frame_Rate() * ((float)elapsed * 0.001F);
				m_AnimFrame = ::fmodf(m_AnimFrame, (float)frames);
			}
			m_RenderObj->Set_Animation(m_Anim, m_AnimFrame, RenderObjClass::ANIM_MODE_MANUAL);
		}
	}

	// A click on the view cube turns the camera over a few frames
	Matrix3D snapped;
	if (m_ViewCube.Update_Snap(m_SphereCenter, m_Camera->Get_Transform(), snapped)) {
		m_Rotation = Build_Quaternion(snapped);
		m_Camera->Set_Transform(snapped);
	}

	WW3D::Begin_Render(true, true, Vector3(bg, bg, bg));

	// Ground first (see Render_Ground_Plane for why it precedes the scene).
	Render_Ground_Plane();

	WW3D::Render(m_Scene, m_Camera, FALSE, FALSE);

	// TheSuperHackers @feature Tria Second pass: tint the normally-culled back faces,
	// matching the main viewport (shared toggle + colour + pass logic).
	CGraphicView::Render_Backface_Tint_Pass(m_Scene, m_Camera);

	// Outline the selected mesh while the whole object is displayed.
	Render_Highlight_Outline();

	// View cube and the 7 key statistics, over everything else
	m_ViewCube.Render(m_hWnd, m_Camera->Get_Transform());
	if (ViewportStats::Is_Visible()) {
		ViewportStats::Render(m_RenderObj);
	}

	WW3D::End_Render(false);

#if defined(BUILD_WITH_D3D9)
	HRESULT hr = m_SwapChain->Present(nullptr, nullptr, nullptr, nullptr, 0);
#else
	HRESULT hr = m_SwapChain->Present(nullptr, nullptr, nullptr, nullptr);
#endif
	DX8Wrapper::Set_Render_Target((IDirect3DSurface8 *)nullptr);

	if (FAILED(hr)) {
		// Device was reset (e.g. main window resize); recreate next frame.
		Free_Swap_Chain();
	}
}

void
CMaterialPreviewPane::OnSize(UINT type, int cx, int cy)
{
	CWnd::OnSize(type, cx, cy);
	if (cx > 0 && cy > 0) {
		// Deferred to the next Render(): WM_SIZE arrives many times per frame
		// during a drag-resize and each recreation hits the shared device.
		m_SizeDirty = true;
	}
}

// Mouse controls mirror the main viewport (CGraphicView): left-drag orbits,
// right-drag zooms, middle-drag pans, middle+Alt orbits, wheel zooms. Cursor is
// clipped to the pane and wrapped at the edges for infinite orbit/pan drags.

void
CMaterialPreviewPane::OnLButtonDown(UINT flags, CPoint point)
{
	// The view cube takes the clicks that land on it
	if (!m_bRMouseDown && !m_bMMouseDown && m_Camera != nullptr
			&& m_ViewCube.On_LButton_Down(m_hWnd, m_Camera->Get_Transform(), point)) {
		SetCapture();
		return;
	}

	SetCapture();
	Clip_Cursor_To_View();
	m_bMouseDown = TRUE;
	m_LastPoint = point;

	// In Per Face mode a Ctrl+click places the light on the clicked surface
	// point immediately (a drag then keeps following the cursor via
	// OnMouseMove). Free Roam is relative, so the click alone does nothing.
	if (flags & MK_CONTROL) {
		Sync_Light_Mesh(true);
		if (m_LightPlacementMode == LIGHT_PER_FACE) {
			Position_Light_At_Cursor(point);
		}
	}

	CWnd::OnLButtonDown(flags, point);
}

void
CMaterialPreviewPane::OnLButtonUp(UINT flags, CPoint point)
{
	if (m_ViewCube.On_LButton_Up()) {
		if (!m_bRMouseDown && !m_bMMouseDown) {
			ReleaseCapture();
		}
		return;
	}

	m_bMouseDown = FALSE;
	if (!m_bRMouseDown && !m_bMMouseDown) {
		::ClipCursor(nullptr);
		ReleaseCapture();
	}
	CWnd::OnLButtonUp(flags, point);
}

void
CMaterialPreviewPane::OnRButtonDown(UINT flags, CPoint point)
{
	SetCapture();
	Clip_Cursor_To_View();
	m_bRMouseDown = TRUE;
	m_LastPoint = point;
	CWnd::OnRButtonDown(flags, point);
}

void
CMaterialPreviewPane::OnRButtonUp(UINT flags, CPoint point)
{
	m_bRMouseDown = FALSE;
	if (!m_bMouseDown && !m_bMMouseDown) {
		::ClipCursor(nullptr);
		ReleaseCapture();
	}
	CWnd::OnRButtonUp(flags, point);
}

void
CMaterialPreviewPane::OnMButtonDown(UINT flags, CPoint point)
{
	SetCapture();
	Clip_Cursor_To_View();
	m_bMMouseDown = TRUE;
	m_LastPoint = point;
	m_MButtonDownPoint = point;
	m_AltOnMDown = (::GetKeyState(VK_MENU) & 0x8000) != 0;
	CWnd::OnMButtonDown(flags, point);
}

void
CMaterialPreviewPane::OnMButtonUp(UINT flags, CPoint point)
{
	m_bMMouseDown = FALSE;
	m_AltOnMDown = false;

	// Shift+middle-click (no drag) re-frames the camera, like the main viewport.
	int dx = point.x - m_MButtonDownPoint.x;
	int dy = point.y - m_MButtonDownPoint.y;
	if ((dx * dx + dy * dy) < 16 && (flags & MK_SHIFT)) {
		Reset_Camera_To_Sphere(m_ViewedSphere);
	}

	if (!m_bMouseDown && !m_bRMouseDown) {
		::ClipCursor(nullptr);
		ReleaseCapture();
	}
	CWnd::OnMButtonUp(flags, point);
}

void
CMaterialPreviewPane::OnMouseMove(UINT flags, CPoint point)
{
	int iDeltaX = m_LastPoint.x - point.x;
	int iDeltaY = m_LastPoint.y - point.y;

	// A drag that began on the view cube orbits the camera
	int cubeDeltaX = 0;
	int cubeDeltaY = 0;
	if (m_ViewCube.On_Mouse_Move(point, cubeDeltaX, cubeDeltaY)) {
		if (cubeDeltaX != 0 || cubeDeltaY != 0) {
			Orbit_Camera(cubeDeltaX, cubeDeltaY);
		}
		return;
	}

	// The light gizmo is visible exactly while Ctrl is held over the pane
	// (mirrors the main viewport's behaviour).
	Sync_Light_Mesh((flags & MK_CONTROL) != 0);

	// Middle + Alt: orbit. Middle only, or left+right: pan. Ctrl+left: rotate
	// the light. Ctrl+right: light distance. Left only: orbit. Right only:
	// zoom on vertical drag.
	if (m_bMMouseDown && m_AltOnMDown) {
		Orbit_Camera(iDeltaX, iDeltaY);
		m_LastPoint = point;
		Wrap_Cursor_In_View(point);
	}
	else if (m_bMMouseDown || (m_bMouseDown && m_bRMouseDown)) {
		Matrix3D transform = m_Camera->Get_Transform();

		RECT rect;
		GetClientRect(&rect);
		float midPointX = float(rect.right >> 1);
		float midPointY = float(rect.bottom >> 1);

		float lastPointX = ((float)m_LastPoint.x - midPointX) / midPointX;
		float lastPointY = (midPointY - (float)m_LastPoint.y) / midPointY;
		float pointX = ((float)point.x - midPointX) / midPointX;
		float pointY = (midPointY - (float)point.y) / midPointY;

		Vector3 cameraPan(-1.0F * m_CameraDistance * (pointX - lastPointX),
			-1.0F * m_CameraDistance * (pointY - lastPointY), 0.0F);
		transform.Translate(cameraPan);

		Matrix3x3 view = Build_Matrix3(m_Rotation);
		m_SphereCenter += view * cameraPan;

		m_Camera->Set_Transform(transform);
		m_LastPoint = point;
	}
	else if ((flags & MK_CONTROL) && m_bMouseDown) {
		if (m_LightPlacementMode == LIGHT_PER_FACE) {
			Position_Light_At_Cursor(point);
		} else {
			Rotate_Light(point);
		}
		m_LastPoint = point;
	}
	else if ((flags & MK_CONTROL) && m_bRMouseDown) {
		Adjust_Light_Distance(iDeltaY);
		m_LastPoint = point;
	}
	else if (m_bMouseDown) {
		Orbit_Camera(iDeltaX, iDeltaY);
		m_LastPoint = point;
		Wrap_Cursor_In_View(point);
	}
	else if (m_bRMouseDown) {
		if (iDeltaY != 0) {
			Matrix3D transform = m_Camera->Get_Transform();

			RECT rect;
			GetClientRect(&rect);
			float deltay = (float)iDeltaY / (float)(rect.bottom - rect.top);
			float adjustment = deltay * m_CameraDistance * 3.0F;

			if ((adjustment < m_MinZoomAdjust) && (adjustment >= 0.0F)) adjustment = m_MinZoomAdjust;
			if ((adjustment > -m_MinZoomAdjust) && (adjustment <= 0.0F)) adjustment = -m_MinZoomAdjust;

			if ((m_CameraDistance + adjustment) > 0.0F) {
				m_CameraDistance += adjustment;
				transform.Translate(Vector3(0.0F, 0.0F, adjustment));
				m_Camera->Set_Transform(transform);
			}
		}
		m_LastPoint = point;
	}

	CWnd::OnMouseMove(flags, point);
}

BOOL
CMaterialPreviewPane::OnMouseWheel(UINT flags, short delta, CPoint point)
{
	if (m_Camera != nullptr) {
		Matrix3D transform = m_Camera->Get_Transform();

		// Positive delta = scroll up = zoom in (toward the object).
		float deltay = -(float)delta / (float)WHEEL_DELTA * 0.1F;
		float adjustment = deltay * m_CameraDistance * 3.0F;

		if ((adjustment < m_MinZoomAdjust) && (adjustment >= 0.0F)) adjustment = m_MinZoomAdjust;
		if ((adjustment > -m_MinZoomAdjust) && (adjustment <= 0.0F)) adjustment = -m_MinZoomAdjust;

		if ((m_CameraDistance + adjustment) > 0.0F) {
			m_CameraDistance += adjustment;
			transform.Translate(Vector3(0.0F, 0.0F, adjustment));
			m_Camera->Set_Transform(transform);
		}
	}
	return TRUE;
}
