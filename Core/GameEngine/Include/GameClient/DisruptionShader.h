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

// FILE: DisruptionShader.h ///////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"

struct FieldParse;

// Disruption shading settings. In an entry of its own a negative value, the default, takes GameData.ini's.
struct DisruptionShaderTuning
{
	Real ringStrength;		///< world units the rings push the scene
	Real ringSize;				///< world units from one ring to the next
	Real ringSpeed;				///< world units a second the rings travel outwards
	Real wobble;					///< world units the noise pushes the scene
	Real wobbleSize;			///< world units across one tile of wobble noise
	Real wobbleSpeed;			///< noise tiles the wobble crosses per second
	Real glitch;					///< world units the glitch bands jump sideways
	Real glitchSize;			///< height of a glitch band, in pixels on a screen 1080 lines tall
	Real glitchRate;			///< glitch jumps per second, 0 freezes the bands
	Real chroma;					///< how much further red bends than green, and blue less, as a fraction
	Real chromaSpread;		///< world units red and blue part even where nothing bends
	Real mask;						///< how quickly the shape's brightness reaches full strength
};

// What one draw hands the disruption shader: the settings of its INI entry and where its rings start.
struct DisruptionShaderInfo
{
	enum Mode CPP_11(: Int)
	{
		MODE_NO,
		MODE_YES,		///< the shape bends the scene behind it and draws over that
		MODE_ONLY,	///< the shape bends the scene and leaves its own art undrawn

		MODE_COUNT
	};

	enum Shape CPP_11(: Int)
	{
		SHAPE_SPRITE,	///< rings spread from the middle of the texture
		SHAPE_CENTER,	///< rings spread from the draw's world origin
		SHAPE_BEAM		///< rings run along a beam
	};

	Int mode;
	Int shape;
	DisruptionShaderTuning tuning;

	DisruptionShaderInfo( Shape ringShape );

	Bool isOn() const { return mode != MODE_NO; }
	Bool hidesArt() const { return mode == MODE_ONLY; }

	/// GameData.ini's settings, with this entry's own on top.
	void resolveTuning( DisruptionShaderTuning &resolved ) const;

	/// DisruptionShader and the setting keys, stored relative to a DisruptionShaderInfo.
	static const FieldParse *getFieldParse();
};

static const char *const DisruptionShaderModeNames[] =
{
	"No", "Yes", "Only", nullptr
};
static_assert(ARRAY_SIZE(DisruptionShaderModeNames) == DisruptionShaderInfo::MODE_COUNT + 1, "Incorrect array size");
