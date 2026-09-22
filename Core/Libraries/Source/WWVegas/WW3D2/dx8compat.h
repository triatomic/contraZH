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

// Maps the engine's D3D8 type names onto D3D9 so the renderer keeps its existing
// names while running on D3D9. Only the places where D3D9 changed semantics are
// edited; everything else stays byte-identical to upstream and merges cleanly.

#pragma once

#if defined(BUILD_WITH_D3D9)

#include <d3d9.h>

typedef IDirect3D9             IDirect3D8;
typedef IDirect3DDevice9       IDirect3DDevice8;
typedef IDirect3DResource9     IDirect3DResource8;
typedef IDirect3DBaseTexture9  IDirect3DBaseTexture8;
typedef IDirect3DTexture9      IDirect3DTexture8;
typedef IDirect3DCubeTexture9  IDirect3DCubeTexture8;
typedef IDirect3DVolumeTexture9 IDirect3DVolumeTexture8;
typedef IDirect3DVolume9       IDirect3DVolume8;
typedef IDirect3DSurface9      IDirect3DSurface8;
typedef IDirect3DVertexBuffer9 IDirect3DVertexBuffer8;
typedef IDirect3DIndexBuffer9  IDirect3DIndexBuffer8;
typedef IDirect3DSwapChain9    IDirect3DSwapChain8;

typedef D3DCAPS9               D3DCAPS8;
typedef D3DADAPTER_IDENTIFIER9 D3DADAPTER_IDENTIFIER8;
typedef D3DLIGHT9              D3DLIGHT8;
typedef D3DMATERIAL9           D3DMATERIAL8;
typedef D3DVIEWPORT9           D3DVIEWPORT8;

typedef IDirect3D9*             LPDIRECT3D8;
typedef IDirect3DDevice9*       LPDIRECT3DDEVICE8;
typedef IDirect3DBaseTexture9*  LPDIRECT3DBASETEXTURE8;
typedef IDirect3DTexture9*      LPDIRECT3DTEXTURE8;
typedef IDirect3DCubeTexture9*  LPDIRECT3DCUBETEXTURE8;
typedef IDirect3DVolumeTexture9* LPDIRECT3DVOLUMETEXTURE8;
typedef IDirect3DSurface9*      LPDIRECT3DSURFACE8;
typedef IDirect3DVertexBuffer9* LPDIRECT3DVERTEXBUFFER8;
typedef IDirect3DIndexBuffer9*  LPDIRECT3DINDEXBUFFER8;

// Removed in D3D9. Kept as distinct sentinel values so the central translation in
// DX8Wrapper::Set_DX8_Render_State can recognise and remap them; they must not
// collide with any live D3DRENDERSTATETYPE.
#define D3DRS_ZBIAS                     ((D3DRENDERSTATETYPE)47)
#define D3DRS_EDGEANTIALIAS             ((D3DRENDERSTATETYPE)40)
#define D3DRS_LINEPATTERN               ((D3DRENDERSTATETYPE)10)
#define D3DRS_ZVISIBLE                  ((D3DRENDERSTATETYPE)30)
#define D3DRS_PATCHSEGMENTS             ((D3DRENDERSTATETYPE)164)
#define D3DRS_SOFTWAREVERTEXPROCESSING  ((D3DRENDERSTATETYPE)153)

// D3D9 moved these from the texture stage to the sampler; the values are kept so
// existing call sites compile, and the wrapper routes them to SetSamplerState.
#define D3DTSS_ADDRESSU      ((D3DTEXTURESTAGESTATETYPE)13)
#define D3DTSS_ADDRESSV      ((D3DTEXTURESTAGESTATETYPE)14)
#define D3DTSS_ADDRESSW      ((D3DTEXTURESTAGESTATETYPE)25)
#define D3DTSS_BORDERCOLOR   ((D3DTEXTURESTAGESTATETYPE)15)
#define D3DTSS_MAGFILTER     ((D3DTEXTURESTAGESTATETYPE)16)
#define D3DTSS_MINFILTER     ((D3DTEXTURESTAGESTATETYPE)17)
#define D3DTSS_MIPFILTER     ((D3DTEXTURESTAGESTATETYPE)18)
#define D3DTSS_MIPMAPLODBIAS ((D3DTEXTURESTAGESTATETYPE)19)
#define D3DTSS_MAXMIPLEVEL   ((D3DTEXTURESTAGESTATETYPE)20)
#define D3DTSS_MAXANISOTROPY ((D3DTEXTURESTAGESTATETYPE)21)

// D3D9 dropped the cubic filter modes. No hardware ever exposed them, so these
// survive only to keep the debug name table compiling and to let the wrapper
// clamp them to linear.
#define D3DTEXF_FLATCUBIC_D3D8     4
#define D3DTEXF_GAUSSIANCUBIC_D3D8 5
#define D3DTEXF_FLATCUBIC          ((D3DTEXTUREFILTERTYPE)D3DTEXF_FLATCUBIC_D3D8)
#define D3DTEXF_GAUSSIANCUBIC      ((D3DTEXTUREFILTERTYPE)D3DTEXF_GAUSSIANCUBIC_D3D8)

#define D3DENUM_NO_WHQL_LEVEL 0

// D3D8 gated depth bias behind a raster cap. D3D9 always supports D3DRS_DEPTHBIAS,
// so the cap is gone and the test must read as supported.
#define D3DPRASTERCAPS_ZBIAS 0

// D3D9 dropped this bump format
#define D3DFMT_W11V11U10 ((D3DFORMAT)65)

// D3D9 locks hand back void** where D3D8 used BYTE**, and the buffer creators
// gained a trailing shared-handle argument. These let the call sites keep one
// spelling for both backends.
typedef void** DX8LockPointer;

#define DX8_CREATE_INDEX_BUFFER(dev, length, usage, format, pool, out) \
	(dev)->CreateIndexBuffer(length, usage, format, pool, out, nullptr)
#define DX8_CREATE_VERTEX_BUFFER(dev, length, usage, fvf, pool, out) \
	(dev)->CreateVertexBuffer(length, usage, fvf, pool, out, nullptr)
#define DX8_CREATE_TEXTURE(dev, w, h, mips, usage, fmt, pool, out) \
	(dev)->CreateTexture(w, h, mips, usage, fmt, pool, out, nullptr)

// D3D8 overloaded SetVertexShader with an FVF code; D3D9 has a separate setter
#define DX8_SET_FVF(dev, fvf) (dev)->SetFVF(fvf)

// Several device getters gained a leading swap chain index in D3D9
#define DX8_SWAPCHAIN 0,
#define DX8_ENUM_FORMAT(fmt) fmt,
// D3D9 reports how many quality levels the sample type supports
#define DX8_MSAA_QUALITY , nullptr
// D3D9 dropped COPY_VSYNC; DISCARD is the supported windowed equivalent
#define DX8_SWAPEFFECT_COPY_VSYNC D3DSWAPEFFECT_DISCARD

// D3D9 renamed this and made it apply to windowed mode too
#define FullScreen_PresentationInterval PresentationInterval

// D3DSURFACE_DESC lost its Size member in D3D9
unsigned Surface_Size(const D3DSURFACE_DESC& desc);

#else

#include <d3d8.h>

typedef unsigned char** DX8LockPointer;

#define DX8_CREATE_INDEX_BUFFER(dev, length, usage, format, pool, out) \
	(dev)->CreateIndexBuffer(length, usage, format, pool, out)
#define DX8_CREATE_VERTEX_BUFFER(dev, length, usage, fvf, pool, out) \
	(dev)->CreateVertexBuffer(length, usage, fvf, pool, out)
#define DX8_CREATE_TEXTURE(dev, w, h, mips, usage, fmt, pool, out) \
	(dev)->CreateTexture(w, h, mips, usage, fmt, pool, out)

#define DX8_SET_FVF(dev, fvf) (dev)->SetVertexShader(fvf)

#define DX8_SWAPCHAIN
#define DX8_ENUM_FORMAT(fmt)
#define DX8_MSAA_QUALITY
#define DX8_SWAPEFFECT_COPY_VSYNC D3DSWAPEFFECT_COPY_VSYNC

inline unsigned Surface_Size(const D3DSURFACE_DESC& desc) { return desc.Size; }

#endif
