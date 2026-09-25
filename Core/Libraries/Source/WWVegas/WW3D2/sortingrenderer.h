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

class SortingNodeStruct;
class SphereClass;
class ShaderClass;
struct VertexFormatXYZNDUV2;

// Shades particle sprites in a pixel shader, around each of their draws.
class SoftParticleHookClass
{
public:
	enum
	{
		EFFECT_SOFT = 1,	// fade where the sprite nears the scene behind it
		EFFECT_FLAME = 2,	// flicker and heat colouring
		EFFECT_HAZE = 4,	// wobble the scene copy behind the sprite
		EFFECT_ELECTRIC = 8,	// arcs, jitter and strobe
		EFFECT_LASER = 16	// hot core and travelling pulses, from the beam coordinates in the second uv set
	};

	virtual ~SoftParticleHookClass() {}

	// Called with the draw's render state applied. False leaves the draw fixed-function, and End uncalled.
	// The data is whatever the inserter handed over with the effects, opaque to the renderer.
	virtual bool Begin(const ShaderClass &shader, unsigned effects, const void *effectData) = 0;
	virtual void End() = 0;
};

class SortingRendererClass
{
	static bool _EnableTriangleDraw;

	static void Flush_Sorting_Pool();
	static void Flush_Additive_Pool();
	static void Insert_To_Sorting_Pool(SortingNodeStruct* state);
	static const VertexFormatXYZNDUV2* Source_Vertices(const SortingNodeStruct* state);
	static const unsigned short* Source_Indices(const SortingNodeStruct* state);

public:
	static void Insert_Triangles(
		const SphereClass& bounding_sphere,
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);

	static void Insert_Triangles(
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);

	static void Flush();
	static void Deinit();

	static void SetMinVertexBufferSize( unsigned val );

	// Rigid translucent meshes keep their own buffers and sort as whole objects.
	static bool Sorts_Meshes_Per_Object();

	static void _Enable_Triangle_Draw(bool enable) { _EnableTriangleDraw=enable; }
	static bool _Is_Triangle_Draw_Enabled() { return _EnableTriangleDraw; }

	// Triangles inserted while effects are set are drawn through the hook with those effects.
	static void Set_Soft_Particle_Hook(SoftParticleHookClass *hook);
	static SoftParticleHookClass *Peek_Soft_Particle_Hook();
	static void Set_Insert_Effects(unsigned effects, const void *effectData);
};
