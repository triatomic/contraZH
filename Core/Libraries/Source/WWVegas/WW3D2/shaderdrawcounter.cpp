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

#include "shaderdrawcounter.h"
#include "dx8wrapper.h"
#include <stdio.h>
#include <stdlib.h>

static bool Enabled = false;
static char Lines[ShaderDrawCounterClass::MAX_LINES][ShaderDrawCounterClass::MAX_LINE_LENGTH];
static int LineCount = 0;

#if defined(BUILD_WITH_D3D9)

// IDirect3DDevice9 vtable slots, fixed by the COM ABI
enum
{
	SLOT_DRAW_PRIMITIVE = 81,
	SLOT_DRAW_INDEXED_PRIMITIVE = 82,
	SLOT_DRAW_PRIMITIVE_UP = 83,
	SLOT_DRAW_INDEXED_PRIMITIVE_UP = 84,
	SLOT_SET_VERTEX_SHADER = 92,
	SLOT_SET_PIXEL_SHADER = 107,
	SLOT_FIRST = SLOT_DRAW_PRIMITIVE,
	SLOT_LAST = SLOT_SET_PIXEL_SHADER
};

typedef HRESULT (__stdcall * DrawPrimitiveType)(IDirect3DDevice9 *, D3DPRIMITIVETYPE, UINT, UINT);
typedef HRESULT (__stdcall * DrawIndexedPrimitiveType)(IDirect3DDevice9 *, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
typedef HRESULT (__stdcall * DrawPrimitiveUPType)(IDirect3DDevice9 *, D3DPRIMITIVETYPE, UINT, const void *, UINT);
typedef HRESULT (__stdcall * DrawIndexedPrimitiveUPType)(IDirect3DDevice9 *, D3DPRIMITIVETYPE, UINT, UINT, UINT, const void *, D3DFORMAT, const void *, UINT);
typedef HRESULT (__stdcall * SetVertexShaderType)(IDirect3DDevice9 *, IDirect3DVertexShader9 *);
typedef HRESULT (__stdcall * SetPixelShaderType)(IDirect3DDevice9 *, IDirect3DPixelShader9 *);

struct PairStruct
{
	const void * VertexShader;
	const void * PixelShader;
	unsigned Draws;
	unsigned Primitives;
};

enum { MAX_PAIRS = 256 };

// Its address stands for a shader bound before the hooks went in.
static const char UnknownShader = 0;

static void ** HookedTable = nullptr;
static void * Originals[SLOT_LAST + 1];
static const void * CurrentVertexShader = &UnknownShader;
static const void * CurrentPixelShader = &UnknownShader;
static PairStruct Pairs[MAX_PAIRS];
static int PairCount = 0;
static int LastPair = 0;
static unsigned TotalDraws = 0;
static unsigned TotalPrimitives = 0;

static void Count_Draw(UINT primitive_count)
{
	++TotalDraws;
	TotalPrimitives += primitive_count;

	if (LastPair >= PairCount || Pairs[LastPair].VertexShader != CurrentVertexShader || Pairs[LastPair].PixelShader != CurrentPixelShader)
	{
		int index = 0;
		while (index < PairCount && (Pairs[index].VertexShader != CurrentVertexShader || Pairs[index].PixelShader != CurrentPixelShader))
		{
			++index;
		}
		if (index == PairCount)
		{
			if (PairCount == MAX_PAIRS)
			{
				return;
			}
			Pairs[index].VertexShader = CurrentVertexShader;
			Pairs[index].PixelShader = CurrentPixelShader;
			Pairs[index].Draws = 0;
			Pairs[index].Primitives = 0;
			++PairCount;
		}
		LastPair = index;
	}

	++Pairs[LastPair].Draws;
	Pairs[LastPair].Primitives += primitive_count;
}

static HRESULT __stdcall Hook_Draw_Primitive(IDirect3DDevice9 * device, D3DPRIMITIVETYPE type, UINT start_vertex, UINT primitive_count)
{
	Count_Draw(primitive_count);
	return ((DrawPrimitiveType)Originals[SLOT_DRAW_PRIMITIVE])(device, type, start_vertex, primitive_count);
}

static HRESULT __stdcall Hook_Draw_Indexed_Primitive(IDirect3DDevice9 * device, D3DPRIMITIVETYPE type, INT base_vertex, UINT min_index,
	UINT vertex_count, UINT start_index, UINT primitive_count)
{
	Count_Draw(primitive_count);
	return ((DrawIndexedPrimitiveType)Originals[SLOT_DRAW_INDEXED_PRIMITIVE])(device, type, base_vertex, min_index, vertex_count, start_index, primitive_count);
}

static HRESULT __stdcall Hook_Draw_Primitive_UP(IDirect3DDevice9 * device, D3DPRIMITIVETYPE type, UINT primitive_count, const void * vertices, UINT stride)
{
	Count_Draw(primitive_count);
	return ((DrawPrimitiveUPType)Originals[SLOT_DRAW_PRIMITIVE_UP])(device, type, primitive_count, vertices, stride);
}

static HRESULT __stdcall Hook_Draw_Indexed_Primitive_UP(IDirect3DDevice9 * device, D3DPRIMITIVETYPE type, UINT min_index, UINT vertex_count,
	UINT primitive_count, const void * indices, D3DFORMAT index_format, const void * vertices, UINT stride)
{
	Count_Draw(primitive_count);
	return ((DrawIndexedPrimitiveUPType)Originals[SLOT_DRAW_INDEXED_PRIMITIVE_UP])(device, type, min_index, vertex_count, primitive_count, indices,
		index_format, vertices, stride);
}

static HRESULT __stdcall Hook_Set_Vertex_Shader(IDirect3DDevice9 * device, IDirect3DVertexShader9 * shader)
{
	CurrentVertexShader = shader;
	return ((SetVertexShaderType)Originals[SLOT_SET_VERTEX_SHADER])(device, shader);
}

static HRESULT __stdcall Hook_Set_Pixel_Shader(IDirect3DDevice9 * device, IDirect3DPixelShader9 * shader)
{
	CurrentPixelShader = shader;
	return ((SetPixelShaderType)Originals[SLOT_SET_PIXEL_SHADER])(device, shader);
}

struct HookStruct
{
	int Slot;
	void * Hook;
};

static const HookStruct Hooks[] =
{
	{ SLOT_DRAW_PRIMITIVE, (void *)&Hook_Draw_Primitive },
	{ SLOT_DRAW_INDEXED_PRIMITIVE, (void *)&Hook_Draw_Indexed_Primitive },
	{ SLOT_DRAW_PRIMITIVE_UP, (void *)&Hook_Draw_Primitive_UP },
	{ SLOT_DRAW_INDEXED_PRIMITIVE_UP, (void *)&Hook_Draw_Indexed_Primitive_UP },
	{ SLOT_SET_VERTEX_SHADER, (void *)&Hook_Set_Vertex_Shader },
	{ SLOT_SET_PIXEL_SHADER, (void *)&Hook_Set_Pixel_Shader },
};

static const int HOOK_COUNT = sizeof(Hooks) / sizeof(Hooks[0]);

// Writes the non null values into the vtable, which lives in read only memory.
static bool Write_Slots(void ** table, void * const * values)
{
	void ** first = &table[SLOT_FIRST];
	const SIZE_T size = (SLOT_LAST - SLOT_FIRST + 1) * sizeof(void *);
	DWORD old_protect = 0;
	if (!VirtualProtect(first, size, PAGE_EXECUTE_READWRITE, &old_protect))
	{
		return false;
	}
	for (int slot = SLOT_FIRST; slot <= SLOT_LAST; ++slot)
	{
		if (values[slot] != nullptr)
		{
			table[slot] = values[slot];
		}
	}
	VirtualProtect(first, size, old_protect, &old_protect);
	return true;
}

static void Reset_Counts()
{
	PairCount = 0;
	LastPair = 0;
	TotalDraws = 0;
	TotalPrimitives = 0;
}

static void Install(IDirect3DDevice9 * device)
{
	void ** table = *(void ***)device;
	void * values[SLOT_LAST + 1] = { nullptr };
	for (int i = 0; i < HOOK_COUNT; ++i)
	{
		Originals[Hooks[i].Slot] = table[Hooks[i].Slot];
		values[Hooks[i].Slot] = Hooks[i].Hook;
	}
	if (Write_Slots(table, values))
	{
		HookedTable = table;
		CurrentVertexShader = &UnknownShader;
		CurrentPixelShader = &UnknownShader;
		Reset_Counts();
	}
}

static void Uninstall()
{
	if (HookedTable == nullptr)
	{
		return;
	}

	// A slot something else has hooked since keeps that hook.
	void * values[SLOT_LAST + 1] = { nullptr };
	for (int i = 0; i < HOOK_COUNT; ++i)
	{
		if (HookedTable[Hooks[i].Slot] == Hooks[i].Hook)
		{
			values[Hooks[i].Slot] = Originals[Hooks[i].Slot];
		}
	}
	Write_Slots(HookedTable, values);
	HookedTable = nullptr;
}

static const char * Shader_Name(const void * shader)
{
	if (shader == nullptr)
	{
		return "fixed";
	}
	if (shader == &UnknownShader)
	{
		return "?";
	}
	const char * name = Find_D3D9_Shader_Name(shader);
	return (name != nullptr && name[0] != '\0') ? name : "unnamed";
}

static int Compare_Pairs(const void * a, const void * b)
{
	const PairStruct * pair_a = static_cast<const PairStruct *>(a);
	const PairStruct * pair_b = static_cast<const PairStruct *>(b);
	if (pair_a->Draws != pair_b->Draws)
	{
		return pair_a->Draws > pair_b->Draws ? -1 : 1;
	}
	return pair_a->Primitives > pair_b->Primitives ? -1 : (pair_a->Primitives < pair_b->Primitives ? 1 : 0);
}

static void Publish()
{
	const int max_lines = ShaderDrawCounterClass::MAX_LINES;
	const int length = ShaderDrawCounterClass::MAX_LINE_LENGTH;

	qsort(Pairs, PairCount, sizeof(PairStruct), Compare_Pairs);

	snprintf(Lines[0], length, "Shader draws: %u draws, %u prims, %d shader pairs", TotalDraws, TotalPrimitives, PairCount);
	snprintf(Lines[1], length, " draws    prims  vertex + pixel");
	int line = 2;
	for (int i = 0; i < PairCount; ++i)
	{
		if (line == max_lines - 1 && i < PairCount - 1)
		{
			unsigned draws = 0;
			unsigned primitives = 0;
			for (int rest = i; rest < PairCount; ++rest)
			{
				draws += Pairs[rest].Draws;
				primitives += Pairs[rest].Primitives;
			}
			snprintf(Lines[line++], length, "%6u %8u  ... %d more pairs", draws, primitives, PairCount - i);
			break;
		}
		snprintf(Lines[line++], length, "%6u %8u  %s + %s", Pairs[i].Draws, Pairs[i].Primitives,
			Shader_Name(Pairs[i].VertexShader), Shader_Name(Pairs[i].PixelShader));
	}
	LineCount = line;
}

#endif

void ShaderDrawCounterClass::Set_Enabled(bool enabled)
{
	Enabled = enabled;
}

bool ShaderDrawCounterClass::Is_Enabled()
{
	return Enabled;
}

void ShaderDrawCounterClass::End_Frame()
{
#if defined(BUILD_WITH_D3D9)
	if (!Enabled)
	{
		Uninstall();
		LineCount = 0;
		return;
	}

	if (HookedTable == nullptr)
	{
		if (DX8Wrapper::_Get_D3D_Device8() != nullptr)
		{
			Install(DX8Wrapper::_Get_D3D_Device8());
		}
		return;
	}

	Publish();
	Reset_Counts();
#endif
}

void ShaderDrawCounterClass::Release_Device()
{
#if defined(BUILD_WITH_D3D9)
	Uninstall();
#endif
}

int ShaderDrawCounterClass::Get_Line_Count()
{
	return LineCount;
}

const char * ShaderDrawCounterClass::Get_Line(int index)
{
	return (index >= 0 && index < LineCount) ? Lines[index] : "";
}
