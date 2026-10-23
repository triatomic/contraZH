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

// FILE: PlanarMirrorShader.cpp ///////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the Game

#include "GameClient/PlanarMirrorShader.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"

static Real Pick( Real own, Real fallback )
{
	return (own >= 0.0f) ? own : fallback;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void PlanarMirrorShaderTuning::setUnset()
{
	reflectivity = -1.0f;
	tint.red = -1.0f;
	tint.green = -1.0f;
	tint.blue = -1.0f;
	distortion = -1.0f;
	frost = -1.0f;
	overrideTexture = FALSE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void PlanarMirrorShaderTuning::resolve( PlanarMirrorShaderTuning &resolved ) const
{
	const PlanarMirrorShaderTuning &defaults = TheGlobalData->m_planarMirrorTuning;

	resolved.reflectivity = Pick( reflectivity, defaults.reflectivity );
	resolved.tint = (tint.red >= 0.0f) ? tint : defaults.tint;
	resolved.distortion = Pick( distortion, defaults.distortion );
	resolved.frost = Pick( frost, defaults.frost );
	resolved.overrideTexture = overrideTexture;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
const FieldParse *PlanarMirrorShaderTuning::getFieldParse()
{
	static const FieldParse planarMirrorFieldParse[] =
	{
		{ "PlanarMirrorReflectivity",	INI::parseReal,				nullptr,	offsetof( PlanarMirrorShaderTuning, reflectivity ) },
		{ "PlanarMirrorTint",					INI::parseRGBColor,		nullptr,	offsetof( PlanarMirrorShaderTuning, tint ) },
		{ "PlanarMirrorDistortion",		INI::parseReal,				nullptr,	offsetof( PlanarMirrorShaderTuning, distortion ) },
		{ "PlanarMirrorFrost",				INI::parseReal,				nullptr,	offsetof( PlanarMirrorShaderTuning, frost ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return planarMirrorFieldParse;
}
