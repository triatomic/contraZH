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

#pragma once

#include "Lib/BaseType.h"
#include "WW3D2/dx8compat.h"

class CameraClass;

class W3DDistanceFog
{
public:
	W3DDistanceFog();
	~W3DDistanceFog();
	void render(CameraClass &camera);
	void ReleaseResources();

private:
	IDirect3DTexture8 *m_sceneCopy;
	DWORD m_shader;
	Bool m_loaded;
};

extern W3DDistanceFog *TheW3DDistanceFog;
