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

// FILE: StormShader.h ////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"

struct FieldParse;

// A storm's settings. In an entry of its own a negative value, the default, takes GameData.ini's for the storm's type.
struct StormShaderInfo
{
	enum Type CPP_11(: Int)
	{
		TYPE_SAND,
		TYPE_SNOW,

		TYPE_COUNT
	};

	Int type;
	Real radius;						///< world units from the storm's middle to its edge
	Real height;						///< world units the storm stands above the ground
	Real edgeFade;					///< share of the radius over which the storm thins out to nothing
	Real fadeFrames;				///< logic frames the storm takes to build up and again to die down
	RGBColor hazeColor;
	Real hazeDensity;				///< how much light 100 world units of haze block, as optical depth
	Real hazeMaxOpacity;		///< the most the haze may hide, 0 to 1
	Real hazeNoiseSize;			///< world units across one tile of haze noise
	Real gusts;							///< how unevenly the haze and grains bunch up, 0 to 1
	Real windAngle;					///< direction the wind blows towards, in radians; UNSET_ANGLE or less is unset
	Real windSpeed;					///< world units a second
	Real fallSpeed;					///< world units a second the grains sink
	Real turbulence;				///< world units the grains swirl off their path
	RGBColor grainColor;
	Int grainCount;					///< grains in view at once
	Real grainSize;					///< world units across a grain
	Real grainStreak;				///< seconds of travel a grain smears along
	Real grainOpacity;			///< 0 to 1

	static const Real UNSET_ANGLE;

	StormShaderInfo();

	/// The settings a storm type starts with, before GameData.ini changes them.
	void setTypeDefaults( Type stormType );

	/// GameData.ini's settings for this storm's type, with this entry's own on top.
	void resolve( StormShaderInfo &resolved ) const;

	/// Type and the setting keys, stored relative to a StormShaderInfo.
	static const FieldParse *getFieldParse();
};

static const char *const StormShaderTypeNames[] =
{
	"SAND", "SNOW", nullptr
};
static_assert(ARRAY_SIZE(StormShaderTypeNames) == StormShaderInfo::TYPE_COUNT + 1, "Incorrect array size");
