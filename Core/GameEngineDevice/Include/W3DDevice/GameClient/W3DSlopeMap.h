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

// FILE: W3DSlopeMap.h ////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"

class RenderInfoClass;
class TextureClass;

// Slope maps of texture brightness, built on the GPU for the bumps derived from a texture without a normal map.
class W3DSlopeMap
{
public:
	/// The texture's slope map, or null until update builds it; asking queues the build. The scales turn stored texels into height per uv.
	static TextureClass *find(TextureClass *texture, Real &scaleU, Real &scaleV);

	/// Builds the slope maps asked for since, a few a frame, and drops long unused ones. Runs once a frame, outside any draw.
	static void update(RenderInfoClass &rinfo);

	/// Drops every slope map and the shader before a device reset or an asset purge; they are built again on demand.
	static void releaseResources();
};
