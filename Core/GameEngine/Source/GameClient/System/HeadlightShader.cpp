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

// FILE: HeadlightShader.cpp //////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the Game

#include "GameClient/HeadlightShader.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"

// A pitch may be negative, so an unset one lies far below any angle.
static const Real UNSET_ANGLE = -1.0e6f;

static Real Pick( Real own, Real fallback )
{
	return (own >= 0.0f) ? own : fallback;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void HeadlightShaderTuning::setUnset()
{
	enabled = TRUE;
	color.red = -1.0f;
	color.green = -1.0f;
	color.blue = -1.0f;
	beamIntensity = -1.0f;
	beamLength = -1.0f;
	beamWidth = -1.0f;
	beamFalloff = -1.0f;
	beamSoftness = -1.0f;
	poolIntensity = -1.0f;
	poolRange = -1.0f;
	poolAngle = -1.0f;
	poolPitch = UNSET_ANGLE;
	poolFalloff = -1.0f;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void HeadlightShaderTuning::resolve( HeadlightShaderTuning &resolved ) const
{
	const HeadlightShaderTuning &defaults = TheGlobalData->m_headlightTuning;

	resolved.enabled = enabled && defaults.enabled;
	resolved.color = (color.red >= 0.0f) ? color : defaults.color;
	resolved.beamIntensity = Pick( beamIntensity, defaults.beamIntensity );
	resolved.beamLength = Pick( beamLength, defaults.beamLength );
	resolved.beamWidth = Pick( beamWidth, defaults.beamWidth );
	resolved.beamFalloff = Pick( beamFalloff, defaults.beamFalloff );
	resolved.beamSoftness = Pick( beamSoftness, defaults.beamSoftness );
	resolved.poolIntensity = Pick( poolIntensity, defaults.poolIntensity );
	resolved.poolRange = Pick( poolRange, defaults.poolRange );
	resolved.poolAngle = Pick( poolAngle, defaults.poolAngle );
	resolved.poolPitch = (poolPitch > UNSET_ANGLE * 0.5f) ? poolPitch : defaults.poolPitch;
	resolved.poolFalloff = Pick( poolFalloff, defaults.poolFalloff );
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
const FieldParse *HeadlightShaderTuning::getFieldParse()
{
	static const FieldParse headlightFieldParse[] =
	{
		{ "HeadlightShader",					INI::parseBool,				nullptr,	offsetof( HeadlightShaderTuning, enabled ) },
		{ "HeadlightColor",						INI::parseRGBColor,		nullptr,	offsetof( HeadlightShaderTuning, color ) },
		{ "HeadlightBeamIntensity",		INI::parseReal,				nullptr,	offsetof( HeadlightShaderTuning, beamIntensity ) },
		{ "HeadlightBeamLength",			INI::parseReal,				nullptr,	offsetof( HeadlightShaderTuning, beamLength ) },
		{ "HeadlightBeamWidth",				INI::parseReal,				nullptr,	offsetof( HeadlightShaderTuning, beamWidth ) },
		{ "HeadlightBeamFalloff",			INI::parseReal,				nullptr,	offsetof( HeadlightShaderTuning, beamFalloff ) },
		{ "HeadlightBeamSoftness",		INI::parseReal,				nullptr,	offsetof( HeadlightShaderTuning, beamSoftness ) },
		{ "HeadlightPoolIntensity",		INI::parseReal,				nullptr,	offsetof( HeadlightShaderTuning, poolIntensity ) },
		{ "HeadlightPoolRange",				INI::parseReal,				nullptr,	offsetof( HeadlightShaderTuning, poolRange ) },
		{ "HeadlightPoolAngle",				INI::parseAngleReal,	nullptr,	offsetof( HeadlightShaderTuning, poolAngle ) },
		{ "HeadlightPoolPitch",				INI::parseAngleReal,	nullptr,	offsetof( HeadlightShaderTuning, poolPitch ) },
		{ "HeadlightPoolFalloff",			INI::parseReal,				nullptr,	offsetof( HeadlightShaderTuning, poolFalloff ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return headlightFieldParse;
}
