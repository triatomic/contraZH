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
#include "formconv.h"
#include "lightenvironment.h"
#include "mesh.h"
#include "meshmdl.h"
#include "meshbuild.h"
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
	INSTANCE_BUFFER_COUNT = 8192,
	MAX_DECLARATIONS = 16,
	MAX_DECLARATION_ELEMENTS = 14,
	MAX_LIGHTS = 4,

	// Vertex shader constants, shared with the shaders in Shaders/instance*.hlsl
	CONSTANT_VIEW_PROJECTION = 0,
	CONSTANT_VIEW = 4,
	CONSTANT_DIFFUSE_ALPHA = 7,
	CONSTANT_MATERIAL_AMBIENT = 8,
	CONSTANT_MATERIAL_DIFFUSE = 9,
	CONSTANT_MATERIAL_EMISSIVE = 10,
	CONSTANT_COLOR_SOURCE = 11,
	CONSTANT_FOG = 12,
	CONSTANT_LIGHT_DIRECTION = 16,
	CONSTANT_LIGHT_DIFFUSE = 20,
	CONSTANT_STAGE_SOURCE = 24,
	CONSTANT_STAGE_COLUMNS = 28,
	CONSTANT_COUNT = 44,
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

static DWORD Get_Device_Render_State(D3DRENDERSTATETYPE state)
{
	// The wrapper's cache can hold a sentinel after an invalidate, and the device is not pure.
	DWORD value = 0;
	DX8Wrapper::_Get_D3D_Device8()->GetRenderState(state, &value);
	return value;
}

static bool Is_Lit(VertexMaterialClass * material)
{
	return material != nullptr && material->Get_Lighting() && !WW3D::Is_Coloring_Enabled();
}

static float Vertex_Color_Weight(VertexMaterialClass::ColorSourceType source, unsigned fvf)
{
	return (source == VertexMaterialClass::COLOR1 && (fvf & D3DFVF_DIFFUSE)) ? 1.0f : 0.0f;
}

// Vertex fog as the fixed-function pipeline computes it. False when the device fogs in a way the shader cannot match.
static bool Get_Fog_Constant(Vector4 & fog)
{
	fog.Set(1.0f, 0.0f, 0.0f, 0.0f);
	if (!Get_Device_Render_State(D3DRS_FOGENABLE) || Get_Device_Render_State(D3DRS_FOGTABLEMODE) != D3DFOG_NONE)
	{
		return true;
	}
	const DWORD mode = Get_Device_Render_State(D3DRS_FOGVERTEXMODE);
	if (mode == D3DFOG_NONE)
	{
		return true;
	}
	if (mode != D3DFOG_LINEAR)
	{
		return false;
	}
	const DWORD start_bits = Get_Device_Render_State(D3DRS_FOGSTART);
	const DWORD end_bits = Get_Device_Render_State(D3DRS_FOGEND);
	const float start = *(const float *)&start_bits;
	const float end = *(const float *)&end_bits;
	if (end != start)
	{
		fog.X = end / (end - start);
		fog.Y = 1.0f / (end - start);
	}
	fog.Z = Get_Device_Render_State(D3DRS_RANGEFOGENABLE) ? 1.0f : 0.0f;
	return true;
}

static void Set_Group_Lights(LightEnvironmentClass * environment)
{
	Vector4 constants[MAX_LIGHTS * 2];
	const int light_count = environment->Get_Light_Count();
	for (int i=0;i<MAX_LIGHTS;++i)
	{
		if (i < light_count)
		{
			Vector3 direction = environment->Get_Light_Direction(i);
			direction.Normalize();
			const Vector3 & diffuse = environment->Get_Light_Diffuse(i);
			constants[i].Set(direction.X, direction.Y, direction.Z, 1.0f);
			constants[MAX_LIGHTS + i].Set(diffuse.X, diffuse.Y, diffuse.Z, 0.0f);
		}
		else
		{
			constants[i].Set(0.0f, 0.0f, 0.0f, 0.0f);
			constants[MAX_LIGHTS + i].Set(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}
	DX8Wrapper::Set_Vertex_Shader_Constant(CONSTANT_LIGHT_DIRECTION, constants, MAX_LIGHTS * 2);
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

// The shaders dot row vectors with these columns, which is the device's own row-vector convention.
static void Get_View_Constants(Vector4 * constants)
{
	D3DMATRIX view;
	D3DMATRIX projection;
	DX8Wrapper::_Get_D3D_Device8()->GetTransform(D3DTS_VIEW, &view);
	DX8Wrapper::_Get_D3D_Device8()->GetTransform(D3DTS_PROJECTION, &projection);

	for (int column=0;column<4;++column)
	{
		float value[4];
		for (int row=0;row<4;++row)
		{
			value[row] = view.m[row][0] * projection.m[0][column] + view.m[row][1] * projection.m[1][column] +
				view.m[row][2] * projection.m[2][column] + view.m[row][3] * projection.m[3][column];
		}
		constants[CONSTANT_VIEW_PROJECTION + column].Set(value[0], value[1], value[2], value[3]);
	}
	for (int column=0;column<3;++column)
	{
		constants[CONSTANT_VIEW + column].Set(view.m[0][column], view.m[1][column], view.m[2][column], view.m[3][column]);
	}
}

// Fixed-function texture coordinates for stages 0-3 as the shader's source weights and transform columns.
// A pass's pixel shader reads only the stages it set up, so a stage with texgen the shader lacks gets zero.
static void Get_Stage_Constants(Vector4 * sources, Vector4 * columns)
{
	IDirect3DDevice9 * device = DX8Wrapper::_Get_D3D_Device8();
	for (int stage=0;stage<4;++stage)
	{
		DWORD index = 0;
		DWORD flags = 0;
		device->GetTextureStageState(stage, D3DTSS_TEXCOORDINDEX, &index);
		device->GetTextureStageState(stage, D3DTSS_TEXTURETRANSFORMFLAGS, &flags);

		switch (index & 0xffff0000)
		{
		case D3DTSS_TCI_PASSTHRU:
			sources[stage].Set((index & 0xffff) == 0 ? 1.0f : 0.0f, (index & 0xffff) == 1 ? 1.0f : 0.0f, 0.0f, 0.0f);
			break;
		case D3DTSS_TCI_CAMERASPACENORMAL:
			sources[stage].Set(0.0f, 0.0f, 1.0f, 0.0f);
			break;
		case D3DTSS_TCI_CAMERASPACEPOSITION:
			sources[stage].Set(0.0f, 0.0f, 0.0f, 1.0f);
			break;
		default:
			sources[stage].Set(0.0f, 0.0f, 0.0f, 0.0f);
			break;
		}

		D3DMATRIX transform;
		if ((flags & 0xff) == D3DTTFF_DISABLE)
		{
			Set_D3DMATRIX_Identity(transform);
		}
		else
		{
			device->GetTransform((D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + stage), &transform);
		}
		for (int column=0;column<4;++column)
		{
			columns[stage * 4 + column].Set(transform.m[0][column], transform.m[1][column], transform.m[2][column], transform.m[3][column]);
		}
	}
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
			Set_Group_Lights(meshes[first]->Get_Lighting_Environment());
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
	return pass != nullptr && (pass == InstancedPasses[0] || pass == InstancedPasses[1]);
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
	if (Pass == PASS_NONE)
	{
		return false;
	}

	if (Pass == PASS_SHADOW_DEPTH)
	{
		// A caster's texture only matters when its shader cuts by alpha.
		if (!shader.Uses_Alpha())
		{
			return true;
		}
		if ((fvf & D3DFVF_TEXCOUNT_MASK) == 0)
		{
			return false;
		}
	}
	else if (shader.Uses_Secondary_Gradient() || (Is_Lit(material) && !(fvf & D3DFVF_NORMAL)))
	{
		return false;
	}
	if (material != nullptr)
	{
		if (material->Get_Diffuse_Color_Source() == VertexMaterialClass::COLOR2 ||
			material->Get_Ambient_Color_Source() == VertexMaterialClass::COLOR2 ||
			material->Get_Emissive_Color_Source() == VertexMaterialClass::COLOR2)
		{
			return false;
		}
		for (int stage=0;stage<MeshBuilderClass::MAX_STAGES;++stage)
		{
			if (material->Peek_Mapper(stage) != nullptr)
			{
				return false;
			}
		}

		// Drivers differ on whether a stage reads its own output or the one TEXCOORDINDEX names. Stage 0
		// on set 0 reads set 0 either way, and the lit shader gives stage 1 the set it names. An unused
		// stage names set 0, which does not matter.
		const int second_source = material->Get_UV_Source(1);
		if (material->Get_UV_Source(0) != 0 || second_source > 1)
		{
			return false;
		}
		if (Pass == PASS_SHADOW_DEPTH && second_stage_textured && second_source != 1)
		{
			return false;
		}
	}
	return true;
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
		Rejections[REJECT_LIGHTS]++;
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
			Rejections[REJECT_LIGHTS]++;
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
		if (Get_Device_Render_State(D3DRS_CLIPPLANEENABLE) != 0)
		{
			Rejections[REJECT_CLIP_PLANE]++;
			return false;
		}
		if (!Get_Fog_Constant(fog))
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

	Vector4 constants[CONSTANT_COUNT];
	Get_View_Constants(constants);

	// Diffuse alpha as fixed-function lighting would produce it, which a cutout caster multiplies into its texture.
	const bool lit = Is_Lit(material);
	float constant_alpha = 1.0f;
	float vertex_alpha = (fvf & D3DFVF_DIFFUSE) ? 1.0f : 0.0f;
	if (lit)
	{
		vertex_alpha = Vertex_Color_Weight(material->Get_Diffuse_Color_Source(), fvf);
		constant_alpha = (vertex_alpha > 0.0f) ? 1.0f : material->Get_Opacity();
	}
	constants[CONSTANT_DIFFUSE_ALPHA].Set(constant_alpha, vertex_alpha, 0.0f, 0.0f);

	int constant_count = CONSTANT_DIFFUSE_ALPHA + 1;
	if (Pass == PASS_LIT)
	{
		Vector3 color(1.0f, 1.0f, 1.0f);
		constants[CONSTANT_MATERIAL_AMBIENT].Set(1.0f, 1.0f, 1.0f, 1.0f);
		constants[CONSTANT_MATERIAL_DIFFUSE].Set(1.0f, 1.0f, 1.0f, 1.0f);
		constants[CONSTANT_MATERIAL_EMISSIVE].Set(0.0f, 0.0f, 0.0f, 1.0f);
		constants[CONSTANT_COLOR_SOURCE].Set(0.0f, 0.0f, 0.0f, 0.0f);
		if (lit)
		{
			material->Get_Ambient(&color);
			constants[CONSTANT_MATERIAL_AMBIENT].Set(color.X, color.Y, color.Z, 1.0f);
			material->Get_Diffuse(&color);
			constants[CONSTANT_MATERIAL_DIFFUSE].Set(color.X, color.Y, color.Z, material->Get_Opacity());
			material->Get_Emissive(&color);
			constants[CONSTANT_MATERIAL_EMISSIVE].Set(color.X, color.Y, color.Z, 1.0f);
			constants[CONSTANT_COLOR_SOURCE].Set(
				Vertex_Color_Weight(material->Get_Ambient_Color_Source(), fvf),
				Vertex_Color_Weight(material->Get_Diffuse_Color_Source(), fvf),
				Vertex_Color_Weight(material->Get_Emissive_Color_Source(), fvf),
				1.0f);
		}
		constants[CONSTANT_FOG] = fog;

		// Stage 0 takes UV set 0 and stage 1 the set its material names, untransformed.
		const float second_source = (material != nullptr) ? (float)material->Get_UV_Source(1) : 1.0f;
		for (int stage=0;stage<4;++stage)
		{
			constants[CONSTANT_STAGE_SOURCE + stage].Set(0.0f, 0.0f, 0.0f, 0.0f);
			for (int column=0;column<4;++column)
			{
				constants[CONSTANT_STAGE_COLUMNS + stage * 4 + column].Set(
					column == 0 ? 1.0f : 0.0f, column == 1 ? 1.0f : 0.0f, column == 2 ? 1.0f : 0.0f, column == 3 ? 1.0f : 0.0f);
			}
		}
		constants[CONSTANT_STAGE_SOURCE].Set(1.0f, 0.0f, 0.0f, 0.0f);
		constants[CONSTANT_STAGE_SOURCE + 1].Set(1.0f - second_source, second_source, 0.0f, 0.0f);
		constant_count = CONSTANT_COUNT;
	}
	DX8Wrapper::Set_Vertex_Shader_Constant(0, constants, constant_count);

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
	Vector4 constants[CONSTANT_COUNT];
	Get_View_Constants(constants);
	Get_Stage_Constants(&constants[CONSTANT_STAGE_SOURCE], &constants[CONSTANT_STAGE_COLUMNS]);

	unsigned offset = 0;
	if (!Write_Instances(meshes, counts, group_count, false, offset))
	{
		Rejections[REJECT_RESOURCE]++;
		return false;
	}
	DX8Wrapper::Set_Vertex_Shader_Constant(CONSTANT_VIEW_PROJECTION, &constants[CONSTANT_VIEW_PROJECTION], CONSTANT_VIEW + 3);
	DX8Wrapper::Set_Vertex_Shader_Constant(CONSTANT_STAGE_SOURCE, &constants[CONSTANT_STAGE_SOURCE], CONSTANT_COUNT - CONSTANT_STAGE_SOURCE);

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
