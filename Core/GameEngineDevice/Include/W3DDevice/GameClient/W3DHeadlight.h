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

// FILE: W3DHeadlight.h ///////////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"
#include "WWMath/vector3.h"
#include "WW3D2/dx8compat.h"

class RenderInfoClass;
class RenderObjClass;
struct HeadlightShaderTuning;

// Vehicle headlights drawn by headlight.hlsl in place of the models' HEADLIGHT meshes.
class W3DHeadlightManager
{
public:
	W3DHeadlightManager();
	~W3DHeadlightManager();

	// One lamp of a HEADLIGHT mesh in the mesh's own space, from the lamp to the far end of its cone.
	struct Beam
	{
		Vector3 start;
		Vector3 end;
		Real radius;	///< the cone's at its far end
	};

	/// Finds the lamps a HEADLIGHT mesh holds and returns how many it wrote. modelMiddle is the model's origin in world space.
	static Int findBeams(RenderObjClass &mesh, const Vector3 &modelMiddle, Beam *beams, Int maxBeams);

	/// True for a mesh that blends nothing, such as a lamp body, which keeps its own look.
	static Bool isOpaque(RenderObjClass &mesh);

	Bool isActive();	///< false where the models have to keep their headlight meshes
	/// One headlight for the coming frame, from the lamp to the far end of its mesh. The radius is the mesh's at that end.
	/// The model's own settings must outlive the frame.
	void add(const Vector3 &start, const Vector3 &end, Real radius, const HeadlightShaderTuning *own);
	void render(RenderInfoClass &rinfo);	///< draws the frame's headlights and forgets them
	void ReleaseResources();	///< drops the shaders before a device reset; render loads them again

private:
	enum { MAX_LIGHTS = 512 };

	struct Light
	{
		Vector3 start;
		Vector3 end;
		Real radius;
		const HeadlightShaderTuning *own;
	};

	Bool loadShaders();

	Light m_lights[MAX_LIGHTS];
	Int m_count;
	UnsignedInt m_frame;	///< the render frame the lights were added in
	Bool m_loaded;
	DWORD m_beamVertexShader;
	DWORD m_beamPixelShader;
	DWORD m_poolVertexShader;
	DWORD m_poolPixelShader;
	DWORD m_poolMaxPixelShader;	///< draws every pool as the brightest at each pixel, for HeadlightPoolClampBrightness
	IDirect3DTexture8 *m_poolTexture;	///< the pools that shader reads, three float texels each
};

extern W3DHeadlightManager *TheW3DHeadlights;
