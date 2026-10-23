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

// FILE: W3DDisruption.h ////////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"
#include "Common/GameType.h"
#include "WW3D2/dx8compat.h"

class RenderInfoClass;
struct DisruptionShaderInfo;

// Discs on the ground that ripple the scene and pull its colours apart, for FXList's Disruption.
class W3DDisruptionManager
{
public:
	W3DDisruptionManager();
	~W3DDisruptionManager();

	/// radius is in world units, and fade the share of the duration spent fading in and again fading out
	/// the settings must outlive the field
	void add(const Coord3D &position, Real radius, const DisruptionShaderInfo *info, UnsignedInt durationMs, Real fade);
	Bool update();	///< drops the fields that ran out, and tells whether any are left
	/// draws the live fields, inside the soft particle hook's disruption pass
	void render(RenderInfoClass &rinfo);
	void ReleaseResources();	///< drops the mask texture before a device reset; render makes it again

private:
	enum { MAX_FIELDS = 32 };

	struct Field
	{
		Coord3D position;
		Real radius;
		Real fade;
		const DisruptionShaderInfo *info;
		UnsignedInt startMs;
		UnsignedInt durationMs;
	};

	void createMask();

	Field m_fields[MAX_FIELDS];
	Int m_count;
	IDirect3DTexture8 *m_mask;	///< a disc that fades from its middle to its edge
};

extern W3DDisruptionManager *TheW3DDisruption;
