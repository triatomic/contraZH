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

// FILE: W3DGroundNoise.h /////////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"

class TextureClass;

// The ground's light and colour variation from the GameData GroundNoise keys, read through groundnoise.hlsli.
class W3DGroundNoise
{
public:
	/// The noise for the current GameData settings, rebuilt when they change, or null when it cannot be made.
	static TextureClass *getTexture();

	/// A white texel for the cloud map's stage when the clouds are off, so the noise keeps its stage.
	static TextureClass *getWhiteTexture();

	/// Points the stage's texcoords at the noise, from the world position, and sets its sampling. The caller binds the texture.
	static void setupStage(Int stage);

	/// Drops the textures before a device reset; the next draw makes them again.
	static void releaseResources();
};
