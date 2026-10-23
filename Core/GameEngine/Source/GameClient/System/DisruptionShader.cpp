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

// FILE: DisruptionShader.cpp /////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the Game

#include "GameClient/DisruptionShader.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
DisruptionShaderInfo::DisruptionShaderInfo( Shape ringShape ) :
	mode(MODE_NO),
	shape(ringShape)
{
	Real *setting = &tuning.ringStrength;
	for (UnsignedInt i = 0; i < sizeof( tuning ) / sizeof( Real ); ++i)
	{
		setting[i] = -1.0f;
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void DisruptionShaderInfo::resolveTuning( DisruptionShaderTuning &resolved ) const
{
	resolved = TheGlobalData->m_disruptionTuning;

	const Real *own = &tuning.ringStrength;
	Real *setting = &resolved.ringStrength;
	for (UnsignedInt i = 0; i < sizeof( tuning ) / sizeof( Real ); ++i)
	{
		if (own[i] >= 0.0f)
		{
			setting[i] = own[i];
		}
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
const FieldParse *DisruptionShaderInfo::getFieldParse()
{
	static const FieldParse disruptionFieldParse[] =
	{
		{ "DisruptionShader",					INI::parseIndexList,	DisruptionShaderModeNames,	offsetof( DisruptionShaderInfo, mode ) },
		{ "DisruptionRingStrength",		INI::parseReal,				nullptr,										offsetof( DisruptionShaderInfo, tuning.ringStrength ) },
		{ "DisruptionRingSize",				INI::parseReal,				nullptr,										offsetof( DisruptionShaderInfo, tuning.ringSize ) },
		{ "DisruptionRingSpeed",			INI::parseReal,				nullptr,										offsetof( DisruptionShaderInfo, tuning.ringSpeed ) },
		{ "DisruptionWobble",					INI::parseReal,				nullptr,										offsetof( DisruptionShaderInfo, tuning.wobble ) },
		{ "DisruptionWobbleSize",			INI::parseReal,				nullptr,										offsetof( DisruptionShaderInfo, tuning.wobbleSize ) },
		{ "DisruptionWobbleSpeed",		INI::parseReal,				nullptr,										offsetof( DisruptionShaderInfo, tuning.wobbleSpeed ) },
		{ "DisruptionGlitch",					INI::parseReal,				nullptr,										offsetof( DisruptionShaderInfo, tuning.glitch ) },
		{ "DisruptionGlitchSize",			INI::parseReal,				nullptr,										offsetof( DisruptionShaderInfo, tuning.glitchSize ) },
		{ "DisruptionGlitchRate",			INI::parseReal,				nullptr,										offsetof( DisruptionShaderInfo, tuning.glitchRate ) },
		{ "DisruptionChroma",					INI::parseReal,				nullptr,										offsetof( DisruptionShaderInfo, tuning.chroma ) },
		{ "DisruptionChromaSpread",		INI::parseReal,				nullptr,										offsetof( DisruptionShaderInfo, tuning.chromaSpread ) },
		{ "DisruptionMask",						INI::parseReal,				nullptr,										offsetof( DisruptionShaderInfo, tuning.mask ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return disruptionFieldParse;
}
