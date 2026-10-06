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

// W3DHeadlight.cpp ///////////////////////////////////////////////////////////////////////////////
// Headlights drawn by headlight.hlsl, as a beam in the air and the light it throws on the scene
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Lib/BaseType.h"
#include "WWLib/always.h"
#include "W3DDevice/GameClient/W3DHeadlight.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "Common/GlobalData.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8caps.h"
#include "WW3D2/dx8vertexbuffer.h"
#include "WW3D2/dx8indexbuffer.h"
#include "WW3D2/dx8fvf.h"
#include "WW3D2/formconv.h"
#include "WW3D2/shader.h"
#include "WW3D2/vertmaterial.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/rendobj.h"
#include "WW3D2/mesh.h"
#include "WW3D2/meshmdl.h"
#include <map>
#include <vector>

W3DHeadlightManager *TheW3DHeadlights = nullptr;

// CONTRA_HEADLIGHTSHADER bisects faults: 0 keeps the headlight meshes, 1 draws the shader.
static const Int HeadlightShaderMode = (getenv("CONTRA_HEADLIGHTSHADER") != nullptr) ? atoi(getenv("CONTRA_HEADLIGHTSHADER")) : 1;

// A beam seen closer to end on than this sine has faded out whole.
static const Real END_ON_FADE = 0.5f;

W3DHeadlightManager::W3DHeadlightManager()
	: m_count(0),
	  m_frame(0),
	  m_loaded(FALSE),
	  m_beamVertexShader(0),
	  m_beamPixelShader(0),
	  m_poolVertexShader(0),
	  m_poolPixelShader(0),
	  m_poolMaxPixelShader(0),
	  m_poolTexture(nullptr)
{
}

W3DHeadlightManager::~W3DHeadlightManager()
{
	ReleaseResources();
}

void W3DHeadlightManager::ReleaseResources()
{
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device != nullptr)
	{
		DX8_DELETE_VERTEX_SHADER(device, m_beamVertexShader);
		DX8_DELETE_PIXEL_SHADER(device, m_beamPixelShader);
		DX8_DELETE_VERTEX_SHADER(device, m_poolVertexShader);
		DX8_DELETE_PIXEL_SHADER(device, m_poolPixelShader);
		DX8_DELETE_PIXEL_SHADER(device, m_poolMaxPixelShader);
	}
	m_beamVertexShader = 0;
	m_beamPixelShader = 0;
	m_poolVertexShader = 0;
	m_poolPixelShader = 0;
	m_poolMaxPixelShader = 0;
	if (m_poolTexture != nullptr)
	{
		m_poolTexture->Release();
		m_poolTexture = nullptr;
	}
	m_loaded = FALSE;
}

// Loaded on first use, once the device can say whether it runs them.
Bool W3DHeadlightManager::loadShaders()
{
#if defined(BUILD_WITH_D3D9)
	if (!m_loaded)
	{
		m_loaded = TRUE;
		const DX8Caps *caps = DX8Wrapper::Get_Current_Caps();
		if (HeadlightShaderMode != 0 && caps != nullptr && caps->Get_Vertex_Shader_Major_Version() >= 3 &&
			caps->Get_Pixel_Shader_Major_Version() >= 3)
		{
			// The draws keep their fixed-function vertex formats bound, so the declaration only has to be valid.
			DWORD declaration[] =
			{
				D3DVSD_STREAM(0),
				D3DVSD_REG(0, D3DVSDT_FLOAT3),
				D3DVSD_END()
			};
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\headlightbeam.vso", declaration, 0, true, &m_beamVertexShader)))
			{
				m_beamVertexShader = 0;
			}
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\headlightbeam.pso", nullptr, 0, false, &m_beamPixelShader)))
			{
				m_beamPixelShader = 0;
			}
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\headlightpool.vso", declaration, 0, true, &m_poolVertexShader)))
			{
				m_poolVertexShader = 0;
			}
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\headlightpool.pso", nullptr, 0, false, &m_poolPixelShader)))
			{
				m_poolPixelShader = 0;
			}

			// Without these the pools draw one by one and light each other's ground twice.
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\headlightpoolmax.pso", nullptr, 0, false, &m_poolMaxPixelShader)))
			{
				m_poolMaxPixelShader = 0;
			}
			if (m_poolMaxPixelShader != 0 && FAILED(DX8Wrapper::_Get_D3D_Device8()->CreateTexture(MAX_LIGHTS * 3, 1, 1, D3DUSAGE_DYNAMIC,
				D3DFMT_A32B32G32R32F, D3DPOOL_DEFAULT, &m_poolTexture, nullptr)))
			{
				m_poolTexture = nullptr;
			}
		}
	}
	return m_beamVertexShader != 0 && m_beamPixelShader != 0 && m_poolVertexShader != 0 && m_poolPixelShader != 0;
#else
	return FALSE;
#endif
}

Bool W3DHeadlightManager::isActive()
{
	return TheGlobalData->m_headlightTuning.enabled && loadShaders();
}

void W3DHeadlightManager::add(const Vector3 &start, const Vector3 &end, Real radius, const HeadlightShaderTuning *own)
{
	// Lights that no render took belong to an older frame.
	const UnsignedInt frame = WW3D::Get_Frame_Count();
	if (frame != m_frame)
	{
		m_frame = frame;
		m_count = 0;
	}

	if (m_count == MAX_LIGHTS || radius <= 0.0f)
	{
		return;
	}

	Light &light = m_lights[m_count++];
	light.start = start;
	light.end = end;
	light.radius = radius;
	light.own = own;
}

// Cones whose middle lines lie closer than this share of their radius draw as one lamp. It joins the
// crossed planes of one cone and twin lamps set side by side.
static const Real MERGE_SHARE = 0.5f;

// A lamp's ends are this share of its length, when telling the narrow end from the wide one.
static const Real END_SHARE = 0.1f;

// Vertices closer than this are one, since a mesh splits them wherever their texture coordinates differ.
static const Real WELD_SIZE = 0.05f;

// A box in a headlight mesh's own space, around the triangles that join up or around one lamp.
struct BeamBox
{
	Vector3 low;
	Vector3 high;

	void grow(const Vector3 &point)
	{
		low.Update_Min(point);
		high.Update_Max(point);
	}
};

static Int Find_Root(std::vector<Int> &parent, Int at)
{
	while (parent[at] != at)
	{
		parent[at] = parent[parent[at]];
		at = parent[at];
	}
	return at;
}

// Boxes the parts of a mesh that its triangles join, and tells which part each vertex is in.
static void Find_Parts(MeshModelClass &model, std::vector<BeamBox> &parts, std::vector<Int> &partOfVertex)
{
	typedef std::pair<Int, std::pair<Int, Int> > WeldKey;

	const Int vertexCount = model.Get_Vertex_Count();
	const Vector3 *vertices = model.Get_Vertex_Array();
	std::map<WeldKey, Int> places;
	std::vector<Int> placeOf(vertexCount);
	for (Int i = 0; i < vertexCount; i++)
	{
		const WeldKey key((Int)floor(vertices[i].X / WELD_SIZE + 0.5f),
			std::make_pair((Int)floor(vertices[i].Y / WELD_SIZE + 0.5f), (Int)floor(vertices[i].Z / WELD_SIZE + 0.5f)));
		placeOf[i] = places.insert(std::make_pair(key, (Int)places.size())).first->second;
	}

	std::vector<Int> parent(places.size());
	for (size_t i = 0; i < parent.size(); i++)
	{
		parent[i] = (Int)i;
	}
	const TriIndex *triangles = model.Get_Polygon_Array();
	for (Int i = 0; i < model.Get_Polygon_Count(); i++)
	{
		const Int root = Find_Root(parent, placeOf[triangles[i][0]]);
		parent[Find_Root(parent, placeOf[triangles[i][1]])] = root;
		parent[Find_Root(parent, placeOf[triangles[i][2]])] = root;
	}

	std::map<Int, Int> partOfRoot;
	partOfVertex.resize(vertexCount);
	for (Int i = 0; i < vertexCount; i++)
	{
		const Int root = Find_Root(parent, placeOf[i]);
		std::map<Int, Int>::iterator found = partOfRoot.find(root);
		if (found == partOfRoot.end())
		{
			partOfVertex[i] = (Int)parts.size();
			partOfRoot[root] = (Int)parts.size();
			BeamBox part;
			part.low = vertices[i];
			part.high = vertices[i];
			parts.push_back(part);
		}
		else
		{
			partOfVertex[i] = found->second;
			parts[found->second].grow(vertices[i]);
		}
	}
}

// The larger of a box's two half sizes across the beam.
static Real Box_Radius(const BeamBox &box, Int sideA, Int sideB)
{
	return max(box.high[sideA] - box.low[sideA], box.high[sideB] - box.low[sideB]) * 0.5f;
}

// Cones whose directions' cosine is above this aim the same way.
static const Real SAME_AIM = 0.97f;

// A cone fitted to some of a headlight mesh's vertices, in the mesh's own space.
struct ConeFit
{
	Vector3 start;	///< the middle of the narrow end, where the lamp is
	Vector3 end;		///< the middle of the wide end
	Real radius;		///< the farthest a vertex lies from the line between them
	Bool narrow;		///< one end is clearly narrower than the other
};

// Fits a cone along the points' long axis, which the spread of the points found by power iteration gives.
static Bool Fit_Cone(const std::vector<Vector3> &points, ConeFit &fit)
{
	Vector3 middle(0.0f, 0.0f, 0.0f);
	for (size_t i = 0; i < points.size(); i++)
	{
		middle += points[i];
	}
	middle /= (Real)points.size();

	const Vector3 zero(0.0f, 0.0f, 0.0f);
	Matrix3x3 spread(zero, zero, zero);
	for (size_t i = 0; i < points.size(); i++)
	{
		const Vector3 offset = points[i] - middle;
		for (Int row = 0; row < 3; row++)
		{
			spread[row] += offset * offset[row];
		}
	}

	// The spread is symmetric, so its rows are its columns.
	Int widest = 0;
	for (Int row = 1; row < 3; row++)
	{
		if (spread[row].Length() > spread[widest].Length())
		{
			widest = row;
		}
	}
	Vector3 axis = spread[widest];
	for (Int step = 0; step < 32; step++)
	{
		axis = spread * axis;

		// The comparison also fails a length that is not a number.
		const Real length = axis.Length();
		if (!(length > 0.0f && length < 1.0e30f))
		{
			return FALSE;
		}
		axis /= length;
	}

	Real lowest = Vector3::Dot_Product(points[0] - middle, axis);
	Real highest = lowest;
	for (size_t i = 1; i < points.size(); i++)
	{
		const Real along = Vector3::Dot_Product(points[i] - middle, axis);
		lowest = min(lowest, along);
		highest = max(highest, along);
	}
	const Real span = highest - lowest;
	if (span <= 0.0001f)
	{
		return FALSE;
	}

	Real lowSpread = 0.0f;
	Real highSpread = 0.0f;
	Vector3 lowSum(0.0f, 0.0f, 0.0f);
	Vector3 highSum(0.0f, 0.0f, 0.0f);
	Int lowCount = 0;
	Int highCount = 0;
	for (size_t i = 0; i < points.size(); i++)
	{
		const Vector3 offset = points[i] - middle;
		const Real along = Vector3::Dot_Product(offset, axis);
		const Real across = (offset - axis * along).Length();
		if (along <= lowest + END_SHARE * span)
		{
			lowSpread = max(lowSpread, across);
			lowSum += points[i];
			lowCount++;
		}
		if (along >= highest - END_SHARE * span)
		{
			highSpread = max(highSpread, across);
			highSum += points[i];
			highCount++;
		}
	}

	fit.narrow = fabs(lowSpread - highSpread) > 0.1f * max(lowSpread, highSpread);
	const Bool lampLow = lowSpread < highSpread;
	fit.start = (lampLow ? lowSum : highSum) / (Real)(lampLow ? lowCount : highCount);
	fit.end = (lampLow ? highSum : lowSum) / (Real)(lampLow ? highCount : lowCount);

	Vector3 aim = fit.end - fit.start;
	const Real length = aim.Length();
	if (length < 0.0001f)
	{
		return FALSE;
	}
	aim /= length;
	fit.radius = 0.0f;
	for (size_t i = 0; i < points.size(); i++)
	{
		const Vector3 offset = points[i] - fit.start;
		fit.radius = max(fit.radius, (offset - aim * Vector3::Dot_Product(offset, aim)).Length());
	}
	return TRUE;
}

// Gives each cone of a mesh its own direction, for meshes whose lamps aim different ways. Zero when a part
// fits no cone or a lamp has no narrow end, which leaves the mesh to the shared axis.
static Int Find_Cone_Beams(MeshModelClass &model, Int partCount, const std::vector<Int> &partOfVertex,
	W3DHeadlightManager::Beam *beams, Int maxBeams)
{
	const Vector3 *vertices = model.Get_Vertex_Array();
	std::vector<std::vector<Vector3> > partPoints(partCount);
	for (size_t i = 0; i < partOfVertex.size(); i++)
	{
		partPoints[partOfVertex[i]].push_back(vertices[i]);
	}

	std::vector<ConeFit> fits(partCount);
	std::vector<Vector3> aims(partCount);
	for (Int i = 0; i < partCount; i++)
	{
		if (!Fit_Cone(partPoints[i], fits[i]))
		{
			return 0;
		}
		aims[i] = fits[i].end - fits[i].start;
		aims[i].Normalize();
	}

	std::vector<Int> lampOfPart(partCount);
	Int lampCount = 0;
	for (Int i = 0; i < partCount; i++)
	{
		lampOfPart[i] = -1;
		for (Int j = 0; j < i && lampOfPart[i] < 0; j++)
		{
			// Cones aiming the same way with lamps within a radius of each other overlap along most of their length.
			const Real reach = max(fits[i].radius, fits[j].radius);
			if ((fits[i].start - fits[j].start).Length() < reach && Vector3::Dot_Product(aims[i], aims[j]) > SAME_AIM)
			{
				lampOfPart[i] = lampOfPart[j];
			}
		}
		if (lampOfPart[i] < 0)
		{
			lampOfPart[i] = lampCount++;
		}
	}

	std::vector<std::vector<Vector3> > lampPoints(lampCount);
	for (Int i = 0; i < partCount; i++)
	{
		lampPoints[lampOfPart[i]].insert(lampPoints[lampOfPart[i]].end(), partPoints[i].begin(), partPoints[i].end());
	}

	Int count = 0;
	for (Int i = 0; i < lampCount; i++)
	{
		ConeFit lamp;
		if (!Fit_Cone(lampPoints[i], lamp) || !lamp.narrow)
		{
			return 0;
		}
		if (count < maxBeams)
		{
			beams[count].start = lamp.start;
			beams[count].end = lamp.end;
			beams[count].radius = lamp.radius;
			count++;
		}
	}
	return count;
}

Bool W3DHeadlightManager::isOpaque(RenderObjClass &mesh)
{
	if (mesh.Class_ID() != RenderObjClass::CLASSID_MESH)
	{
		return FALSE;
	}
	MeshModelClass *model = ((MeshClass &)mesh).Peek_Model();
	if (model == nullptr || model->Get_Pass_Count() == 0)
	{
		return FALSE;
	}

	for (Int pass = 0; pass < model->Get_Pass_Count(); pass++)
	{
		if (model->Has_Shader_Array(pass))
		{
			for (Int polygon = 0; polygon < model->Get_Polygon_Count(); polygon++)
			{
				if (model->Get_Shader(polygon, pass).Get_Dst_Blend_Func() != ShaderClass::DSTBLEND_ZERO)
				{
					return FALSE;
				}
			}
		}
		else if (model->Get_Single_Shader(pass).Get_Dst_Blend_Func() != ShaderClass::DSTBLEND_ZERO)
		{
			return FALSE;
		}
	}
	return TRUE;
}

Int W3DHeadlightManager::findBeams(RenderObjClass &mesh, const Vector3 &modelMiddle, Beam *beams, Int maxBeams)
{
	std::vector<BeamBox> parts;
	std::vector<Int> partOfVertex;
	MeshModelClass *model = nullptr;
	if (mesh.Class_ID() == RenderObjClass::CLASSID_MESH)
	{
		model = ((MeshClass &)mesh).Peek_Model();
		if (model != nullptr && model->Get_Vertex_Count() > 0)
		{
			Find_Parts(*model, parts, partOfVertex);
		}
	}
	if (parts.empty())
	{
		AABoxClass box;
		mesh.Get_Obj_Space_Bounding_Box(box);
		BeamBox whole;
		whole.low = box.Center - box.Extent;
		whole.high = box.Center + box.Extent;
		parts.push_back(whole);
	}

	if (TheGlobalData->m_headlightTuning.perConeAim && model != nullptr && !partOfVertex.empty())
	{
		const Int count = Find_Cone_Beams(*model, (Int)parts.size(), partOfVertex, beams, maxBeams);
		if (count > 0)
		{
			return count;
		}
	}

	BeamBox all = parts[0];
	for (size_t i = 1; i < parts.size(); i++)
	{
		all.grow(parts[i].low);
		all.grow(parts[i].high);
	}
	Vector3 middle;
	Matrix3D::Inverse_Transform_Vector(mesh.Get_Transform(), modelMiddle, &middle);

	// The beams run along the side of the mesh that is long and leads away from the model's middle.
	// Length alone fails a mesh of several lamps in a row, which is wider than its cones are long.
	Int axis = 0;
	Real best = 0.0f;
	for (Int i = 0; i < 3; i++)
	{
		const Real extent = (all.high[i] - all.low[i]) * 0.5f;
		const Real score = (fabs((all.low[i] + all.high[i]) * 0.5f - middle[i]) + 0.01f) * extent;
		if (score > best)
		{
			best = score;
			axis = i;
		}
	}
	if (best <= 0.0f)
	{
		return 0;
	}
	const Int sideA = (axis + 1) % 3;
	const Int sideB = (axis + 2) % 3;

	std::vector<BeamBox> lamps;
	std::vector<Int> lampOfPart(parts.size());
	for (size_t i = 0; i < parts.size(); i++)
	{
		const BeamBox &part = parts[i];
		const Real partRadius = Box_Radius(part, sideA, sideB);
		Bool merged = FALSE;
		for (size_t j = 0; j < lamps.size() && !merged; j++)
		{
			BeamBox &lamp = lamps[j];
			const Real reach = MERGE_SHARE * max(partRadius, Box_Radius(lamp, sideA, sideB));
			const Real offA = (part.low[sideA] + part.high[sideA] - lamp.low[sideA] - lamp.high[sideA]) * 0.5f;
			const Real offB = (part.low[sideB] + part.high[sideB] - lamp.low[sideB] - lamp.high[sideB]) * 0.5f;
			if (offA * offA + offB * offB < reach * reach)
			{
				lamp.grow(part.low);
				lamp.grow(part.high);
				lampOfPart[i] = (Int)j;
				merged = TRUE;
			}
		}
		if (!merged)
		{
			lampOfPart[i] = (Int)lamps.size();
			lamps.push_back(part);
		}
	}

	// How wide each lamp's vertices spread near its two ends, since the lamp is the narrow end of its cone.
	std::vector<BeamBox> lowEnds(lamps.size());
	std::vector<BeamBox> highEnds(lamps.size());
	std::vector<Bool> lowSet(lamps.size(), FALSE);
	std::vector<Bool> highSet(lamps.size(), FALSE);
	for (size_t i = 0; i < partOfVertex.size(); i++)
	{
		const Int lamp = lampOfPart[partOfVertex[i]];
		const Vector3 &vertex = model->Get_Vertex_Array()[i];
		const Real along = (vertex[axis] - lamps[lamp].low[axis]) / max(lamps[lamp].high[axis] - lamps[lamp].low[axis], 0.0001f);
		if (along <= END_SHARE)
		{
			if (!lowSet[lamp])
			{
				lowEnds[lamp].low = vertex;
				lowEnds[lamp].high = vertex;
				lowSet[lamp] = TRUE;
			}
			lowEnds[lamp].grow(vertex);
		}
		if (along >= 1.0f - END_SHARE)
		{
			if (!highSet[lamp])
			{
				highEnds[lamp].low = vertex;
				highEnds[lamp].high = vertex;
				highSet[lamp] = TRUE;
			}
			highEnds[lamp].grow(vertex);
		}
	}

	Int count = 0;
	for (size_t i = 0; i < lamps.size() && count < maxBeams; i++)
	{
		const BeamBox &lamp = lamps[i];
		const Real radius = Box_Radius(lamp, sideA, sideB);
		if (lamp.high[axis] - lamp.low[axis] <= 0.0f || radius <= 0.0f)
		{
			continue;
		}

		// A shape with no narrow end, such as a flat quad, has its lamp at the end nearer the model's middle.
		Bool lampLow = fabs(lamp.low[axis] - middle[axis]) < fabs(lamp.high[axis] - middle[axis]);
		if (lowSet[i] && highSet[i])
		{
			const Real lowSpread = Box_Radius(lowEnds[i], sideA, sideB);
			const Real highSpread = Box_Radius(highEnds[i], sideA, sideB);
			if (fabs(lowSpread - highSpread) > 0.1f * max(lowSpread, highSpread))
			{
				lampLow = lowSpread < highSpread;
			}
		}

		Beam &beam = beams[count++];
		beam.start = (lamp.low + lamp.high) * 0.5f;
		beam.end = beam.start;
		beam.start[axis] = lampLow ? lamp.low[axis] : lamp.high[axis];
		beam.end[axis] = lampLow ? lamp.high[axis] : lamp.low[axis];
		beam.radius = radius;
	}
	return count;
}

#if defined(BUILD_WITH_D3D9)
// What one headlight's pool draws with.
struct PoolDraw
{
	Vector3 origin;
	Vector3 aim;
	Real range;
	Real spread;		///< tangent of the cone's half angle
	Vector4 color;	///< rgb = light over the scene's light, a = falloff power
	Real rect[4];		///< the light's place on screen in clip space: left, bottom, right, top
};

// The box around a sphere on screen, or the whole screen when the box reaches behind the camera. False when none of it shows.
static Bool Screen_Rect(const D3DMATRIX &clip, const Vector3 &center, Real radius, Real *rect)
{
	rect[0] = rect[1] = 1.0f;
	rect[2] = rect[3] = -1.0f;

	Bool behind = FALSE;
	for (Int corner = 0; corner < 8; corner++)
	{
		const Real x = center.X + ((corner & 1) ? radius : -radius);
		const Real y = center.Y + ((corner & 2) ? radius : -radius);
		const Real z = center.Z + ((corner & 4) ? radius : -radius);
		const Real clipX = x * clip.m[0][0] + y * clip.m[1][0] + z * clip.m[2][0] + clip.m[3][0];
		const Real clipY = x * clip.m[0][1] + y * clip.m[1][1] + z * clip.m[2][1] + clip.m[3][1];
		const Real clipW = x * clip.m[0][3] + y * clip.m[1][3] + z * clip.m[2][3] + clip.m[3][3];
		if (clipW < 0.01f)
		{
			behind = TRUE;
			break;
		}
		rect[0] = min(rect[0], clipX / clipW);
		rect[1] = min(rect[1], clipY / clipW);
		rect[2] = max(rect[2], clipX / clipW);
		rect[3] = max(rect[3], clipY / clipW);
	}

	if (behind)
	{
		rect[0] = rect[1] = -1.0f;
		rect[2] = rect[3] = 1.0f;
		return TRUE;
	}

	rect[0] = max(rect[0], -1.0f);
	rect[1] = max(rect[1], -1.0f);
	rect[2] = min(rect[2], 1.0f);
	rect[3] = min(rect[3], 1.0f);
	return rect[0] < rect[2] && rect[1] < rect[3];
}

// World travel per unit of clip-space w along the view ray through a point in clip space.
static Vector3 View_Ray(const D3DMATRIX &projection, const D3DMATRIX &toWorld, Real clipX, Real clipY)
{
	const Real cameraZ = 1.0f / projection.m[2][3];
	const Real cameraX = (clipX - cameraZ * projection.m[2][0]) / projection.m[0][0];
	const Real cameraY = (clipY - cameraZ * projection.m[2][1]) / projection.m[1][1];
	return Vector3(
		cameraX * toWorld.m[0][0] + cameraY * toWorld.m[1][0] + cameraZ * toWorld.m[2][0],
		cameraX * toWorld.m[0][1] + cameraY * toWorld.m[1][1] + cameraZ * toWorld.m[2][1],
		cameraX * toWorld.m[0][2] + cameraY * toWorld.m[1][2] + cameraZ * toWorld.m[2][2]);
}

static void Write_Pool_Quad(VertexFormatXYZNDUV2 *&verts, const Real *rect, const D3DMATRIX &projection, const D3DMATRIX &toWorld,
	const Vector4 &depthMap)
{
	for (Int corner = 0; corner < 4; corner++)
	{
		const Real clipX = (corner & 1) ? rect[2] : rect[0];
		const Real clipY = (corner & 2) ? rect[3] : rect[1];
		const Vector3 ray = View_Ray(projection, toWorld, clipX, clipY);
		verts->x = clipX;
		verts->y = clipY;
		verts->z = 0.5f;
		verts->nx = ray.X;
		verts->ny = ray.Y;
		verts->nz = ray.Z;
		verts->diffuse = 0xffffffffu;
		verts->u1 = clipX * depthMap.X + depthMap.Z;
		verts->v1 = clipY * depthMap.Y + depthMap.W;
		verts->u2 = 0.0f;
		verts->v2 = 0.0f;
		verts++;
	}
}

static void Write_Quad_Indices(UnsignedShort *indices, Int quads)
{
	for (Int i = 0; i < quads; i++)
	{
		const UnsignedShort base = (UnsignedShort)(i * 4);
		indices[i * 6 + 0] = base + 0;
		indices[i * 6 + 1] = base + 1;
		indices[i * 6 + 2] = base + 2;
		indices[i * 6 + 3] = base + 2;
		indices[i * 6 + 4] = base + 1;
		indices[i * 6 + 5] = base + 3;
	}
}
#endif

void W3DHeadlightManager::render(RenderInfoClass &rinfo)
{
#if defined(BUILD_WITH_D3D9)
	const Int count = m_count;
	m_count = 0;
	if (count == 0 || !isActive())
	{
		return;
	}

	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	rinfo.Camera.Apply();

	// Apply defers the view to the next state flush, so it is read from the camera, not the device.
	const D3DMATRIX view = To_D3DMATRIX(rinfo.Camera.Get_View_Matrix());
	D3DMATRIX projection;
	device->GetTransform(D3DTS_PROJECTION, &projection);
	if (fabs(projection.m[2][3]) < 1.0e-6f || fabs(projection.m[0][0]) < 1.0e-6f || fabs(projection.m[1][1]) < 1.0e-6f)
	{
		return;
	}
	D3DMATRIX toWorld;
	float det;
	Invert_D3DMATRIX(toWorld, &det, view);
	const D3DMATRIX clip = view * projection;
	const Vector3 eye(toWorld.m[3][0], toWorld.m[3][1], toWorld.m[3][2]);

	// Both draws read the scene's depth where that is the bound one, which leaves out reflections.
	IDirect3DTexture8 *depthTexture = DX8Wrapper::Peek_Scene_Depth_Texture();
	if (depthTexture != nullptr)
	{
		IDirect3DSurface8 *bound = nullptr;
		if (FAILED(device->GetDepthStencilSurface(&bound)) || bound == nullptr)
		{
			depthTexture = nullptr;
		}
		else
		{
			if (bound != DX8Wrapper::Peek_Scene_Depth_Surface())
			{
				depthTexture = nullptr;
			}
			bound->Release();
		}
	}
	Vector4 depthMap(0.0f, 0.0f, 0.0f, 0.0f);
	if (depthTexture != nullptr)
	{
		D3DSURFACE_DESC desc;
		depthTexture->GetLevelDesc(0, &desc);
		depthMap = W3DShaderManager::getClipToTargetMapping((Real)desc.Width, (Real)desc.Height);
	}

	// The light arrives divided by the scene's light, so a dark night lights up as much as a dim one.
	const RGBColor &ambient = TheGlobalData->m_terrainAmbient[0];
	const RGBColor &diffuse = TheGlobalData->m_terrainDiffuse[0];
	const Vector3 sceneLight(min(max(ambient.red + diffuse.red, 0.1f), 1.0f), min(max(ambient.green + diffuse.green, 0.1f), 1.0f),
		min(max(ambient.blue + diffuse.blue, 0.1f), 1.0f));

	// Cleared through the wrapper, so its record of the stage matches the device once it is unbound below.
	DX8Wrapper::Set_Texture(0, nullptr);
	VertexMaterialClass *material = VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(material);
	REF_PTR_RELEASE(material);
	DX8Wrapper::Set_Shader(ShaderClass::_PresetAdditiveSpriteShader);
	DX8Wrapper::Set_Transform(D3DTS_WORLD, Matrix3D(true));

	DynamicVBAccessClass vbAccess(BUFFER_TYPE_DYNAMIC_DX8, dynamic_fvf_type, count * 8);
	PoolDraw pools[MAX_LIGHTS];
	Int poolCount = 0;
	Int poolQuads = 0;
	Int beamCount = 0;
	const Bool clampPools = TheGlobalData->m_headlightTuning.poolClampBrightness && m_poolMaxPixelShader != 0 && m_poolTexture != nullptr;
	{
		DynamicVBAccessClass::WriteLockClass lock(&vbAccess);
		VertexFormatXYZNDUV2 *verts = lock.Get_Formatted_Vertex_Array();

		// The beams' quads come first, in world space.
		for (Int i = 0; i < count; i++)
		{
			const Light &light = m_lights[i];
			HeadlightShaderTuning tuning = TheGlobalData->m_headlightTuning;
			if (light.own != nullptr)
			{
				light.own->resolve(tuning);
			}
			if (tuning.beamIntensity <= 0.0f || tuning.beamLength <= 0.0f || tuning.beamWidth <= 0.0f)
			{
				continue;
			}

			Vector3 along = light.end - light.start;
			const Real length = along.Length();
			if (length < 0.01f)
			{
				continue;
			}
			along /= length;
			const Vector3 end = light.start + along * (length * tuning.beamLength);

			Vector3 toEye = eye - (light.start + end) * 0.5f;
			toEye.Normalize();
			Vector3 side;
			Vector3::Cross_Product(along, toEye, &side);
			const Real sine = side.Length();
			if (sine < 0.001f)
			{
				continue;
			}
			side *= light.radius * tuning.beamWidth / sine;

			Real level = min(sine / END_ON_FADE, 1.0f);
			level = level * level * (3.0f - 2.0f * level);
			const unsigned grey = (unsigned)(level * 255.0f + 0.5f);

			for (Int corner = 0; corner < 4; corner++)
			{
				const Real across = (corner & 1) ? 1.0f : -1.0f;
				const Vector3 at = ((corner & 2) ? end : light.start) + side * across;
				verts->x = at.X;
				verts->y = at.Y;
				verts->z = at.Z;
				// The beam's own settings ride in the vertex, so beams of every model share one draw.
				verts->nx = tuning.color.red * tuning.beamIntensity;
				verts->ny = tuning.color.green * tuning.beamIntensity;
				verts->nz = tuning.color.blue * tuning.beamIntensity;
				verts->diffuse = 0xff000000u | (grey << 16) | (grey << 8) | grey;
				verts->u1 = (corner & 2) ? 1.0f : 0.0f;
				verts->v1 = across;
				verts->u2 = tuning.beamFalloff;
				verts->v2 = 1.0f / max(tuning.beamSoftness, 0.01f);
				verts++;
			}
			beamCount++;
		}

		// The pools' quads follow, in clip space over each light's place on screen.
		for (Int i = 0; depthTexture != nullptr && i < count; i++)
		{
			const Light &light = m_lights[i];
			HeadlightShaderTuning tuning = TheGlobalData->m_headlightTuning;
			if (light.own != nullptr)
			{
				light.own->resolve(tuning);
			}
			if (tuning.poolIntensity <= 0.0f || tuning.poolRange <= 0.0f)
			{
				continue;
			}

			Vector3 aim = light.end - light.start;
			const Real length = aim.Length();
			if (length < 0.01f)
			{
				continue;
			}
			aim = aim * (cos(tuning.poolPitch) / length) - Vector3(0.0f, 0.0f, sin(tuning.poolPitch));
			const Real aimLength = aim.Length();
			if (aimLength < 0.001f)
			{
				continue;
			}
			aim /= aimLength;

			PoolDraw &pool = pools[poolCount];
			pool.origin = light.start;
			pool.aim = aim;
			pool.range = length * tuning.poolRange;
			pool.spread = tan(min(max(tuning.poolAngle, 0.01f), 1.4f));
			pool.color.Set(tuning.color.red * tuning.poolIntensity / sceneLight.X, tuning.color.green * tuning.poolIntensity / sceneLight.Y,
				tuning.color.blue * tuning.poolIntensity / sceneLight.Z, tuning.poolFalloff);

			// A sphere around the cone's middle that reaches its far rim.
			const Real reach = pool.range * sqrt(0.25f + pool.spread * pool.spread);
			if (!Screen_Rect(clip, pool.origin + aim * (pool.range * 0.5f), reach, pool.rect))
			{
				continue;
			}
			poolCount++;
		}

		// Clamped pools draw as one quad over them all, since each pixel has to see every pool that reaches it.
		for (Int first = 0; first < poolCount; first += clampPools ? poolCount : 1)
		{
			Real rect[4] = { pools[first].rect[0], pools[first].rect[1], pools[first].rect[2], pools[first].rect[3] };
			const Int last = clampPools ? poolCount : first + 1;
			for (Int i = first + 1; i < last; i++)
			{
				rect[0] = min(rect[0], pools[i].rect[0]);
				rect[1] = min(rect[1], pools[i].rect[1]);
				rect[2] = max(rect[2], pools[i].rect[2]);
				rect[3] = max(rect[3], pools[i].rect[3]);
			}
			Write_Pool_Quad(verts, rect, projection, toWorld, depthMap);
			poolQuads++;
		}
	}
	if (beamCount == 0 && poolQuads == 0)
	{
		return;
	}

	const Int quads = beamCount + poolQuads;
	DynamicIBAccessClass ibAccess(BUFFER_TYPE_DYNAMIC_DX8, quads * 6);
	{
		DynamicIBAccessClass::WriteLockClass lock(&ibAccess);
		Write_Quad_Indices(lock.Get_Index_Array(), quads);
	}

	DX8Wrapper::Set_Vertex_Buffer(vbAccess);
	DX8Wrapper::Set_Index_Buffer(ibAccess, 0);
	DX8Wrapper::Apply_Render_State_Changes();

	// No shader restores ZENABLE, so it is put back after.
	DWORD depthTest = D3DZB_TRUE;
	device->GetRenderState(D3DRS_ZENABLE, &depthTest);

	device->SetTexture(0, depthTexture);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_POINT);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);

	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_CULLMODE, D3DCULL_NONE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, TRUE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, D3DBLEND_ONE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGENABLE, FALSE);

	// The view depth the shaders read is clip-space w, which the projection's sign turns camera z into.
	const Vector4 linearize(projection.m[3][2], projection.m[3][3], projection.m[2][3], projection.m[2][2]);

	// The pools go first, so the beams stay as bright over lit ground as over dark.
	if (poolQuads > 0)
	{
		device->SetVertexShader(Peek_D3D9_Vertex_Shader(m_poolVertexShader));
		DX8Wrapper::Set_Pixel_Shader(clampPools ? m_poolMaxPixelShader : m_poolPixelShader);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, FALSE);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);

		const Vector4 eyePosition(eye.X, eye.Y, eye.Z, 0.0f);
		const Vector4 params(projection.m[2][3], (Real)poolCount, 1.0f / (Real)(MAX_LIGHTS * 3), 0.0f);
		DX8Wrapper::Set_Pixel_Shader_Constant(0, &eyePosition, 1);
		DX8Wrapper::Set_Pixel_Shader_Constant(1, &linearize, 1);
		DX8Wrapper::Set_Pixel_Shader_Constant(2, &params, 1);

		if (clampPools)
		{
			D3DLOCKED_RECT locked;
			if (SUCCEEDED(m_poolTexture->LockRect(0, &locked, nullptr, D3DLOCK_DISCARD)))
			{
				Vector4 *texels = (Vector4 *)locked.pBits;
				for (Int i = 0; i < poolCount; i++)
				{
					const PoolDraw &pool = pools[i];
					texels[i * 3 + 0].Set(pool.origin.X, pool.origin.Y, pool.origin.Z, 1.0f / pool.range);
					texels[i * 3 + 1].Set(pool.aim.X, pool.aim.Y, pool.aim.Z, pool.spread);
					texels[i * 3 + 2] = pool.color;
				}
				m_poolTexture->UnlockRect(0);

				// Cleared through the wrapper, as stage 0 is above.
				DX8Wrapper::Set_Texture(1, nullptr);
				DX8Wrapper::Apply_Render_State_Changes();
				device->SetTexture(1, m_poolTexture);
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_POINT);
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_POINT);
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MIPFILTER, D3DTEXF_NONE);
				DX8Wrapper::Draw_Triangles((unsigned short)(beamCount * 6), 2, (unsigned short)(beamCount * 4), 4);
				device->SetTexture(1, nullptr);
			}
		}
		else
		{
			for (Int i = 0; i < poolCount; i++)
			{
				const PoolDraw &pool = pools[i];
				const Vector4 origin(pool.origin.X, pool.origin.Y, pool.origin.Z, 1.0f / pool.range);
				const Vector4 aim(pool.aim.X, pool.aim.Y, pool.aim.Z, pool.spread);
				DX8Wrapper::Set_Pixel_Shader_Constant(3, &origin, 1);
				DX8Wrapper::Set_Pixel_Shader_Constant(4, &aim, 1);
				DX8Wrapper::Set_Pixel_Shader_Constant(5, &pool.color, 1);
				DX8Wrapper::Draw_Triangles((unsigned short)((beamCount + i) * 6), 2, (unsigned short)((beamCount + i) * 4), 4);
			}
		}
	}

	if (beamCount > 0)
	{
		device->SetVertexShader(Peek_D3D9_Vertex_Shader(m_beamVertexShader));
		DX8Wrapper::Set_Pixel_Shader(m_beamPixelShader);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND, D3DBLEND_ONE);

		// The shader fades the beam at the scene's depth, and the depth test hides it where that depth is not bound.
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, (depthTexture != nullptr) ? D3DZB_FALSE : D3DZB_TRUE);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

		float clipColumns[4][4];
		for (Int column = 0; column < 4; column++)
		{
			for (Int row = 0; row < 4; row++)
			{
				clipColumns[column][row] = clip.m[row][column];
			}
		}
		const Vector4 params(projection.m[2][3], (depthTexture != nullptr) ? 1.0f : 0.0f, 0.0f, 0.0f);
		DX8Wrapper::Set_Vertex_Shader_Constant(0, clipColumns, 4);
		DX8Wrapper::Set_Pixel_Shader_Constant(5, &linearize, 1);
		DX8Wrapper::Set_Pixel_Shader_Constant(6, &params, 1);
		DX8Wrapper::Set_Pixel_Shader_Constant(7, &depthMap, 1);
		DX8Wrapper::Draw_Triangles(0, (unsigned short)(beamCount * 2), 0, (unsigned short)(beamCount * 4));
	}

	device->SetTexture(0, nullptr);
	device->SetVertexShader(nullptr);
	DX8Wrapper::Set_Pixel_Shader(0);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, depthTest);
	DX8Wrapper::Set_Vertex_Buffer(nullptr);
	DX8Wrapper::Set_Index_Buffer(nullptr, 0);

	// Blend, cull, fog and the depth states are ShaderClass state, so the next shader set restores them.
	ShaderClass::Invalidate();
#else
	m_count = 0;
#endif
}
