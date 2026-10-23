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

// FILE: PlanarMirrorShader.h /////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"

struct FieldParse;

// The PlanarMirror look keys of GameData and of a model's draw module. In a draw module a key left out is unset and
// takes GameData.ini's value.
struct PlanarMirrorShaderTuning
{
	Real reflectivity;	///< share of the reflection seen head on, rising to all of it at grazing angles
	RGBColor tint;			///< multiplies what the mirror shows
	Real distortion;		///< how far, in screen widths, the texture's normal map bends the reflection and what shows through
	Real frost;					///< 0 clear to 1 frosted: blurs the reflection and what shows through, and clouds the latter
	Bool overrideTexture;	///< draws the mesh as clear glass in place of its texture; a draw module's own, never GameData's

	void setUnset();	///< what a draw module starts with
	void resolve( PlanarMirrorShaderTuning &resolved ) const;	///< the settings with GameData.ini's where this one sets none

	/// The look keys, stored relative to a PlanarMirrorShaderTuning.
	static const FieldParse *getFieldParse();

	void setDefaults()
	{
		reflectivity = 0.35f;
		tint.red = 1.0f;
		tint.green = 1.0f;
		tint.blue = 1.0f;
		distortion = 0.01f;
		frost = 0.0f;
		overrideTexture = FALSE;
	}
};
