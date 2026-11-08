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
 *                 Project Name : DX8 Caps                                                     *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/dx8caps.h                              $*
 *                                                                                             *
 *              Original Author:: Hector Yee                                                   *
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 06/27/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 24                                                          $*
 *                                                                                             *
 * 06/27/02 KM Z Format support																						*
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "WWLib/always.h"
#include "ww3dformat.h"
#include "dx8compat.h"

class DX8Caps
{
public:
	enum VendorIdType {
		VENDOR_UNKNOWN,
		VENDOR_NVIDIA,
		VENDOR_ATI,
		VENDOR_INTEL,
		VENDOR_S3,
		VENDOR_POWERVR,
		VENDOR_MATROX,
		VENDOR_3DFX,
		VENDOR_3DLABS,
		VENDOR_CIRRUSLOGIC,
		VENDOR_RENDITION,
		VENDOR_VMWARE,

		VENDOR_COUNT
	};

	DX8Caps(IDirect3D8* direct3d, IDirect3DDevice8* D3DDevice,WW3DFormat display_format, const D3DADAPTER_IDENTIFIER8& adapter_id);

	void Compute_Caps(WW3DFormat display_format, const D3DADAPTER_IDENTIFIER8& adapter_id);
	bool Support_TnL() const { return SupportTnL; };
	bool Support_DXTC() const { return SupportDXTC; }
	bool Support_Gamma() const { return supportGamma; }
	bool Support_NPatches() const { return SupportNPatches; }
	bool Support_Bump_Envmap() const { return SupportBumpEnvmap; }
	bool Support_Bump_Envmap_Luminance() const { return SupportBumpEnvmapLuminance; }
	bool Support_ZBias() const { return SupportZBias; }
	bool Support_Anisotropic_Filtering() const { return SupportAnisotropicFiltering; }
	bool Support_ModAlphaAddClr() const { return SupportModAlphaAddClr; }
	bool Support_Dot3() const { return SupportDot3; }
	bool Support_PointSprites() const { return SupportPointSprites; }
	bool Support_Cubemaps() const { return SupportCubemaps; }
	bool Is_Fog_Allowed() const { return IsFogAllowed; }

	int Get_Max_Textures_Per_Pass() const { return MaxTexturesPerPass; }

	// -------------------------------------------------------------------------
	//
	// Vertex shader support. Version number is split in major and minor, such that 1.0 would
	// have 1 as major and 0 as minor version number.
	//
	// -------------------------------------------------------------------------

	int Get_Vertex_Shader_Major_Version() const { return 0xff&(VertexShaderVersion>>8); }
	int Get_Vertex_Shader_Minor_Version() const { return 0xff&(VertexShaderVersion); }
	int Get_Pixel_Shader_Major_Version() const { return 0xff&(PixelShaderVersion>>8); }
	int Get_Pixel_Shader_Minor_Version() const { return 0xff&(PixelShaderVersion); }
	int Get_Max_Simultaneous_Textures()	const { return MaxSimultaneousTextures;}

	bool Support_Texture_Format(WW3DFormat format) const { return SupportTextureFormat[format]; }
	bool Support_Render_To_Texture_Format(WW3DFormat format) const { return SupportRenderToTextureFormat[format]; }
	bool Support_Depth_Stencil_Format(WW3DZFormat format) const { return SupportDepthStencilFormat[format]; }

	D3DCAPS8 const & Get_DX8_Caps() const { return Caps; }

	unsigned Get_Vendor() const { return VendorId; }

private:
	static VendorIdType Define_Vendor(unsigned vendor_id);

	void Init_Caps(IDirect3DDevice8* D3DDevice);
	void Check_Texture_Format_Support(WW3DFormat display_format,const D3DCAPS8& caps);
	void Check_Render_To_Texture_Support(WW3DFormat display_format,const D3DCAPS8& caps);
	void Check_Depth_Stencil_Support(WW3DFormat display_format, const D3DCAPS8& caps);
	void Check_Texture_Compression_Support(const D3DCAPS8& caps);
	void Check_Bumpmap_Support(const D3DCAPS8& caps);
	void Check_Shader_Support(const D3DCAPS8& caps);
	void Check_Maximum_Texture_Support(const D3DCAPS8& caps);
	void Vendor_Specific_Hacks(const D3DADAPTER_IDENTIFIER8& adapter_id);

	D3DCAPS8 Caps;
	bool SupportTnL;
	bool SupportDXTC;
	bool supportGamma;
	bool SupportNPatches;
	bool SupportBumpEnvmap;
	bool SupportBumpEnvmapLuminance;
	bool SupportTextureFormat[WW3D_FORMAT_COUNT];
	bool SupportRenderToTextureFormat[WW3D_FORMAT_COUNT];
	bool SupportDepthStencilFormat[WW3D_ZFORMAT_COUNT];
	bool SupportZBias;
	bool SupportAnisotropicFiltering;
	bool SupportModAlphaAddClr;
	bool SupportDot3;
	bool SupportPointSprites;
	bool SupportCubemaps;
	bool IsFogAllowed;
	int MaxTexturesPerPass;
	int VertexShaderVersion;
	int PixelShaderVersion;
	int MaxSimultaneousTextures;
	VendorIdType VendorId;
	IDirect3D8* Direct3D; // warning XDK name conflict KJM
};
