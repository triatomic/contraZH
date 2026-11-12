/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

#include "../StdAfx.h"
#include "W3dMaterialParser.h"

#include "WWLib/chunkio.h"
#include "WWLib/ffactory.h"
#include "WWLib/RAWFILE.h"
#include "WW3D2/w3d_file.h"

namespace W3dMaterialViewer
{

namespace
{

std::string Read_Chunk_String(ChunkLoadClass &cload)
{
	uint32 length = cload.Cur_Chunk_Length();
	if (length == 0) {
		return std::string();
	}
	std::string buffer(length, '\0');
	cload.Read(&buffer[0], length);
	// Chunk strings are null-terminated; trim at the first null.
	size_t null_pos = buffer.find('\0');
	if (null_pos != std::string::npos) {
		buffer.resize(null_pos);
	}
	return buffer;
}

void Parse_Vertex_Material(ChunkLoadClass &cload, VertexMaterialData &material)
{
	while (cload.Open_Chunk()) {
		switch (cload.Cur_Chunk_ID()) {

			case W3D_CHUNK_VERTEX_MATERIAL_NAME:
				material.name = Read_Chunk_String(cload);
				break;

			case W3D_CHUNK_VERTEX_MATERIAL_INFO:
			{
				W3dVertexMaterialStruct info;
				if (cload.Read(&info, sizeof(info)) == sizeof(info)) {
					material.attributes = info.Attributes;
					material.ambient[0] = info.Ambient.R;   material.ambient[1] = info.Ambient.G;   material.ambient[2] = info.Ambient.B;
					material.diffuse[0] = info.Diffuse.R;   material.diffuse[1] = info.Diffuse.G;   material.diffuse[2] = info.Diffuse.B;
					material.specular[0] = info.Specular.R; material.specular[1] = info.Specular.G; material.specular[2] = info.Specular.B;
					material.emissive[0] = info.Emissive.R; material.emissive[1] = info.Emissive.G; material.emissive[2] = info.Emissive.B;
					material.shininess = info.Shininess;
					material.opacity = info.Opacity;
					material.translucency = info.Translucency;
				}
				break;
			}

			case W3D_CHUNK_VERTEX_MAPPER_ARGS0:
				material.mapperArgs0 = Read_Chunk_String(cload);
				break;

			case W3D_CHUNK_VERTEX_MAPPER_ARGS1:
				material.mapperArgs1 = Read_Chunk_String(cload);
				break;
		}
		cload.Close_Chunk();
	}
}

void Parse_Texture(ChunkLoadClass &cload, TextureData &texture)
{
	while (cload.Open_Chunk()) {
		switch (cload.Cur_Chunk_ID()) {

			case W3D_CHUNK_TEXTURE_NAME:
				texture.name = Read_Chunk_String(cload);
				break;

			case W3D_CHUNK_TEXTURE_INFO:
			{
				W3dTextureInfoStruct info;
				if (cload.Read(&info, sizeof(info)) == sizeof(info)) {
					texture.hasInfo = true;
					texture.attributes = info.Attributes;
					texture.animType = info.AnimType;
					texture.frameCount = info.FrameCount;
					texture.frameRate = info.FrameRate;
				}
				break;
			}
		}
		cload.Close_Chunk();
	}
}

void Parse_Material_Pass(ChunkLoadClass &cload, PassData &pass)
{
	int stage = 0;

	while (cload.Open_Chunk()) {
		switch (cload.Cur_Chunk_ID()) {

			case W3D_CHUNK_VERTEX_MATERIAL_IDS:
			{
				// Single or per-vertex array; the first entry is the pass material.
				uint32 id = 0;
				if (cload.Read(&id, sizeof(id)) == sizeof(id)) {
					pass.vertexMaterialIndex = (int)id;
				}
				break;
			}

			case W3D_CHUNK_SHADER_IDS:
			{
				uint32 id = 0;
				if (cload.Read(&id, sizeof(id)) == sizeof(id)) {
					pass.shaderIndex = (int)id;
				}
				break;
			}

			case W3D_CHUNK_TEXTURE_STAGE:
			{
				while (cload.Open_Chunk()) {
					if (cload.Cur_Chunk_ID() == W3D_CHUNK_TEXTURE_IDS && stage < 2) {
						uint32 id = 0;
						if (cload.Read(&id, sizeof(id)) == sizeof(id)) {
							pass.stageTextureIndex[stage] = (int)id;
						}
					}
					cload.Close_Chunk();
				}
				stage++;
				break;
			}
		}
		cload.Close_Chunk();
	}
}

// Handles one material chunk inside a mesh (or inside a prelit wrapper).
// Returns true if the chunk was a material chunk.
bool Parse_Material_Chunk(ChunkLoadClass &cload, MeshMaterialData &mesh)
{
	switch (cload.Cur_Chunk_ID()) {

		case W3D_CHUNK_MATERIAL_INFO:
		{
			W3dMaterialInfoStruct info;
			if (cload.Read(&info, sizeof(info)) == sizeof(info)) {
				mesh.passCount = (int)info.PassCount;
			}
			return true;
		}

		case W3D_CHUNK_SHADERS:
		{
			uint32 count = cload.Cur_Chunk_Length() / sizeof(W3dShaderStruct);
			for (uint32 i = 0; i < count; i++) {
				W3dShaderStruct w3d_shader;
				if (cload.Read(&w3d_shader, sizeof(w3d_shader)) != sizeof(w3d_shader)) {
					break;
				}
				ShaderData shader;
				shader.depthCompare = w3d_shader.DepthCompare;
				shader.depthMask = w3d_shader.DepthMask;
				shader.destBlend = w3d_shader.DestBlend;
				shader.priGradient = w3d_shader.PriGradient;
				shader.secGradient = w3d_shader.SecGradient;
				shader.srcBlend = w3d_shader.SrcBlend;
				shader.texturing = w3d_shader.Texturing;
				shader.detailColorFunc = w3d_shader.DetailColorFunc;
				shader.detailAlphaFunc = w3d_shader.DetailAlphaFunc;
				shader.alphaTest = w3d_shader.AlphaTest;
				shader.postDetailColorFunc = w3d_shader.PostDetailColorFunc;
				shader.postDetailAlphaFunc = w3d_shader.PostDetailAlphaFunc;
				mesh.shaders.push_back(shader);
			}
			return true;
		}

		case W3D_CHUNK_VERTEX_MATERIALS:
		{
			while (cload.Open_Chunk()) {
				if (cload.Cur_Chunk_ID() == W3D_CHUNK_VERTEX_MATERIAL) {
					VertexMaterialData material;
					Parse_Vertex_Material(cload, material);
					mesh.vertexMaterials.push_back(material);
				}
				cload.Close_Chunk();
			}
			return true;
		}

		case W3D_CHUNK_TEXTURES:
		{
			while (cload.Open_Chunk()) {
				if (cload.Cur_Chunk_ID() == W3D_CHUNK_TEXTURE) {
					TextureData texture;
					Parse_Texture(cload, texture);
					mesh.textures.push_back(texture);
				}
				cload.Close_Chunk();
			}
			return true;
		}

		case W3D_CHUNK_MATERIAL_PASS:
		{
			PassData pass;
			Parse_Material_Pass(cload, pass);
			mesh.passes.push_back(pass);
			return true;
		}
	}

	return false;
}

void Parse_Mesh(ChunkLoadClass &cload, MaterialDocument &document)
{
	MeshMaterialData mesh;
	bool has_materials = false;

	while (cload.Open_Chunk()) {
		switch (cload.Cur_Chunk_ID()) {

			case W3D_CHUNK_MESH_HEADER3:
			{
				W3dMeshHeader3Struct header;
				if (cload.Read(&header, sizeof(header)) == sizeof(header)) {
					header.MeshName[W3D_NAME_LEN - 1] = '\0';
					header.ContainerName[W3D_NAME_LEN - 1] = '\0';
					mesh.meshName = header.ContainerName;
					if (!mesh.meshName.empty()) {
						mesh.meshName += ".";
					}
					mesh.meshName += header.MeshName;
					mesh.sortLevel = header.SortLevel;
				}
				break;
			}

			case W3D_CHUNK_TRIANGLES:
			{
				// Surface type is per-triangle; the Max exporter writes one value
				// for the whole mesh, so the first triangle is representative.
				W3dTriStruct tri;
				if (cload.Read(&tri, sizeof(tri)) == sizeof(tri)) {
					mesh.surfaceType = tri.Attributes;
				}
				break;
			}

			case W3D_CHUNK_PRELIT_UNLIT:
			case W3D_CHUNK_PRELIT_VERTEX:
			case W3D_CHUNK_PRELIT_LIGHTMAP_MULTI_PASS:
			case W3D_CHUNK_PRELIT_LIGHTMAP_MULTI_TEXTURE:
			{
				// Prelit meshes wrap their material chunks. Take the first
				// wrapper's materials; skip the others.
				if (!has_materials) {
					mesh.prelit = true;
					has_materials = true;
					while (cload.Open_Chunk()) {
						Parse_Material_Chunk(cload, mesh);
						cload.Close_Chunk();
					}
				}
				break;
			}

			default:
				if (Parse_Material_Chunk(cload, mesh)) {
					has_materials = true;
				}
				break;
		}
		cload.Close_Chunk();
	}

	document.meshes.push_back(mesh);
}

void Parse_HLod(ChunkLoadClass &cload, MaterialDocument &document)
{
	while (cload.Open_Chunk()) {
		if (cload.Cur_Chunk_ID() == W3D_CHUNK_HLOD_HEADER) {
			W3dHLodHeaderStruct header;
			if (cload.Read(&header, sizeof(header)) == sizeof(header)) {
				header.Name[W3D_NAME_LEN - 1] = '\0';
				document.topLevelName = header.Name;
			}
		}
		cload.Close_Chunk();
	}
}

bool Parse_File(FileClass &file, MaterialDocument &document)
{
	if (!file.Open()) {
		return false;
	}

	ChunkLoadClass cload(&file);

	while (cload.Open_Chunk()) {
		switch (cload.Cur_Chunk_ID()) {

			case W3D_CHUNK_MESH:
				Parse_Mesh(cload, document);
				break;

			case W3D_CHUNK_HLOD:
				Parse_HLod(cload, document);
				break;
		}
		cload.Close_Chunk();
	}

	file.Close();

	if (document.topLevelName.empty() && !document.meshes.empty()) {
		document.topLevelName = document.meshes.front().meshName;
	}

	return !document.meshes.empty();
}

} // namespace

bool ParseMaterialDocument(const char *path, MaterialDocument &document)
{
	document = MaterialDocument();
	document.filePath = path;
	document.resolvedDiskPath = path;

	RawFileClass file(path);
	return Parse_File(file, document);
}

bool ParseMaterialDocumentFromFactory(const char *filename, MaterialDocument &document)
{
	document = MaterialDocument();
	document.filePath = filename;

	FileClass *file = _TheFileFactory->Get_File(filename);
	if (file == nullptr) {
		return false;
	}

	// Record where the file actually lives so edit mode can save it back.
	const char *resolved = file->File_Name();
	if (resolved != nullptr && resolved[0] != '\0') {
		document.resolvedDiskPath = resolved;
	}

	bool result = Parse_File(*file, document);
	_TheFileFactory->Return_File(file);
	return result;
}

} // namespace W3dMaterialViewer
