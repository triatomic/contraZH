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

#pragma once

#include "WW3D2/shader.h"
#include "WWLib/sharebuf.h"
#include "WWMath/vector4.h"
#include "WWMath/vector3.h"

class TextureClass;
class WorldHeightMap;
struct VertexFormatXYZNDUV2;

class W3DTerrainParticle
{
public:
	W3DTerrainParticle();
	~W3DTerrainParticle();

	void Set_Texture(TextureClass* texture);
	void Set_Shader(ShaderClass shader);
	void Set_Arrays(ShareBufferClass<Vector3>* locs,
	                ShareBufferClass<Vector4>* diffuse = nullptr,
	                ShareBufferClass<float>* sizes = nullptr,
	                ShareBufferClass<unsigned char>* orientations = nullptr,
	                int active_point_count = -1);
	void Render();

private:
	void UpdateShaderSettings();
	void ProcessParticle(WorldHeightMap* map, const Vector3& loc, UnsignedInt diffuse, Real size, UnsignedByte orientation);
	void FlushTerrainParticleBatch();
	void AddTerrainTriangle(UnsignedShort indexA, UnsignedShort indexB, UnsignedShort indexC, UnsignedByte outcodeA, UnsignedByte outcodeB, UnsignedByte outcodeC);
	void CreateTerrainMesh(WorldHeightMap* map, const Vector3& loc, UnsignedInt diffuse, Real size, Real cosine, Real sine, Real projectedRadius);

	VertexFormatXYZNDUV2* VertexData;
	unsigned short* IndexData;
	unsigned short NumberOfVertices;
	unsigned short NumberOfIndices;
	unsigned char* TileOutcodes;

	TextureClass* Texture;
	ShaderClass Shader;

	ShareBufferClass<Vector3>* PointLoc;    // World/cameraspace point locs
	ShareBufferClass<Vector4>* PointDiffuse;    // (null if not used) RGBA values
	ShareBufferClass<Real>* PointSize;    // (null if not used) size override table
	ShareBufferClass<UnsignedByte>* PointOrientation;    // (null if not used) orientation indices
	int PointCount;    // Total point count

	float DefaultPointSize;    // point size (size array overrides if present)
	Vector3 DefaultPointColor;    // point color (color array overrides if present)
	float DefaultPointAlpha;    // point alpha (alpha array overrides if present)
	UnsignedByte DefaultPointOrientation;    // point orientation (orientation array overrides if present)
	UnsignedInt DefaultDiffuse;
};
