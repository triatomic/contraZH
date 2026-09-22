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
 *                     $Archive:: /Commando/Code/ww3d2/formconv.cpp                           $*
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
#include "formconv.h"
#include "WWMath/matrix4.h"
#include "dx8wrapper.h"
#include "dx8caps.h"

D3DFORMAT WW3DFormatToD3DFormatConversionArray[WW3D_FORMAT_COUNT] = {
	D3DFMT_UNKNOWN,
	D3DFMT_R8G8B8,
	D3DFMT_A8R8G8B8,
	D3DFMT_X8R8G8B8,
	D3DFMT_R5G6B5,
	D3DFMT_X1R5G5B5,
	D3DFMT_A1R5G5B5,
	D3DFMT_A4R4G4B4,
	D3DFMT_R3G3B2,
	D3DFMT_A8,
	D3DFMT_A8R3G3B2,
	D3DFMT_X4R4G4B4,
	D3DFMT_A8P8,
	D3DFMT_P8,
	D3DFMT_L8,
	D3DFMT_A8L8,
	D3DFMT_A4L4,
	D3DFMT_V8U8,		// Bumpmap
	D3DFMT_L6V5U5,		// Bumpmap
	D3DFMT_X8L8V8U8,	// Bumpmap
	D3DFMT_DXT1,
	D3DFMT_DXT2,
	D3DFMT_DXT3,
	D3DFMT_DXT4,
	D3DFMT_DXT5
};

// adding depth stencil format conversion
D3DFORMAT WW3DZFormatToD3DFormatConversionArray[WW3D_ZFORMAT_COUNT] =
{
	D3DFMT_UNKNOWN,
	D3DFMT_D16_LOCKABLE, // 16-bit z-buffer bit depth. This is an application-lockable surface format.
	D3DFMT_D32, // 32-bit z-buffer bit depth.
	D3DFMT_D15S1, // 16-bit z-buffer bit depth where 15 bits are reserved for the depth channel and 1 bit is reserved for the stencil channel.
	D3DFMT_D24S8, // 32-bit z-buffer bit depth using 24 bits for the depth channel and 8 bits for the stencil channel.
	D3DFMT_D16, // 16-bit z-buffer bit depth.
	D3DFMT_D24X8, // 32-bit z-buffer bit depth using 24 bits for the depth channel.
	D3DFMT_D24X4S4, // 32-bit z-buffer bit depth using 24 bits for the depth channel and 4 bits for the stencil channel.
};


/*
#define HIGHEST_SUPPORTED_D3DFORMAT D3DFMT_X8L8V8U8	//A4L4
WW3DFormat D3DFormatToWW3DFormatConversionArray[HIGHEST_SUPPORTED_D3DFORMAT + 1] = {
	WW3D_FORMAT_UNKNOWN,		// 0
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_R8G8B8,		// 20
	WW3D_FORMAT_A8R8G8B8,
	WW3D_FORMAT_X8R8G8B8,
	WW3D_FORMAT_R5G6B5,
	WW3D_FORMAT_X1R5G5B5,
	WW3D_FORMAT_A1R5G5B5,
	WW3D_FORMAT_A4R4G4B4,
	WW3D_FORMAT_R3G3B2,
	WW3D_FORMAT_A8,
	WW3D_FORMAT_A8R3G3B2,
	WW3D_FORMAT_X4R4G4B4,	// 30
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_A8P8,			// 40
	WW3D_FORMAT_P8,
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_L8,			// 50
	WW3D_FORMAT_A8L8,
	WW3D_FORMAT_A4L4
};
*/

#define HIGHEST_SUPPORTED_D3DFORMAT D3DFMT_X8L8V8U8
#define HIGHEST_SUPPORTED_D3DZFORMAT D3DFMT_D16
WW3DFormat D3DFormatToWW3DFormatConversionArray[HIGHEST_SUPPORTED_D3DFORMAT + 1];
WW3DZFormat D3DFormatToWW3DZFormatConversionArray[HIGHEST_SUPPORTED_D3DZFORMAT + 1];

D3DFORMAT WW3DFormat_To_D3DFormat(WW3DFormat ww3d_format) {
	if (ww3d_format >= WW3D_FORMAT_COUNT) {
		return D3DFMT_UNKNOWN;
	} else {
		return WW3DFormatToD3DFormatConversionArray[(unsigned int)ww3d_format];
	}
}

WW3DFormat D3DFormat_To_WW3DFormat(D3DFORMAT d3d_format)
{
	switch (d3d_format) {
	// The DXT-codes are created with FOURCC macro and thus can't be placed in the conversion table
	case D3DFMT_DXT1: return WW3D_FORMAT_DXT1;
	case D3DFMT_DXT2: return WW3D_FORMAT_DXT2;
	case D3DFMT_DXT3: return WW3D_FORMAT_DXT3;
	case D3DFMT_DXT4: return WW3D_FORMAT_DXT4;
	case D3DFMT_DXT5: return WW3D_FORMAT_DXT5;
	default:
		if (d3d_format > HIGHEST_SUPPORTED_D3DFORMAT) {
			return WW3D_FORMAT_UNKNOWN;
		} else {
			return D3DFormatToWW3DFormatConversionArray[(unsigned int)d3d_format];
		}
		break;
	}
}

//**********************************************************************************************
//! Depth Stencil W3D to D3D format conversion
/*! KJM
*/
D3DFORMAT WW3DZFormat_To_D3DFormat(WW3DZFormat ww3d_zformat)
{
	if (ww3d_zformat >= WW3D_ZFORMAT_COUNT)
	{
		return D3DFMT_UNKNOWN;
	}
	else
	{
		return WW3DZFormatToD3DFormatConversionArray[(unsigned int)ww3d_zformat];
	}
}

//**********************************************************************************************
//! D3D to Depth Stencil W3D format conversion
/*! KJM
*/
WW3DZFormat D3DFormat_To_WW3DZFormat(D3DFORMAT d3d_format)
{
	if (d3d_format>HIGHEST_SUPPORTED_D3DZFORMAT)
	{
		return WW3D_ZFORMAT_UNKNOWN;
	}
	else
	{
		return D3DFormatToWW3DZFormatConversionArray[(unsigned int)d3d_format];
	}
}

//**********************************************************************************************
//! Init format conversion tables
/*!
 * 06/27/02 KM Z Format support																						*
*/
void Init_D3D_To_WW3_Conversion()
{
	int i=0;
	for (;i<HIGHEST_SUPPORTED_D3DFORMAT;++i) {
		D3DFormatToWW3DFormatConversionArray[i]=WW3D_FORMAT_UNKNOWN;
	}

	D3DFormatToWW3DFormatConversionArray[D3DFMT_R8G8B8]=WW3D_FORMAT_R8G8B8;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_A8R8G8B8]=WW3D_FORMAT_A8R8G8B8;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_X8R8G8B8]=WW3D_FORMAT_X8R8G8B8;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_R5G6B5]=WW3D_FORMAT_R5G6B5;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_X1R5G5B5]=WW3D_FORMAT_X1R5G5B5;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_A1R5G5B5]=WW3D_FORMAT_A1R5G5B5;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_A4R4G4B4]=WW3D_FORMAT_A4R4G4B4;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_R3G3B2]=WW3D_FORMAT_R3G3B2;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_A8]=WW3D_FORMAT_A8;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_A8R3G3B2]=WW3D_FORMAT_A8R3G3B2;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_X4R4G4B4]=WW3D_FORMAT_X4R4G4B4;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_A8P8]=WW3D_FORMAT_A8P8;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_P8]=WW3D_FORMAT_P8;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_L8]=WW3D_FORMAT_L8;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_A8L8]=WW3D_FORMAT_A8L8;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_A4L4]=WW3D_FORMAT_A4L4;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_V8U8]=WW3D_FORMAT_U8V8;				// Bumpmap
	D3DFormatToWW3DFormatConversionArray[D3DFMT_L6V5U5]=WW3D_FORMAT_L6V5U5;		// Bumpmap
	D3DFormatToWW3DFormatConversionArray[D3DFMT_X8L8V8U8]=WW3D_FORMAT_X8L8V8U8;	// Bumpmap

	// init depth stencil conversion
	for (i=0; i<HIGHEST_SUPPORTED_D3DZFORMAT; i++)
	{
		D3DFormatToWW3DZFormatConversionArray[i]=WW3D_ZFORMAT_UNKNOWN;
	}

	D3DFormatToWW3DZFormatConversionArray[D3DFMT_D16_LOCKABLE]=WW3D_ZFORMAT_D16_LOCKABLE;
	D3DFormatToWW3DZFormatConversionArray[D3DFMT_D32]=WW3D_ZFORMAT_D32;
	D3DFormatToWW3DZFormatConversionArray[D3DFMT_D15S1]=WW3D_ZFORMAT_D15S1;
	D3DFormatToWW3DZFormatConversionArray[D3DFMT_D24S8]=WW3D_ZFORMAT_D24S8;
	D3DFormatToWW3DZFormatConversionArray[D3DFMT_D16]=WW3D_ZFORMAT_D16;
	D3DFormatToWW3DZFormatConversionArray[D3DFMT_D24X8]=WW3D_ZFORMAT_D24X8;
	D3DFormatToWW3DZFormatConversionArray[D3DFMT_D24X4S4]=WW3D_ZFORMAT_D24X4S4;
};

#if defined(BUILD_WITH_D3D9)
unsigned Surface_Size(const D3DSURFACE_DESC& desc)
{
	// Block compressed formats round up to whole 4x4 blocks
	switch (desc.Format)
	{
	case D3DFMT_DXT1:
		return ((desc.Width+3)/4)*((desc.Height+3)/4)*8;
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
		return ((desc.Width+3)/4)*((desc.Height+3)/4)*16;
	default:
		break;
	}

	const WW3DFormat format=D3DFormat_To_WW3DFormat(desc.Format);
	if (format==WW3D_FORMAT_UNKNOWN)
	{
		return 0;
	}
	return desc.Width*desc.Height*Get_Bytes_Per_Pixel(format);
}
#endif

void Invert_D3DMATRIX(D3DMATRIX& out, float* det_out, const D3DMATRIX& m)
{
	// Matrix inverse commutes with transpose, so the two conventions need no
	// conversion here; the same bytes are valid input either way.
	Matrix4x4 inverted;
	float det=0.0f;

	if (Matrix4x4::Inverse(&inverted, &det, (const Matrix4x4*)&m)==nullptr)
	{
		// Singular. D3DX left the output alone, which callers never checked, so
		// return identity rather than whatever happened to be on the stack.
		inverted.Make_Identity();
	}

	memcpy(&out, &inverted, sizeof(out));

	if (det_out)
	{
		*det_out=det;
	}
}

// These keep D3D's row-vector convention, so expressions built from them read
// and evaluate exactly as the D3DX originals did.

D3DMATRIX operator*(const D3DMATRIX& a, const D3DMATRIX& b)
{
	D3DMATRIX out;
	for (int row=0; row<4; ++row)
	{
		for (int col=0; col<4; ++col)
		{
			out.m[row][col]=
				a.m[row][0]*b.m[0][col]+
				a.m[row][1]*b.m[1][col]+
				a.m[row][2]*b.m[2][col]+
				a.m[row][3]*b.m[3][col];
		}
	}
	return out;
}

D3DMATRIX& operator*=(D3DMATRIX& a, const D3DMATRIX& b)
{
	a = a * b;
	return a;
}

void Set_D3DMATRIX_Identity(D3DMATRIX& out)
{
	memset(&out, 0, sizeof(out));
	out.m[0][0]=1.0f;
	out.m[1][1]=1.0f;
	out.m[2][2]=1.0f;
	out.m[3][3]=1.0f;
}

void Set_D3DMATRIX_Scaling(D3DMATRIX& out, float x, float y, float z)
{
	Set_D3DMATRIX_Identity(out);
	out.m[0][0]=x;
	out.m[1][1]=y;
	out.m[2][2]=z;
}

void Set_D3DMATRIX_Translation(D3DMATRIX& out, float x, float y, float z)
{
	Set_D3DMATRIX_Identity(out);
	out.m[3][0]=x;
	out.m[3][1]=y;
	out.m[3][2]=z;
}

void Transpose_D3DMATRIX(D3DMATRIX& out, const D3DMATRIX& m)
{
	D3DMATRIX result;
	for (int row=0; row<4; ++row)
	{
		for (int col=0; col<4; ++col)
		{
			result.m[row][col]=m.m[col][row];
		}
	}
	out=result;
}

const char* Get_D3D_Error_Name(unsigned res)
{
	switch (res)
	{
	case D3D_OK:								return "D3D_OK";
	case D3DERR_WRONGTEXTUREFORMAT:				return "D3DERR_WRONGTEXTUREFORMAT";
	case D3DERR_UNSUPPORTEDCOLOROPERATION:		return "D3DERR_UNSUPPORTEDCOLOROPERATION";
	case D3DERR_UNSUPPORTEDCOLORARG:			return "D3DERR_UNSUPPORTEDCOLORARG";
	case D3DERR_UNSUPPORTEDALPHAOPERATION:		return "D3DERR_UNSUPPORTEDALPHAOPERATION";
	case D3DERR_UNSUPPORTEDALPHAARG:			return "D3DERR_UNSUPPORTEDALPHAARG";
	case D3DERR_TOOMANYOPERATIONS:				return "D3DERR_TOOMANYOPERATIONS";
	case D3DERR_CONFLICTINGTEXTUREFILTER:		return "D3DERR_CONFLICTINGTEXTUREFILTER";
	case D3DERR_UNSUPPORTEDFACTORVALUE:			return "D3DERR_UNSUPPORTEDFACTORVALUE";
	case D3DERR_CONFLICTINGRENDERSTATE:			return "D3DERR_CONFLICTINGRENDERSTATE";
	case D3DERR_UNSUPPORTEDTEXTUREFILTER:		return "D3DERR_UNSUPPORTEDTEXTUREFILTER";
	case D3DERR_CONFLICTINGTEXTUREPALETTE:		return "D3DERR_CONFLICTINGTEXTUREPALETTE";
	case D3DERR_DRIVERINTERNALERROR:			return "D3DERR_DRIVERINTERNALERROR";
	case D3DERR_NOTFOUND:						return "D3DERR_NOTFOUND";
	case D3DERR_MOREDATA:						return "D3DERR_MOREDATA";
	case D3DERR_DEVICELOST:						return "D3DERR_DEVICELOST";
	case D3DERR_DEVICENOTRESET:					return "D3DERR_DEVICENOTRESET";
	case D3DERR_NOTAVAILABLE:					return "D3DERR_NOTAVAILABLE";
	case D3DERR_OUTOFVIDEOMEMORY:				return "D3DERR_OUTOFVIDEOMEMORY";
	case D3DERR_INVALIDDEVICE:					return "D3DERR_INVALIDDEVICE";
	case D3DERR_INVALIDCALL:					return "D3DERR_INVALIDCALL";
	case D3DERR_DRIVERINVALIDCALL:				return "D3DERR_DRIVERINVALIDCALL";
#if defined(BUILD_WITH_D3D9)
	case D3DERR_DEVICEHUNG:						return "D3DERR_DEVICEHUNG";
	case D3DERR_DEVICEREMOVED:					return "D3DERR_DEVICEREMOVED";
	case D3DERR_WASSTILLDRAWING:				return "D3DERR_WASSTILLDRAWING";
	case D3DERR_UNSUPPORTEDOVERLAY:				return "D3DERR_UNSUPPORTEDOVERLAY";
	case D3DERR_UNSUPPORTEDOVERLAYFORMAT:		return "D3DERR_UNSUPPORTEDOVERLAYFORMAT";
	case D3DERR_CANNOTPROTECTCONTENT:			return "D3DERR_CANNOTPROTECTCONTENT";
	case D3DERR_UNSUPPORTEDCRYPTO:				return "D3DERR_UNSUPPORTEDCRYPTO";
	case D3DERR_PRESENT_STATISTICS_DISJOINT:	return "D3DERR_PRESENT_STATISTICS_DISJOINT";
#endif
	case E_OUTOFMEMORY:							return "E_OUTOFMEMORY";
	case E_INVALIDARG:							return "E_INVALIDARG";
	case E_FAIL:								return "E_FAIL";
	default:									return "Unknown D3D error";
	}
}

// D3DXCreateTexture silently substituted the nearest supported format. Nothing in
// the engine checked what it got back, so the substitution has to be reproduced.
WW3DFormat Get_Closest_Supported_Texture_Format(WW3DFormat format, bool render_target)
{
	const DX8Caps* caps=DX8Wrapper::Get_Current_Caps();

	if (format==WW3D_FORMAT_UNKNOWN)
	{
		return WW3D_FORMAT_UNKNOWN;
	}

	const bool supported=render_target
		? caps->Support_Render_To_Texture_Format(format)
		: caps->Support_Texture_Format(format);

	if (supported)
	{
		return format;
	}

	// Ordered by how much of the original each fallback preserves
	static const WW3DFormat with_alpha[]=
	{
		WW3D_FORMAT_A8R8G8B8, WW3D_FORMAT_A1R5G5B5, WW3D_FORMAT_A4R4G4B4, WW3D_FORMAT_UNKNOWN
	};
	static const WW3DFormat without_alpha[]=
	{
		WW3D_FORMAT_X8R8G8B8, WW3D_FORMAT_R8G8B8, WW3D_FORMAT_R5G6B5, WW3D_FORMAT_X1R5G5B5,
		WW3D_FORMAT_A8R8G8B8, WW3D_FORMAT_UNKNOWN
	};

	const WW3DFormat* candidates=Has_Alpha(format) ? with_alpha : without_alpha;

	for (unsigned i=0; candidates[i]!=WW3D_FORMAT_UNKNOWN; ++i)
	{
		const bool candidate_supported=render_target
			? caps->Support_Render_To_Texture_Format(candidates[i])
			: caps->Support_Texture_Format(candidates[i]);

		if (candidate_supported)
		{
			return candidates[i];
		}
	}

	return format;
}
