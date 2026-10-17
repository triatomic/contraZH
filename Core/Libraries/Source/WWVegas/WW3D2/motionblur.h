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

#pragma once

#include "WWLib/always.h"
#include "WWMath/matrix3d.h"
#include "WWMath/vector3.h"

class RenderObjClass;

// Earlier poses of fast spinning parts and fast movers. The mesh renderer queues them as
// translucent copies of the opaque mesh, which the sorting renderer draws after the opaque scene.
class MotionBlurClass
{
public:
	enum { MAX_GHOSTS = 32 };

	// Ghost i of Count sits at Translate(-Offset * (i+1) / Count) * mesh transform * Spin[i], oldest last.
	struct GhostsStruct
	{
		int Count;
		float Opacity;
		Vector3 Offset;
		bool Spins;
		Matrix3D Spin[MAX_GHOSTS];
	};

	// Only the D3D9 device blends by a constant factor, which keeps the mesh's own alpha free for its alpha test.
	static bool Is_Supported();

	// Ghosts for the mesh, to fill for the next render of the main scene, or null when the pool is full.
	static GhostsStruct * Add(RenderObjClass * mesh);

	// The mesh's ghosts, handed out only while the main scene's draws run.
	static void Set_Armed(bool armed);
	static bool Is_Armed();
	static const GhostsStruct * Peek(const RenderObjClass * mesh);

	// Debug draws every ghost in a flat colour, cyan for a spin and magenta for a move, or 0 when off.
	static void Set_Debug(bool debug);
	static unsigned Get_Debug_Colour(const GhostsStruct & ghosts);

	// Drops every ghost added before this point.
	static void End_Frame();

	static void Get_Transform(const GhostsStruct & ghosts, int index, const Matrix3D & world, Matrix3D * result);
	static float Get_Alpha(const GhostsStruct & ghosts, int index);
};
