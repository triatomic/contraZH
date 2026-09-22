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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/formconv.h                             $*
 *                                                                                             *
 *              Original Author:: Nathaniel Hoffman                                            *
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 06/27/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 3                                                           $*
 *                                                                                             *
 * 06/27/02 KM Z Format support																						*
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "ww3dformat.h"
#include "dx8compat.h"

/*
** This file is used for conversions between D3DFORMAT and WW3DFormat.
*/

D3DFORMAT WW3DFormat_To_D3DFormat(WW3DFormat ww3d_format);
WW3DFormat D3DFormat_To_WW3DFormat(D3DFORMAT d3d_format);

// Inverts in place of D3DXMatrixInverse, writing identity when singular
HRESULT Filter_Texture_Mipmaps(IDirect3DTexture8* texture);
HRESULT Load_Surface_From_Surface(IDirect3DSurface8* dest_surface, const RECT* dest_rect,
	IDirect3DSurface8* src_surface, const RECT* src_rect);

WW3DFormat Get_Closest_Supported_Texture_Format(WW3DFormat format, bool render_target);

const char* Get_D3D_Error_Name(unsigned res);

void Invert_D3DMATRIX(D3DMATRIX& out, float* det_out, const D3DMATRIX& m);

D3DMATRIX operator*(const D3DMATRIX& a, const D3DMATRIX& b);
D3DMATRIX& operator*=(D3DMATRIX& a, const D3DMATRIX& b);
void Set_D3DMATRIX_Identity(D3DMATRIX& out);
void Set_D3DMATRIX_Scaling(D3DMATRIX& out, float x, float y, float z);
void Set_D3DMATRIX_Translation(D3DMATRIX& out, float x, float y, float z);
void Transpose_D3DMATRIX(D3DMATRIX& out, const D3DMATRIX& m);

D3DFORMAT WW3DZFormat_To_D3DFormat(WW3DZFormat ww3d_zformat);
WW3DZFormat D3DFormat_To_WW3DZFormat(D3DFORMAT d3d_format);

void Init_D3D_To_WW3_Conversion();
