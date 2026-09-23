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

#include "dx8skinning.h"
#include "dx8wrapper.h"
#include "dx8caps.h"
#include "dx8fvf.h"
#include "dx8instancing.h"
#include "dx8rendererdebugger.h"
#include "htree.h"
#include "lightenvironment.h"
#include "mesh.h"
#include "meshmdl.h"
#include "vertmaterial.h"
#include "ww3d.h"
#include <stdlib.h>
#include <string.h>

DX8VertexShadingClass::PassType DX8SkinningClass::Pass = DX8VertexShadingClass::PASS_NONE;
IDirect3DVertexShader9 * DX8SkinningClass::PassShader = nullptr;
IDirect3DVertexShader9 * DX8SkinningClass::MainShader = nullptr;
bool DX8SkinningClass::Sweeping = false;

// Position, normal, colour, the palette index in the low byte of the specular colour, and two UV sets.
static const unsigned SKIN_VERTEX_FVF = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX2;

// The format fixed function sees for the skins it deforms, which decides how vertex colours light.
static const unsigned DEFORMED_VERTEX_FVF = DX8_FVF_XYZNDUV2;

unsigned DX8SkinningClass::Get_Vertex_FVF()
{
	return SKIN_VERTEX_FVF;
}

#if defined(BUILD_WITH_D3D9)

// Bisects skinning faults without a rebuild. CONTRA_GPU_SKINNING=0 deforms every skin on the CPU,
// 1 skins only the shadow depth pass, and 2 adds the main scene's skins without material passes.
enum { SKINNING_OFF = 0, SKINNING_SHADOW_DEPTH = 1, SKINNING_BASE = 2, SKINNING_FULL = 3 };

static int Get_Skinning_Mode()
{
	const char *value = getenv("CONTRA_GPU_SKINNING");
	return (value != nullptr) ? atoi(value) : SKINNING_FULL;
}

static const int SkinningMode = Get_Skinning_Mode();

static IDirect3DVertexDeclaration9 *	Declaration = nullptr;
static DX8SkinningClass::StatsStruct	Stats;

// What the draw hook reads for the current mesh.
static MeshClass *							CurrentMesh = nullptr;
static VertexMaterialClass *				CurrentMaterial = nullptr;
static LightEnvironmentClass *			CurrentLights = nullptr;
static Vector3									CurrentAmbient(0.0f, 0.0f, 0.0f);
static bool										MaterialPassDraws = false;

static IDirect3DVertexDeclaration9 * Get_Declaration()
{
	if (Declaration != nullptr)
	{
		return Declaration;
	}
	const D3DVERTEXELEMENT9 elements[] =
	{
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
		{ 0, 24, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
		{ 0, 28, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 1 },
		{ 0, 32, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		{ 0, 40, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
		D3DDECL_END()
	};
	if (FAILED(DX8Wrapper::_Get_D3D_Device8()->CreateVertexDeclaration(elements, &Declaration)))
	{
		Declaration = nullptr;
	}
	return Declaration;
}

// The shader indexes the palette with vs_2_0's address register, which needs every register it can name.
bool DX8SkinningClass::Is_Supported()
{
	if (SkinningMode == SKINNING_OFF)
	{
		return false;
	}
	const DX8Caps * caps = DX8Wrapper::Get_Current_Caps();
	const D3DCAPS8 & dx8caps = caps->Get_DX8_Caps();
	if (caps->Get_Vertex_Shader_Major_Version() < 2 || dx8caps.MaxVertexShaderConst < DX8VertexShadingClass::CONSTANT_REGISTERS ||
		!(dx8caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT))
	{
		return false;
	}
	return DX8Wrapper::_Get_D3D_Device8()->GetSoftwareVertexProcessing() == FALSE;
}

void DX8SkinningClass::Set_Main_Shader(IDirect3DVertexShader9 * shader)
{
	MainShader = shader;
}

void DX8SkinningClass::Begin_Shadow_Depth_Pass(IDirect3DVertexShader9 * shader)
{
	if (shader == nullptr || SkinningMode < SKINNING_SHADOW_DEPTH || !Is_Supported())
	{
		return;
	}
	Pass = DX8VertexShadingClass::PASS_SHADOW_DEPTH;
	PassShader = shader;
}

void DX8SkinningClass::Begin_Lit_Pass()
{
	if (MainShader == nullptr || SkinningMode < SKINNING_BASE || !Is_Supported())
	{
		return;
	}
	Pass = DX8VertexShadingClass::PASS_LIT;
	PassShader = MainShader;
}

void DX8SkinningClass::End_Pass()
{
	WWASSERT(!Sweeping);
	Pass = DX8VertexShadingClass::PASS_NONE;
	PassShader = nullptr;
}

// Camera-facing, sorted and point-lit skins, and skins with their own passes, deform on the CPU.
bool DX8SkinningClass::Allows_Mesh(MeshClass * mesh)
{
	MeshModelClass * model = mesh->Peek_Model();
	if (model->Get_Flag(MeshModelClass::ALIGNED) || model->Get_Flag(MeshModelClass::ORIENTED))
	{
		return false;
	}
	if (model->Get_Flag(MeshGeometryClass::SORT) && WW3D::Is_Sorting_Enabled())
	{
		return false;
	}
	if (mesh->Get_Container() == nullptr || mesh->Get_Container()->Get_HTree() == nullptr)
	{
		return false;
	}
	if (DX8RendererDebugger::Is_Enabled())
	{
		return false;
	}
	if (Pass == DX8VertexShadingClass::PASS_LIT)
	{
		if (mesh->Has_Material_Pass_Override())
		{
			return false;
		}
		LightEnvironmentClass * environment = mesh->Get_Lighting_Environment();
		if (environment == nullptr)
		{
			return false;
		}
		for (int i=0;i<environment->Get_Light_Count();++i)
		{
			if (environment->isPointLight(i))
			{
				return false;
			}
		}
	}
	return true;
}

bool DX8SkinningClass::Allows_Category(const ShaderClass & shader, VertexMaterialClass * material, bool second_stage_textured)
{
	return DX8VertexShadingClass::Allows_Category(Pass, shader, material, DEFORMED_VERTEX_FVF, second_stage_textured);
}

// A pass tests depth EQUAL against its base pass, so a mesh's passes must go through the same shader.
bool DX8SkinningClass::Allows_Material_Pass(const MaterialPassClass * pass)
{
	return Pass == DX8VertexShadingClass::PASS_LIT && SkinningMode >= SKINNING_FULL && DX8InstancingClass::Is_Vertex_Shader_Material_Pass(pass);
}

void DX8SkinningClass::Record_Rejection(RejectionType reason)
{
	Stats.Rejections[reason]++;
}

void DX8SkinningClass::Record_Skinned_Meshes(int count)
{
	Stats.SkinnedMeshes[Pass == DX8VertexShadingClass::PASS_LIT ? 0 : 1] += count;
}

bool DX8SkinningClass::Begin_Sweep()
{
	WWASSERT(!Sweeping);
	if (Pass == DX8VertexShadingClass::PASS_NONE || Get_Declaration() == nullptr)
	{
		return false;
	}
	if (Pass == DX8VertexShadingClass::PASS_LIT)
	{
		// Clip planes are given in world space, which a vertex shader would reinterpret in clip space.
		if (DX8VertexShadingClass::Get_Device_Render_State(D3DRS_CLIPPLANEENABLE) != 0)
		{
			Stats.Rejections[REJECT_STATE]++;
			return false;
		}
		const DWORD fog_mode = DX8VertexShadingClass::Get_Device_Render_State(D3DRS_FOGVERTEXMODE);
		if (fog_mode != D3DFOG_NONE && fog_mode != D3DFOG_LINEAR)
		{
			Stats.Rejections[REJECT_STATE]++;
			return false;
		}
	}
	Sweeping = true;
	MaterialPassDraws = false;
	CurrentMesh = nullptr;
	CurrentMaterial = nullptr;
	CurrentLights = nullptr;
	DX8Wrapper::Begin_Vertex_Shader_Override(Declaration, PassShader, &Apply_Draw_Constants);
	return true;
}

void DX8SkinningClass::End_Sweep()
{
	WWASSERT(Sweeping);
	DX8Wrapper::End_Vertex_Shader_Override();
	Sweeping = false;
	CurrentMesh = nullptr;
	CurrentMaterial = nullptr;
	CurrentLights = nullptr;
}

void DX8SkinningClass::Set_Mesh(MeshClass * mesh, const PaletteStruct & palette, VertexMaterialClass * material)
{
	WWASSERT(Sweeping);
	CurrentMaterial = material;
	if (mesh == CurrentMesh)
	{
		return;
	}
	CurrentMesh = mesh;

	Vector4 rows[MAX_BONES * 3];
	const HTreeClass * htree = mesh->Get_Container()->Get_HTree();
	for (int bone=0;bone<palette.Count;++bone)
	{
		const Matrix3D & transform = htree->Get_Transform(palette.Pivots[bone]);
		rows[bone * 3] = transform[0];
		rows[bone * 3 + 1] = transform[1];
		rows[bone * 3 + 2] = transform[2];
	}
	DX8Wrapper::Set_Vertex_Shader_Constant(DX8VertexShadingClass::CONSTANT_PALETTE, rows, palette.Count * 3);

	CurrentLights = nullptr;
	CurrentAmbient.Set(0.0f, 0.0f, 0.0f);
	if (Pass == DX8VertexShadingClass::PASS_LIT)
	{
		CurrentLights = mesh->Get_Lighting_Environment();
		CurrentAmbient = CurrentLights->Get_Equivalent_Ambient();
	}
}

void DX8SkinningClass::Begin_Material_Passes()
{
	WWASSERT(Sweeping);
	MaterialPassDraws = true;
}

// Runs before each skinned draw, once the device holds the state the draw uses.
void DX8SkinningClass::Apply_Draw_Constants()
{
	Vector4 constants[DX8VertexShadingClass::CONSTANT_COUNT];
	DX8VertexShadingClass::Get_View_Constants(constants);

	// The lighting constants the base pass left behind light nothing a pass's pixel shader reads.
	if (MaterialPassDraws)
	{
		DX8VertexShadingClass::Get_Stage_Constants(&constants[DX8VertexShadingClass::CONSTANT_STAGE_SOURCE], &constants[DX8VertexShadingClass::CONSTANT_STAGE_COLUMNS]);
		DX8Wrapper::Set_Vertex_Shader_Constant(DX8VertexShadingClass::CONSTANT_VIEW_PROJECTION, &constants[DX8VertexShadingClass::CONSTANT_VIEW_PROJECTION], DX8VertexShadingClass::CONSTANT_VIEW + 3);
		DX8Wrapper::Set_Vertex_Shader_Constant(DX8VertexShadingClass::CONSTANT_STAGE_SOURCE, &constants[DX8VertexShadingClass::CONSTANT_STAGE_SOURCE],
			DX8VertexShadingClass::CONSTANT_COUNT - DX8VertexShadingClass::CONSTANT_STAGE_SOURCE);
		return;
	}

	// Begin_Sweep ruled out the fog modes the shader cannot match.
	Vector4 fog(1.0f, 0.0f, 0.0f, 0.0f);
	if (Pass == DX8VertexShadingClass::PASS_LIT)
	{
		DX8VertexShadingClass::Get_Fog_Constant(fog);
	}
	DX8VertexShadingClass::Get_Material_Constants(Pass, CurrentMaterial, DEFORMED_VERTEX_FVF, fog, constants);
	if (Pass != DX8VertexShadingClass::PASS_LIT)
	{
		DX8Wrapper::Set_Vertex_Shader_Constant(0, constants, DX8VertexShadingClass::CONSTANT_DIFFUSE_ALPHA + 1);
		return;
	}

	constants[DX8VertexShadingClass::CONSTANT_SKIN_AMBIENT].Set(CurrentAmbient.X, CurrentAmbient.Y, CurrentAmbient.Z, 0.0f);
	constants[DX8VertexShadingClass::CONSTANT_SKIN_AMBIENT + 1].Set(0.0f, 0.0f, 0.0f, 0.0f);
	constants[DX8VertexShadingClass::CONSTANT_SKIN_AMBIENT + 2].Set(0.0f, 0.0f, 0.0f, 0.0f);
	DX8VertexShadingClass::Get_Light_Constants(CurrentLights, &constants[DX8VertexShadingClass::CONSTANT_LIGHT_DIRECTION]);
	DX8Wrapper::Set_Vertex_Shader_Constant(0, constants, DX8VertexShadingClass::CONSTANT_COUNT);
}

void DX8SkinningClass::Take_Stats(StatsStruct & stats)
{
	stats = Stats;
	memset(&Stats, 0, sizeof(Stats));
}

void DX8SkinningClass::Shutdown()
{
	if (Declaration != nullptr)
	{
		Declaration->Release();
		Declaration = nullptr;
	}
	Sweeping = false;
	End_Pass();
}

#else

void DX8SkinningClass::Take_Stats(StatsStruct & stats) { memset(&stats, 0, sizeof(stats)); }
bool DX8SkinningClass::Is_Supported() { return false; }
void DX8SkinningClass::Shutdown() {}
void DX8SkinningClass::Set_Main_Shader(IDirect3DVertexShader9 * shader) {}
void DX8SkinningClass::Begin_Shadow_Depth_Pass(IDirect3DVertexShader9 * shader) {}
void DX8SkinningClass::Begin_Lit_Pass() {}
void DX8SkinningClass::End_Pass() {}
bool DX8SkinningClass::Allows_Mesh(MeshClass * mesh) { return false; }
bool DX8SkinningClass::Allows_Category(const ShaderClass & shader, VertexMaterialClass * material, bool second_stage_textured) { return false; }
bool DX8SkinningClass::Allows_Material_Pass(const MaterialPassClass * pass) { return false; }
void DX8SkinningClass::Record_Rejection(RejectionType reason) {}
void DX8SkinningClass::Record_Skinned_Meshes(int count) {}
bool DX8SkinningClass::Begin_Sweep() { return false; }
void DX8SkinningClass::End_Sweep() {}
void DX8SkinningClass::Set_Mesh(MeshClass * mesh, const PaletteStruct & palette, VertexMaterialClass * material) {}
void DX8SkinningClass::Begin_Material_Passes() {}
void DX8SkinningClass::Apply_Draw_Constants() {}

#endif
