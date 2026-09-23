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

#include "dx8vertexshading.h"
#include "dx8wrapper.h"
#include "formconv.h"
#include "lightenvironment.h"
#include "meshbuild.h"
#include "vertmaterial.h"
#include "shader.h"
#include "ww3d.h"

#if defined(BUILD_WITH_D3D9)

DWORD DX8VertexShadingClass::Get_Device_Render_State(D3DRENDERSTATETYPE state)
{
	// The wrapper's cache can hold a sentinel after an invalidate, and the device is not pure.
	DWORD value = 0;
	DX8Wrapper::_Get_D3D_Device8()->GetRenderState(state, &value);
	return value;
}

bool DX8VertexShadingClass::Is_Lit(VertexMaterialClass * material)
{
	return material != nullptr && material->Get_Lighting() && !WW3D::Is_Coloring_Enabled();
}

static float Vertex_Color_Weight(VertexMaterialClass::ColorSourceType source, unsigned fvf)
{
	return (source == VertexMaterialClass::COLOR1 && (fvf & D3DFVF_DIFFUSE)) ? 1.0f : 0.0f;
}

bool DX8VertexShadingClass::Allows_Category(PassType pass, const ShaderClass & shader, VertexMaterialClass * material, unsigned fvf, bool second_stage_textured)
{
	if (pass == PASS_NONE)
	{
		return false;
	}

	if (pass == PASS_SHADOW_DEPTH)
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
		if (pass == PASS_SHADOW_DEPTH && second_stage_textured && second_source != 1)
		{
			return false;
		}
	}
	return true;
}

// Vertex fog as the fixed-function pipeline computes it.
bool DX8VertexShadingClass::Get_Fog_Constant(Vector4 & fog)
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

void DX8VertexShadingClass::Get_Light_Constants(LightEnvironmentClass * environment, Vector4 * constants)
{
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
}

// The shaders dot row vectors with these columns, which is the device's own row-vector convention.
void DX8VertexShadingClass::Get_View_Constants(Vector4 * constants)
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
void DX8VertexShadingClass::Get_Stage_Constants(Vector4 * sources, Vector4 * columns)
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

void DX8VertexShadingClass::Get_Material_Constants(PassType pass, VertexMaterialClass * material, unsigned fvf, const Vector4 & fog, Vector4 * constants)
{
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

	if (pass != PASS_LIT)
	{
		return;
	}

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
}

#endif
