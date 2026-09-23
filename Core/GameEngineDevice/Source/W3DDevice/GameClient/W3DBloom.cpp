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

// W3DBloom.cpp ////////////////////////////////////////////////////////////////////////////////
// Bloom post effect for additive particles, built from render targets and screen quads
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Lib/BaseType.h"
#include "WWLib/always.h"
#include "W3DDevice/GameClient/W3DBloom.h"
#include "Common/GlobalData.h"
#include "Common/GameMemory.h"
#include "WW3D2/texture.h"
#include "WW3D2/surfaceclass.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8vertexbuffer.h"
#include "WW3D2/dx8indexbuffer.h"
#include "WW3D2/dx8renderer.h"
#include "WW3D2/formconv.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WW3D2/vertmaterial.h"
#include "WW3D2/dx8caps.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"

W3DBloom *TheW3DBloom = nullptr;

// The blur runs at this fraction of the screen on each axis. Half keeps the halo smooth; a quarter
// shows its own texels as blocks once the glow is bright.
enum { BLOOM_DOWNSCALE = 2, BLOOM_BLUR_PASSES = 4 };

// the most quads one pass draws, which sizes the shared index buffer and the per pass vertex lock
enum { BLOOM_MAX_TAPS = 5 };

// A separable gaussian, run as a horizontal pass then a vertical one. Each tap sits between two
// texels so bilinear filtering folds two samples into one, which keeps a wide radius cheap. Four
// diagonal taps of equal weight would be a box blur instead, and a box stays square however many
// times it runs, which is what made the halo look blocky.
static const Real BLOOM_TAP_OFFSET[3] = { 0.0f, 1.3846154f, 3.2307692f };
static const Real BLOOM_TAP_WEIGHT[3] = { 0.2270270f, 0.3162162f, 0.0702703f };

// screen space quads; the copy variant replaces the target instead of adding to it
static ShaderClass makeQuadShader(Bool additive)
{
	ShaderClass shader = ShaderClass::_PresetAdditiveShader;
	shader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);
	shader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
	shader.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);
	shader.Set_Primary_Gradient(ShaderClass::GRADIENT_MODULATE);
	if (!additive)
	{
		shader.Set_Dst_Blend_Func(ShaderClass::DSTBLEND_ZERO);
	}
	return shader;
}

static TextureClass *createTarget(Int width, Int height, WW3DFormat format)
{
	TextureClass *target = MSGNEW("TextureClass") TextureClass(width, height, format, MIP_LEVELS_1, TextureClass::POOL_DEFAULT, true);
	if (target->Peek_D3D_Texture() == nullptr)
	{
		REF_PTR_RELEASE(target);
		return nullptr;
	}

	// the targets are not powers of two, which most hardware only samples correctly when clamped
	TextureFilterClass &filter = target->Get_Filter();
	filter.Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	filter.Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	filter.Set_Min_Filter(TextureFilterClass::FILTER_TYPE_FAST);
	filter.Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_FAST);
	filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
	return target;
}

W3DBloom::W3DBloom()
	: m_addShader(makeQuadShader(TRUE)),
	m_copyShader(makeQuadShader(FALSE))
{
	for (Int i = 0; i < TARGET_COUNT; ++i)
	{
		m_target[i] = nullptr;
		m_targetSurface[i] = nullptr;
	}
	m_quadIndices = nullptr;
	m_defaultTarget = nullptr;
	m_defaultDepth = nullptr;
	m_blurShader = 0;
	m_disabled = false;
}

W3DBloom::~W3DBloom()
{
	ReleaseResources();
}

void W3DBloom::ReleaseResources()
{
	releaseTargets();
	releaseDefaults();
	m_disabled = false;
}

Bool W3DBloom::acquireTargets(Int width, Int height, WW3DFormat format)
{
	for (Int i = 0; i < TARGET_COUNT; ++i)
	{
		const Int scale = (i == TARGET_FULL) ? 1 : BLOOM_DOWNSCALE;
		m_target[i] = createTarget(width / scale, height / scale, format);
		if (m_target[i] == nullptr)
		{
			releaseTargets();
			return false;
		}

		// one surface per target for the whole device lifetime, so binding one costs no allocation
		SurfaceClass *surface = m_target[i]->Get_Surface_Level();
		if (surface == nullptr)
		{
			releaseTargets();
			return false;
		}
		m_targetSurface[i] = surface->Peek_D3D_Surface();
		m_targetSurface[i]->AddRef();
		REF_PTR_RELEASE(surface);
	}

#if defined(BUILD_WITH_D3D9)
	if (DX8Wrapper::Get_Current_Caps()->Get_Pixel_Shader_Major_Version() >= 2 &&
		FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\bloomblur.pso", nullptr, 0, false, &m_blurShader)))
	{
		m_blurShader = 0;
	}
#endif

	// quad k uses vertices k*4 to k*4+3, so every pass indexes the same buffer
	m_quadIndices = NEW_REF(DX8IndexBufferClass, (BLOOM_MAX_TAPS * 6));
	{
		DX8IndexBufferClass::WriteLockClass lock(m_quadIndices);
		UnsignedShort *indices = lock.Get_Index_Array();
		for (Int quad = 0; quad < BLOOM_MAX_TAPS; ++quad)
		{
			const UnsignedShort base = (UnsignedShort)(quad * 4);
			indices[quad * 6 + 0] = base + 0;
			indices[quad * 6 + 1] = base + 1;
			indices[quad * 6 + 2] = base + 2;
			indices[quad * 6 + 3] = base + 2;
			indices[quad * 6 + 4] = base + 1;
			indices[quad * 6 + 5] = base + 3;
		}
	}

	return true;
}

void W3DBloom::releaseTargets()
{
	if (m_blurShader != 0)
	{
		DX8_DELETE_PIXEL_SHADER(DX8Wrapper::_Get_D3D_Device8(), m_blurShader);
		m_blurShader = 0;
	}
	REF_PTR_RELEASE(m_quadIndices);
	for (Int i = 0; i < TARGET_COUNT; ++i)
	{
		if (m_targetSurface[i])
		{
			m_targetSurface[i]->Release();
			m_targetSurface[i] = nullptr;
		}
		REF_PTR_RELEASE(m_target[i]);
	}
}

void W3DBloom::releaseDefaults()
{
	if (m_defaultTarget)
	{
		m_defaultTarget->Release();
		m_defaultTarget = nullptr;
	}
	if (m_defaultDepth)
	{
		m_defaultDepth->Release();
		m_defaultDepth = nullptr;
	}
}

// every target borrows the screen's depth buffer, which DX8 allows because none is larger than it
Bool W3DBloom::setTarget(Int target)
{
	return SUCCEEDED(DX8Wrapper::Set_DX8_Render_Target_Surfaces(m_targetSurface[target], m_defaultDepth));
}

Bool W3DBloom::begin(RenderInfoClass &rinfo, Bool anythingToDraw)
{
	const Bool wanted = !m_disabled && TheGlobalData->m_useBloom && TheGlobalData->m_bloomStrength > 0.0f;

	// the meshes of this frame are already drawn, so this decides the capture for the next one
	DX8MeshRendererClass::Enable_Bloom_Capture(wanted);
	if (!wanted)
	{
		TheDX8MeshRenderer.Clear_Bloom_Lists();
		return false;
	}

	// a blurred black target adds nothing, so a frame without additive draws skips the whole pass
	if (!anythingToDraw)
	{
		return false;
	}

	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	if (FAILED(device->GetRenderTarget(DX8_SWAPCHAIN &m_defaultTarget)) || FAILED(device->GetDepthStencilSurface(&m_defaultDepth)))
	{
		releaseDefaults();
		return false;
	}

	D3DSURFACE_DESC targetDesc;
	D3DSURFACE_DESC depthDesc;
	m_defaultTarget->GetDesc(&targetDesc);
	m_defaultDepth->GetDesc(&depthDesc);

	// DX8 cannot redirect a multisampled scene into a texture
	if (targetDesc.MultiSampleType != D3DMULTISAMPLE_NONE || depthDesc.MultiSampleType != D3DMULTISAMPLE_NONE)
	{
		releaseDefaults();
		return false;
	}

	TextureClass *full = m_target[TARGET_FULL];
	if (full == nullptr || (Int)targetDesc.Width != full->Get_Width() || (Int)targetDesc.Height != full->Get_Height())
	{
		releaseTargets();
		if (!acquireTargets(targetDesc.Width, targetDesc.Height, D3DFormat_To_WW3DFormat(targetDesc.Format)))
		{
			releaseDefaults();
			return false;
		}
	}

	if (!setTarget(TARGET_FULL))
	{
		releaseTargets();
		releaseDefaults();
		m_disabled = true;
		return false;
	}

	// the new target starts with a full size viewport, so this clears all of it
	DX8Wrapper::Clear(true, false, Vector3(0.0f, 0.0f, 0.0f));
	rinfo.Camera.Apply();
	return true;
}

void W3DBloom::end(RenderInfoClass &rinfo)
{
	// additive meshes join the particles in the bloom target
	TheDX8MeshRenderer.Flush_Bloom();

	// the quads are already in clip space
	Matrix4x4 identity(true);
	DX8Wrapper::Set_World_Identity();
	DX8Wrapper::Set_View_Identity();
	DX8Wrapper::Set_Transform(D3DTS_PROJECTION, identity);

	VertexMaterialClass *material = VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(material);
	REF_PTR_RELEASE(material);
	DX8Wrapper::Set_Index_Buffer(m_quadIndices, 0);

	// shrink the scene into the first blur target, averaging a 2x2 block per texel
	TextureClass *full = m_target[TARGET_FULL];
	Bool drawn = blurPass(full, TARGET_BLUR, 0.5f / (Real)full->Get_Width(), 0.5f / (Real)full->Get_Height(), TRUE);

	// alternating the axis makes the kernel separable, and each pair widens the halo
	const Real blurWidth = (Real)m_target[TARGET_BLUR]->Get_Width();
	const Real blurHeight = (Real)m_target[TARGET_BLUR]->Get_Height();
	for (Int pass = 0; drawn && pass < BLOOM_BLUR_PASSES; ++pass)
	{
		const Real spread = 1.0f + (Real)(pass / 2);
		const Bool horizontal = (pass & 1) == 0;
		drawn = blurPass(m_target[TARGET_BLUR + (pass & 1)], TARGET_BLUR + ((pass + 1) & 1),
			horizontal ? spread / blurWidth : 0.0f,
			horizontal ? 0.0f : spread / blurHeight,
			FALSE);
	}

	DX8Wrapper::Set_DX8_Render_Target_Surfaces(m_defaultTarget, m_defaultDepth);

	if (drawn)
	{
		// the blur covers the whole screen, the quad only the camera's part of it, so the viewport
		// is set back to the camera's rectangle the same way CameraClass::Apply computes it
		Vector2 viewMin;
		Vector2 viewMax;
		rinfo.Camera.Get_Viewport(viewMin, viewMax);

		D3DVIEWPORT8 viewport;
		viewport.X = (DWORD)(viewMin.X * (Real)full->Get_Width());
		viewport.Y = (DWORD)(viewMin.Y * (Real)full->Get_Height());
		viewport.Width = (DWORD)((viewMax.X - viewMin.X) * (Real)full->Get_Width());
		viewport.Height = (DWORD)((viewMax.Y - viewMin.Y) * (Real)full->Get_Height());
		viewport.MinZ = 0.0f;
		viewport.MaxZ = 1.0f;
		DX8Wrapper::Set_Viewport(&viewport);

		// the debug view shows the glow alone on black, so it is obvious what reached the target
		const Bool debug = TheGlobalData->m_bloomDebug;
		Tap composite;
		composite.u0 = viewMin.X;
		composite.v0 = viewMin.Y;
		composite.u1 = viewMax.X;
		composite.v1 = viewMax.Y;
		composite.brightness = debug ? 1.0f : TheGlobalData->m_bloomStrength;
		drawTaps(m_target[TARGET_BLUR + (BLOOM_BLUR_PASSES & 1)], &composite, 1, debug ? m_copyShader : m_addShader);
	}

	DX8Wrapper::Set_Texture(0, nullptr);
	DX8Wrapper::Set_Index_Buffer(nullptr, 0);
	rinfo.Camera.Apply();
	releaseDefaults();
}

// the first tap replaces the target so no clear is needed, the rest add onto it
Bool W3DBloom::blurPass(TextureClass *source, Int target, Real offsetU, Real offsetV, Bool shrink)
{
	static const Real corners[4][2] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { -1.0f, 1.0f }, { 1.0f, 1.0f } };

	if (!setTarget(target))
	{
		return false;
	}

	Tap taps[BLOOM_MAX_TAPS];
	Int count = 0;

	// the shrink samples a square of the larger source, a blur pass runs along one axis of its own size
	if (shrink)
	{
		for (Int tap = 0; tap < 4; ++tap)
		{
			const Real du = corners[tap][0] * offsetU;
			const Real dv = corners[tap][1] * offsetV;
			taps[count].u0 = du;
			taps[count].v0 = dv;
			taps[count].u1 = 1.0f + du;
			taps[count].v1 = 1.0f + dv;
			taps[count].brightness = 0.25f;
			++count;
		}
	}
	else
	{
		taps[count].u0 = 0.0f;
		taps[count].v0 = 0.0f;
		taps[count].u1 = 1.0f;
		taps[count].v1 = 1.0f;
		taps[count].brightness = BLOOM_TAP_WEIGHT[0];
		++count;

		for (Int tap = 1; tap < 3; ++tap)
		{
			const Real du = offsetU * BLOOM_TAP_OFFSET[tap];
			const Real dv = offsetV * BLOOM_TAP_OFFSET[tap];
			for (Int side = -1; side <= 1; side += 2)
			{
				taps[count].u0 = side * du;
				taps[count].v0 = side * dv;
				taps[count].u1 = 1.0f + side * du;
				taps[count].v1 = 1.0f + side * dv;
				taps[count].brightness = BLOOM_TAP_WEIGHT[tap];
				++count;
			}
		}
	}

	if (m_blurShader != 0)
	{
		drawShaderTaps(source, taps, count);
	}
	else
	{
		drawTaps(source, taps, count, m_copyShader);
	}
	return true;
}

// one quad whose pixel shader sums the taps, so the target is written once at full precision
void W3DBloom::drawShaderTaps(TextureClass *source, const Tap *taps, Int count)
{
	Vector4 constants[BLOOM_MAX_TAPS];
	for (Int i = 0; i < BLOOM_MAX_TAPS; ++i)
	{
		constants[i] = (i < count) ? Vector4(taps[i].u0, taps[i].v0, taps[i].brightness, 0.0f) : Vector4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	Tap quad;
	quad.u0 = 0.0f;
	quad.v0 = 0.0f;
	quad.u1 = 1.0f;
	quad.v1 = 1.0f;
	quad.brightness = 1.0f;

	DX8Wrapper::Set_Pixel_Shader_Constant(0, constants, BLOOM_MAX_TAPS);
	DX8Wrapper::Set_Pixel_Shader(m_blurShader);
	drawTaps(source, &quad, 1, m_copyShader);
	DX8Wrapper::Set_Pixel_Shader(0);
}

// every quad of a pass shares one vertex lock; the quads fill the viewport in clip space, so no half
// pixel shift is needed
void W3DBloom::drawTaps(TextureClass *source, const Tap *taps, Int count, const ShaderClass &firstShader)
{
	static const Real cornerX[4] = { -1.0f, 1.0f, -1.0f, 1.0f };
	static const Real cornerY[4] = { 1.0f, 1.0f, -1.0f, -1.0f };

	DynamicVBAccessClass vb(BUFFER_TYPE_DYNAMIC_DX8, dynamic_fvf_type, count * 4);
	{
		DynamicVBAccessClass::WriteLockClass lock(&vb);
		VertexFormatXYZNDUV2 *verts = lock.Get_Formatted_Vertex_Array();
		for (Int quad = 0; quad < count; ++quad)
		{
			const Tap &tap = taps[quad];
			const unsigned color = DX8Wrapper::Convert_Color_Clamp(Vector4(tap.brightness, tap.brightness, tap.brightness, 1.0f));
			for (Int i = 0; i < 4; ++i)
			{
				VertexFormatXYZNDUV2 &vert = verts[quad * 4 + i];
				vert.x = cornerX[i];
				vert.y = cornerY[i];
				vert.z = 0.0f;
				vert.nx = 0.0f;
				vert.ny = 0.0f;
				vert.nz = 0.0f;
				vert.diffuse = color;
				vert.u1 = (i & 1) ? tap.u1 : tap.u0;
				vert.v1 = (i & 2) ? tap.v1 : tap.v0;
				vert.u2 = 0.0f;
				vert.v2 = 0.0f;
			}
		}
	}

	DX8Wrapper::Set_Vertex_Buffer(vb);
	DX8Wrapper::Set_Texture(0, source);
	for (Int quad = 0; quad < count; ++quad)
	{
		DX8Wrapper::Set_Shader(quad == 0 ? firstShader : m_addShader);
		DX8Wrapper::Draw_Triangles((UnsignedShort)(quad * 6), 2, (UnsignedShort)(quad * 4), 4);
	}
}
