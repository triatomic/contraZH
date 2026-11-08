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
 *                 Project Name : dx8 caps                                                     *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/dx8caps.cpp                            $*
 *                                                                                             *
 *              Original Author:: Hector Yee                                                   *
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 06/27/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 31                                                          $*
 *                                                                                             *
 * 06/27/02 KM Z Format support																						*
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "WWLib/always.h"
#include "dx8caps.h"
#include "dx8wrapper.h"
#include "formconv.h"
#pragma warning (disable : 4201)		// nonstandard extension - nameless struct
#include <windows.h>
#include <mmsystem.h>

static const char* const VendorNames[]={
	"Unknown",
	"NVidia",
	"ATI",
	"Intel",
	"S3",
	"PowerVR",
	"Matrox",
	"3Dfx",
	"3DLabs",
	"CirrusLogic",
	"Rendition",
	"VMware",
};
static_assert(ARRAY_SIZE(VendorNames) == DX8Caps::VENDOR_COUNT, "Incorrect array size");

DX8Caps::VendorIdType DX8Caps::Define_Vendor(unsigned vendor_id)
{
	switch (vendor_id) {
	case 0x3d3d:
	case 0x104c: return VENDOR_3DLABS;
	case 0x12D2: // STB - NVIDIA's Riva128
	case 0x14AF: // Guillemot's NVIDIA based cards
	case 0x10de: return VENDOR_NVIDIA;
	case 0x1002: return VENDOR_ATI;
	case 0x8086: return VENDOR_INTEL;
	case 0x5333: return VENDOR_S3;
	case 0x104A: return VENDOR_POWERVR;
	case 0x102B: return VENDOR_MATROX;
	case 0x1142: // Alliance based reference cards
	case 0x109D: // Macronix based reference cards
	case 0x121A: return VENDOR_3DFX;
	case 0x15AD: return VENDOR_VMWARE;
	default:
		return VENDOR_UNKNOWN;
	}
}

DX8Caps::DX8Caps(
	IDirect3D8* direct3d,
	IDirect3DDevice8* D3DDevice,
	WW3DFormat display_format,
	const D3DADAPTER_IDENTIFIER8& adapter_id)
	:
	Direct3D(direct3d)
{
	Init_Caps(D3DDevice);
	Compute_Caps(display_format, adapter_id);
}

// ----------------------------------------------------------------------------
//
// Init the caps structure
//
// ----------------------------------------------------------------------------

void DX8Caps::Init_Caps(IDirect3DDevice8* D3DDevice)
{
	// D3D9 caps do not depend on software vertex processing
#if !defined(BUILD_WITH_D3D9)
	D3DDevice->SetRenderState(D3DRS_SOFTWAREVERTEXPROCESSING,TRUE);
#endif
	DX8CALL(GetDeviceCaps(&Caps));

	if ((Caps.DevCaps&D3DDEVCAPS_HWTRANSFORMANDLIGHT)==D3DDEVCAPS_HWTRANSFORMANDLIGHT) {
		SupportTnL=true;

#if !defined(BUILD_WITH_D3D9)
		D3DDevice->SetRenderState(D3DRS_SOFTWAREVERTEXPROCESSING,FALSE);
		DX8CALL(GetDeviceCaps(&Caps));
#endif
	} else {
		SupportTnL=false;
	}
}

// ----------------------------------------------------------------------------
//
// Compute the caps bits
//
// ----------------------------------------------------------------------------
void DX8Caps::Compute_Caps(WW3DFormat display_format, const D3DADAPTER_IDENTIFIER8& adapter_id)
{
//	Init_Caps(D3DDevice);

	VendorId=Define_Vendor(adapter_id.VendorId);

	SupportPointSprites = (Caps.MaxPointSize > 1.0f);
	SupportNPatches = ((Caps.DevCaps&D3DDEVCAPS_NPATCHES)==D3DDEVCAPS_NPATCHES);
	SupportZBias = ((Caps.RasterCaps&D3DPRASTERCAPS_ZBIAS)==D3DPRASTERCAPS_ZBIAS);
	supportGamma=((Caps.Caps2&D3DCAPS2_FULLSCREENGAMMA)==D3DCAPS2_FULLSCREENGAMMA);
	SupportModAlphaAddClr = (Caps.TextureOpCaps & D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR) == D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR;
	SupportDot3=(Caps.TextureOpCaps & D3DTEXOPCAPS_DOTPRODUCT3) == D3DTEXOPCAPS_DOTPRODUCT3;
	SupportCubemaps=(Caps.TextureCaps & D3DPTEXTURECAPS_CUBEMAP) == D3DPTEXTURECAPS_CUBEMAP;
	SupportAnisotropicFiltering=
		(Caps.TextureFilterCaps&D3DPTFILTERCAPS_MAGFANISOTROPIC) && (Caps.TextureFilterCaps&D3DPTFILTERCAPS_MINFANISOTROPIC);

	Check_Texture_Format_Support(display_format,Caps);
	Check_Render_To_Texture_Support(display_format,Caps);
	Check_Depth_Stencil_Support(display_format,Caps);
	Check_Texture_Compression_Support(Caps);
	Check_Bumpmap_Support(Caps);
	Check_Shader_Support(Caps);
	Check_Maximum_Texture_Support(Caps);

	MaxTexturesPerPass=Caps.MaxSimultaneousTextures;

	Vendor_Specific_Hacks(adapter_id);

	RENDER_LOG(("Video card: %s, vendor %s (0x%04x), device 0x%04x",
		adapter_id.Description,VendorNames[VendorId],adapter_id.VendorId,adapter_id.DeviceId));
	RENDER_LOG(("Driver: %s %d.%d.%d.%d",adapter_id.Driver,
		HIWORD(adapter_id.DriverVersion.HighPart),LOWORD(adapter_id.DriverVersion.HighPart),
		HIWORD(adapter_id.DriverVersion.LowPart),LOWORD(adapter_id.DriverVersion.LowPart)));
	RENDER_LOG(("Vertex shader %d.%d, pixel shader %d.%d, hardware T&L %s, max textures per pass %d",
		Get_Vertex_Shader_Major_Version(),Get_Vertex_Shader_Minor_Version(),
		Get_Pixel_Shader_Major_Version(),Get_Pixel_Shader_Minor_Version(),
		SupportTnL ? "yes" : "no",MaxTexturesPerPass));
}

// ----------------------------------------------------------------------------
//
// Check bump map texture support
//
// ----------------------------------------------------------------------------

void DX8Caps::Check_Bumpmap_Support(const D3DCAPS8& caps)
{
	SupportBumpEnvmap=!!(caps.TextureOpCaps & D3DTEXOPCAPS_BUMPENVMAP);
	SupportBumpEnvmapLuminance=!!(caps.TextureOpCaps & D3DTEXOPCAPS_BUMPENVMAPLUMINANCE);
}

// ----------------------------------------------------------------------------
//
// Check compressed texture support
//
// ----------------------------------------------------------------------------

void DX8Caps::Check_Texture_Compression_Support(const D3DCAPS8& caps)
{
	SupportDXTC=SupportTextureFormat[WW3D_FORMAT_DXT1]|
		SupportTextureFormat[WW3D_FORMAT_DXT2]|
		SupportTextureFormat[WW3D_FORMAT_DXT3]|
		SupportTextureFormat[WW3D_FORMAT_DXT4]|
		SupportTextureFormat[WW3D_FORMAT_DXT5];
}

void DX8Caps::Check_Texture_Format_Support(WW3DFormat display_format,const D3DCAPS8& caps)
{
	if (display_format==WW3D_FORMAT_UNKNOWN) {
		for (unsigned i=0;i<WW3D_FORMAT_COUNT;++i) {
			SupportTextureFormat[i]=false;
		}
		return;
	}
	D3DFORMAT d3d_display_format=WW3DFormat_To_D3DFormat(display_format);
	for (unsigned i=0;i<WW3D_FORMAT_COUNT;++i) {
		if (i==WW3D_FORMAT_UNKNOWN) {
			SupportTextureFormat[i]=false;
		}
		else {
			WW3DFormat format=(WW3DFormat)i;
			SupportTextureFormat[i]=SUCCEEDED(
				Direct3D->CheckDeviceFormat(
					caps.AdapterOrdinal,
					caps.DeviceType,
					d3d_display_format,
					0,
					D3DRTYPE_TEXTURE,
					WW3DFormat_To_D3DFormat(format)));
		}
	}
}

void DX8Caps::Check_Render_To_Texture_Support(WW3DFormat display_format,const D3DCAPS8& caps)
{
	if (display_format==WW3D_FORMAT_UNKNOWN) {
		for (unsigned i=0;i<WW3D_FORMAT_COUNT;++i) {
			SupportRenderToTextureFormat[i]=false;
		}
		return;
	}
	D3DFORMAT d3d_display_format=WW3DFormat_To_D3DFormat(display_format);
	for (unsigned i=0;i<WW3D_FORMAT_COUNT;++i) {
		if (i==WW3D_FORMAT_UNKNOWN) {
			SupportRenderToTextureFormat[i]=false;
		}
		else {
			WW3DFormat format=(WW3DFormat)i;
			SupportRenderToTextureFormat[i]=SUCCEEDED(
				Direct3D->CheckDeviceFormat(
					caps.AdapterOrdinal,
					caps.DeviceType,
					d3d_display_format,
					D3DUSAGE_RENDERTARGET,
					D3DRTYPE_TEXTURE,
					WW3DFormat_To_D3DFormat(format)));
		}
	}
}

//**********************************************************************************************
//! Check Depth Stencil Format Support
/*! KJM
*/
void DX8Caps::Check_Depth_Stencil_Support(WW3DFormat display_format, const D3DCAPS8& caps)
{
	if (display_format==WW3D_FORMAT_UNKNOWN)
	{
		for (unsigned i=0;i<WW3D_ZFORMAT_COUNT;++i)
		{
			SupportDepthStencilFormat[i]=false;
		}
		return;
	}

	D3DFORMAT d3d_display_format=WW3DFormat_To_D3DFormat(display_format);

	for (unsigned i=0;i<WW3D_ZFORMAT_COUNT;++i)
	{
		if (i==WW3D_ZFORMAT_UNKNOWN)
		{
			SupportDepthStencilFormat[i]=false;
		}
		else
		{
			WW3DZFormat format=(WW3DZFormat)i;
			SupportDepthStencilFormat[i]=SUCCEEDED
			(
				Direct3D->CheckDeviceFormat
				(
					caps.AdapterOrdinal,
					caps.DeviceType,
					d3d_display_format,
					D3DUSAGE_DEPTHSTENCIL,
					D3DRTYPE_TEXTURE,
					WW3DZFormat_To_D3DFormat(format)
				)
			);
		}
	}
}

void DX8Caps::Check_Maximum_Texture_Support(const D3DCAPS8& caps)
{
	MaxSimultaneousTextures=caps.MaxSimultaneousTextures;
}

void DX8Caps::Check_Shader_Support(const D3DCAPS8& caps)
{
	VertexShaderVersion=caps.VertexShaderVersion;
	PixelShaderVersion=caps.PixelShaderVersion;
}

// ----------------------------------------------------------------------------
//
// Implement some vendor-specific hacks to fix certain driver bugs that can't be
// avoided otherwise.
//
// ----------------------------------------------------------------------------

void DX8Caps::Vendor_Specific_Hacks(const D3DADAPTER_IDENTIFIER8& adapter_id)
{
	if (VendorId==VENDOR_NVIDIA)
    {
		if (SupportNPatches) {
			RENDER_LOG(("NVidia Driver reported N-Patch support, disabling."));
		}

		SupportNPatches = false;	// Driver incorrectly report N-Patch support
		// The original code disabled DXT1 here for GeForce 1-4 era banding. Modern
		// NVidia hardware decodes DXT1 correctly, so all vendors share the native path.
	}

	if (VendorId==VENDOR_VMWARE) {
		// TheSuperHackers @bugfix Stubbjax 15/01/2026 Disable DOT3 support for VMWare's virtual GPU.
		// The D3DTA_ALPHAREPLICATE modifier fails when passed to a D3DTOP_MULTIPLYADD operation.
		RENDER_LOG(("Disabling DOT3 on VMWare"));
		SupportDot3 = false;
	}
}

