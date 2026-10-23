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

// FILE: W3DColorLut.h //////////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"
#include "Common/AsciiString.h"
#include "WW3D2/dx8compat.h"

class CameraClass;

// Grades a view's finished 3D scene: colour trims, a colour table, levels, then a vignette, grain and dither.
// The table is a TGA strip of N slices, each N by N, with red across a slice, green down it and blue
// from slice to slice. GameData's ColorLut keys choose and tune it, so a map.ini can carry its own look.
class W3DColorLut
{
public:
	W3DColorLut();
	~W3DColorLut();

	/// grades the bound target inside the camera's viewport, once the view's scene is drawn and before any interface
	void render(CameraClass &camera);
	void ReleaseResources();	///< drops the scene copy, table and shaders before a device reset; render recreates them

private:
	void loadTable(const AsciiString &name);

	IDirect3DTexture8 *m_sceneCopy;
	IDirect3DTexture8 *m_table;
	AsciiString m_tableName;	///< the file m_table came from, kept after a failed load so a bad file is read once per save
	Int64 m_tableStamp;				///< when that file was last saved, 0 unless it is a loose file
	UnsignedInt m_tableCheckTime;	///< when the file was last looked at for a newer save
	Int m_tableSize;					///< slices in the table, and texels along each side of one
	DWORD m_tableShader;
	DWORD m_gradeShader;			///< the grade without a table
	UnsignedInt m_frame;			///< counts renders, to move the grain and dither
	Bool m_loaded;
};

extern W3DColorLut *TheW3DColorLut;
