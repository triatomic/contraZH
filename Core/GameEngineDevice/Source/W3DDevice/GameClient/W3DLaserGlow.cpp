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

// W3DLaserGlow.cpp ///////////////////////////////////////////////////////////////////////////////
// Lights the terrain along laser beams per pixel, on a mesh that follows the heightmap
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Lib/BaseType.h"
#include "WWLib/always.h"
#include "W3DDevice/GameClient/W3DLaserGlow.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DSoftParticles.h"
#include "W3DDevice/GameClient/BaseHeightMap.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
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
#include "WWMath/vector4.h"

W3DLaserGlow *TheW3DLaserGlow = nullptr;

// CONTRA_LASERGLOW bisects faults: 0 lights the ground with dynamic lights, 1 with the shader.
static Int Get_Laser_Glow_Mode()
{
	const char *value = getenv("CONTRA_LASERGLOW");
	return (value != nullptr) ? atoi(value) : 1;
}
static const Int LaserGlowMode = Get_Laser_Glow_Mode();

// The mesh sits this far above the terrain, as terrain-conforming particles do.
static const Real GLOW_LIFT = MAP_XY_FACTOR / 10.0f;

// A footprint past this many heightmap samples is left dark.
static const Int MAX_GLOW_SAMPLES = 16384;

// The dynamic index buffer counts in 16 bits, so a footprint lighting more cells than this is left dark too.
static const Int MAX_GLOW_CELLS = 0xffff / 6;

// Scene light below this is treated as this, so the glow on black ground stays in bounds.
static const Real MIN_SCENE_LIGHT = 0.15f;

// The group shaders, smallest first, and how many beams each lights in one pass.
static const char *const GroupShaderFiles[] = { "shaders\\laserglow2.pso", "shaders\\laserglow4.pso", "shaders\\laserglow7.pso" };
static const Int GroupShaderBeams[] = { 2, 4, 7 };

W3DLaserGlow::W3DLaserGlow()
	: m_count(0),
	  m_shader(0),
	  m_loaded(FALSE)
{
	for (Int i = 0; i < GROUP_SHADERS; i++)
	{
		m_groupShaders[i] = 0;
	}
}

W3DLaserGlow::~W3DLaserGlow()
{
	ReleaseResources();
}

void W3DLaserGlow::ReleaseResources()
{
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device != nullptr)
	{
		DX8_DELETE_PIXEL_SHADER(device, m_shader);
		for (Int i = 0; i < GROUP_SHADERS; i++)
		{
			DX8_DELETE_PIXEL_SHADER(device, m_groupShaders[i]);
		}
	}
	m_shader = 0;
	for (Int i = 0; i < GROUP_SHADERS; i++)
	{
		m_groupShaders[i] = 0;
	}
	m_loaded = FALSE;
}

Bool W3DLaserGlow::isEnabled()
{
#if defined(BUILD_WITH_D3D9)
	if (LaserGlowMode == 0)
	{
		return FALSE;
	}
	if (!m_loaded)
	{
		m_loaded = TRUE;
		const DX8Caps *caps = DX8Wrapper::Get_Current_Caps();
		if (caps != nullptr && caps->Get_Pixel_Shader_Major_Version() >= 2 &&
			FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\laserglow.pso", nullptr, 0, false, &m_shader)))
		{
			m_shader = 0;
		}
		// Without ps_2_a, overlapping glows draw one at a time.
		if (m_shader != 0 && W3DShaderManager::supportsPixelShader2a())
		{
			for (Int i = 0; i < GROUP_SHADERS; i++)
			{
				if (FAILED(W3DShaderManager::LoadAndCreateD3DShader(GroupShaderFiles[i], nullptr, 0, false, &m_groupShaders[i])))
				{
					m_groupShaders[i] = 0;
				}
			}
		}
	}
	return m_shader != 0;
#else
	return FALSE;
#endif
}

void W3DLaserGlow::appendObjectLights(std::vector<W3DShaderManager::PixelLight> &lights) const
{
	for (Int i = 0; i < m_count; i++)
	{
		const Glow &glow = m_glows[i];
		const Vector3 delta = glow.end - glow.start;
		const Real length = delta.Length();
		const Int count = min(max((Int)ceilf(length / max(glow.reach, 1.0f)), 1), 16);
		const Real spacing = length / count;
		for (Int j = 0; j < count; j++)
		{
			W3DShaderManager::PixelLight light;
			light.position = glow.start + delta * ((j + 0.5f) / count);
			light.innerRadius = 0.5f;
			light.outerRadius = max(max(glow.reach, spacing), 1.0f);
			const Real intensity = min(glow.reach / light.outerRadius, 1.0f);
			light.diffuse = glow.color * (intensity * (count > 1 ? min(spacing / light.outerRadius, 1.0f) : 1.0f));
			light.ambientScale = 0.0f;
			light.terrainOnly = FALSE;
			light.objectsOnly = TheGlobalData->m_laserRef;
			lights.push_back(light);
		}
	}
}

void W3DLaserGlow::add(const Vector3 &start, const Vector3 &end, Real reach, const Vector3 &color, const BeamShaderTuning *pulses)
{
	if (m_count == MAX_GLOWS || reach <= 0.0f)
	{
		return;
	}

	Glow &glow = m_glows[m_count++];
	glow.start = start;
	glow.end = end;
	glow.reach = reach;
	glow.color = color;
	glow.pulses = pulses;
}

// The height at a heightmap sample, counted from the playable origin and clamped to the map.
static Real Sample_Height(WorldHeightMap *map, Int x, Int y)
{
	x = min(max(x + map->getBorderSizeInline(), 0), map->getXExtent() - 1);
	y = min(max(y + map->getBorderSizeInline(), 0), map->getYExtent() - 1);
	return map->getDataPtr()[x + y * map->getXExtent()] * MAP_HEIGHT_SCALE;
}

// The terrain's ambient and sun light on flat ground, which the glow is measured against.
static Vector3 Scene_Light()
{
	const RGBColor &ambient = TheGlobalData->m_terrainAmbient[0];
	Vector3 light(ambient.red, ambient.green, ambient.blue);
	for (Int i = 0; i < TheGlobalData->m_numGlobalLights; i++)
	{
		const Coord3D &position = TheGlobalData->m_terrainLightPos[i];
		const Real length = sqrt(position.x * position.x + position.y * position.y + position.z * position.z);
		const Real facing = (length > 0.0f) ? max(-position.z / length, 0.0f) : 0.0f;
		const RGBColor &diffuse = TheGlobalData->m_terrainDiffuse[i];
		light += Vector3(diffuse.red, diffuse.green, diffuse.blue) * facing;
	}
	light.X = max(light.X, MIN_SCENE_LIGHT);
	light.Y = max(light.Y, MIN_SCENE_LIGHT);
	light.Z = max(light.Z, MIN_SCENE_LIGHT);
	return light;
}

Vector3 W3DLaserGlow::getSceneLight()
{
	return Scene_Light();
}

// The squared distance on the ground from a point to the beam.
static Real Ground_Distance_Squared(Real x, Real y, const Vector3 &start, const Vector3 &end)
{
	const Real spanX = end.X - start.X;
	const Real spanY = end.Y - start.Y;
	const Real lengthSquared = spanX * spanX + spanY * spanY;
	Real t = (lengthSquared > 0.0f) ? ((x - start.X) * spanX + (y - start.Y) * spanY) / lengthSquared : 0.0f;
	t = min(max(t, 0.0f), 1.0f);
	const Real dx = start.X + spanX * t - x;
	const Real dy = start.Y + spanY * t - y;
	return dx * dx + dy * dy;
}

// The glow's light over the scene's light, which the shader adds to the ground's own colour.
static Vector3 Relative_Light(const Vector3 &color, const Vector3 &sceneLight)
{
	return Vector3(color.X / sceneLight.X, color.Y / sceneLight.Y, color.Z / sceneLight.Z);
}

// The falloff and wrap every beam shares.
static Vector4 Glow_Shape()
{
	return Vector4(max(TheGlobalData->m_laserGlowFalloff, 0.1f), min(max(TheGlobalData->m_laserGlowWrap, 0.0f), 1.0f), 0.0f, 0.0f);
}

void W3DLaserGlow::render(RenderInfoClass &rinfo)
{
#if defined(BUILD_WITH_D3D9)
	const Int count = m_count;
	m_count = 0;
	if (count == 0 || !TheGlobalData->m_laserRef || !isEnabled() || TheTerrainRenderObject == nullptr || TheTerrainRenderObject->getMap() == nullptr)
	{
		return;
	}

	WorldHeightMap *map = TheTerrainRenderObject->getMap();
	const Vector3 sceneLight = Scene_Light();

	rinfo.Camera.Apply();
	Matrix4x4 identity(true);
	DX8Wrapper::Set_Transform(D3DTS_WORLD, identity);
	DX8Wrapper::Set_Texture(0, nullptr);
	DX8Wrapper::Set_Texture(1, nullptr);
	VertexMaterialClass *material = VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(material);
	REF_PTR_RELEASE(material);
	DX8Wrapper::Set_Shader(ShaderClass::_PresetOpaque2DShader);

	// Every glow reads the same noise texture.
	Vector4 pulses[MAX_GLOWS];
	IDirect3DTexture8 *noise = nullptr;
	for (Int i = 0; i < count; i++)
	{
		pulses[i] = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
		noise = (TheW3DSoftParticles != nullptr) ? TheW3DSoftParticles->getLaserPulse(m_glows[i].pulses, pulses[i]) : nullptr;
	}

	// Glows whose footprints share a cell join one group, labelled by its first glow.
	Footprint boxes[MAX_GLOWS];
	Int group[MAX_GLOWS];
	for (Int i = 0; i < count; i++)
	{
		boxes[i] = findFootprint(map, m_glows[i]);
		group[i] = i;
		if (!TheGlobalData->m_laserGlowOverlap)
		{
			continue;
		}
		for (Int j = 0; j < i; j++)
		{
			if (group[j] != group[i] &&
				max(boxes[i].loX, boxes[j].loX) < min(boxes[i].hiX, boxes[j].hiX) &&
				max(boxes[i].loY, boxes[j].loY) < min(boxes[i].hiY, boxes[j].hiY))
			{
				const Int from = max(group[i], group[j]);
				const Int to = min(group[i], group[j]);
				for (Int k = 0; k <= i; k++)
				{
					if (group[k] == from)
					{
						group[k] = to;
					}
				}
			}
		}
	}

	for (Int i = 0; i < count; i++)
	{
		if (group[i] != i)
		{
			continue;
		}
		Int members[MAX_GLOWS];
		Int memberCount = 0;
		for (Int k = i; k < count; k++)
		{
			if (group[k] == i)
			{
				members[memberCount++] = k;
			}
		}
		if (memberCount > 1 && drawGroup(map, members, memberCount, sceneLight, pulses, noise))
		{
			continue;
		}
		for (Int k = 0; k < memberCount; k++)
		{
			drawGlow(map, m_glows[members[k]], sceneLight, pulses[members[k]], noise);
		}
	}

	DX8Wrapper::Set_Pixel_Shader(0);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	DX8Wrapper::Set_Vertex_Buffer(nullptr);
	DX8Wrapper::Set_Index_Buffer(nullptr, 0);

	// Z, blend, cull and fog are ShaderClass state, so the next shader set restores them in full.
	ShaderClass::Invalidate();
#endif
}

// The heightmap samples under the beam's reach, clamped to the map.
W3DLaserGlow::Footprint W3DLaserGlow::findFootprint(WorldHeightMap *map, const Glow &glow)
{
	const Int border = map->getBorderSizeInline();
	Footprint box;
	box.loX = max((Int)floor((min(glow.start.X, glow.end.X) - glow.reach) / MAP_XY_FACTOR), -border);
	box.loY = max((Int)floor((min(glow.start.Y, glow.end.Y) - glow.reach) / MAP_XY_FACTOR), -border);
	box.hiX = min((Int)ceil((max(glow.start.X, glow.end.X) + glow.reach) / MAP_XY_FACTOR), map->getXExtent() - border - 1);
	box.hiY = min((Int)ceil((max(glow.start.Y, glow.end.Y) + glow.reach) / MAP_XY_FACTOR), map->getYExtent() - border - 1);
	return box;
}

// The four constants the shader reads per beam, with the pulses measured from the beam's start.
void W3DLaserGlow::beamConstants(const Glow &glow, const Vector3 &sceneLight, const Vector4 &pulse, Vector4 *constants)
{
	const Vector3 span = glow.end - glow.start;
	const Real lengthSquared = span.Length2();
	const Real length = sqrt(lengthSquared);
	const Real alongStart = (length > 0.0f) ? Vector3::Dot_Product(glow.start, span) / length * pulse.Y : 0.0f;
	const Vector3 light = Relative_Light(glow.color, sceneLight);

	constants[0] = Vector4(glow.start.X, glow.start.Y, glow.start.Z, alongStart);
	constants[1] = Vector4(span.X, span.Y, span.Z, (lengthSquared > 0.0f) ? 1.0f / lengthSquared : 0.0f);
	constants[2] = Vector4(light.X, light.Y, light.Z, 1.0f / glow.reach);
	constants[3] = Vector4(pulse.X, length * pulse.Y, pulse.Z, 0.0f);
}

// Whether any glow's light reaches the cell on the ground. Half a cell's diagonal covers its corners.
// Each glow keeps to its own footprint, so glows in different groups never light the same cell.
Bool W3DLaserGlow::reachesCell(const Glow *const *glows, const Footprint *footprints, Int count, Int x, Int y)
{
	const Real centerX = (x + 0.5f) * MAP_XY_FACTOR;
	const Real centerY = (y + 0.5f) * MAP_XY_FACTOR;
	for (Int i = 0; i < count; i++)
	{
		if (x < footprints[i].loX || x >= footprints[i].hiX || y < footprints[i].loY || y >= footprints[i].hiY)
		{
			continue;
		}
		const Real cellReach = glows[i]->reach + MAP_XY_FACTOR * 0.75f;
		if (Ground_Distance_Squared(centerX, centerY, glows[i]->start, glows[i]->end) < cellReach * cellReach)
		{
			return TRUE;
		}
	}
	return FALSE;
}

void W3DLaserGlow::drawGlow(WorldHeightMap *map, const Glow &glow, const Vector3 &sceneLight, const Vector4 &pulse, IDirect3DTexture8 *noise)
{
#if defined(BUILD_WITH_D3D9)
	Vector4 constants[5];
	beamConstants(glow, sceneLight, pulse, constants);
	constants[4] = Glow_Shape();

	// A footprint past the limits is left dark.
	const Glow *glows[1] = { &glow };
	drawCells(map, glows, 1, findFootprint(map, glow), m_shader, constants, 5, noise);
#endif
}

// Lights the group's cells once, with the shader combining its glows. False leaves them to draw one at a time.
Bool W3DLaserGlow::drawGroup(WorldHeightMap *map, const Int *members, Int count, const Vector3 &sceneLight, const Vector4 *pulses, IDirect3DTexture8 *noise)
{
#if defined(BUILD_WITH_D3D9)
	Int order[MAX_GLOWS];
	Real brightness[MAX_GLOWS];
	for (Int i = 0; i < count; i++)
	{
		const Glow &glow = m_glows[members[i]];
		const Vector3 light = Relative_Light(glow.color, sceneLight);
		order[i] = members[i];
		brightness[i] = (light.X + light.Y + light.Z) * glow.reach;
	}

	// Past the largest shader's beams, the brightest glows light the group and the rest go dark.
	const Int kept = min(count, (Int)MAX_GROUP_BEAMS);
	if (count > kept)
	{
		for (Int i = 0; i < kept; i++)
		{
			Int brightest = i;
			for (Int j = i + 1; j < count; j++)
			{
				if (brightness[j] > brightness[brightest])
				{
					brightest = j;
				}
			}
			const Int index = order[i];
			order[i] = order[brightest];
			order[brightest] = index;
			const Real value = brightness[i];
			brightness[i] = brightness[brightest];
			brightness[brightest] = value;
		}
	}

	Int shader = 0;
	while (shader < GROUP_SHADERS && (GroupShaderBeams[shader] < kept || m_groupShaders[shader] == 0))
	{
		shader++;
	}
	if (shader == GROUP_SHADERS)
	{
		return FALSE;
	}

	const Glow *glows[MAX_GROUP_BEAMS];
	Vector4 constants[1 + MAX_GROUP_BEAMS * 4];
	constants[0] = Glow_Shape();
	Footprint box = findFootprint(map, m_glows[order[0]]);
	for (Int i = 0; i < GroupShaderBeams[shader]; i++)
	{
		Vector4 *beam = &constants[1 + i * 4];
		if (i >= kept)
		{
			// A beam slot past the group's glows gives no light.
			for (Int j = 0; j < 4; j++)
			{
				beam[j] = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
			}
			continue;
		}
		glows[i] = &m_glows[order[i]];
		beamConstants(*glows[i], sceneLight, pulses[order[i]], beam);
		const Footprint footprint = findFootprint(map, *glows[i]);
		box.loX = min(box.loX, footprint.loX);
		box.loY = min(box.loY, footprint.loY);
		box.hiX = max(box.hiX, footprint.hiX);
		box.hiY = max(box.hiY, footprint.hiY);
	}

	return drawCells(map, glows, kept, box, m_groupShaders[shader], constants, 1 + GroupShaderBeams[shader] * 4, noise);
#else
	return FALSE;
#endif
}

// Lights the box's cells that any of the glows reach, each once. False draws nothing, when the box or its
// cells run past what one draw holds.
Bool W3DLaserGlow::drawCells(WorldHeightMap *map, const Glow *const *glows, Int count, const Footprint &box, DWORD shader,
	const Vector4 *constants, Int constantCount, IDirect3DTexture8 *noise)
{
#if defined(BUILD_WITH_D3D9)
	const Int border = map->getBorderSizeInline();
	const Int loX = box.loX;
	const Int loY = box.loY;
	const Int hiX = box.hiX;
	const Int hiY = box.hiY;
	const Int width = hiX - loX + 1;
	const Int height = hiY - loY + 1;
	if (width < 2 || height < 2)
	{
		return TRUE;
	}
	if (width * height > MAX_GLOW_SAMPLES)
	{
		return FALSE;
	}

	Footprint footprints[MAX_GROUP_BEAMS];
	for (Int i = 0; i < count; i++)
	{
		footprints[i] = findFootprint(map, *glows[i]);
	}

	Int cells = 0;
	for (Int y = loY; y < hiY; y++)
	{
		for (Int x = loX; x < hiX; x++)
		{
			if (reachesCell(glows, footprints, count, x, y))
			{
				cells++;
			}
		}
	}
	if (cells == 0)
	{
		return TRUE;
	}
	if (cells > MAX_GLOW_CELLS)
	{
		return FALSE;
	}

	DynamicVBAccessClass vbAccess(BUFFER_TYPE_DYNAMIC_DX8, dynamic_fvf_type, width * height);
	{
		DynamicVBAccessClass::WriteLockClass lock(&vbAccess);
		VertexFormatXYZNDUV2 *verts = lock.Get_Formatted_Vertex_Array();
		for (Int y = loY; y <= hiY; y++)
		{
			for (Int x = loX; x <= hiX; x++)
			{
				const Real z = Sample_Height(map, x, y);
				Vector3 normal(Sample_Height(map, x - 1, y) - Sample_Height(map, x + 1, y),
					Sample_Height(map, x, y - 1) - Sample_Height(map, x, y + 1), 2.0f * MAP_XY_FACTOR);
				normal.Normalize();

				verts->x = x * MAP_XY_FACTOR;
				verts->y = y * MAP_XY_FACTOR;
				verts->z = z + GLOW_LIFT;
				verts->nx = normal.X;
				verts->ny = normal.Y;
				verts->nz = normal.Z;
				// The pixel shader rebuilds the normal from x and y, since terrain always faces up.
				verts->diffuse = DX8Wrapper::Convert_Color_Clamp(Vector4(normal.X * 0.5f + 0.5f, normal.Y * 0.5f + 0.5f, 0.0f, 1.0f));
				verts->u1 = verts->x;
				verts->v1 = verts->y;
				verts->u2 = verts->z;
				verts->v2 = 0.0f;
				verts++;
			}
		}
	}

	// Triangles split along each cell's flip, as the terrain's do, so the mesh stays on the ground.
	DynamicIBAccessClass ibAccess(BUFFER_TYPE_DYNAMIC_DX8, cells * 6);
	{
		DynamicIBAccessClass::WriteLockClass lock(&ibAccess);
		UnsignedShort *indices = lock.Get_Index_Array();
		for (Int y = loY; y < hiY; y++)
		{
			for (Int x = loX; x < hiX; x++)
			{
				if (!reachesCell(glows, footprints, count, x, y))
				{
					continue;
				}
				const UnsignedShort topLeft = (UnsignedShort)((y - loY) * width + (x - loX));
				const UnsignedShort topRight = topLeft + 1;
				const UnsignedShort bottomLeft = topLeft + width;
				const UnsignedShort bottomRight = bottomLeft + 1;
				if (map->getFlipState(x + border, y + border))
				{
					*indices++ = topRight;
					*indices++ = bottomLeft;
					*indices++ = topLeft;
					*indices++ = topRight;
					*indices++ = bottomRight;
					*indices++ = bottomLeft;
				}
				else
				{
					*indices++ = topLeft;
					*indices++ = bottomRight;
					*indices++ = bottomLeft;
					*indices++ = topLeft;
					*indices++ = topRight;
					*indices++ = bottomRight;
				}
			}
		}
	}

	DX8Wrapper::Set_Vertex_Buffer(vbAccess);
	DX8Wrapper::Set_Index_Buffer(ibAccess, 0);
	DX8Wrapper::Apply_Render_State_Changes();

	// Lights what is already drawn: dest * (1 + light). Units in front hide it, and it writes no depth.
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, TRUE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, TRUE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, D3DBLEND_ONE);
	// LaserGroundGlowDebug turns it into dest * (1 - light), a shadow the shape of the light.
	DX8Wrapper::Set_DX8_Render_State(D3DRS_BLENDOP, TheGlobalData->m_laserGlowDebug ? D3DBLENDOP_REVSUBTRACT : D3DBLENDOP_ADD);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_CULLMODE, D3DCULL_NONE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGENABLE, FALSE);

	// Both stages hold a texture, so their coordinates arrive as TEXCOORD0 and TEXCOORD1.
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	for (Int stage = 0; stage < 2; stage++)
	{
		device->SetTexture(stage, noise);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, stage);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MIPFILTER, D3DTEXF_NONE);
	}

	DX8Wrapper::Set_Pixel_Shader(shader);
	DX8Wrapper::Set_Pixel_Shader_Constant(0, constants, constantCount);
	DX8Wrapper::Draw_Triangles(0, cells * 2, 0, width * height);

	device->SetTexture(0, nullptr);
	device->SetTexture(1, nullptr);
	DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU | 1);
	return TRUE;
#else
	return FALSE;
#endif
}
