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

#pragma once

#include "WWLib/always.h"
#include "dx8compat.h"
#include "dx8vertexshading.h"

class DX8PolygonRendererClass;
class MeshClass;
class VertexMaterialClass;
class ShaderClass;
class MaterialPassClass;
struct IDirect3DVertexShader9;

// Draws every mesh that shares a polygon renderer in one hardware instanced call.
// Only the D3D9 backend can do this; elsewhere the pass never becomes active.
class DX8InstancingClass
{
public:

	enum PassType
	{
		PASS_NONE = DX8VertexShadingClass::PASS_NONE,
		PASS_SHADOW_DEPTH = DX8VertexShadingClass::PASS_SHADOW_DEPTH,
		PASS_LIT = DX8VertexShadingClass::PASS_LIT,
	};

	// A single mesh draws faster through the fixed-function path.
	enum { MIN_GROUP_SIZE = 2 };

	// The most meshes one Draw_Groups or Draw_Material_Pass_Groups call takes.
	enum { MAX_INSTANCES = 8192 };

	// Why group draws fell back to fixed function, for the debug log.
	enum RejectionType
	{
		REJECT_CLIP_PLANE,
		REJECT_FOG,
		REJECT_RESOURCE,
		REJECT_LIGHTS,
		REJECT_COUNT
	};
	static void			Take_Rejections(int * counts);
	static void			Add_Rejections(RejectionType type, int count);

	static void			Shutdown();
	static void			Release_Resources();

	// The caller owns the shaders and keeps them alive while they are set.
	static void			Set_Main_Shader(IDirect3DVertexShader9 * shader);

	// The material passes the main scene can instance. A mesh with any other pass draws fixed function throughout.
	static void			Set_Instanced_Material_Passes(const MaterialPassClass * first, const MaterialPassClass * second);
	static bool			Is_Instanced_Material_Pass(const MaterialPassClass * pass);

	// Whether the pass is one the vertex shaders can redraw, whatever instancing's own state.
	static bool			Is_Vertex_Shader_Material_Pass(const MaterialPassClass * pass);
	static void			Begin_Shadow_Depth_Pass(IDirect3DVertexShader9 * shader);
	static void			Begin_Lit_Pass();
	static void			End_Pass();
	static PassType	Get_Pass() { return Pass; }

	static bool			Allows_Category(const ShaderClass & shader, VertexMaterialClass * material, unsigned fvf, bool second_stage_textured);
	static bool			Allows_Mesh(MeshClass * mesh);
	static bool			Can_Share_Group(MeshClass * reference, MeshClass * mesh);

	// Draws one category's groups, group i being the next counts[i] meshes, all sharing renderers[i].
	// Returns false, having drawn nothing, when the groups cannot be instanced.
	static bool			Draw_Groups(DX8PolygonRendererClass * const * renderers, const int * counts, int group_count, MeshClass * const * meshes, VertexMaterialClass * material, unsigned fvf);

	// The same for a material pass, which the caller has installed.
	static bool			Draw_Material_Pass_Groups(const MaterialPassClass * pass, DX8PolygonRendererClass * const * renderers, const int * counts, int group_count, MeshClass * const * meshes, unsigned fvf);

private:

	static bool			Is_Supported();

	static PassType					Pass;
	static IDirect3DVertexShader9 *	PassShader;
	static IDirect3DVertexShader9 *	MainShader;
	static const MaterialPassClass *	InstancedPasses[2];
};
