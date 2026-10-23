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

// FILE: StormShader.cpp //////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the Game

#include "GameClient/StormShader.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"

const Real StormShaderInfo::UNSET_ANGLE = -1.0e6f;

// What each storm type starts with, before GameData.ini changes it.
struct StormDefaults
{
	Real radius;
	Real height;
	Real edgeFade;
	Real fadeFrames;
	RGBColor hazeColor;
	Real hazeDensity;
	Real hazeMaxOpacity;
	Real hazeNoiseSize;
	Real gusts;
	Real windSpeed;
	Real fallSpeed;
	Real turbulence;
	RGBColor grainColor;
	Int grainCount;
	Real grainSize;
	Real grainStreak;
	Real grainOpacity;
};

static const StormDefaults TheStormDefaults[StormShaderInfo::TYPE_COUNT] =
{
	// sand: a dense tan wall driven flat by the wind
	{ 300.0f, 120.0f, 0.35f, 3.0f * LOGICFRAMES_PER_SECONDS_REAL, { 0.76f, 0.62f, 0.42f }, 0.8f, 0.8f, 300.0f, 0.7f,
		140.0f, 4.0f, 5.0f, { 0.86f, 0.74f, 0.55f }, 6000, 1.0f, 0.06f, 0.6f },
	// snow: a thin white veil of flakes that sink as they drift
	{ 300.0f, 150.0f, 0.35f, 3.0f * LOGICFRAMES_PER_SECONDS_REAL, { 0.85f, 0.88f, 0.92f }, 0.35f, 0.55f, 350.0f, 0.5f,
		45.0f, 30.0f, 9.0f, { 1.0f, 1.0f, 1.0f }, 5000, 1.8f, 0.02f, 0.85f },
};

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
StormShaderInfo::StormShaderInfo() :
	type(TYPE_SAND),
	radius(-1.0f),
	height(-1.0f),
	edgeFade(-1.0f),
	fadeFrames(-1.0f),
	hazeDensity(-1.0f),
	hazeMaxOpacity(-1.0f),
	hazeNoiseSize(-1.0f),
	gusts(-1.0f),
	windAngle(2.0f * UNSET_ANGLE),
	windSpeed(-1.0f),
	fallSpeed(-1.0f),
	turbulence(-1.0f),
	grainCount(-1),
	grainSize(-1.0f),
	grainStreak(-1.0f),
	grainOpacity(-1.0f)
{
	hazeColor.red = hazeColor.green = hazeColor.blue = -1.0f;
	grainColor.red = grainColor.green = grainColor.blue = -1.0f;
}

static Real Pick( Real own, Real fallback )
{
	return (own >= 0.0f) ? own : fallback;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void StormShaderInfo::setTypeDefaults( Type stormType )
{
	const StormDefaults &defaults = TheStormDefaults[stormType];

	type = stormType;
	radius = defaults.radius;
	height = defaults.height;
	edgeFade = defaults.edgeFade;
	fadeFrames = defaults.fadeFrames;
	hazeColor = defaults.hazeColor;
	hazeDensity = defaults.hazeDensity;
	hazeMaxOpacity = defaults.hazeMaxOpacity;
	hazeNoiseSize = defaults.hazeNoiseSize;
	gusts = defaults.gusts;
	windAngle = 0.0f;
	windSpeed = defaults.windSpeed;
	fallSpeed = defaults.fallSpeed;
	turbulence = defaults.turbulence;
	grainColor = defaults.grainColor;
	grainCount = defaults.grainCount;
	grainSize = defaults.grainSize;
	grainStreak = defaults.grainStreak;
	grainOpacity = defaults.grainOpacity;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void StormShaderInfo::resolve( StormShaderInfo &resolved ) const
{
	const Int known = (type >= 0 && type < TYPE_COUNT) ? type : TYPE_SAND;
	const StormShaderInfo &defaults = TheGlobalData->m_stormTuning[known];

	resolved.type = known;
	resolved.radius = Pick( radius, defaults.radius );
	resolved.height = Pick( height, defaults.height );
	resolved.edgeFade = Pick( edgeFade, defaults.edgeFade );
	resolved.fadeFrames = Pick( fadeFrames, defaults.fadeFrames );
	resolved.hazeColor = (hazeColor.red >= 0.0f) ? hazeColor : defaults.hazeColor;
	resolved.hazeDensity = Pick( hazeDensity, defaults.hazeDensity );
	resolved.hazeMaxOpacity = Pick( hazeMaxOpacity, defaults.hazeMaxOpacity );
	resolved.hazeNoiseSize = Pick( hazeNoiseSize, defaults.hazeNoiseSize );
	resolved.gusts = Pick( gusts, defaults.gusts );
	resolved.windAngle = (windAngle > UNSET_ANGLE) ? windAngle : defaults.windAngle;
	resolved.windSpeed = Pick( windSpeed, defaults.windSpeed );
	resolved.fallSpeed = Pick( fallSpeed, defaults.fallSpeed );
	resolved.turbulence = Pick( turbulence, defaults.turbulence );
	resolved.grainColor = (grainColor.red >= 0.0f) ? grainColor : defaults.grainColor;
	resolved.grainCount = (grainCount >= 0) ? grainCount : defaults.grainCount;
	resolved.grainSize = Pick( grainSize, defaults.grainSize );
	resolved.grainStreak = Pick( grainStreak, defaults.grainStreak );
	resolved.grainOpacity = Pick( grainOpacity, defaults.grainOpacity );
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
const FieldParse *StormShaderInfo::getFieldParse()
{
	static const FieldParse stormFieldParse[] =
	{
		{ "Type",							INI::parseIndexList,			StormShaderTypeNames,	offsetof( StormShaderInfo, type ) },
		{ "Radius",						INI::parseReal,						nullptr,							offsetof( StormShaderInfo, radius ) },
		{ "Height",						INI::parseReal,						nullptr,							offsetof( StormShaderInfo, height ) },
		{ "EdgeFade",					INI::parsePercentToReal,	nullptr,							offsetof( StormShaderInfo, edgeFade ) },
		{ "FadeTime",					INI::parseDurationReal,		nullptr,							offsetof( StormShaderInfo, fadeFrames ) },
		{ "HazeColor",				INI::parseRGBColor,				nullptr,							offsetof( StormShaderInfo, hazeColor ) },
		{ "HazeDensity",			INI::parseReal,						nullptr,							offsetof( StormShaderInfo, hazeDensity ) },
		{ "HazeMaxOpacity",		INI::parsePercentToReal,	nullptr,							offsetof( StormShaderInfo, hazeMaxOpacity ) },
		{ "HazeNoiseSize",		INI::parseReal,						nullptr,							offsetof( StormShaderInfo, hazeNoiseSize ) },
		{ "Gusts",						INI::parsePercentToReal,	nullptr,							offsetof( StormShaderInfo, gusts ) },
		{ "WindAngle",				INI::parseAngleReal,			nullptr,							offsetof( StormShaderInfo, windAngle ) },
		{ "WindSpeed",				INI::parseReal,						nullptr,							offsetof( StormShaderInfo, windSpeed ) },
		{ "FallSpeed",				INI::parseReal,						nullptr,							offsetof( StormShaderInfo, fallSpeed ) },
		{ "Turbulence",				INI::parseReal,						nullptr,							offsetof( StormShaderInfo, turbulence ) },
		{ "GrainColor",				INI::parseRGBColor,				nullptr,							offsetof( StormShaderInfo, grainColor ) },
		{ "GrainCount",				INI::parseInt,						nullptr,							offsetof( StormShaderInfo, grainCount ) },
		{ "GrainSize",				INI::parseReal,						nullptr,							offsetof( StormShaderInfo, grainSize ) },
		{ "GrainStreak",			INI::parseReal,						nullptr,							offsetof( StormShaderInfo, grainStreak ) },
		{ "GrainOpacity",			INI::parsePercentToReal,	nullptr,							offsetof( StormShaderInfo, grainOpacity ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return stormFieldParse;
}
