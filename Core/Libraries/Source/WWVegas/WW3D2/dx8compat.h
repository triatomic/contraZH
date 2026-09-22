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

#define D3DENUM_NO_WHQL_LEVEL 0

#else

#include <d3d8.h>

#endif
