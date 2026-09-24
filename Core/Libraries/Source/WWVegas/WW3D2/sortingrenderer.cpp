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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : ww3d                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/sortingrenderer.cpp                    $*
 *                                                                                             *
 *              Original Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 06/27/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 2                                                           $*
 *                                                                                             *
 * 06/26/02 KM Matrix name change to avoid MAX conflicts                                       *
 * 06/27/02 KM Changes to max texture stage caps																*
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "sortingrenderer.h"
#include "dx8vertexbuffer.h"
#include "dx8indexbuffer.h"
#include "dx8wrapper.h"
#include "vertmaterial.h"
#include "texture.h"
#include "dx8compat.h"
#include "formconv.h"
#include "statistics.h"
#include <WWDebug/wwprofile.h>
#include <algorithm>
#include <functional>
#include <vector>
#include <stdlib.h>
#include <string.h>


bool SortingRendererClass::_EnableTriangleDraw=true;

static SoftParticleHookClass *SoftHook = nullptr;
static unsigned InsertEffects = 0;

void SortingRendererClass::Set_Soft_Particle_Hook(SoftParticleHookClass *hook)
{
	SoftHook = hook;
}

SoftParticleHookClass *SortingRendererClass::Peek_Soft_Particle_Hook()
{
	return SoftHook;
}

void SortingRendererClass::Set_Insert_Effects(unsigned effects)
{
	InsertEffects = effects;
}
static unsigned DEFAULT_SORTING_POLY_COUNT = 16384;	// (count * 3) must be less than 65536
static unsigned DEFAULT_SORTING_VERTEX_COUNT = 32768;	// count must be less than 65536

// CONTRA_BLENDSORT: 0 sorts every triangle, 1 batches additive and merges state, 2 sorts rigid meshes per object.
enum { BLENDSORT_OFF = 0, BLENDSORT_BATCH = 1, BLENDSORT_FULL = 2 };

#if defined(BUILD_WITH_D3D9)

static int Get_Blend_Sort_Mode()
{
	const char *value = getenv("CONTRA_BLENDSORT");
	return (value != nullptr) ? atoi(value) : BLENDSORT_FULL;
}

static const int BlendSortMode = Get_Blend_Sort_Mode();

#else

static const int BlendSortMode = BLENDSORT_OFF;

#endif

static const bool BlendBatching = BlendSortMode >= BLENDSORT_BATCH;

bool SortingRendererClass::Sorts_Meshes_Per_Object()
{
	return BlendSortMode >= BLENDSORT_FULL;
}

void SortingRendererClass::SetMinVertexBufferSize( unsigned val )
{
	DEFAULT_SORTING_VERTEX_COUNT = val;
	DEFAULT_SORTING_POLY_COUNT = val/2;	//typically have 2:1 vertex:triangle ratio.
}

struct ShortVectorIStruct
{
	unsigned short i;
	unsigned short j;
	unsigned short k;
};

struct TempIndexStruct
{
	ShortVectorIStruct tri;
	unsigned short idx;
	float z;
};

bool operator <(const TempIndexStruct &l, const TempIndexStruct &r) { return l.z < r.z; }
bool operator <=(const TempIndexStruct &l, const TempIndexStruct &r) { return l.z <= r.z; }
bool operator >(const TempIndexStruct &l, const TempIndexStruct &r) { return l.z > r.z; }
bool operator >=(const TempIndexStruct &l, const TempIndexStruct &r) { return l.z >= r.z; }
bool operator ==(const TempIndexStruct &l, const TempIndexStruct &r) { return l.z == r.z; }
// ----------------------------------------------------------------------------
static
void InsertionSort(TempIndexStruct *begin, TempIndexStruct *end)
{
	for (TempIndexStruct *iter = begin + 1; iter < end; ++iter) {
		TempIndexStruct val = iter[0];
		TempIndexStruct *insert = iter;
		while (insert != begin && insert[-1] > val) {
			insert[0] = insert[-1];
			insert -= 1;
		}
		insert[0] = val;
	}
}

// ----------------------------------------------------------------------------
static
void Sort(TempIndexStruct *begin, TempIndexStruct *end)
{
	const int diff = end - begin;
	if (diff <= 16) {
		// Insertion sort has less overhead for small arrays
		InsertionSort(begin, end);
	} else {
		// Choose the median of begin, mid, and (end - 1) as the partitioning element.
		// Rearrange so that *(begin + 1) <= *begin <= *(end - 1).  These will be guard
		// elements.
		TempIndexStruct *mid = begin + diff/2;
		std::swap(mid[0], begin[1]);
		if (begin[1] > end[-1]) {
			std::swap(begin[1], end[-1]);
		}
		if (begin[0] > end[-1]) {
			std::swap(begin[0], end[-1]);
		}
		if (begin[1] > begin[0]) {
			std::swap(begin[1], begin[0]);
		}

		// *begin is now the partitioning element
		TempIndexStruct *begin1 = begin + 1;	// TODO: Temp fix until I find out who is passing me NaN
		TempIndexStruct *end1 = end - 1;			// TODO: Temp fix until I find out who is passing me NaN
		TempIndexStruct *left = begin + 1;
		TempIndexStruct *right = end - 1;
		for (;;) {
#if 0		// TODO: Temp fix until I find out who is passing me NaN.
			do ++left; while (left[0] < begin[0]);		// Scan up to find element >= than partition
			do --right; while (right[0] > begin[0]);	// Scan down to find element <= than partition
#else
			do ++left; while (left < end1 && left[0] < begin[0]);		// Scan up to find element >= than partition
			do --right; while (right > begin1 && right[0] > begin[0]);	// Scan down to find element <= than partition
#endif
			if (right < left) break;									// Pointers crossed.  Partitioning completed.
			std::swap(left[0], right[0]);							// Exchange elements.
		}
		std::swap(begin[0], right[0]);							// Insert partition element

		// Sort the smaller subarray first then the larger
		if (right - begin > end - (right + 1)) {
			Sort(right + 1, end);
			Sort(begin, right);
		} else {
			Sort(begin, right);
			Sort(right + 1, end);
		}
	}
}

// ----------------------------------------------------------------------------

class SortingNodeStruct
{
	W3DMPO_CODE(SortingNodeStruct)

public:
	RenderStateStruct sorting_state;

	float depth;								// View space depth of the bounding sphere center, for object nodes
	unsigned char effects;					// Particle effects drawn through the soft particle hook, 0 for none
	unsigned short start_index;			// First index used in the ib
	unsigned short polygon_count;			// Polygon count to process (3 indices = one polygon)
	unsigned short min_vertex_index;		// First index used in the vb
	unsigned short vertex_count;			// Number of vertices used in vb
};

typedef std::vector<SortingNodeStruct*> SortingNodeStructList;
static SortingNodeStructList additive_list;
static SortingNodeStructList object_list;
static SortingNodeStructList additive_object_list;
static SortingNodeStructList clean_list;
static unsigned total_sorting_vertices;

static void Delete_Nodes(SortingNodeStructList& list)
{
	for (size_t i=0;i<list.size();++i) {
		delete list[i];
	}
	list.clear();
}

static bool Uses_Sorting_Buffers(const RenderStateStruct& state)
{
	return (state.index_buffer_type==BUFFER_TYPE_SORTING || state.index_buffer_type==BUFFER_TYPE_DYNAMIC_SORTING) &&
		(state.vertex_buffer_types[0]==BUFFER_TYPE_SORTING || state.vertex_buffer_types[0]==BUFFER_TYPE_DYNAMIC_SORTING);
}

const VertexFormatXYZNDUV2* SortingRendererClass::Source_Vertices(const SortingNodeStruct* state)
{
	const SortingVertexBufferClass* vertex_buffer=static_cast<const SortingVertexBufferClass*>(state->sorting_state.vertex_buffers[0]);
	WWASSERT(vertex_buffer);
	return vertex_buffer->VertexBuffer+state->sorting_state.vba_offset+state->sorting_state.index_base_offset+state->min_vertex_index;
}

const unsigned short* SortingRendererClass::Source_Indices(const SortingNodeStruct* state)
{
	const SortingIndexBufferClass* index_buffer=static_cast<const SortingIndexBufferClass*>(state->sorting_state.index_buffer);
	WWASSERT(index_buffer);
	return index_buffer->index_buffer+state->start_index+state->sorting_state.iba_offset;
}

static bool Object_Depth_Order(const SortingNodeStruct* a, const SortingNodeStruct* b)
{
	return a->depth < b->depth;
}

// The hook binds its own state, so it comes after the node's is applied.
static bool Begin_Soft(const SortingNodeStruct* state)
{
	if (state->effects == 0 || SoftHook == nullptr) {
		return false;
	}
	DX8Wrapper::Apply_Render_State_Changes();
	return SoftHook->Begin(state->sorting_state.shader, state->effects);
}

static void End_Soft(bool soft)
{
	if (soft) {
		SoftHook->End();
	}
}

// Release_Render_State would clear the texture cache but not the device, so the state stays set.
static void Draw_Object_Node(SortingNodeStruct* state)
{
	DX8Wrapper::Set_Render_State(state->sorting_state);
	DX8Wrapper::Draw_Triangles(state->start_index,state->polygon_count,state->min_vertex_index,state->vertex_count);
}

// Object nodes are sorted far to near and drawn between the pool's triangles as the depths pass them.
static size_t next_object;

static bool Has_Object_Before(float depth)
{
	return next_object<object_list.size() && object_list[next_object]->depth<depth;
}

static void Draw_Objects_Before(float depth)
{
	while (Has_Object_Before(depth)) {
		Draw_Object_Node(object_list[next_object]);
		++next_object;
	}
}

static SortingNodeStruct* Get_Sorting_Struct()
{
	if (!clean_list.empty()) {
		SortingNodeStruct* state = clean_list.back();
		clean_list.pop_back();
		return state;
	}
	return W3DNEW SortingNodeStruct();
}

// ----------------------------------------------------------------------------
//
// Additive blends add to the frame buffer, so any draw order gives the same result.
//
// ----------------------------------------------------------------------------

static bool Is_Order_Independent(const ShaderClass& shader)
{
	if (shader.Get_Depth_Mask() != ShaderClass::DEPTH_WRITE_DISABLE) {
		return false;
	}
	const ShaderClass::SrcBlendFuncType src = shader.Get_Src_Blend_Func();
	return shader.Get_Dst_Blend_Func() == ShaderClass::DSTBLEND_ONE &&
		(src == ShaderClass::SRCBLEND_ONE || src == ShaderClass::SRCBLEND_SRC_ALPHA);
}

// ----------------------------------------------------------------------------
//
// Compares everything Apply_Render_State sets, so equal nodes can share one draw.
//
// ----------------------------------------------------------------------------

static bool Same_Render_State(const RenderStateStruct& a, const RenderStateStruct& b)
{
	if (a.shader.Get_Bits() != b.shader.Get_Bits() || a.material != b.material) {
		return false;
	}
	for (int i=0;i<DX8Wrapper::Get_Current_Caps()->Get_Max_Textures_Per_Pass();++i) {
		if (a.Textures[i] != b.Textures[i]) {
			return false;
		}
	}
	if (memcmp(&a.world, &b.world, sizeof(D3DMATRIX)) != 0 || memcmp(&a.view, &b.view, sizeof(D3DMATRIX)) != 0) {
		return false;
	}
	if (a.material == nullptr || !a.material->Get_Lighting()) {
		return true;
	}
	for (int i=0;i<4;++i) {
		if (a.LightEnable[i] != b.LightEnable[i]) {
			return false;
		}
		if (!a.LightEnable[i]) {
			break;
		}
		if (memcmp(&a.Lights[i], &b.Lights[i], sizeof(D3DLIGHT8)) != 0) {
			return false;
		}
	}
	return true;
}

static bool Same_Node_State(const SortingNodeStruct* a, const SortingNodeStruct* b)
{
	return a->effects == b->effects && Same_Render_State(a->sorting_state, b->sorting_state);
}

// ----------------------------------------------------------------------------

static bool Additive_Draw_Order(const SortingNodeStruct* a, const SortingNodeStruct* b)
{
	const RenderStateStruct& l = a->sorting_state;
	const RenderStateStruct& r = b->sorting_state;
	if (l.shader.Get_Bits() != r.shader.Get_Bits()) {
		return l.shader.Get_Bits() < r.shader.Get_Bits();
	}
	if (l.Textures[0] != r.Textures[0]) {
		return std::less<TextureBaseClass*>()(l.Textures[0], r.Textures[0]);
	}
	if (l.material != r.material) {
		return std::less<VertexMaterialClass*>()(l.material, r.material);
	}
	return a->effects < b->effects;
}

// ----------------------------------------------------------------------------
//
// Temporary arrays for the sorting system
//
// ----------------------------------------------------------------------------

static TempIndexStruct* temp_index_array;
static unsigned temp_index_array_count;

static TempIndexStruct* Get_Temp_Index_Array(unsigned count)
{
	if (count < DEFAULT_SORTING_POLY_COUNT)
		count = DEFAULT_SORTING_POLY_COUNT;
	if (count>temp_index_array_count) {
		delete[] temp_index_array;
		temp_index_array=W3DNEWARRAY TempIndexStruct[count];
		temp_index_array_count=count;
	}
	return temp_index_array;
}

// ----------------------------------------------------------------------------
//
// Insert triangles to the sorting system.
//
// ----------------------------------------------------------------------------

void SortingRendererClass::Insert_Triangles(
	const SphereClass& bounding_sphere,
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	if (!WW3D::Is_Sorting_Enabled()) {
		DX8Wrapper::Draw_Triangles(start_index,polygon_count,min_vertex_index,vertex_count);
		return;
	}

	SNAPSHOT_SAY(("SortingRenderer::Insert(start_i: %d, polygons : %d, min_vi: %d, vertex_count: %d)",
		start_index,polygon_count,min_vertex_index,vertex_count));


	DX8_RECORD_SORTING_RENDER(polygon_count,vertex_count);

	SortingNodeStruct* state=Get_Sorting_Struct();

	DX8Wrapper::Get_Render_State(state->sorting_state);

	state->start_index=start_index;
	state->polygon_count=polygon_count;
	state->min_vertex_index=min_vertex_index;
	state->vertex_count=vertex_count;
	state->depth=0.0f;
	state->effects=(SoftHook != nullptr) ? (unsigned char)InsertEffects : 0;

	const bool additive=BlendBatching && Is_Order_Independent(state->sorting_state.shader);

	if (!Uses_Sorting_Buffers(state->sorting_state)) {
		WWASSERT(Sorts_Meshes_Per_Object());
		if (bounding_sphere.Is_Valid()) {
			// Mesh bounding spheres are already in world space, so only the view's depth column applies.
			const float (&view)[4][4]=state->sorting_state.view.m;
			const Vector3& center=bounding_sphere.Center;
			state->depth=center.X*view[0][2]+center.Y*view[1][2]+center.Z*view[2][2]+view[3][2];
		}
		(additive ? additive_object_list : object_list).push_back(state);
		return;
	}

#ifdef WWDEBUG
	SortingVertexBufferClass* vertex_buffer=static_cast<SortingVertexBufferClass*>(state->sorting_state.vertex_buffers[0]);
	WWASSERT(vertex_buffer);
	WWASSERT(state->vertex_count<=vertex_buffer->Get_Vertex_Count());

	unsigned short* indices=nullptr;
	SortingIndexBufferClass* index_buffer=static_cast<SortingIndexBufferClass*>(state->sorting_state.index_buffer);
	WWASSERT(index_buffer);
	indices=index_buffer->index_buffer;
	WWASSERT(indices);
	indices+=state->start_index;
	indices+=state->sorting_state.iba_offset;

	for (int i=0;i<state->polygon_count;++i) {
		unsigned short idx1=indices[i*3]-state->min_vertex_index;
		unsigned short idx2=indices[i*3+1]-state->min_vertex_index;
		unsigned short idx3=indices[i*3+2]-state->min_vertex_index;
		WWASSERT(idx1<state->vertex_count);
		WWASSERT(idx2<state->vertex_count);
		WWASSERT(idx3<state->vertex_count);
	}
#endif // WWDEBUG

	// The additive pool writes each node's indices as one dynamic IB block, which holds at most 65535.
	if (additive && polygon_count*3 <= 65535) {
		additive_list.push_back(state);
	}
	else {
		Insert_To_Sorting_Pool(state);
	}
}

// ----------------------------------------------------------------------------
//
// Insert triangles to the sorting system, with no bounding information.
//
// ----------------------------------------------------------------------------

void SortingRendererClass::Insert_Triangles(
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	Insert_Triangles(SphereClass(),start_index,polygon_count,min_vertex_index,vertex_count);
}

// ----------------------------------------------------------------------------
//
// Flush all sorting polygons.
//
// ----------------------------------------------------------------------------

void Release_Refs(SortingNodeStruct* state)
{
	int i;
	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		REF_PTR_RELEASE(state->sorting_state.vertex_buffers[i]);
	}
	REF_PTR_RELEASE(state->sorting_state.index_buffer);
	REF_PTR_RELEASE(state->sorting_state.material);
	for (i=0;i<DX8Wrapper::Get_Current_Caps()->Get_Max_Textures_Per_Pass();++i)
	{
		REF_PTR_RELEASE(state->sorting_state.Textures[i]);
	}
}

static void Recycle_Nodes(SortingNodeStructList& list)
{
	for (size_t i=0;i<list.size();++i) {
		Release_Refs(list[i]);
		clean_list.push_back(list[i]);
	}
	list.clear();
}

static unsigned overlapping_node_count;
static unsigned overlapping_polygon_count;
static unsigned overlapping_vertex_count;

// TempIndexStruct stores the node id in 16 bits.
static const unsigned MAX_OVERLAPPING_NODES=65536;
static std::vector<SortingNodeStruct*> overlapping_nodes;

// ----------------------------------------------------------------------------

void SortingRendererClass::Insert_To_Sorting_Pool(SortingNodeStruct* state)
{
	if (overlapping_node_count>=MAX_OVERLAPPING_NODES) {
		Release_Refs(state);
		delete state;
		WWASSERT(0);
		return;
	}

	if (overlapping_node_count>=overlapping_nodes.size()) {
		overlapping_nodes.resize(overlapping_nodes.empty() ? 4096 : overlapping_nodes.size()*2);
	}
	overlapping_nodes[overlapping_node_count]=state;
	overlapping_vertex_count+=state->vertex_count;
	overlapping_polygon_count+=state->polygon_count;
	overlapping_node_count++;
}

// ----------------------------------------------------------------------------
//static unsigned prevLight = 0xffffffff;

static void Apply_Render_State(RenderStateStruct& render_state)
{
	DX8Wrapper::Set_Shader(render_state.shader);

	DX8Wrapper::Set_Material(render_state.material);

	for (int i=0;i<DX8Wrapper::Get_Current_Caps()->Get_Max_Textures_Per_Pass();++i)
	{
		DX8Wrapper::Set_Texture(i,render_state.Textures[i]);
	}

	DX8Wrapper::_Set_DX8_Transform(D3DTS_WORLD,render_state.world);
	DX8Wrapper::_Set_DX8_Transform(D3DTS_VIEW,render_state.view);


	if (!render_state.material->Get_Lighting())
		return;	//no point changing lights if they are ignored.
  //prevLight = render_state.lightsHash;

	if (render_state.LightEnable[0]) {
		DX8Wrapper::Set_DX8_Light(0,&render_state.Lights[0]);
		if (render_state.LightEnable[1]) {
			DX8Wrapper::Set_DX8_Light(1,&render_state.Lights[1]);
			if (render_state.LightEnable[2]) {
				DX8Wrapper::Set_DX8_Light(2,&render_state.Lights[2]);
				if (render_state.LightEnable[3]) {
					DX8Wrapper::Set_DX8_Light(3,&render_state.Lights[3]);
				}
				else {
					DX8Wrapper::Set_DX8_Light(3,nullptr);
				}
			}
			else {
				DX8Wrapper::Set_DX8_Light(2,nullptr);
			}
		}
		else {
			DX8Wrapper::Set_DX8_Light(1,nullptr);
		}
	}
	else {
		DX8Wrapper::Set_DX8_Light(0,nullptr);
	}


}

// ----------------------------------------------------------------------------

static void Draw_Pool_Run(SortingNodeStruct* state, unsigned start_index, unsigned polygon_count, unsigned first_vertex, unsigned end_vertex)
{
	Apply_Render_State(state->sorting_state);
	const bool soft = Begin_Soft(state);

	DX8Wrapper::Draw_Triangles(
		start_index*3,
		polygon_count,
		first_vertex,
		end_vertex-first_vertex);

	End_Soft(soft);
}

// ----------------------------------------------------------------------------

void SortingRendererClass::Flush_Sorting_Pool()
{
	if (!overlapping_node_count) return;

	SNAPSHOT_SAY(("SortingSystem - Flush"));

	// Fill dynamic index buffer with sorting index buffer vertices
	TempIndexStruct* tis=Get_Temp_Index_Array(overlapping_polygon_count);

	unsigned vertexAllocCount = overlapping_vertex_count;
	if (DynamicVBAccessClass::Get_Default_Vertex_Count() < DEFAULT_SORTING_VERTEX_COUNT)
		vertexAllocCount = DEFAULT_SORTING_VERTEX_COUNT;	//make sure that we force the DX8 dynamic vertex buffer to maximum size
	if (overlapping_vertex_count > vertexAllocCount)
		vertexAllocCount = overlapping_vertex_count;
	WWASSERT(DEFAULT_SORTING_VERTEX_COUNT == 1 || vertexAllocCount <= DEFAULT_SORTING_VERTEX_COUNT);
	DynamicVBAccessClass dyn_vb_access(BUFFER_TYPE_DYNAMIC_DX8,dynamic_fvf_type,vertexAllocCount/*overlapping_vertex_count*/);
	{
		DynamicVBAccessClass::WriteLockClass lock(&dyn_vb_access);
		VertexFormatXYZNDUV2* dest_verts=(VertexFormatXYZNDUV2 *)lock.Get_Formatted_Vertex_Array();

		unsigned polygon_array_offset=0;
		unsigned vertex_array_offset=0;
		for (unsigned node_id=0;node_id<overlapping_node_count;++node_id) {
			SortingNodeStruct* state=overlapping_nodes[node_id];
			const VertexFormatXYZNDUV2* src_verts=Source_Vertices(state);

			// If you have a crash in here and "dest_verts" points to illegal memory area,
			// it is because D3D is in illegal state, and the only known cure is rebooting.
			// This illegal state is usually caused by Quake3-engine powered games such as MOHAA.
			memcpy(dest_verts, src_verts, sizeof(VertexFormatXYZNDUV2)*state->vertex_count);
			dest_verts += state->vertex_count;

			// Only the third column of world*view is needed, to get each triangle's
			// view depth. Both operands are D3DMATRIX, so this stays row-major.
			const D3DMATRIX& world=state->sorting_state.world;
			const D3DMATRIX& view=state->sorting_state.view;
			const D3DMATRIX product=world*view;
			const float (&mtx)[4][4]=product.m;

			const unsigned short* indices=Source_Indices(state);

			if (mtx[0][2] == 0.0f && mtx[1][2] == 0.0f && mtx[3][2] == 0.0f && mtx[2][2] == 1.0f) {
				// The common case for particle systems.
				for (int i=0;i<state->polygon_count;++i) {
					unsigned short idx1=indices[i*3]-state->min_vertex_index;
					unsigned short idx2=indices[i*3+1]-state->min_vertex_index;
					unsigned short idx3=indices[i*3+2]-state->min_vertex_index;
					WWASSERT(idx1<state->vertex_count);
					WWASSERT(idx2<state->vertex_count);
					WWASSERT(idx3<state->vertex_count);
					const VertexFormatXYZNDUV2 *v1 = src_verts + idx1;
					const VertexFormatXYZNDUV2 *v2 = src_verts + idx2;
					const VertexFormatXYZNDUV2 *v3 = src_verts + idx3;
					unsigned array_index=i+polygon_array_offset;
					WWASSERT(array_index<overlapping_polygon_count);
					TempIndexStruct *tis_ptr = tis + array_index;
					tis_ptr->tri.i = idx1 + vertex_array_offset;
					tis_ptr->tri.j = idx2 + vertex_array_offset;
					tis_ptr->tri.k = idx3 + vertex_array_offset;
					tis_ptr->idx = node_id;
					tis_ptr->z = (v1->z + v2->z + v3->z)/3.0f;
					DEBUG_ASSERTCRASH((! _isnan(tis_ptr->z) && _finite(tis_ptr->z)), ("Triangle has invalid center"));
				}
			} else {
				for (int i=0;i<state->polygon_count;++i) {
					unsigned short idx1=indices[i*3]-state->min_vertex_index;
					unsigned short idx2=indices[i*3+1]-state->min_vertex_index;
					unsigned short idx3=indices[i*3+2]-state->min_vertex_index;
					WWASSERT(idx1<state->vertex_count);
					WWASSERT(idx2<state->vertex_count);
					WWASSERT(idx3<state->vertex_count);
					const VertexFormatXYZNDUV2 *v1 = src_verts + idx1;
					const VertexFormatXYZNDUV2 *v2 = src_verts + idx2;
					const VertexFormatXYZNDUV2 *v3 = src_verts + idx3;
					unsigned array_index=i+polygon_array_offset;
					WWASSERT(array_index<overlapping_polygon_count);
					TempIndexStruct *tis_ptr = tis + array_index;
					tis_ptr->tri.i = idx1 + vertex_array_offset;
					tis_ptr->tri.j = idx2 + vertex_array_offset;
					tis_ptr->tri.k = idx3 + vertex_array_offset;
					tis_ptr->idx = node_id;
					tis_ptr->z = (mtx[0][2]*(v1->x + v2->x + v3->x) +
												mtx[1][2]*(v1->y + v2->y + v3->y) +
												mtx[2][2]*(v1->z + v2->z + v3->z))/3.0f + mtx[3][2];
					DEBUG_ASSERTCRASH((! _isnan(tis_ptr->z) && _finite(tis_ptr->z)), ("Triangle has invalid center"));
				}
			}

			state->min_vertex_index=vertex_array_offset;

			polygon_array_offset+=state->polygon_count;
			vertex_array_offset+=state->vertex_count;
		}
	}

	Sort(tis, tis + overlapping_polygon_count);

	// TheSuperHackers @fix stephanmeesters 10/06/2026
	// Split rendering into chunks to prevent a crash when exceeding the 16-bit index buffer limit.
	constexpr const unsigned MAX_INDEX_CHUNK = 65535;
	unsigned chunkOffset = 0;
	while (chunkOffset < overlapping_polygon_count)
	{
		unsigned chunkCount = overlapping_polygon_count - chunkOffset;
		if (chunkCount * 3 > MAX_INDEX_CHUNK) {
			chunkCount = MAX_INDEX_CHUNK / 3;
		}
		const unsigned chunkEnd = chunkOffset + chunkCount;

		DynamicIBAccessClass dyn_ib_access(BUFFER_TYPE_DYNAMIC_DX8,chunkCount*3);
		{
			DynamicIBAccessClass::WriteLockClass lock(&dyn_ib_access);
			ShortVectorIStruct* sorted_polygon_index_array=(ShortVectorIStruct*)lock.Get_Index_Array();

			for (unsigned a=0;a<chunkCount;++a) {
				sorted_polygon_index_array[a]=tis[chunkOffset + a].tri;
			}
		}

		// Set index buffer and render!

		DX8Wrapper::Set_Index_Buffer(dyn_ib_access,0); // Override with this buffer (do something to prevent need for this!)
		DX8Wrapper::Set_Vertex_Buffer(dyn_vb_access); // Override with this buffer (do something to prevent need for this!)

		DX8Wrapper::Apply_Render_State_Changes();

		unsigned count_to_render=0;
		unsigned start_index=0;
		unsigned node_id=0;
		unsigned first_vertex=0;
		unsigned end_vertex=0;
		for (unsigned i=chunkOffset;i<chunkEnd;++i) {
			SortingNodeStruct* node=overlapping_nodes[tis[i].idx];
			const bool object_first=Has_Object_Before(tis[i].z);
			const bool new_state=(tis[i].idx!=node_id) &&
				!(BlendBatching && Same_Node_State(overlapping_nodes[node_id], node));

			if (count_to_render && (object_first || new_state)) {
				Draw_Pool_Run(overlapping_nodes[node_id],start_index,count_to_render,first_vertex,end_vertex);
				count_to_render=0;
			}

			// Object nodes bind their own buffers, so the pool's go back afterwards.
			if (object_first) {
				Draw_Objects_Before(tis[i].z);
				DX8Wrapper::Set_Index_Buffer(dyn_ib_access,0);
				DX8Wrapper::Set_Vertex_Buffer(dyn_vb_access);
			}

			if (!count_to_render) {
				start_index=i - chunkOffset;
				first_vertex=node->min_vertex_index;
				end_vertex=first_vertex+node->vertex_count;
			}
			else {
				first_vertex=std::min<unsigned>(first_vertex, node->min_vertex_index);
				end_vertex=std::max<unsigned>(end_vertex, node->min_vertex_index+node->vertex_count);
			}
			node_id=tis[i].idx;
			count_to_render++;	//keep track of number of polygons of same kind
		}

		// Render any remaining polygons...
		if (count_to_render) {
			Draw_Pool_Run(overlapping_nodes[node_id],start_index,count_to_render,first_vertex,end_vertex);
		}

		chunkOffset += chunkCount;
	}

	// Release all references and return nodes back to the clean list for the frame...
	for (unsigned node_id=0;node_id<overlapping_node_count;++node_id) {
		SortingNodeStruct* state=overlapping_nodes[node_id];
		Release_Refs(state);
		clean_list.push_back(state);
	}
	overlapping_node_count=0;
	overlapping_polygon_count=0;
	overlapping_vertex_count=0;

	SNAPSHOT_SAY(("SortingSystem - Done flushing"));

}

// ----------------------------------------------------------------------------
//
// Draws the additive nodes after the depth sorted pool, one draw per run of equal state.
//
// ----------------------------------------------------------------------------

void SortingRendererClass::Flush_Additive_Pool()
{
	if (additive_list.empty()) {
		return;
	}

	std::stable_sort(additive_list.begin(), additive_list.end(), Additive_Draw_Order);

	const unsigned MAX_BATCH_COUNT = 65535;
	size_t first = 0;
	while (first < additive_list.size())
	{
		unsigned vertex_count = 0;
		unsigned index_count = 0;
		size_t last = first;
		while (last < additive_list.size()) {
			const SortingNodeStruct* state = additive_list[last];
			if (vertex_count + state->vertex_count > MAX_BATCH_COUNT || index_count + state->polygon_count*3 > MAX_BATCH_COUNT) {
				break;
			}
			vertex_count += state->vertex_count;
			index_count += state->polygon_count*3;
			++last;
		}

		DynamicVBAccessClass dyn_vb_access(BUFFER_TYPE_DYNAMIC_DX8,dynamic_fvf_type,vertex_count);
		DynamicIBAccessClass dyn_ib_access(BUFFER_TYPE_DYNAMIC_DX8,index_count);
		{
			DynamicVBAccessClass::WriteLockClass vb_lock(&dyn_vb_access);
			DynamicIBAccessClass::WriteLockClass ib_lock(&dyn_ib_access);
			VertexFormatXYZNDUV2* dest_verts=(VertexFormatXYZNDUV2 *)vb_lock.Get_Formatted_Vertex_Array();
			unsigned short* dest_indices=ib_lock.Get_Index_Array();

			unsigned vertex_offset = 0;
			unsigned index_offset = 0;
			for (size_t i=first;i<last;++i) {
				SortingNodeStruct* state = additive_list[i];
				memcpy(dest_verts+vertex_offset, Source_Vertices(state), sizeof(VertexFormatXYZNDUV2)*state->vertex_count);

				const unsigned short* src_indices=Source_Indices(state);
				const unsigned node_index_count=state->polygon_count*3;
				for (unsigned j=0;j<node_index_count;++j) {
					dest_indices[index_offset+j]=(unsigned short)(src_indices[j]-state->min_vertex_index+vertex_offset);
				}

				state->start_index=(unsigned short)index_offset;
				state->min_vertex_index=(unsigned short)vertex_offset;
				vertex_offset+=state->vertex_count;
				index_offset+=node_index_count;
			}
		}

		DX8Wrapper::Set_Index_Buffer(dyn_ib_access,0);
		DX8Wrapper::Set_Vertex_Buffer(dyn_vb_access);
		DX8Wrapper::Apply_Render_State_Changes();

		size_t run = first;
		while (run < last) {
			SortingNodeStruct* head = additive_list[run];
			unsigned polygon_count = head->polygon_count;
			unsigned end_vertex = head->min_vertex_index + head->vertex_count;
			size_t next = run + 1;
			while (next < last && Same_Node_State(head, additive_list[next])) {
				polygon_count += additive_list[next]->polygon_count;
				end_vertex = additive_list[next]->min_vertex_index + additive_list[next]->vertex_count;
				++next;
			}

			Apply_Render_State(head->sorting_state);
			const bool soft = Begin_Soft(head);
			DX8Wrapper::Draw_Triangles(
				head->start_index,
				polygon_count,
				head->min_vertex_index,
				end_vertex - head->min_vertex_index);
			End_Soft(soft);
			run = next;
		}

		first = last;
	}

	Recycle_Nodes(additive_list);
}

// ----------------------------------------------------------------------------

void SortingRendererClass::Flush()
{
	WWPROFILE("SortingRenderer::Flush");
	Matrix4x4 old_view;
	Matrix4x4 old_world;
	DX8Wrapper::Get_Transform(D3DTS_VIEW,old_view);
	DX8Wrapper::Get_Transform(D3DTS_WORLD,old_world);

	std::stable_sort(object_list.begin(), object_list.end(), Object_Depth_Order);
	next_object=0;

	bool old_enable=DX8Wrapper::_Is_Triangle_Draw_Enabled();
	DX8Wrapper::_Enable_Triangle_Draw(_EnableTriangleDraw);
	Flush_Sorting_Pool();
	while (next_object<object_list.size()) {
		Draw_Object_Node(object_list[next_object++]);
	}
	Flush_Additive_Pool();
	for (size_t i=0;i<additive_object_list.size();++i) {
		Draw_Object_Node(additive_object_list[i]);
	}
	DX8Wrapper::_Enable_Triangle_Draw(old_enable);

	Recycle_Nodes(object_list);
	Recycle_Nodes(additive_object_list);

	DX8Wrapper::Set_Index_Buffer(nullptr,0);
	DX8Wrapper::Set_Vertex_Buffer(nullptr);
	total_sorting_vertices=0;

	DynamicIBAccessClass::_Reset(false);
	DynamicVBAccessClass::_Reset(false);


	DX8Wrapper::Set_Transform(D3DTS_VIEW,old_view);
	DX8Wrapper::Set_Transform(D3DTS_WORLD,old_world);

}

// ----------------------------------------------------------------------------

void SortingRendererClass::Deinit()
{
	//
	//	Flush the sorting pool
	//
	for (unsigned node_id=0;node_id<overlapping_node_count;++node_id) {
		delete overlapping_nodes[node_id];
	}
	overlapping_node_count=0;
	overlapping_polygon_count=0;
	overlapping_vertex_count=0;

	Delete_Nodes(additive_list);
	Delete_Nodes(object_list);
	Delete_Nodes(additive_object_list);
	Delete_Nodes(clean_list);

	delete[] temp_index_array;
	temp_index_array=nullptr;
	temp_index_array_count=0;
}
