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

// FILE: HeadlightShader.h ////////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"

struct FieldParse;

// The Headlight keys of GameData and of a model's draw module. The beam is the cone in the air, the pool the light
// it throws on the scene. In a draw module a key left out is unset and takes GameData.ini's value.
struct HeadlightShaderTuning
{
	Bool enabled;						///< off keeps the headlight meshes, of every model in GameData and of one in its module
	RGBColor color;
	Real beamIntensity;
	Real beamLength;				///< times the headlight mesh's length
	Real beamWidth;					///< times the headlight mesh's width
	Real beamFalloff;				///< how fast the beam dims along its length
	Real beamSoftness;			///< world units over which the beam fades into what it touches
	Real poolIntensity;
	Real poolRange;					///< times the headlight mesh's length
	Real poolAngle;					///< radians from the light's middle to its edge
	Real poolPitch;					///< radians the light tilts down from the mesh
	Real poolFalloff;				///< how fast the light dims with distance
	Bool poolClampBrightness;	///< overlapping pools light no more than the brightest one; GameData only
	Bool perConeAim;				///< each cone of a HEADLIGHT mesh takes its own direction; GameData only

	void setUnset();	///< what a draw module starts with
	void resolve( HeadlightShaderTuning &resolved ) const;	///< the settings with GameData.ini's where this one sets none

	/// HeadlightShader and the setting keys, stored relative to a HeadlightShaderTuning.
	static const FieldParse *getFieldParse();

	void setDefaults()
	{
		enabled = TRUE;
		color.red = 1.0f;
		color.green = 0.95f;
		color.blue = 0.82f;
		beamIntensity = 0.35f;
		beamLength = 1.0f;
		beamWidth = 1.0f;
		beamFalloff = 1.5f;
		beamSoftness = 8.0f;
		poolIntensity = 0.8f;
		poolRange = 2.5f;
		poolAngle = 0.45f;
		poolPitch = 0.2f;
		poolFalloff = 1.5f;
		poolClampBrightness = TRUE;
		perConeAim = FALSE;
	}
};
