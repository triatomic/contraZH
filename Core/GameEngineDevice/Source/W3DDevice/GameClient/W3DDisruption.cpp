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

// W3DDisruption.cpp //////////////////////////////////////////////////////////////////////////////
// Discs on the ground that mask the disruption shader, with no art of their own
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Lib/BaseType.h"
#include "WWLib/always.h"
#include "W3DDevice/GameClient/W3DDisruption.h"
#include "W3DDevice/GameClient/W3DSoftParticles.h"
#include "GameClient/DisruptionShader.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8vertexbuffer.h"
#include "WW3D2/dx8indexbuffer.h"
#include "WW3D2/dx8fvf.h"
#include "WW3D2/shader.h"
#include "WW3D2/vertmaterial.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WW3D2/ww3d.h"

W3DDisruptionManager *TheW3DDisruption = nullptr;

static const Int MASK_SIZE = 64;

W3DDisruptionManager::W3DDisruptionManager()
	: m_count(0),
	  m_mask(nullptr)
{
}

W3DDisruptionManager::~W3DDisruptionManager()
{
	ReleaseResources();
}

void W3DDisruptionManager::ReleaseResources()
{
	if (m_mask != nullptr)
	{
		m_mask->Release();
		m_mask = nullptr;
	}
}

void W3DDisruptionManager::add(const Coord3D &position, Real radius, const DisruptionShaderInfo *info, UnsignedInt durationMs, Real fade)
{
	if (radius <= 0.0f || durationMs == 0 || info == nullptr)
	{
		return;
	}

	// A full list gives up its oldest field.
	if (m_count == MAX_FIELDS)
	{
		for (Int i = 1; i < m_count; i++)
		{
			m_fields[i - 1] = m_fields[i];
		}
		m_count--;
	}

	Field &field = m_fields[m_count++];
	field.position = position;
	field.radius = radius;
	field.fade = fade;
	field.info = info;
	field.startMs = WW3D::Get_Sync_Time();
	field.durationMs = durationMs;
}

Bool W3DDisruptionManager::update()
{
	const UnsignedInt now = WW3D::Get_Sync_Time();
	Int live = 0;
	for (Int i = 0; i < m_count; i++)
	{
		if (now - m_fields[i].startMs < m_fields[i].durationMs)
		{
			m_fields[live++] = m_fields[i];
		}
	}
	m_count = live;
	return m_count > 0;
}

// White at the middle, easing to black at the rim, so the shader's mask fades the same way.
void W3DDisruptionManager::createMask()
{
#if defined(BUILD_WITH_D3D9)
	m_mask = DX8Wrapper::_Create_DX8_Texture(MASK_SIZE, MASK_SIZE, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1, D3DPOOL_MANAGED, false);
	if (m_mask == nullptr)
	{
		return;
	}

	IDirect3DTexture8 *lockable = DX8Wrapper::_Peek_Lockable_Texture(m_mask);
	D3DLOCKED_RECT locked;
	if (FAILED(lockable->LockRect(0, &locked, nullptr, 0)))
	{
		m_mask->Release();
		m_mask = nullptr;
		return;
	}

	const Real middle = (MASK_SIZE - 1) * 0.5f;
	for (Int y = 0; y < MASK_SIZE; y++)
	{
		UnsignedInt *row = (UnsignedInt *)((UnsignedByte *)locked.pBits + y * locked.Pitch);
		for (Int x = 0; x < MASK_SIZE; x++)
		{
			const Real dx = (x - middle) / middle;
			const Real dy = (y - middle) / middle;
			Real inside = 1.0f - sqrt(dx * dx + dy * dy);
			inside = (inside > 0.0f) ? inside : 0.0f;
			const UnsignedInt level = (UnsignedInt)(inside * inside * (3.0f - 2.0f * inside) * 255.0f + 0.5f);
			row[x] = 0xff000000u | (level << 16) | (level << 8) | level;
		}
	}

	lockable->UnlockRect(0);
	DX8Wrapper::_Upload_Lockable_Texture(m_mask);
#endif
}

void W3DDisruptionManager::render(RenderInfoClass &rinfo)
{
#if defined(BUILD_WITH_D3D9)
	if (m_count == 0 || TheW3DSoftParticles == nullptr)
	{
		return;
	}
	if (m_mask == nullptr)
	{
		createMask();
		if (m_mask == nullptr)
		{
			return;
		}
	}

	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	const UnsignedInt now = WW3D::Get_Sync_Time();
	rinfo.Camera.Apply();

	DX8Wrapper::Set_Texture(0, nullptr);
	VertexMaterialClass *material = VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(material);
	REF_PTR_RELEASE(material);
	// An additive shader makes the hook read the mask from the disc's colour alone.
	const ShaderClass shader = ShaderClass::_PresetAdditiveSpriteShader;
	DX8Wrapper::Set_Shader(shader);

	DynamicVBAccessClass vbAccess(BUFFER_TYPE_DYNAMIC_DX8, dynamic_fvf_type, m_count * 4);
	{
		DynamicVBAccessClass::WriteLockClass lock(&vbAccess);
		VertexFormatXYZNDUV2 *verts = lock.Get_Formatted_Vertex_Array();
		static const Real corners[4][2] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { -1.0f, 1.0f }, { 1.0f, 1.0f } };
		for (Int i = 0; i < m_count; i++)
		{
			const Field &field = m_fields[i];

			// The field eases in and out over the fade's share of its life.
			const Real age = (Real)(now - field.startMs) / (Real)field.durationMs;
			Real level = 1.0f;
			if (field.fade > 0.0f)
			{
				level = min(min(age, 1.0f - age) / field.fade, 1.0f);
			}
			const unsigned grey = (unsigned)(max(level, 0.0f) * 255.0f + 0.5f);

			// The quad sits around the origin, and the world transform puts it at the field, where the rings then start.
			for (Int c = 0; c < 4; c++)
			{
				verts->x = corners[c][0] * field.radius;
				verts->y = corners[c][1] * field.radius;
				verts->z = 0.0f;
				verts->nx = 0.0f;
				verts->ny = 0.0f;
				verts->nz = 1.0f;
				verts->diffuse = 0xff000000u | (grey << 16) | (grey << 8) | grey;
				verts->u1 = corners[c][0] * 0.5f + 0.5f;
				verts->v1 = corners[c][1] * 0.5f + 0.5f;
				verts->u2 = 0.0f;
				verts->v2 = 0.0f;
				verts++;
			}
		}
	}

	DynamicIBAccessClass ibAccess(BUFFER_TYPE_DYNAMIC_DX8, m_count * 6);
	{
		DynamicIBAccessClass::WriteLockClass lock(&ibAccess);
		UnsignedShort *indices = lock.Get_Index_Array();
		for (Int i = 0; i < m_count; i++)
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

	DX8Wrapper::Set_Vertex_Buffer(vbAccess);
	DX8Wrapper::Set_Index_Buffer(ibAccess, 0);

	// No shader restores ZENABLE, so it is put back after.
	DWORD depthTest = D3DZB_TRUE;
	device->GetRenderState(D3DRS_ZENABLE, &depthTest);

	for (Int i = 0; i < m_count; i++)
	{
		const Field &field = m_fields[i];
		Matrix3D transform(true);
		transform.Set_Translation(Vector3(field.position.x, field.position.y, field.position.z));
		DX8Wrapper::Set_Transform(D3DTS_WORLD, transform);
		DX8Wrapper::Apply_Render_State_Changes();

		if (!TheW3DSoftParticles->Begin(shader, SoftParticleHookClass::EFFECT_DISRUPT, field.info))
		{
			continue;
		}

		device->SetTexture(0, m_mask);
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);

		// The field ripples the air over uneven ground, so nothing in front of the flat disc hides it.
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, FALSE);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_CULLMODE, D3DCULL_NONE);
		DX8Wrapper::Draw_Triangles(i * 6, 2, i * 4, 4);
		TheW3DSoftParticles->End();
	}

	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, depthTest);
	DX8Wrapper::Set_Transform(D3DTS_WORLD, Matrix3D(true));
	device->SetTexture(0, nullptr);
	DX8Wrapper::Set_Vertex_Buffer(nullptr);
	DX8Wrapper::Set_Index_Buffer(nullptr, 0);

	// Cull is ShaderClass state, so the next shader set restores it.
	ShaderClass::Invalidate();
#endif
}
