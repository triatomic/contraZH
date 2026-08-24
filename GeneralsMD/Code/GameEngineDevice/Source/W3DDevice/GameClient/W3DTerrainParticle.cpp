/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

#include "W3DDevice/GameClient/W3DTerrainParticle.h"

#include <algorithm>
#include <cstring>

#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8indexbuffer.h"
#include "WW3D2/dx8vertexbuffer.h"
#include "WWLib/refcount.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/texture.h"
#include "WW3D2/vertmaterial.h"
#include "GameClient/ParticleSys.h"
#include "Lib/BaseType.h"
#include "Lib/BaseTypeCore.h"
#include "W3DDevice/GameClient/BaseHeightMap.h"
#include "WWMath/wwmath.h"
#include "WWMath/vector3.h"
#include "WWMath/vector4.h"

enum CPP_11( : Int)
{
	MAX_BATCH_VERTICES = 65535,
	MAX_BATCH_INDICES = 65535,
	MAX_TILE_CELL_DIMENSION = 80
};

enum UVClipOutcode CPP_11( : Int)
{
	UV_CLIP_U_MIN = 1 << 0,
	UV_CLIP_U_MAX = 1 << 1,
	UV_CLIP_V_MIN = 1 << 2,
	UV_CLIP_V_MAX = 1 << 3
};

static Real getMapHeight(WorldHeightMap* map, Int x, Int y)
{
	x += map->getBorderSizeInline();
	y += map->getBorderSizeInline();
	return map->getDataPtr()[x + y * map->getXExtent()] * MAP_HEIGHT_SCALE;
}

static IRegion2D calcBounds(WorldHeightMap* map, const Vector3& loc, const Real projectedRadius)
{
	IRegion2D bounds;
	bounds.lo.x = REAL_TO_INT_FLOOR((loc.X - projectedRadius) / MAP_XY_FACTOR);
	bounds.lo.y = REAL_TO_INT_FLOOR((loc.Y - projectedRadius) / MAP_XY_FACTOR);
	bounds.lo.x = max(-map->getBorderSizeInline(), bounds.lo.x);
	bounds.lo.y = max(-map->getBorderSizeInline(), bounds.lo.y);
	bounds.hi.x = REAL_TO_INT_CEIL((loc.X + projectedRadius) / MAP_XY_FACTOR) + 1;
	bounds.hi.y = REAL_TO_INT_CEIL((loc.Y + projectedRadius) / MAP_XY_FACTOR) + 1;
	bounds.hi.x = min(map->getXExtent() - map->getBorderSizeInline(), bounds.hi.x);
	bounds.hi.y = min(map->getYExtent() - map->getBorderSizeInline(), bounds.hi.y);
	return bounds;
}

static UnsignedByte getUVClipOutcode(const VertexFormatXYZNDUV2& vertex)
{
	UnsignedByte outcode = 0;
	if (vertex.u1 < 0.0f)
		outcode |= UV_CLIP_U_MIN;
	else if (vertex.u1 > 1.0f)
		outcode |= UV_CLIP_U_MAX;
	if (vertex.v1 < 0.0f)
		outcode |= UV_CLIP_V_MIN;
	else if (vertex.v1 > 1.0f)
		outcode |= UV_CLIP_V_MAX;
	return outcode;
}

static Real getUVClipDistance(const VertexFormatXYZNDUV2& vertex, UnsignedByte boundary)
{
	switch (boundary)
	{
		case UV_CLIP_U_MIN:
			return vertex.u1;
		case UV_CLIP_U_MAX:
			return 1.0f - vertex.u1;
		case UV_CLIP_V_MIN:
			return vertex.v1;
		case UV_CLIP_V_MAX:
			return 1.0f - vertex.v1;
	}
	return -1.0f;
}

static VertexFormatXYZNDUV2 interpolateUVClipVertex(const VertexFormatXYZNDUV2& a,
                                                    const VertexFormatXYZNDUV2& b,
                                                    Real t,
                                                    UnsignedByte boundary)
{
	VertexFormatXYZNDUV2 result = a;
	result.x = a.x + (b.x - a.x) * t;
	result.y = a.y + (b.y - a.y) * t;
	result.z = a.z + (b.z - a.z) * t;
	result.diffuse = a.diffuse;
	result.u1 = a.u1 + (b.u1 - a.u1) * t;
	result.v1 = a.v1 + (b.v1 - a.v1) * t;

	switch (boundary)
	{
		case UV_CLIP_U_MIN:
			result.u1 = 0.0f;
			break;
		case UV_CLIP_U_MAX:
			result.u1 = 1.0f;
			break;
		case UV_CLIP_V_MIN:
			result.v1 = 0.0f;
			break;
		case UV_CLIP_V_MAX:
			result.v1 = 1.0f;
			break;
	}
	return result;
}

static UnsignedShort clipUVPolygon(const VertexFormatXYZNDUV2* input,
                                   UnsignedShort inputCount,
                                   VertexFormatXYZNDUV2* output,
                                   UnsignedByte boundary)
{
	if (inputCount <= 0)
		return 0;

	Int outputCount = 0;
	VertexFormatXYZNDUV2 previous = input[inputCount - 1];
	Real previousDistance = getUVClipDistance(previous, boundary);
	Bool previousInside = previousDistance >= 0.0f;

	for (Int i = 0; i < inputCount; i++)
	{
		const VertexFormatXYZNDUV2 current = input[i];
		const Real currentDistance = getUVClipDistance(current, boundary);
		const Bool currentInside = currentDistance >= 0.0f;
		if (currentInside != previousInside)
		{
			const Real t = previousDistance / (previousDistance - currentDistance);
			output[outputCount++] = interpolateUVClipVertex(previous, current, t, boundary);
		}
		if (currentInside)
			output[outputCount++] = current;
		previous = current;
		previousDistance = currentDistance;
		previousInside = currentInside;
	}
	return outputCount;
}

W3DTerrainParticle::W3DTerrainParticle()
  : VertexData(W3DNEWARRAY VertexFormatXYZNDUV2[MAX_BATCH_VERTICES])
  , IndexData(W3DNEWARRAY UnsignedShort[MAX_BATCH_INDICES])
  , NumberOfVertices(0)
  , NumberOfIndices(0)
  , TileOutcodes(W3DNEWARRAY UnsignedByte[(MAX_TILE_CELL_DIMENSION + 1) * (MAX_TILE_CELL_DIMENSION + 1)])
  , Texture(nullptr)
  , PointLoc(nullptr)
  , PointDiffuse(nullptr)
  , PointSize(nullptr)
  , PointOrientation(nullptr)
  , PointCount(0)
  , DefaultPointSize(0.0f)
  , DefaultPointColor(1.0f, 1.0f, 1.0f)
  , DefaultPointAlpha(1.0f)
  , DefaultPointOrientation(0)
{
	DefaultDiffuse = DX8Wrapper::Convert_Color_Clamp(Vector4(DefaultPointColor.X, DefaultPointColor.Y, DefaultPointColor.Z, DefaultPointAlpha));
}

W3DTerrainParticle::~W3DTerrainParticle()
{
	delete[] VertexData;
	delete[] IndexData;
	delete[] TileOutcodes;
	REF_PTR_RELEASE(Texture);
	REF_PTR_RELEASE(PointLoc);
	REF_PTR_RELEASE(PointDiffuse);
	REF_PTR_RELEASE(PointSize);
	REF_PTR_RELEASE(PointOrientation);
}

void W3DTerrainParticle::Set_Texture(TextureClass* texture)
{
	REF_PTR_SET(Texture, texture);
}

void W3DTerrainParticle::Set_Shader(ShaderClass shader)
{
	Shader = shader;
}

void W3DTerrainParticle::Set_Arrays(
  ShareBufferClass<Vector3>* locs,
  ShareBufferClass<Vector4>* diffuse,
  ShareBufferClass<Real>* sizes,
  ShareBufferClass<UnsignedByte>* orientations,
  int active_point_count)
{
	WWASSERT(locs);
	WWASSERT(active_point_count <= locs->Get_Count());

	// Ensure lengths of all arrays are the same
	WWASSERT(!diffuse || locs->Get_Count() == diffuse->Get_Count());
	WWASSERT(!sizes || locs->Get_Count() == sizes->Get_Count());
	WWASSERT(!orientations || locs->Get_Count() == orientations->Get_Count());

	REF_PTR_SET(PointLoc, locs);
	REF_PTR_SET(PointDiffuse, diffuse);
	REF_PTR_SET(PointSize, sizes);
	REF_PTR_SET(PointOrientation, orientations);

	PointCount = active_point_count >= 0 ? active_point_count : locs->Get_Count();
}

void W3DTerrainParticle::Render()
{
	if (PointCount <= 0 || !PointLoc || !TheTerrainRenderObject)
		return;

	WorldHeightMap* map = TheTerrainRenderObject->getMap();
	if (!map)
		return;

	UpdateShaderSettings();

	DX8Wrapper::Set_World_Identity();
	VertexMaterialClass* material = VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(material);
	REF_PTR_RELEASE(material);
	DX8Wrapper::Set_Shader(Shader);
	DX8Wrapper::Set_Texture(0, Texture);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

	for (Int p = 0; p < PointCount; p++)
	{
		Vector3 loc = PointLoc->Get_Array()[p];
		UnsignedInt diffuse = PointDiffuse ? DX8Wrapper::Convert_Color_Clamp(PointDiffuse->Get_Array()[p]) : DefaultDiffuse;
		Real size = PointSize ? PointSize->Get_Array()[p] : DefaultPointSize;
		UnsignedByte orientation = PointOrientation ? PointOrientation->Get_Array()[p] : DefaultPointOrientation;

		ProcessParticle(map, loc, diffuse, size, orientation);
	}

	FlushTerrainParticleBatch();

	if (Texture)
		Texture->Get_Filter().Apply(0);
}

void W3DTerrainParticle::ProcessParticle(WorldHeightMap* map, const Vector3& loc, UnsignedInt diffuse, Real size, UnsignedByte orientation)
{
	const Real angle = orientation / 255.0f * 2.0f * WWMATH_PI;
	const Real cosine = WWMath::Cos(angle);
	const Real sine = WWMath::Sin(angle);
	const Real projectedRadius = size * (WWMath::Fabs(cosine) + WWMath::Fabs(sine));
	CreateTerrainMesh(map, loc, diffuse, size, cosine, sine, projectedRadius);
	// @todo Process particles that on a bridge here
}

void W3DTerrainParticle::CreateTerrainMesh(WorldHeightMap* map, const Vector3& loc, UnsignedInt diffuse, Real size, const Real cosine, const Real sine, const Real projectedRadius)
{
	IRegion2D bounds = calcBounds(map, loc, projectedRadius);
	if (bounds.width() < 2 || bounds.height() < 2)
		return;

	const Real particleHeightOffset = MAP_XY_FACTOR / 10;

	for (Int tileMinY = bounds.lo.y; tileMinY < bounds.hi.y - 1; tileMinY += MAX_TILE_CELL_DIMENSION)
	{
		const Int tileMaxY = std::min(tileMinY + MAX_TILE_CELL_DIMENSION + 1, bounds.hi.y);
		for (Int tileMinX = bounds.lo.x; tileMinX < bounds.hi.x - 1; tileMinX += MAX_TILE_CELL_DIMENSION)
		{
			const Int tileMaxX = std::min(tileMinX + MAX_TILE_CELL_DIMENSION + 1, bounds.hi.x);
			const Int tileWidth = tileMaxX - tileMinX;
			const Int tileHeight = tileMaxY - tileMinY;
			const Int tileVertexCount = tileWidth * tileHeight;
			const Int tileCellWidth = tileWidth - 1;
			const Int tileCellHeight = tileHeight - 1;
			// A straight footprint edge crosses at most width + height grid cells.
			// Account conservatively for two triangles per cell and all four edges.
			const Int maxBoundaryTriangleCount = 8 * (tileCellWidth + tileCellHeight + 1);
			const Int maxTileVertexCount = tileVertexCount + 7 * maxBoundaryTriangleCount;
			const Int maxTileIndexCount = 6 * tileCellWidth * tileCellHeight + 15 * maxBoundaryTriangleCount;

			// batch is full: draw to screen
			if (NumberOfVertices + maxTileVertexCount > MAX_BATCH_VERTICES ||
			    NumberOfIndices + maxTileIndexCount > MAX_BATCH_INDICES)
			{
				FlushTerrainParticleBatch();
			}

			Int i, j;
			const UnsignedShort baseVertex = NumberOfVertices;
			for (j = tileMinY; j < tileMaxY; j++)
			{
				for (i = tileMinX; i < tileMaxX; i++)
				{
					VertexFormatXYZNDUV2 vertex;
					vertex.diffuse = diffuse;
					vertex.x = i * MAP_XY_FACTOR;
					vertex.y = j * MAP_XY_FACTOR;
					vertex.z = getMapHeight(map, i, j);
					vertex.z += particleHeightOffset;
					vertex.nx = 0.0f;
					vertex.ny = 0.0f;
					vertex.nz = 1.0f;
					const Real deltaX = vertex.x - loc.X;
					const Real deltaY = vertex.y - loc.Y;
					const Real localX = cosine * deltaX - sine * deltaY;
					const Real localY = sine * deltaX + cosine * deltaY;
					vertex.u1 = 0.5f - localX / (2.0f * size);
					vertex.v1 = 0.5f - localY / (2.0f * size);
					vertex.u2 = 0.0f;
					vertex.v2 = 0.0f;
					TileOutcodes[(j - tileMinY) * tileWidth + i - tileMinX] = getUVClipOutcode(vertex);
					VertexData[NumberOfVertices++] = vertex;
				}
			}

			for (j = 0; j < tileHeight - 1; j++)
			{
				for (i = 0; i < tileWidth - 1; i++)
				{
					const UnsignedShort topLeftOffset = j * tileWidth + i;
					const UnsignedShort topRightOffset = topLeftOffset + 1;
					const UnsignedShort bottomLeftOffset = topLeftOffset + tileWidth;
					const UnsignedShort bottomRightOffset = bottomLeftOffset + 1;
					const UnsignedShort topLeft = baseVertex + topLeftOffset;
					const UnsignedShort topRight = baseVertex + topRightOffset;
					const UnsignedShort bottomLeft = baseVertex + bottomLeftOffset;
					const UnsignedShort bottomRight = baseVertex + bottomRightOffset;
					const Int mapCellX = tileMinX + i + map->getBorderSizeInline();
					const Int mapCellY = tileMinY + j + map->getBorderSizeInline();
					if (map->getFlipState(mapCellX, mapCellY))
					{
						AddTerrainTriangle(topRight, bottomLeft, topLeft,
						                   TileOutcodes[topRightOffset],
						                   TileOutcodes[bottomLeftOffset],
						                   TileOutcodes[topLeftOffset]);
						AddTerrainTriangle(topRight, bottomRight, bottomLeft,
						                   TileOutcodes[topRightOffset],
						                   TileOutcodes[bottomRightOffset],
						                   TileOutcodes[bottomLeftOffset]);
					}
					else
					{
						AddTerrainTriangle(topLeft, bottomRight, bottomLeft,
						                   TileOutcodes[topLeftOffset],
						                   TileOutcodes[bottomRightOffset],
						                   TileOutcodes[bottomLeftOffset]);
						AddTerrainTriangle(topLeft, topRight, bottomRight,
						                   TileOutcodes[topLeftOffset],
						                   TileOutcodes[topRightOffset],
						                   TileOutcodes[bottomRightOffset]);
					}
				}
			}
		}
	}
}

void W3DTerrainParticle::FlushTerrainParticleBatch()
{
	if (NumberOfIndices > 0 && NumberOfVertices > 0)
	{
		DynamicVBAccessClass vertexAccess(BUFFER_TYPE_DYNAMIC_DX8, dynamic_fvf_type, NumberOfVertices);
		{
			DynamicVBAccessClass::WriteLockClass vertexLock(&vertexAccess);
			memcpy(vertexLock.Get_Formatted_Vertex_Array(),
			       VertexData,
			       NumberOfVertices * sizeof(VertexFormatXYZNDUV2));
		}

		DynamicIBAccessClass indexAccess(BUFFER_TYPE_DYNAMIC_DX8, NumberOfIndices);
		{
			DynamicIBAccessClass::WriteLockClass indexLock(&indexAccess);
			memcpy(indexLock.Get_Index_Array(),
			       IndexData,
			       NumberOfIndices * sizeof(UnsignedShort));
		}

		DX8Wrapper::Set_Index_Buffer(indexAccess, 0);
		DX8Wrapper::Set_Vertex_Buffer(vertexAccess);
		DX8Wrapper::Draw_Triangles(0,
		                           NumberOfIndices / 3,
		                           0,
		                           NumberOfVertices);
	}

	NumberOfVertices = 0;
	NumberOfIndices = 0;
}

void W3DTerrainParticle::AddTerrainTriangle(UnsignedShort indexA,
                                            UnsignedShort indexB,
                                            UnsignedShort indexC,
                                            UnsignedByte outcodeA,
                                            UnsignedByte outcodeB,
                                            UnsignedByte outcodeC)
{
	const UnsignedByte combinedOutcode = outcodeA | outcodeB | outcodeC;
	if (combinedOutcode == 0)
	{
		IndexData[NumberOfIndices++] = indexA;
		IndexData[NumberOfIndices++] = indexB;
		IndexData[NumberOfIndices++] = indexC;
		return;
	}

	// A shared outside plane means the complete triangle misses the footprint.
	if ((outcodeA & outcodeB & outcodeC) != 0)
		return;

	VertexFormatXYZNDUV2 polygonA[8];
	VertexFormatXYZNDUV2 polygonB[8];
	polygonA[0] = VertexData[indexA];
	polygonA[1] = VertexData[indexB];
	polygonA[2] = VertexData[indexC];
	UnsignedShort polygonCount = 3;
	VertexFormatXYZNDUV2* input = polygonA;
	VertexFormatXYZNDUV2* output = polygonB;

	// Only visit boundaries crossed by this triangle. Most triangles never get here.
	for (UnsignedByte boundary = UV_CLIP_U_MIN; boundary <= UV_CLIP_V_MAX; boundary <<= 1)
	{
		if ((combinedOutcode & boundary) == 0)
			continue;
		polygonCount = clipUVPolygon(input, polygonCount, output, boundary);
		if (polygonCount < 3)
			return;
		std::swap(input, output);
	}

	const UnsignedShort baseVertex = NumberOfVertices;
	UnsignedShort i;
	for (i = 0; i < polygonCount; i++)
	{
		VertexData[NumberOfVertices++] = input[i];
	}
	for (i = 1; i < polygonCount - 1; i++)
	{
		IndexData[NumberOfIndices++] = baseVertex;
		IndexData[NumberOfIndices++] = baseVertex + i;
		IndexData[NumberOfIndices++] = baseVertex + i + 1;
	}
}

void W3DTerrainParticle::UpdateShaderSettings()
{
	// If there is a color or alpha array enable gradient in shader - otherwise disable.
	float value_255 = 0.9961f;    // 254 / 255
	bool default_white_opaque = DefaultPointColor.X > value_255 &&
	                            DefaultPointColor.Y > value_255 &&
	                            DefaultPointColor.Z > value_255 &&
	                            DefaultPointAlpha > value_255;

	// The reason we check for lack of texture here is that SR seems to render black triangles
	// rather than white triangles as would be expected) when there is no texture AND no gradient.
	if (PointDiffuse || !default_white_opaque || !Texture)
	{
		Shader.Set_Primary_Gradient(ShaderClass::GRADIENT_MODULATE);
	}
	else
	{
		Shader.Set_Primary_Gradient(ShaderClass::GRADIENT_DISABLE);
	}

	// If Texture is non-null enable texturing in shader - otherwise disable.
	if (Texture)
	{
		Shader.Set_Texturing(ShaderClass::TEXTURING_ENABLE);
	}
	else
	{
		Shader.Set_Texturing(ShaderClass::TEXTURING_DISABLE);
	}

	Shader.Set_Cull_Mode(ShaderClass::CULL_MODE_ENABLE);
}
