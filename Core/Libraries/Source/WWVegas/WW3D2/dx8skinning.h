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

class MeshClass;
class MaterialPassClass;
class ShaderClass;
class VertexMaterialClass;
struct IDirect3DVertexShader9;

// Skins meshes in a vertex shader from a static vertex buffer. Each vertex follows one bone, whose
// world matrix comes from a palette of the bones its mesh uses. Only the D3D9 backend can do this;
// elsewhere the pass never becomes active and skins deform on the CPU.
class DX8SkinningClass
{
public:

	enum { MAX_BONES = (DX8VertexShadingClass::CONSTANT_REGISTERS - DX8VertexShadingClass::CONSTANT_PALETTE) / 3 };

	// The HTree pivots a skin model's vertices follow, in palette order.
	struct PaletteStruct
	{
		int				Count;
		unsigned short	Pivots[MAX_BONES];
	};

	// Why skins fell back to the CPU, for the debug log.
	enum RejectionType
	{
		REJECT_STATE,
		REJECT_MODEL,
		REJECT_MESH,
		REJECT_CATEGORY,
		REJECT_PASS,
		REJECT_COUNT
	};

	struct StatsStruct
	{
		int	SkinnedMeshes[2];	// main scene, shadow depth
		int	Rejections[REJECT_COUNT];
	};
	static void			Take_Stats(StatsStruct & stats);

	static bool			Is_Supported();
	static unsigned	Get_Vertex_FVF();
	static void			Shutdown();

	// The caller owns the shaders and keeps them alive while they are set.
	static void			Set_Main_Shader(IDirect3DVertexShader9 * shader);
	static void			Begin_Shadow_Depth_Pass(IDirect3DVertexShader9 * shader);
	static void			Begin_Lit_Pass();
	static void			End_Pass();
	static DX8VertexShadingClass::PassType Get_Pass() { return Pass; }

	static bool			Allows_Mesh(MeshClass * mesh);
	static bool			Allows_Category(const ShaderClass & shader, VertexMaterialClass * material, bool second_stage_textured);
	static bool			Allows_Material_Pass(const MaterialPassClass * pass);
	static void			Record_Rejection(RejectionType reason);
	static void			Record_Skinned_Meshes(int count);

	// Draws between Begin and End go through the skin shader with the static vertex buffer bound.
	// Begin returns false, and nothing may be drawn skinned, when the device state rules it out.
	static bool			Begin_Sweep();
	static void			End_Sweep();
	static bool			Is_Sweeping() { return Sweeping; }

	// Loads a mesh's palette and lights for its following draws, which take their material constants from material.
	static void			Set_Mesh(MeshClass * mesh, const PaletteStruct & palette, VertexMaterialClass * material);

	// Following draws are material passes, which take their texture coordinates from the stages the pass set up.
	static void			Begin_Material_Passes();

private:

	static void			Apply_Draw_Constants();

	static DX8VertexShadingClass::PassType	Pass;
	static IDirect3DVertexShader9 *			PassShader;
	static IDirect3DVertexShader9 *			MainShader;
	static bool										Sweeping;
};
