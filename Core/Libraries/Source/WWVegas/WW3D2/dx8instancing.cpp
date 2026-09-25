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

#include "dx8instancing.h"
#include "dx8wrapper.h"
#include "dx8caps.h"
#include "dx8fvf.h"
#include "dx8polygonrenderer.h"
#include "dx8rendererdebugger.h"
#include "dx8vertexshading.h"
#include "lightenvironment.h"
#include "mesh.h"
#include "meshmdl.h"
#include "vertmaterial.h"
#include "shader.h"
#include "matpass.h"
#include "ww3d.h"
#include <stddef.h>
#include <stdlib.h>

DX8InstancingClass::PassType DX8InstancingClass::Pass = DX8InstancingClass::PASS_NONE;
IDirect3DVertexShader9 * DX8InstancingClass::PassShader = nullptr;
IDirect3DVertexShader9 * DX8InstancingClass::MainShader = nullptr;
const MaterialPassClass * DX8InstancingClass::InstancedPasses[2] = { nullptr, nullptr };

bool DX8InstancingClass::Is_Vertex_Shader_Material_Pass(const MaterialPassClass * pass)
{
	if (pass == nullptr)
	{
		return false;
	}
	const MaterialPassClass * key = pass->Peek_Vertex_Shading_Key();
	return key == InstancedPasses[0] || key == InstancedPasses[1];
}

#if defined(BUILD_WITH_D3D9)

// Bisects instancing faults without a rebuild. CONTRA_INSTANCING=0 draws every mesh on its own,
// 1 instances only the shadow depth pass, and 2 adds the main scene's meshes without material passes.
enum { INSTANCING_OFF = 0, INSTANCING_SHADOW_DEPTH = 1, INSTANCING_BASE = 2, INSTANCING_FULL = 3 };

static int Get_Instancing_Mode()
{
	const char *value = getenv("CONTRA_INSTANCING");
	return (value != nullptr) ? atoi(value) : INSTANCING_FULL;
}

static const int InstancingMode = Get_Instancing_Mode();

// World rows 0-2, then the lighting that differs from the group's, plus opaque white for
// meshes without vertex colours.
struct InstanceRecord
{
	float		World[3][4];
	float		Ambient[3];
	DWORD		White;
	float		DiffuseOffset[3];
	float		Unused;
};

enum
{
	INSTANCE_BUFFER_COUNT = DX8InstancingClass::MAX_INSTANCES,
	MAX_DECLARATIONS = 16,
	MAX_DECLARATION_ELEMENTS = 14,
};

struct DeclarationEntry
{
	unsigned								FVF;
	IDirect3DVertexDeclaration9 *	Declaration;
};

static IDirect3DVertexBuffer9 *	InstanceBuffer = nullptr;
static int								Rejections[DX8InstancingClass::REJECT_COUNT];
static unsigned						InstanceBufferUsed = 0;
static DeclarationEntry				Declarations[MAX_DECLARATIONS];
static int								DeclarationCount = 0;

static void Add_Element(D3DVERTEXELEMENT9 * elements, int & count, WORD stream, WORD offset, BYTE type, BYTE usage, BYTE usage_index)
{
	D3DVERTEXELEMENT9 & element = elements[count++];
	element.Stream = stream;
	element.Offset = offset;
	element.Type = type;
	element.Method = D3DDECLMETHOD_DEFAULT;
	element.Usage = usage;
	element.UsageIndex = usage_index;
}

// Stream 0 is the container's vertex buffer as it is. Inputs its format lacks read from the instance stream.
static IDirect3DVertexDeclaration9 * Get_Declaration(unsigned fvf)
{
	for (int i=0;i<DeclarationCount;++i)
	{
		if (Declarations[i].FVF == fvf)
		{
			return Declarations[i].Declaration;
		}
	}
	if (DeclarationCount == MAX_DECLARATIONS)
	{
		return nullptr;
	}

	const FVFInfoClass info(fvf);
	const unsigned tex_count = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	const WORD ambient_offset = (WORD)offsetof(InstanceRecord, Ambient);
	const WORD diffuse_offset = (WORD)offsetof(InstanceRecord, DiffuseOffset);

	D3DVERTEXELEMENT9 elements[MAX_DECLARATION_ELEMENTS];
	int count = 0;

	Add_Element(elements, count, 0, (WORD)info.Get_Location_Offset(), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0);
	if (fvf & D3DFVF_NORMAL)
	{
		Add_Element(elements, count, 0, (WORD)info.Get_Normal_Offset(), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL, 0);
	}
	if (fvf & D3DFVF_DIFFUSE)
	{
		Add_Element(elements, count, 0, (WORD)info.Get_Diffuse_Offset(), D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 0);
	}
	if (tex_count >= 1)
	{
		Add_Element(elements, count, 0, (WORD)info.Get_Tex_Offset(0), D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0);
		Add_Element(elements, count, 0, (WORD)info.Get_Tex_Offset(tex_count >= 2 ? 1 : 0), D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1);
	}

	Add_Element(elements, count, 1, 0, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_TEXCOORD, 4);
	Add_Element(elements, count, 1, 16, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_TEXCOORD, 5);
	Add_Element(elements, count, 1, 32, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_TEXCOORD, 6);
	Add_Element(elements, count, 1, ambient_offset, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_TEXCOORD, 7);
	Add_Element(elements, count, 1, diffuse_offset, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_TEXCOORD, 3);

	// Only meshes that ignore these inputs qualify without them, so any value will do.
	if (!(fvf & D3DFVF_NORMAL))
	{
		Add_Element(elements, count, 1, diffuse_offset, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL, 0);
	}
	if (tex_count == 0)
	{
		Add_Element(elements, count, 1, ambient_offset, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0);
		Add_Element(elements, count, 1, ambient_offset, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1);
	}
	if (!(fvf & D3DFVF_DIFFUSE))
	{
		Add_Element(elements, count, 1, (WORD)offsetof(InstanceRecord, White), D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 0);
	}

	const D3DVERTEXELEMENT9 end = D3DDECL_END();
	elements[count++] = end;
	WWASSERT(count <= MAX_DECLARATION_ELEMENTS);

	IDirect3DVertexDeclaration9 * declaration = nullptr;
	if (FAILED(DX8Wrapper::_Get_D3D_Device8()->CreateVertexDeclaration(elements, &declaration)))
	{
		return nullptr;
	}
	Declarations[DeclarationCount].FVF = fvf;
	Declarations[DeclarationCount].Declaration = declaration;
	DeclarationCount++;
	return declaration;
}

// Hands out room for count records, starting over with a discarded buffer when the ring is full.
static InstanceRecord * Lock_Instances(int count, unsigned & offset)
{
	if (InstanceBuffer == nullptr)
	{
		if (FAILED(DX8Wrapper::_Get_D3D_Device8()->CreateVertexBuffer(INSTANCE_BUFFER_COUNT * sizeof(InstanceRecord),
			D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &InstanceBuffer, nullptr)))
		{
			InstanceBuffer = nullptr;
			return nullptr;
		}
		InstanceBufferUsed = INSTANCE_BUFFER_COUNT;
	}

	DWORD flags = D3DLOCK_NOOVERWRITE;
	if (InstanceBufferUsed + count > INSTANCE_BUFFER_COUNT)
	{
		flags = D3DLOCK_DISCARD;
		InstanceBufferUsed = 0;
	}

	offset = InstanceBufferUsed * sizeof(InstanceRecord);
	void * data = nullptr;
	if (FAILED(InstanceBuffer->Lock(offset, count * sizeof(InstanceRecord), &data, flags)))
	{
		return nullptr;
	}
	InstanceBufferUsed += count;
	return static_cast<InstanceRecord *>(data);
}

// Writes each group's world matrices, and with lit set, what its lighting adds to the group's first mesh.
static bool Write_Instances(MeshClass * const * meshes, const int * counts, int group_count, bool lit, unsigned & offset)
{
	int total = 0;
	for (int group=0;group<group_count;++group)
	{
		total += counts[group];
	}
	InstanceRecord * records = (total <= INSTANCE_BUFFER_COUNT) ? Lock_Instances(total, offset) : nullptr;
	if (records == nullptr)
	{
		return false;
	}

	int first = 0;
	for (int group=0;group<group_count;++group)
	{
		LightEnvironmentClass * reference = lit ? meshes[first]->Get_Lighting_Environment() : nullptr;
		for (int i=first;i<first+counts[group];++i)
		{
			InstanceRecord & record = records[i];
			const Matrix3D & world = meshes[i]->Get_Transform();
			for (int row=0;row<3;++row)
			{
				record.World[row][0] = world[row].X;
				record.World[row][1] = world[row].Y;
				record.World[row][2] = world[row].Z;
				record.World[row][3] = world[row].W;
			}
			Vector3 ambient(0.0f, 0.0f, 0.0f);
			Vector3 diffuse_offset(0.0f, 0.0f, 0.0f);
			if (reference != nullptr)
			{
				LightEnvironmentClass * environment = meshes[i]->Get_Lighting_Environment();
				ambient = environment->Get_Equivalent_Ambient();
				if (environment->Get_Light_Count() > 0)
				{
					diffuse_offset = environment->Get_Light_Diffuse(0) - reference->Get_Light_Diffuse(0);
				}
			}
			record.Ambient[0] = ambient.X;
			record.Ambient[1] = ambient.Y;
			record.Ambient[2] = ambient.Z;
			record.White = 0xffffffff;
			record.DiffuseOffset[0] = diffuse_offset.X;
			record.DiffuseOffset[1] = diffuse_offset.Y;
			record.DiffuseOffset[2] = diffuse_offset.Z;
			record.Unused = 0.0f;
		}
		first += counts[group];
	}
	InstanceBuffer->Unlock();
	return true;
}

// Issues one draw per group. Lit groups take their first mesh's lights, and a material pass installs each group's own textures.
static void Draw_Instances(IDirect3DVertexDeclaration9 * declaration, IDirect3DVertexShader9 * shader, DX8PolygonRendererClass * const * renderers,
	const int * counts, int group_count, MeshClass * const * meshes, unsigned offset, const MaterialPassClass * pass)
{
	const bool lit = (pass == nullptr && DX8InstancingClass::Get_Pass() == DX8InstancingClass::PASS_LIT);
	DX8Wrapper::Begin_Instanced_Drawing(declaration, shader);
	int first = 0;
	for (int group=0;group<group_count;++group)
	{
		DX8PolygonRendererClass * renderer = renderers[group];
		if (lit)
		{
			Vector4 lights[DX8VertexShadingClass::MAX_LIGHTS * 2];
			DX8VertexShadingClass::Get_Light_Constants(meshes[first]->Get_Lighting_Environment(), lights);
			DX8Wrapper::Set_Vertex_Shader_Constant(DX8VertexShadingClass::CONSTANT_LIGHT_DIRECTION, lights, DX8VertexShadingClass::MAX_LIGHTS * 2);
		}
		if (pass != nullptr)
		{
			pass->Install_Polygon_Materials(renderer);
		}
		DX8Wrapper::Set_Index_Buffer_Index_Offset(meshes[first]->Get_Base_Vertex_Offset());
		DX8Wrapper::Draw_Instanced_Triangles(
			renderer->Get_Index_Offset(),
			renderer->Get_Index_Count() / 3,
			renderer->Get_Min_Vertex_Index(),
			renderer->Get_Vertex_Index_Range(),
			InstanceBuffer,
			offset + first * sizeof(InstanceRecord),
			sizeof(InstanceRecord),
			counts[group]);
		first += counts[group];
	}
	DX8Wrapper::End_Instanced_Drawing();
}

// Instancing needs shader model 3 hardware doing the vertex work, even though the shaders are vs_2_0.
bool DX8InstancingClass::Is_Supported()
{
	if (InstancingMode == INSTANCING_OFF)
	{
		return false;
	}
	const DX8Caps * caps = DX8Wrapper::Get_Current_Caps();
	const D3DCAPS8 & dx8caps = caps->Get_DX8_Caps();
	if (caps->Get_Vertex_Shader_Major_Version() < 3 || dx8caps.MaxStreams < 2 || !(dx8caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT))
	{
		return false;
	}
	return DX8Wrapper::_Get_D3D_Device8()->GetSoftwareVertexProcessing() == FALSE;
}

void DX8InstancingClass::Set_Main_Shader(IDirect3DVertexShader9 * shader)
{
	MainShader = shader;
}

void DX8InstancingClass::Set_Instanced_Material_Passes(const MaterialPassClass * first, const MaterialPassClass * second)
{
	InstancedPasses[0] = first;
	InstancedPasses[1] = second;
}

bool DX8InstancingClass::Is_Instanced_Material_Pass(const MaterialPassClass * pass)
{
	if (InstancingMode < INSTANCING_FULL || MainShader == nullptr)
	{
		return false;
	}
	return Is_Vertex_Shader_Material_Pass(pass);
}

void DX8InstancingClass::Begin_Shadow_Depth_Pass(IDirect3DVertexShader9 * shader)
{
	if (shader == nullptr || InstancingMode < INSTANCING_SHADOW_DEPTH || !Is_Supported())
	{
		return;
	}
	Pass = PASS_SHADOW_DEPTH;
	PassShader = shader;
}

void DX8InstancingClass::Begin_Lit_Pass()
{
	if (MainShader == nullptr || InstancingMode < INSTANCING_BASE || !Is_Supported())
	{
		return;
	}
	Pass = PASS_LIT;
	PassShader = MainShader;
}

void DX8InstancingClass::End_Pass()
{
	Pass = PASS_NONE;
	PassShader = nullptr;
}

bool DX8InstancingClass::Allows_Category(const ShaderClass & shader, VertexMaterialClass * material, unsigned fvf, bool second_stage_textured)
{
	return DX8VertexShadingClass::Allows_Category((DX8VertexShadingClass::PassType)Pass, shader, material, fvf, second_stage_textured);
}

// The instance data carries the world matrix and a tint, so camera-facing, skinned, overridden
// and point-lit meshes draw alone.
bool DX8InstancingClass::Allows_Mesh(MeshClass * mesh)
{
	MeshModelClass * model = mesh->Peek_Model();
	if (model->Get_Flag(MeshModelClass::ALIGNED) || model->Get_Flag(MeshModelClass::ORIENTED) || model->Get_Flag(MeshModelClass::SKIN))
	{
		return false;
	}
	if (model->Get_Flag(MeshGeometryClass::SORT) && WW3D::Is_Sorting_Enabled())
	{
		return false;
	}
	if (mesh->Get_Alpha_Override() != 1.0f || (mesh->Get_User_Data() && *(int *)mesh->Get_User_Data() == RenderObjClass::USER_DATA_MATERIAL_OVERRIDE))
	{
		return false;
	}
	if (DX8RendererDebugger::Is_Enabled())
	{
		return false;
	}
	if (Pass == PASS_LIT)
	{
		// Later passes of a multi-pass model and per-mesh pass overrides stay fixed function, and
		// a pass testing depth EQUAL must follow its base pass down the same path.
		if (model->Get_Pass_Count() != 1 || mesh->Has_Material_Pass_Override())
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

// Scene lights reach every object alike except for a tint added to each light and to the ambient.
bool DX8InstancingClass::Can_Share_Group(MeshClass * reference, MeshClass * mesh)
{
	if (Pass != PASS_LIT)
	{
		return true;
	}
	LightEnvironmentClass * a = reference->Get_Lighting_Environment();
	LightEnvironmentClass * b = mesh->Get_Lighting_Environment();
	const int light_count = a->Get_Light_Count();
	if (b->Get_Light_Count() != light_count)
	{
		return false;
	}
	if (light_count == 0)
	{
		return true;
	}
	const Vector3 offset = b->Get_Light_Diffuse(0) - a->Get_Light_Diffuse(0);
	for (int i=0;i<light_count;++i)
	{
		const Vector3 difference = b->Get_Light_Diffuse(i) - a->Get_Light_Diffuse(i) - offset;
		if (!(a->Get_Light_Direction(i) == b->Get_Light_Direction(i)) || difference.Length2() > 1.0e-8f)
		{
			return false;
		}
	}
	return true;
}

bool DX8InstancingClass::Draw_Groups(DX8PolygonRendererClass * const * renderers, const int * counts, int group_count, MeshClass * const * meshes, VertexMaterialClass * material, unsigned fvf)
{
	if (Pass == PASS_NONE || group_count == 0)
	{
		return false;
	}
	IDirect3DVertexDeclaration9 * declaration = Get_Declaration(fvf);
	if (declaration == nullptr)
	{
		Rejections[REJECT_RESOURCE]++;
		return false;
	}

	// Pending changes reach the device here, so the state read back below is the state in use.
	DX8Wrapper::Apply_Render_State_Changes();

	Vector4 fog;
	if (Pass == PASS_LIT)
	{
		// Clip planes are given in world space, which a vertex shader would reinterpret in clip space.
		if (DX8VertexShadingClass::Get_Device_Render_State(D3DRS_CLIPPLANEENABLE) != 0)
		{
			Rejections[REJECT_CLIP_PLANE]++;
			return false;
		}
		if (!DX8VertexShadingClass::Get_Fog_Constant(fog))
		{
			Rejections[REJECT_FOG]++;
			return false;
		}
	}

	unsigned offset = 0;
	if (!Write_Instances(meshes, counts, group_count, Pass == PASS_LIT, offset))
	{
		Rejections[REJECT_RESOURCE]++;
		return false;
	}

	Vector4 constants[DX8VertexShadingClass::CONSTANT_COUNT];
	DX8VertexShadingClass::Get_View_Constants(constants);
	DX8VertexShadingClass::Get_Material_Constants((DX8VertexShadingClass::PassType)Pass, material, fvf, fog, constants);
	if (Pass == PASS_LIT)
	{
		// The registers between fog and the stages were never filled, and the lights go per group.
		DX8Wrapper::Set_Vertex_Shader_Constant(0, constants, DX8VertexShadingClass::CONSTANT_FOG + 1);
		DX8Wrapper::Set_Vertex_Shader_Constant(DX8VertexShadingClass::CONSTANT_STAGE_SOURCE, &constants[DX8VertexShadingClass::CONSTANT_STAGE_SOURCE],
			DX8VertexShadingClass::CONSTANT_COUNT - DX8VertexShadingClass::CONSTANT_STAGE_SOURCE);
	}
	else
	{
		DX8Wrapper::Set_Vertex_Shader_Constant(0, constants, DX8VertexShadingClass::CONSTANT_DIFFUSE_ALPHA + 1);
	}

	Draw_Instances(declaration, PassShader, renderers, counts, group_count, meshes, offset, nullptr);
	return true;
}

bool DX8InstancingClass::Draw_Material_Pass_Groups(const MaterialPassClass * pass, DX8PolygonRendererClass * const * renderers, const int * counts, int group_count, MeshClass * const * meshes, unsigned fvf)
{
	if (Pass != PASS_LIT || group_count == 0)
	{
		return false;
	}
	IDirect3DVertexDeclaration9 * declaration = Get_Declaration(fvf);
	if (declaration == nullptr)
	{
		Rejections[REJECT_RESOURCE]++;
		return false;
	}

	DX8Wrapper::Apply_Render_State_Changes();

	// Only the matrices and stages change. The lighting constants the base pass left behind light
	// nothing a pass's pixel shader reads.
	Vector4 constants[DX8VertexShadingClass::CONSTANT_COUNT];
	DX8VertexShadingClass::Get_View_Constants(constants);
	DX8VertexShadingClass::Get_Stage_Constants(&constants[DX8VertexShadingClass::CONSTANT_STAGE_SOURCE], &constants[DX8VertexShadingClass::CONSTANT_STAGE_COLUMNS]);

	unsigned offset = 0;
	if (!Write_Instances(meshes, counts, group_count, false, offset))
	{
		Rejections[REJECT_RESOURCE]++;
		return false;
	}
	DX8Wrapper::Set_Vertex_Shader_Constant(DX8VertexShadingClass::CONSTANT_VIEW_PROJECTION, &constants[DX8VertexShadingClass::CONSTANT_VIEW_PROJECTION], DX8VertexShadingClass::CONSTANT_VIEW + 3);
	DX8Wrapper::Set_Vertex_Shader_Constant(DX8VertexShadingClass::CONSTANT_STAGE_SOURCE, &constants[DX8VertexShadingClass::CONSTANT_STAGE_SOURCE],
		DX8VertexShadingClass::CONSTANT_COUNT - DX8VertexShadingClass::CONSTANT_STAGE_SOURCE);

	Draw_Instances(declaration, MainShader, renderers, counts, group_count, meshes, offset, pass);
	return true;
}

void DX8InstancingClass::Take_Rejections(int * counts)
{
	for (int i=0;i<REJECT_COUNT;++i)
	{
		counts[i] = Rejections[i];
		Rejections[i] = 0;
	}
}

void DX8InstancingClass::Add_Rejections(RejectionType type, int count)
{
	Rejections[type] += count;
}

void DX8InstancingClass::Release_Resources()
{
	if (InstanceBuffer != nullptr)
	{
		InstanceBuffer->Release();
		InstanceBuffer = nullptr;
	}
}

void DX8InstancingClass::Shutdown()
{
	Release_Resources();
	for (int i=0;i<DeclarationCount;++i)
	{
		Declarations[i].Declaration->Release();
	}
	DeclarationCount = 0;
	End_Pass();
}

#else

void DX8InstancingClass::Shutdown() {}
void DX8InstancingClass::Release_Resources() {}
void DX8InstancingClass::Take_Rejections(int * counts)
{
	for (int i=0;i<REJECT_COUNT;++i)
	{
		counts[i] = 0;
	}
}
void DX8InstancingClass::Add_Rejections(RejectionType type, int count) {}
void DX8InstancingClass::Set_Main_Shader(IDirect3DVertexShader9 * shader) {}
void DX8InstancingClass::Set_Instanced_Material_Passes(const MaterialPassClass * first, const MaterialPassClass * second) {}
bool DX8InstancingClass::Is_Instanced_Material_Pass(const MaterialPassClass * pass) { return false; }
bool DX8InstancingClass::Draw_Material_Pass_Groups(const MaterialPassClass * pass, DX8PolygonRendererClass * const * renderers, const int * counts, int group_count, MeshClass * const * meshes, unsigned fvf) { return false; }
void DX8InstancingClass::Begin_Shadow_Depth_Pass(IDirect3DVertexShader9 * shader) {}
void DX8InstancingClass::Begin_Lit_Pass() {}
void DX8InstancingClass::End_Pass() {}
bool DX8InstancingClass::Is_Supported() { return false; }
bool DX8InstancingClass::Allows_Category(const ShaderClass & shader, VertexMaterialClass * material, unsigned fvf, bool second_stage_textured) { return false; }
bool DX8InstancingClass::Allows_Mesh(MeshClass * mesh) { return false; }
bool DX8InstancingClass::Can_Share_Group(MeshClass * reference, MeshClass * mesh) { return true; }
bool DX8InstancingClass::Draw_Groups(DX8PolygonRendererClass * const * renderers, const int * counts, int group_count, MeshClass * const * meshes, VertexMaterialClass * material, unsigned fvf) { return false; }

#endif
