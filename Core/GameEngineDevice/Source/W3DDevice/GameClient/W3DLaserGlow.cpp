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

// Scene light below this is treated as this, so the glow on black ground stays in bounds.
static const Real MIN_SCENE_LIGHT = 0.15f;

W3DLaserGlow::W3DLaserGlow()
	: m_count(0),
	  m_shader(0),
	  m_loaded(FALSE)
{
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
	}
	m_shader = 0;
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
	}
	return m_shader != 0;
#else
	return FALSE;
#endif
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

void W3DLaserGlow::render(RenderInfoClass &rinfo)
{
#if defined(BUILD_WITH_D3D9)
	const Int count = m_count;
	m_count = 0;
	if (count == 0 || !isEnabled() || TheTerrainRenderObject == nullptr || TheTerrainRenderObject->getMap() == nullptr)
	{
		return;
	}

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

	for (Int i = 0; i < count; i++)
	{
		Vector4 pulse(0.0f, 0.0f, 0.0f, 0.0f);
		IDirect3DTexture8 *noise = (TheW3DSoftParticles != nullptr) ? TheW3DSoftParticles->getLaserPulse(m_glows[i].pulses, pulse) : nullptr;
		drawGlow(TheTerrainRenderObject->getMap(), m_glows[i], sceneLight, pulse, noise);
	}

	DX8Wrapper::Set_Pixel_Shader(0);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	DX8Wrapper::Set_Vertex_Buffer(nullptr);
	DX8Wrapper::Set_Index_Buffer(nullptr, 0);

	// Z, blend, cull and fog are ShaderClass state, so the next shader set restores them in full.
	ShaderClass::Invalidate();
#endif
}

void W3DLaserGlow::drawGlow(WorldHeightMap *map, const Glow &glow, const Vector3 &sceneLight, const Vector4 &pulse, IDirect3DTexture8 *noise)
{
#if defined(BUILD_WITH_D3D9)
	const Int border = map->getBorderSizeInline();
	const Int loX = max((Int)floor((min(glow.start.X, glow.end.X) - glow.reach) / MAP_XY_FACTOR), -border);
	const Int loY = max((Int)floor((min(glow.start.Y, glow.end.Y) - glow.reach) / MAP_XY_FACTOR), -border);
	const Int hiX = min((Int)ceil((max(glow.start.X, glow.end.X) + glow.reach) / MAP_XY_FACTOR), map->getXExtent() - border - 1);
	const Int hiY = min((Int)ceil((max(glow.start.Y, glow.end.Y) + glow.reach) / MAP_XY_FACTOR), map->getYExtent() - border - 1);
	const Int width = hiX - loX + 1;
	const Int height = hiY - loY + 1;
	if (width < 2 || height < 2 || width * height > MAX_GLOW_SAMPLES)
	{
		return;
	}

	// Only cells the light can reach on the ground are drawn. Half a cell's diagonal covers their corners.
	const Real cellReach = glow.reach + MAP_XY_FACTOR * 0.75f;
	Int cells = 0;
	for (Int y = loY; y < hiY; y++)
	{
		for (Int x = loX; x < hiX; x++)
		{
			if (Ground_Distance_Squared((x + 0.5f) * MAP_XY_FACTOR, (y + 0.5f) * MAP_XY_FACTOR, glow.start, glow.end) < cellReach * cellReach)
			{
				cells++;
			}
		}
	}
	if (cells == 0)
	{
		return;
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
				if (Ground_Distance_Squared((x + 0.5f) * MAP_XY_FACTOR, (y + 0.5f) * MAP_XY_FACTOR, glow.start, glow.end) >= cellReach * cellReach)
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

	// Constants for the laser shader's pulses, measured from the beam's start.
	const Vector3 span = glow.end - glow.start;
	const Real lengthSquared = span.Length2();
	const Real length = sqrt(lengthSquared);
	const Real alongStart = (length > 0.0f) ? Vector3::Dot_Product(glow.start, span) / length * pulse.Y : 0.0f;

	const Vector3 light(glow.color.X / sceneLight.X, glow.color.Y / sceneLight.Y, glow.color.Z / sceneLight.Z);
	const Vector4 beamStart(glow.start.X, glow.start.Y, glow.start.Z, alongStart);
	const Vector4 beamSpan(span.X, span.Y, span.Z, (lengthSquared > 0.0f) ? 1.0f / lengthSquared : 0.0f);
	const Vector4 glowLight(light.X, light.Y, light.Z, 1.0f / glow.reach);
	const Vector4 glowPulse(pulse.X, length * pulse.Y, pulse.Z, 0.0f);
	const Vector4 shape(max(TheGlobalData->m_laserGlowFalloff, 0.1f), min(max(TheGlobalData->m_laserGlowWrap, 0.0f), 1.0f), 0.0f, 0.0f);

	DX8Wrapper::Set_Pixel_Shader(m_shader);
	DX8Wrapper::Set_Pixel_Shader_Constant(0, &beamStart, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(1, &beamSpan, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(2, &glowLight, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(3, &glowPulse, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(4, &shape, 1);
	DX8Wrapper::Draw_Triangles(0, cells * 2, 0, width * height);

	device->SetTexture(0, nullptr);
	device->SetTexture(1, nullptr);
	DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU | 1);
#endif
}
