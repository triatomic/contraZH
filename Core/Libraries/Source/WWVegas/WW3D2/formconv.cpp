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
#include "bitmaphandler.h"

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

// Stands in for D3DXFilterTexture: box filters each mip level from the one above.
// Compressed levels cannot be locked, so those textures are left alone.
HRESULT Filter_Texture_Mipmaps(IDirect3DTexture8* texture)
{
	if (texture==nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3DSURFACE_DESC top_desc;
	HRESULT hr=texture->GetLevelDesc(0, &top_desc);
	if (FAILED(hr))
	{
		return hr;
	}

	const WW3DFormat format=D3DFormat_To_WW3DFormat(top_desc.Format);
	if (format==WW3D_FORMAT_UNKNOWN ||
		 (format>=WW3D_FORMAT_DXT1 && format<=WW3D_FORMAT_DXT5))
	{
		return D3D_OK;
	}

	for (unsigned level=1; level<texture->GetLevelCount(); ++level)
	{
		D3DSURFACE_DESC src_desc;
		D3DSURFACE_DESC dest_desc;
		if (FAILED(texture->GetLevelDesc(level-1, &src_desc)) ||
			 FAILED(texture->GetLevelDesc(level, &dest_desc)))
		{
			return D3DERR_INVALIDCALL;
		}

		D3DLOCKED_RECT src_rect;
		D3DLOCKED_RECT dest_rect;
		hr=texture->LockRect(level-1, &src_rect, nullptr, D3DLOCK_READONLY);
		if (FAILED(hr))
		{
			return hr;
		}
		hr=texture->LockRect(level, &dest_rect, nullptr, 0);
		if (FAILED(hr))
		{
			texture->UnlockRect(level-1);
			return hr;
		}

		// Box filter each 2x2 block of the larger level down to one texel. Copy_Image
		// cannot do this: it writes a same-size destination and takes any downsample
		// through a separate mip surface.
		const unsigned bpp=Get_Bytes_Per_Pixel(format);
		const unsigned char* src=(const unsigned char*)src_rect.pBits;
		unsigned char* dest=(unsigned char*)dest_rect.pBits;

		for (unsigned y=0; y<dest_desc.Height; ++y)
		{
			const unsigned src_y0=(y*2 < src_desc.Height) ? y*2 : src_desc.Height-1;
			const unsigned src_y1=(y*2+1 < src_desc.Height) ? y*2+1 : src_y0;
			const unsigned char* row0=src+src_y0*src_rect.Pitch;
			const unsigned char* row1=src+src_y1*src_rect.Pitch;
			unsigned char* dest_row=dest+y*dest_rect.Pitch;

			for (unsigned x=0; x<dest_desc.Width; ++x)
			{
				const unsigned src_x0=(x*2 < src_desc.Width) ? x*2 : src_desc.Width-1;
				const unsigned src_x1=(x*2+1 < src_desc.Width) ? x*2+1 : src_x0;

				unsigned p00,p01,p10,p11;
				BitmapHandlerClass::Read_B8G8R8A8(p00,row0+src_x0*bpp,format,nullptr,0);
				BitmapHandlerClass::Read_B8G8R8A8(p01,row0+src_x1*bpp,format,nullptr,0);
				BitmapHandlerClass::Read_B8G8R8A8(p10,row1+src_x0*bpp,format,nullptr,0);
				BitmapHandlerClass::Read_B8G8R8A8(p11,row1+src_x1*bpp,format,nullptr,0);

				const unsigned averaged=BitmapHandlerClass::Combine_A8R8G8B8(p00,p01,p10,p11);
				BitmapHandlerClass::Write_B8G8R8A8(dest_row+x*bpp,format,averaged);
			}
		}

		texture->UnlockRect(level);
		texture->UnlockRect(level-1);
	}

	return D3D_OK;
}

// Stands in for D3DXLoadSurfaceFromSurface, which converted format and scaled as
// needed. Source and destination must both be lockable.
HRESULT Load_Surface_From_Surface(
	IDirect3DSurface8* dest_surface,
	const RECT* dest_rect,
	IDirect3DSurface8* src_surface,
	const RECT* src_rect)
{
	if (dest_surface==nullptr || src_surface==nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3DSURFACE_DESC dest_desc;
	D3DSURFACE_DESC src_desc;
	if (FAILED(dest_surface->GetDesc(&dest_desc)) || FAILED(src_surface->GetDesc(&src_desc)))
	{
		return D3DERR_INVALIDCALL;
	}

	const WW3DFormat dest_format=D3DFormat_To_WW3DFormat(dest_desc.Format);
	const WW3DFormat src_format=D3DFormat_To_WW3DFormat(src_desc.Format);
	if (dest_format==WW3D_FORMAT_UNKNOWN || src_format==WW3D_FORMAT_UNKNOWN)
	{
		return D3DERR_WRONGTEXTUREFORMAT;
	}

	// Compressed surfaces are copied block row by block row; converting or scaling them is not supported
	const bool src_compressed=(src_format>=WW3D_FORMAT_DXT1 && src_format<=WW3D_FORMAT_DXT5);
	const bool dest_compressed=(dest_format>=WW3D_FORMAT_DXT1 && dest_format<=WW3D_FORMAT_DXT5);
	if (src_compressed || dest_compressed)
	{
		const bool src_whole=(src_rect==nullptr) || (src_rect->left==0 && src_rect->top==0 &&
			(UINT)src_rect->right==src_desc.Width && (UINT)src_rect->bottom==src_desc.Height);
		const bool dest_whole=(dest_rect==nullptr) || (dest_rect->left==0 && dest_rect->top==0 &&
			(UINT)dest_rect->right==dest_desc.Width && (UINT)dest_rect->bottom==dest_desc.Height);
		if (src_format!=dest_format || !src_whole || !dest_whole ||
			src_desc.Width!=dest_desc.Width || src_desc.Height!=dest_desc.Height)
		{
			return D3DERR_WRONGTEXTUREFORMAT;
		}

		D3DLOCKED_RECT locked_src;
		D3DLOCKED_RECT locked_dest;
		HRESULT hr=src_surface->LockRect(&locked_src, nullptr, D3DLOCK_READONLY);
		if (FAILED(hr))
		{
			return hr;
		}
		hr=dest_surface->LockRect(&locked_dest, nullptr, 0);
		if (FAILED(hr))
		{
			src_surface->UnlockRect();
			return hr;
		}

		const unsigned block_bytes=(src_format==WW3D_FORMAT_DXT1) ? 8 : 16;
		const unsigned row_bytes=((src_desc.Width+3)/4)*block_bytes;
		const unsigned block_rows=(src_desc.Height+3)/4;
		for (unsigned row=0; row<block_rows; ++row)
		{
			memcpy((unsigned char*)locked_dest.pBits+row*locked_dest.Pitch,
				(const unsigned char*)locked_src.pBits+row*locked_src.Pitch, row_bytes);
		}

		dest_surface->UnlockRect();
		src_surface->UnlockRect();
		return D3D_OK;
	}

	D3DLOCKED_RECT locked_src;
	D3DLOCKED_RECT locked_dest;
	HRESULT hr=src_surface->LockRect(&locked_src, src_rect, D3DLOCK_READONLY);
	if (FAILED(hr))
	{
		DEBUG_LOG(("Load_Surface_From_Surface: src lock failed 0x%08X pool=%d usage=0x%X fmt=%d -> dest pool=%d fmt=%d",
			hr, src_desc.Pool, src_desc.Usage, src_desc.Format, dest_desc.Pool, dest_desc.Format));
		return hr;
	}
	hr=dest_surface->LockRect(&locked_dest, dest_rect, 0);
	if (FAILED(hr))
	{
		DEBUG_LOG(("Load_Surface_From_Surface: dest lock failed 0x%08X pool=%d usage=0x%X fmt=%d <- src pool=%d fmt=%d",
			hr, dest_desc.Pool, dest_desc.Usage, dest_desc.Format, src_desc.Pool, src_desc.Format));
		src_surface->UnlockRect();
		return hr;
	}

	const unsigned src_width=src_rect ? (src_rect->right-src_rect->left) : src_desc.Width;
	const unsigned src_height=src_rect ? (src_rect->bottom-src_rect->top) : src_desc.Height;
	const unsigned dest_width=dest_rect ? (dest_rect->right-dest_rect->left) : dest_desc.Width;
	const unsigned dest_height=dest_rect ? (dest_rect->bottom-dest_rect->top) : dest_desc.Height;

	BitmapHandlerClass::Copy_Image(
		(unsigned char*)locked_dest.pBits,
		dest_width,
		dest_height,
		locked_dest.Pitch,
		dest_format,
		(unsigned char*)locked_src.pBits,
		src_width,
		src_height,
		locked_src.Pitch,
		src_format,
		nullptr,
		0,
		false);

	dest_surface->UnlockRect();
	src_surface->UnlockRect();

	return D3D_OK;
}

#if defined(BUILD_WITH_D3D9)

// D3D9 shader objects behind the DWORD handles the engine still passes around.
struct D3D9ShaderEntry
{
	IDirect3DVertexShader9*      VertexShader;
	IDirect3DVertexDeclaration9* Declaration;
	IDirect3DPixelShader9*       PixelShader;
};

// Index 0 is reserved so a handle of 0 keeps meaning "no shader". The engine loads
// a fixed, small set of shaders, so a flat array avoids any allocation concerns.
static const unsigned MAX_D3D9_SHADERS=64;
static D3D9ShaderEntry _D3D9Shaders[MAX_D3D9_SHADERS];
static unsigned _D3D9ShaderCount=1;

static DWORD Add_Shader_Entry(const D3D9ShaderEntry& entry)
{
	// Shaders are released and recreated on every device reset, so freed slots are reused
	for (unsigned index=1; index<_D3D9ShaderCount; ++index)
	{
		const D3D9ShaderEntry& slot=_D3D9Shaders[index];
		if (slot.VertexShader==nullptr && slot.Declaration==nullptr && slot.PixelShader==nullptr)
		{
			_D3D9Shaders[index]=entry;
			return DX8_SHADER_HANDLE_TAG | (DWORD)index;
		}
	}

	if (_D3D9ShaderCount>=MAX_D3D9_SHADERS)
	{
		WWASSERT(0);
		return 0;
	}

	_D3D9Shaders[_D3D9ShaderCount]=entry;
	return DX8_SHADER_HANDLE_TAG | (DWORD)(_D3D9ShaderCount++);
}

static D3D9ShaderEntry* Peek_Shader_Entry(DWORD handle)
{
	if ((handle & DX8_SHADER_HANDLE_TAG)==0)
	{
		return nullptr;
	}

	const unsigned index=handle & ~DX8_SHADER_HANDLE_TAG;
	if (index==0 || index>=_D3D9ShaderCount)
	{
		return nullptr;
	}

	return &_D3D9Shaders[index];
}

DWORD Register_D3D9_Vertex_Shader(IDirect3DVertexShader9* shader, IDirect3DVertexDeclaration9* declaration)
{
	D3D9ShaderEntry entry={ shader, declaration, nullptr };
	return Add_Shader_Entry(entry);
}

DWORD Register_D3D9_Pixel_Shader(IDirect3DPixelShader9* shader)
{
	D3D9ShaderEntry entry={ nullptr, nullptr, shader };
	return Add_Shader_Entry(entry);
}

void Release_D3D9_Shader(DWORD handle)
{
	D3D9ShaderEntry* entry=Peek_Shader_Entry(handle);
	if (entry==nullptr)
	{
		return;
	}

	if (entry->VertexShader)
	{
		entry->VertexShader->Release();
	}
	if (entry->Declaration)
	{
		entry->Declaration->Release();
	}
	if (entry->PixelShader)
	{
		entry->PixelShader->Release();
	}

	// The slot can be reused, so a cached copy of this handle must not match its successor
	DX8Wrapper::Forget_Shader_Handle(handle);

	entry->VertexShader=nullptr;
	entry->Declaration=nullptr;
	entry->PixelShader=nullptr;
}

IDirect3DVertexShader9* Peek_D3D9_Vertex_Shader(DWORD handle)
{
	const D3D9ShaderEntry* entry=Peek_Shader_Entry(handle);
	return entry ? entry->VertexShader : nullptr;
}

IDirect3DVertexDeclaration9* Peek_D3D9_Vertex_Declaration(DWORD handle)
{
	const D3D9ShaderEntry* entry=Peek_Shader_Entry(handle);
	return entry ? entry->Declaration : nullptr;
}

IDirect3DPixelShader9* Peek_D3D9_Pixel_Shader(DWORD handle)
{
	const D3D9ShaderEntry* entry=Peek_Shader_Entry(handle);
	return entry ? entry->PixelShader : nullptr;
}

// The D3D8 token stream names a vertex register per element; D3D9 names a usage
// semantic. The register to semantic mapping is the fixed one documented for
// converting between the two declaration forms.
static bool Register_To_Usage(unsigned reg, BYTE& usage, BYTE& usage_index)
{
	switch (reg)
	{
	case 0:  usage=D3DDECLUSAGE_POSITION;     usage_index=0; return true;
	case 1:  usage=D3DDECLUSAGE_BLENDWEIGHT;  usage_index=0; return true;
	case 2:  usage=D3DDECLUSAGE_BLENDINDICES; usage_index=0; return true;
	case 3:  usage=D3DDECLUSAGE_NORMAL;       usage_index=0; return true;
	case 4:  usage=D3DDECLUSAGE_PSIZE;        usage_index=0; return true;
	case 5:  usage=D3DDECLUSAGE_COLOR;        usage_index=0; return true;
	case 6:  usage=D3DDECLUSAGE_COLOR;        usage_index=1; return true;
	case 15: usage=D3DDECLUSAGE_POSITION;     usage_index=1; return true;
	case 16: usage=D3DDECLUSAGE_NORMAL;       usage_index=1; return true;
	default: break;
	}

	if (reg>=7 && reg<=14)
	{
		usage=D3DDECLUSAGE_TEXCOORD;
		usage_index=(BYTE)(reg-7);
		return true;
	}

	return false;
}

static bool Data_Type_To_Decl_Type(unsigned data_type, BYTE& decl_type, unsigned& size)
{
	switch (data_type)
	{
	case 0: decl_type=D3DDECLTYPE_FLOAT1;   size=4;  return true;
	case 1: decl_type=D3DDECLTYPE_FLOAT2;   size=8;  return true;
	case 2: decl_type=D3DDECLTYPE_FLOAT3;   size=12; return true;
	case 3: decl_type=D3DDECLTYPE_FLOAT4;   size=16; return true;
	case 4: decl_type=D3DDECLTYPE_D3DCOLOR; size=4;  return true;
	case 5: decl_type=D3DDECLTYPE_UBYTE4;   size=4;  return true;
	case 6: decl_type=D3DDECLTYPE_SHORT2;   size=4;  return true;
	case 7: decl_type=D3DDECLTYPE_SHORT4;   size=8;  return true;
	default: return false;
	}
}

HRESULT Create_D3D9_Declaration_From_D3D8(const DWORD* d3d8_declaration, IDirect3DVertexDeclaration9** out)
{
	if (d3d8_declaration==nullptr || out==nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH+1];
	unsigned count=0;
	WORD stream=0;
	WORD offset=0;

	for (const DWORD* token=d3d8_declaration; *token!=0xFFFFFFFF; ++token)
	{
		const unsigned token_type=(*token & 0xE0000000) >> 29;

		if (token_type==0)			// D3DVSD_TOKEN_NOP
		{
			continue;
		}
		else if (token_type==1)		// D3DVSD_TOKEN_STREAM
		{
			stream=(WORD)(*token & 0xF);
			offset=0;
		}
		else if (token_type==2)		// D3DVSD_TOKEN_STREAMDATA
		{
			// The high bit of a stream data token marks a skip rather than a register
			if (*token & 0x10000000)
			{
				offset=(WORD)(offset + ((*token >> 16) & 0xF) * sizeof(DWORD));
				continue;
			}

			BYTE usage=0;
			BYTE usage_index=0;
			BYTE decl_type=0;
			unsigned size=0;

			if (!Register_To_Usage(*token & 0xF, usage, usage_index) ||
				 !Data_Type_To_Decl_Type((*token >> 16) & 0xF, decl_type, size) ||
				 count>=MAXD3DDECLLENGTH)
			{
				return D3DERR_INVALIDCALL;
			}

			elements[count].Stream=stream;
			elements[count].Offset=offset;
			elements[count].Type=decl_type;
			elements[count].Method=D3DDECLMETHOD_DEFAULT;
			elements[count].Usage=usage;
			elements[count].UsageIndex=usage_index;
			++count;
			offset=(WORD)(offset+size);
		}
		else
		{
			// Constant memory and tessellator tokens are unused by this engine
			return D3DERR_INVALIDCALL;
		}
	}

	const D3DVERTEXELEMENT9 end=D3DDECL_END();
	elements[count]=end;

	return DX8Wrapper::_Get_D3D_Device8()->CreateVertexDeclaration(elements, out);
}

#endif
