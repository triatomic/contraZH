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

class LightEnvironmentClass;
class ShaderClass;
class Vector4;
class VertexMaterialClass;

// Emulates the fixed-function vertex pipeline in the vertex shaders that instancing and skinning
// share. Only the D3D9 backend runs those shaders.
class DX8VertexShadingClass
{
public:

	// Vertex shader registers, shared with the shaders in Shaders/instance*.hlsl
	enum
	{
		CONSTANT_VIEW_PROJECTION = 0,
		CONSTANT_VIEW = 4,
		CONSTANT_DIFFUSE_ALPHA = 7,
		CONSTANT_MATERIAL_AMBIENT = 8,
		CONSTANT_MATERIAL_DIFFUSE = 9,
		CONSTANT_MATERIAL_EMISSIVE = 10,
		CONSTANT_COLOR_SOURCE = 11,
		CONSTANT_FOG = 12,
		CONSTANT_SKIN_AMBIENT = 13,
		CONSTANT_LIGHT_DIRECTION = 16,
		CONSTANT_LIGHT_DIFFUSE = 20,
		CONSTANT_STAGE_SOURCE = 24,
		CONSTANT_STAGE_COLUMNS = 28,
		CONSTANT_COUNT = 44,
		CONSTANT_PALETTE = 44,
		CONSTANT_REGISTERS = 256,
		MAX_LIGHTS = 4,
	};

	enum PassType
	{
		PASS_NONE,
		PASS_SHADOW_DEPTH,
		PASS_LIT,
	};

	static DWORD	Get_Device_Render_State(D3DRENDERSTATETYPE state);
	static bool		Is_Lit(VertexMaterialClass * material);

	// Whether a category's fixed-function setup is one the shaders reproduce.
	static bool		Allows_Category(PassType pass, const ShaderClass & shader, VertexMaterialClass * material, unsigned fvf, bool second_stage_textured);

	// False when the device fogs in a way the shader cannot match.
	static bool		Get_Fog_Constant(Vector4 & fog);

	// Fills MAX_LIGHTS directions followed by MAX_LIGHTS diffuse colours.
	static void		Get_Light_Constants(LightEnvironmentClass * environment, Vector4 * constants);
	static void		Get_View_Constants(Vector4 * constants);
	static void		Get_Stage_Constants(Vector4 * sources, Vector4 * columns);

	// Fills CONSTANT_DIFFUSE_ALPHA, and for the lit pass everything up to CONSTANT_COUNT except the view and lights.
	static void		Get_Material_Constants(PassType pass, VertexMaterialClass * material, unsigned fvf, const Vector4 & fog, Vector4 * constants);
};
