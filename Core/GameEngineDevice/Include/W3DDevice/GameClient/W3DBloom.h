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

// FILE: W3DBloom.h /////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"
#include "WW3D2/ww3dformat.h"
#include "WW3D2/shader.h"

class TextureClass;
class RenderInfoClass;
class DX8IndexBufferClass;
#if defined(BUILD_WITH_D3D9)
#include "WW3D2/dx8compat.h"
#else
struct IDirect3DSurface8;
#endif
struct VertexFormatXYZNDUV2;

// Adds a soft glow around additive particles and additive meshes. The caller draws the particles a
// second time into the bloom target between begin and end; end replays the meshes the DX8 mesh
// renderer kept, blurs the target and adds it onto the back buffer.
class W3DBloom
{
public:
	W3DBloom();
	~W3DBloom();

	// true when the caller should draw the additive pass now; anythingToDraw lets a frame with no
	// additive draws skip the whole pass without touching the device
	Bool begin(RenderInfoClass &rinfo, Bool anythingToDraw);
	void end(RenderInfoClass &rinfo);
	void ReleaseResources();						///< drops the targets before a device reset; begin recreates them

private:
	// one screen space quad of a pass, in source texture coordinates
	struct Tap
	{
		Real u0;
		Real v0;
		Real u1;
		Real v1;
		Real brightness;
	};

	Bool acquireTargets(Int width, Int height, WW3DFormat format, Int sampleType, UnsignedInt sampleQuality);
	void releaseTargets();
	void releaseDefaults();
	Bool setTarget(Int target);
	Bool blurPass(TextureClass *source, Int target, Real offsetU, Real offsetV, Bool shrink);
	void drawTaps(TextureClass *source, const Tap *taps, Int count, const ShaderClass &firstShader);
	void drawShaderTaps(TextureClass *source, const Tap *taps, Int count);

	// the full sized target that receives the additive draws, then two reduced ones to ping-pong the blur through
	enum { TARGET_FULL = 0, TARGET_BLUR = 1, TARGET_COUNT = 3 };
	TextureClass *m_target[TARGET_COUNT];
	IDirect3DSurface8 *m_targetSurface[TARGET_COUNT];	///< held so binding a target allocates nothing
	IDirect3DSurface8 *m_sampledSurface;	///< takes the additive draws when the scene is multisampled, then resolves into the full target
	Int m_sampleType;										///< the scene's multisample type the targets were made for
	DX8IndexBufferClass *m_quadIndices;	///< the same two triangles for every quad a pass can draw
	IDirect3DSurface8 *m_defaultTarget;	///< the back buffer, held only between begin and end
	IDirect3DSurface8 *m_defaultDepth;
	ShaderClass m_addShader;						///< adds the source onto the target
	ShaderClass m_copyShader;						///< replaces the target, so a pass needs no clear
	DWORD m_blurShader;									///< sums a pass's taps in one quad; zero draws a quad per tap
	Bool m_disabled;										///< the device refused the target; stays set until ReleaseResources
};

extern W3DBloom *TheW3DBloom;
