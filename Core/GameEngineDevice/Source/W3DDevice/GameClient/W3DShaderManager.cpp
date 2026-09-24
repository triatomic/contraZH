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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: W3DShaderManager.cpp ////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:   RTS3
//
// File name: W3DShaderManager.cpp
//
// Created:   Mark Wilczynski, August 2001
//
// Desc:      Perform tests on currently selected WW3D/D3D device to determine
//			  which of our rendering features are supported.  The system allows
//			  setting up a few custom shaders that are selected based on video
//			  card features.
//
//			  To add a new shader to the system:
//			  0) Add your shader to the ShaderTypes enum
//			  1) Create shader using W3DShaderInterface
//			  2) Repeat step 1 for any alternate shaders
//			  3) Create list of alternate shaders sorted by order of preference.
//				 The first shader which passes hardware validation will be selected.
//			  4) Add list from step 3) to MasterShaderList[].
//
//-----------------------------------------------------------------------------

#include "WW3D2/dx8wrapper.h"
#include "WW3D2/assetmgr.h"
#include "Lib/BaseType.h"
#include "Common/file.h"
#include "Common/FileSystem.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DShroud.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/W3DCustomScene.h"
#include "W3DDevice/GameClient/W3DSmudge.h"
#include "W3DDevice/GameClient/W3DShadowMap.h"
#include "GameClient/View.h"
#include "GameClient/CommandXlat.h"
#include "GameClient/Display.h"
#include "GameClient/Water.h"
#include "GameLogic/GameLogic.h"
#include "Common/GlobalData.h"
#include "Common/GameLOD.h"
#include "WW3D2/dx8caps.h"
#include "WW3D2/formconv.h"
#include "WW3D2/dx8renderer.h"
#include "WW3D2/dx8instancing.h"
#include "WW3D2/dx8skinning.h"
#include "WW3D2/dx8polygonrenderer.h"
#include "WW3D2/matpass.h"
#include "WW3D2/texture.h"
#include "WWLib/ffactory.h"


// Turn this on to turn off pixel shaders. jba[4/3/2003]
#define do_not_DISABLE_PIXEL_SHADERS 1

/** Interface definition for custom shaders we define in our app.  These shaders can perform more complex
	operations than those allowed in the WW3D2 shader system.
*/
class W3DShaderInterface
{
public:
	Int getNumPasses() {return m_numPasses;};	///<return number of passes needed for this shader
	virtual Int set(Int pass) {return TRUE;};		///<setup shader for the specified rendering pass.
	 ///do any custom resetting necessary to bring W3D in sync.
	virtual void reset() {
		ShaderClass::Invalidate();
		DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, nullptr);
		DX8Wrapper::_Get_D3D_Device8()->SetTexture(1, nullptr);};
	virtual Int init() = 0;			///<perform any one time initialization and validation
	virtual Int shutdown() { return TRUE;};			///<release resources used by shader
protected:
	Int m_numPasses;						///<number of passes to complete shader
};

//this table will contain custom versions of each shader tuned for specific video card and user options.
static W3DFilterInterface *W3DFilters[FT_MAX];
static W3DShaderInterface *W3DShaders[W3DShaderManager::ST_MAX];
static Int W3DShadersPassCount[W3DShaderManager::ST_MAX];	//number of passes for each of the above shaders
TextureClass *W3DShaderManager::m_Textures[8];
W3DShaderManager::ShaderTypes W3DShaderManager::m_currentShader;
FilterTypes W3DShaderManager::m_currentFilter=FT_NULL_FILTER; ///< Last filter that was set.
Int W3DShaderManager::m_currentShaderPass;
ChipsetType W3DShaderManager::m_currentChipset;
GraphicsVenderID W3DShaderManager::m_currentVendor;
__int64 W3DShaderManager::m_driverVersion;

Bool W3DShaderManager::m_renderingToTexture = false;
IDirect3DSurface8 *W3DShaderManager::m_oldRenderSurface=nullptr;	///<previous render target
IDirect3DTexture8 *W3DShaderManager::m_renderTexture=nullptr;		///<texture into which rendering will be redirected.
IDirect3DSurface8 *W3DShaderManager::m_newRenderSurface=nullptr;	///<new render target inside m_renderTexture
IDirect3DSurface8 *W3DShaderManager::m_oldDepthSurface=nullptr;	///<previous depth buffer surface
/*===========================================================================================*/
/*=========      Screen Shaders	=============================================================*/
/*===========================================================================================*/

class ScreenDefaultFilter : public W3DFilterInterface
{
public:
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual Bool preRender(Bool &skipRender, CustomScenePassModes &scenePassMode) override; ///< Set up at start of render.  Only applies to screen filter shaders.
	virtual Bool postRender(FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender) override; ///< Called after render.  Only applies to screen filter shaders.
	virtual Bool setup(FilterModes mode) override {return true;} ///< Called when the filter is started, one time before the first prerender.
protected:
	virtual Int set(FilterModes mode) override;		///<setup shader for the specified rendering pass.
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
};

ScreenDefaultFilter screenDefaultFilter;

///Default filter that just renders screen to off-screen texture and then copies it the the screen.
///Useful because we added some full-time unit effects (microwave tank smudge) to Generals MD that need access
///to the background as a texture.  This filter makes that texture always available for these effects.
W3DFilterInterface *ScreenDefaultFilterList[]=
{
	&screenDefaultFilter,
	nullptr
};

Int ScreenDefaultFilter::init()
{
	if (!W3DShaderManager::canRenderToTexture()) {
		// Have to be able to render to texture.
		return FALSE;
	}

	//Can render to texture, but we don't know if it can read and write to the same texture.
	//Since there is no D3D caps bit to tell you this, we will just hard-code some specific
	//cards that we know should work.

	Int res;

	if ((res=W3DShaderManager::getChipset()) != DC_UNKNOWN)
	{
		if ( res >=	DC_GEFORCE2)
		{
			//Check if their driver is newer than what we tested for this vendor
/*			if (TheGameLODManager)
			{
				if (TheGameLODManager->getTestedDriverVersion(W3DShaderManager::getCurrentVendor()) < W3DShaderManager::getCurrentDriverVersion())
					return FALSE;
			}*/
		}
	}

	W3DFilters[FT_VIEW_DEFAULT]=&screenDefaultFilter;

	return TRUE;
}

Bool ScreenDefaultFilter::preRender(Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	// TheSuperHackers @bugfix Disable Render To Texture redirection for the default filter
	// When MSAA is forced by Nvidia driver profile depth buffer is multisampled internally.
	// Rendering to non-MSAA texture with this depth buffer corrupts depth testing producing black screen
	// The smudge system has its own Copy path that works without Render To Texture.
	return FALSE;
}

Bool ScreenDefaultFilter::postRender(FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender)
{
	IDirect3DTexture8 * tex =	W3DShaderManager::endRenderToTexture();
	DEBUG_ASSERTCRASH(tex, ("Require rendered texture."));
	if (!tex) return false;
	if (!set(mode)) return false;

	LPDIRECT3DDEVICE8 pDev=DX8Wrapper::_Get_D3D_Device8();

	struct _TRANS_LIT_TEX_VERTEX {
		Vector4 p;
		DWORD color;   // diffuse color
		float	u;
		float	v;
	} v[4];

	Int xpos, ypos, width, height;

	DX8Wrapper::_Get_D3D_Device8()->SetTexture(0,tex);	//previously rendered frame inside this texture
	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

	//bottom right
	v[0].p = Vector4( xpos+width-0.5f, ypos+height-0.5f, 0.0f, 1.0f );
	v[0].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top right
	v[1].p = Vector4( xpos+width-0.5f, ypos-0.5f, 0.0f, 1.0f );
	v[1].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	//bottom left
	v[2].p = Vector4(  xpos-0.5f, ypos+height-0.5f, 0.0f, 1.0f );
	v[2].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top left
	v[3].p = Vector4(  xpos-0.5f,  ypos-0.5f, 0.0f, 1.0f );
	v[3].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	v[0].color = 0xffffffff;
	v[1].color = 0xffffffff;
	v[2].color = 0xffffffff;
	v[3].color = 0xffffffff;

	//draw polygons like this is very inefficient but for only 2 triangles, it's
	//not worth bothering with index/vertex buffers.
	DX8_SET_FVF(pDev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);

	pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(_TRANS_LIT_TEX_VERTEX));

	reset();
	return true;
}

Int ScreenDefaultFilter::set(FilterModes mode)
{
	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vmat);
	REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.
	DX8Wrapper::Set_Shader(ShaderClass::_PresetOpaqueShader);
	DX8Wrapper::Set_Texture(0,nullptr);
	DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC,D3DCMP_ALWAYS);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE,FALSE);
	DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

	return true;
}

void ScreenDefaultFilter::reset()
{
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(0,nullptr);	//previously rendered frame inside this texture
	DX8Wrapper::Invalidate_Cached_Render_States();
}

/*=========  ScreenBWFilter	=============================================================*/
///converts viewport to black & white.

Int ScreenBWFilter::m_fadeFrames;
Int ScreenBWFilter::m_curFadeFrame;
Real ScreenBWFilter::m_curFadeValue;
Int ScreenBWFilter::m_fadeDirection;

ScreenBWFilter screenBWFilter;
ScreenBWFilterDOT3 screenBWFilterDOT3;	//slower version for older cards without pixel shaders.

///List of different BW shader implementations in order of preference
W3DFilterInterface *ScreenBWFilterList[]=
{
	&screenBWFilter,
	&screenBWFilterDOT3,	//slower version for older cards without pixel shaders.
	nullptr
};

Int ScreenBWFilter::init()
{
	Int res;
	HRESULT hr;

	m_dwBWPixelShader = 0;
	m_curFadeFrame = 0;

	if (!W3DShaderManager::canRenderToTexture()) {
		// Have to be able to render to texture.
		return false;
	}

	if ((res=W3DShaderManager::getChipset()) != 0)
	{
		if (res >= DC_GENERIC_PIXEL_SHADER_1_1)
		{
			//this shader needs some assets that need to be loaded
			//shader decleration
			DWORD Declaration[]=
			{
				(D3DVSD_STREAM(0)),
				(D3DVSD_REG(0, D3DVSDT_FLOAT3)), // Position
				(D3DVSD_REG(1, D3DVSDT_D3DCOLOR)), // Diffuse
				(D3DVSD_REG(2, D3DVSDT_FLOAT2)), //  Texture Coordinates
				(D3DVSD_END())
			};

			//Monochrome pixel shader.
			hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\monochrome.pso", &Declaration[0], 0, false, &m_dwBWPixelShader);
			if (FAILED(hr))
				return FALSE;

			W3DFilters[FT_VIEW_BW_FILTER]=&screenBWFilter;

			return TRUE;
		}
	}
	return FALSE;
}

Bool ScreenBWFilter::preRender(Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	skipRender = false;
	W3DShaderManager::startRenderToTexture();
	return true;
}

Bool ScreenBWFilter::postRender(FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender)
{
	IDirect3DTexture8 * tex =	W3DShaderManager::endRenderToTexture();
	DEBUG_ASSERTCRASH(tex, ("Require rendered texture."));
	if (!tex) return false;
	if (!set(mode)) return false;

	LPDIRECT3DDEVICE8 pDev=DX8Wrapper::_Get_D3D_Device8();

	struct _TRANS_LIT_TEX_VERTEX {
		Vector4 p;
		DWORD color;   // diffuse color
		float	u;
		float	v;
	} v[4];

	Int xpos, ypos, width, height;

	DX8Wrapper::_Get_D3D_Device8()->SetTexture(0,tex);	//previously rendered frame inside this texture
	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

	//bottom right
	v[0].p = Vector4( xpos+width-0.5f, ypos+height-0.5f, 0.0f, 1.0f );
	v[0].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top right
	v[1].p = Vector4( xpos+width-0.5f, ypos-0.5f, 0.0f, 1.0f );
	v[1].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	//bottom left
	v[2].p = Vector4(  xpos-0.5f, ypos+height-0.5f, 0.0f, 1.0f );
	v[2].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top left
	v[3].p = Vector4(  xpos-0.5f,  ypos-0.5f, 0.0f, 1.0f );
	v[3].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	v[0].color = 0xffffffff;
	v[1].color = 0xffffffff;
	v[2].color = 0xffffffff;
	v[3].color = 0xffffffff;

	//draw polygons like this is very inefficient but for only 2 triangles, it's
	//not worth bothering with index/vertex buffers.
	DX8_SET_FVF(pDev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);

	pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(_TRANS_LIT_TEX_VERTEX));

	reset();
	return true;
}

Int ScreenBWFilter::set(FilterModes mode)
{

	if (mode > FM_NULL_MODE)
	{	//rendering a quad with redirected rendering surface tinted by pixel shader

		if (m_fadeDirection > 0)
		{	//turning effect on
			m_curFadeFrame++;
			Int fade = m_curFadeFrame;

			if (fade<m_fadeFrames)
			{
				m_curFadeValue = (Real)fade/(Real)m_fadeFrames;
			}
			else
			{
				m_curFadeFrame = 0;
				m_curFadeValue = 1.0f;
				m_fadeDirection = 0;
			}
		}
		else
		if (m_fadeDirection < 0)
		{	//turning effect off
			m_curFadeFrame++;
			Int fade = m_curFadeFrame;
			if (fade<m_fadeFrames)
			{
				m_curFadeValue = 1.0f - (Real)fade/(Real)m_fadeFrames;
			}
			else
			{	m_curFadeValue = 0.0f;
				TheTacticalView->setViewFilterMode(FM_NULL_MODE);
				TheTacticalView->setViewFilter(FT_NULL_FILTER);
				m_curFadeFrame = 0;
				m_fadeDirection = 0;
			}
		}

		VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
		DX8Wrapper::Set_Material(vmat);
		REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.
		DX8Wrapper::Set_Shader(ShaderClass::_PresetOpaqueShader);
		DX8Wrapper::Set_Texture(0,nullptr);
		DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

		DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC,D3DCMP_ALWAYS);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE,FALSE);
		DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

		DX8Wrapper::Set_Pixel_Shader(m_dwBWPixelShader);
		const Vector4 luminance_weights(0.3f, 0.59f, 0.11f, 1.0f);
		DX8Wrapper::Set_Pixel_Shader_Constant(0, &luminance_weights, 1);

		Vector4	color(1.0f,1.0f,1.0f,1.0f);	//multiply color

		if (mode == FM_VIEW_BW_BLACK_AND_WHITE)
		{	//back & white mode
			color.X=1.0f;
			color.Y=1.0f;
			color.Z=1.0f;
		}
		if (mode == FM_VIEW_BW_RED_AND_WHITE)
		{	//red is on
			color.X = 1.0f;
			color.Y = 0.0f;
			color.Z = 0.0f;
			//inverse red is on
			//red is on
//			color.X = 0.0f;
//			color.Y = 1.0f;
//			color.Z = 1.0f;
		}
		if (mode == FM_VIEW_BW_GREEN_AND_WHITE)
		{
			color.X = 0.0f;
			color.Y = 1.0f;
			color.Z = 0.0f;
		}

		DX8Wrapper::Set_Pixel_Shader_Constant(1, &color, 1);
		const Vector4 fade_level(m_curFadeValue, m_curFadeValue, m_curFadeValue, 1.0f);
		DX8Wrapper::Set_Pixel_Shader_Constant(2, &fade_level, 1);
/*		const Vector4 grey_level(150.0f/255.0f, 150.0f/255.0f, 150.0f/255.0f, 0.0f);
		DX8Wrapper::Set_Pixel_Shader_Constant(2, &grey_level, 1);
		const Vector4 luminance_scale((765.0f/450.0f)/3, (765.0f/450.0f)/3, (765.0f/450.0f)/3, 1.0f);
		DX8Wrapper::Set_Pixel_Shader_Constant(3, &luminance_scale, 1);
		const Vector4 half_intensity(0.5f, 0.5f, 0.5f, 0);
		DX8Wrapper::Set_Pixel_Shader_Constant(4, &half_intensity, 1);
		const Vector4 shadow_tint((60.0f)/255.0f, (60.0f)/255.0f, (60.0f)/255.0f, 0);
		DX8Wrapper::Set_Pixel_Shader_Constant(5, &shadow_tint, 1);
		const Vector4 highlight_tint((157.0f)/255.0f, (157.0f)/255.0f, (157.0f)/255.0f, 0);
		DX8Wrapper::Set_Pixel_Shader_Constant(6, &highlight_tint, 1);
		const Vector4 midtone_tint((30.0f)/255.0f, (30.0f)/255.0f, (30.0f)/255.0f, 0);
		DX8Wrapper::Set_Pixel_Shader_Constant(7, &midtone_tint, 1);
*/
		return true;
	}
	return false;
}

void ScreenBWFilter::reset()
{
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(0,nullptr);	//previously rendered frame inside this texture
	DX8Wrapper::Set_Pixel_Shader(0);	//turn off pixel shader
	DX8Wrapper::Invalidate_Cached_Render_States();
}

Int ScreenBWFilter::shutdown()
{
	if (m_dwBWPixelShader)
		DX8_DELETE_PIXEL_SHADER(DX8Wrapper::_Get_D3D_Device8(), m_dwBWPixelShader);

	m_dwBWPixelShader=0;

	return TRUE;
}

/**Alternate version of the above filter which does not require pixel shaders - good for older cards*/
Int ScreenBWFilterDOT3::init()
{
	Int res;

	m_curFadeFrame = 0;

	if (!W3DShaderManager::canRenderToTexture()) {
		// Have to be able to render to texture.
		return false;
	}

	if ((res=W3DShaderManager::getChipset()) != 0)
	{
			W3DFilters[FT_VIEW_BW_FILTER]=&screenBWFilterDOT3;
			return TRUE;
	}
	return FALSE;
}

Bool ScreenBWFilterDOT3::preRender(Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	skipRender = false;
	W3DShaderManager::startRenderToTexture();
	return true;
}

Bool ScreenBWFilterDOT3::postRender(FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender)
{
	IDirect3DTexture8 * tex =	W3DShaderManager::endRenderToTexture();
	DEBUG_ASSERTCRASH(tex, ("Require rendered texture."));
	if (!tex) return false;
	if (!set(mode)) return false;

	LPDIRECT3DDEVICE8 pDev=DX8Wrapper::_Get_D3D_Device8();

	struct _TRANS_LIT_TEX_VERTEX {
		Vector4 p;
		DWORD color;   // diffuse color
		float	u;
		float	v;
	} v[4];

	Int xpos, ypos, width, height;

	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

	//bottom right
	v[0].p = Vector4( xpos+width-0.5f, ypos+height-0.5f, 0.0f, 1.0f );
	v[0].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top right
	v[1].p = Vector4( xpos+width-0.5f, ypos-0.5f, 0.0f, 1.0f );
	v[1].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	//bottom left
	v[2].p = Vector4(  xpos-0.5f, ypos+height-0.5f, 0.0f, 1.0f );
	v[2].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top left
	v[3].p = Vector4(  xpos-0.5f,  ypos-0.5f, 0.0f, 1.0f );
	v[3].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos)/(Real)TheDisplay->getHeight();

	DWORD currentFade=(((Int)((1.0f-m_curFadeValue) * 255.0f))<<24) | 0x00ffffff;	//store alpha value

	v[0].color = currentFade;
	v[1].color = currentFade;
	v[2].color = currentFade;
	v[3].color = currentFade;

	//draw polygons like this is very inefficient but for only 2 triangles, it's
	//not worth bothering with index/vertex buffers.
	DX8_SET_FVF(pDev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);

	//Draw B&W version first
	if (DX8Wrapper::Get_Current_Caps()->Support_Dot3())
	{	//Override W3D states with customizations for grayscale
		DX8Wrapper::Set_DX8_Render_State(D3DRS_TEXTUREFACTOR, 0x80A5CA8E);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG0, D3DTA_TFACTOR | D3DTA_ALPHAREPLICATE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG2, D3DTA_TFACTOR | D3DTA_ALPHAREPLICATE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLOROP, D3DTOP_MULTIPLYADD);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG1, D3DTA_CURRENT);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG2, D3DTA_TFACTOR);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP, D3DTOP_DOTPRODUCT3);
	}
	else
	{	//doesn't have DOT3 blend mode so fake it another way.
		DX8Wrapper::Set_DX8_Render_State(D3DRS_TEXTUREFACTOR, 0x60606060);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	}

	DX8Wrapper::_Get_D3D_Device8()->SetTexture(0,tex);	//previously rendered frame inside this texture

	pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(_TRANS_LIT_TEX_VERTEX));

	//Draw normal view blended by current fade level
	ShaderClass::Invalidate();	//reset DOT3 blend from above.
	ShaderClass shader=ShaderClass::_PresetAlphaShader;
	shader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);
	DX8Wrapper::Set_Shader(shader);
	DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices
	//replace texture alpha with vertex alpha
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);

	pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(_TRANS_LIT_TEX_VERTEX));

	reset();
	return true;
}

Int ScreenBWFilterDOT3::set(FilterModes mode)
{
	if (mode > FM_NULL_MODE)
	{	//rendering a quad with redirected rendering surface tinted by pixel shader

		if (m_fadeDirection > 0)
		{	//turning effect on
			m_curFadeFrame++;
			Int fade = m_curFadeFrame;

			if (fade<m_fadeFrames)
			{
				m_curFadeValue = (Real)fade/(Real)m_fadeFrames;
			}
			else
			{
				m_curFadeFrame = 0;
				m_curFadeValue = 1.0f;
				m_fadeDirection = 0;
			}
		}
		else
		if (m_fadeDirection < 0)
		{	//turning effect off
			m_curFadeFrame++;
			Int fade = m_curFadeFrame;
			if (fade<m_fadeFrames)
			{
				m_curFadeValue = 1.0f - (Real)fade/(Real)m_fadeFrames;
			}
			else
			{	m_curFadeValue = 0.0f;
				TheTacticalView->setViewFilterMode(FM_NULL_MODE);
				TheTacticalView->setViewFilter(FT_NULL_FILTER);
				m_curFadeFrame = 0;
				m_fadeDirection = 0;
			}
		}

		VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
		DX8Wrapper::Set_Material(vmat);
		REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.
		DX8Wrapper::Set_Shader(ShaderClass::_PresetOpaqueShader);
		DX8Wrapper::Set_Texture(0,nullptr);
		DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

		DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC,D3DCMP_ALWAYS);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE,FALSE);
		DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

		return true;
	}
	return false;
}

void ScreenBWFilterDOT3::reset()
{
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(0,nullptr);	//previously rendered frame inside this texture
	DX8Wrapper::Invalidate_Cached_Render_States();
}

Int ScreenBWFilterDOT3::shutdown()
{
	return TRUE;
}

/*=========  ScreenCrossFadeFilter	=============================================================*/
///Fades screen between 2 different views of the scene with both being visible at once.

Int ScreenCrossFadeFilter::m_fadeFrames;
Int ScreenCrossFadeFilter::m_curFadeFrame;
Real ScreenCrossFadeFilter::m_curFadeValue;
Int ScreenCrossFadeFilter::m_fadeDirection;
TextureClass *ScreenCrossFadeFilter::m_fadePatternTexture=nullptr;
Bool ScreenCrossFadeFilter::m_skipRender = FALSE;

ScreenCrossFadeFilter screenCrossFadeFilter;

///List of different BW shader implementations in order of preference
///@todo: Add a version that doesn't require pixel shader
W3DFilterInterface *ScreenCrossFadeFilterList[]=
{
	&screenCrossFadeFilter,
	nullptr
};

Int ScreenCrossFadeFilter::init()
{
	if (!TheDisplay)
		return FALSE;	//effect is useless without a view so no point initializing for the WB, etc.

	m_curFadeFrame = 0;

	if (!W3DShaderManager::canRenderToTexture())
		// Have to be able to render to texture.
		return FALSE;

	//Load an alpha mask texture that will mix foreground/background views.
	m_fadePatternTexture=WW3DAssetManager::Get_Instance()->Get_Texture("exmask_g.tga");
	if (!m_fadePatternTexture)
		return FALSE;
	m_fadePatternTexture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	m_fadePatternTexture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	m_fadePatternTexture->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);

	W3DFilters[FT_VIEW_CROSSFADE]=&screenCrossFadeFilter;

	return TRUE;
}

Bool ScreenCrossFadeFilter::updateFadeLevel()
{
	if (m_fadeDirection > 0)
	{	//turning effect on
		m_curFadeFrame++;
		Int fade = m_curFadeFrame;

		if (fade<m_fadeFrames)
		{
			m_curFadeValue = (Real)fade/(Real)m_fadeFrames;
		}
		else
		{
			m_curFadeFrame = 0;
			m_curFadeValue = 1.0f;
			m_fadeDirection = 0;
			return false;
		}
	}
	else
	if (m_fadeDirection < 0)
	{	//turning effect off
		Int fade = m_curFadeFrame;
		if (fade<m_fadeFrames)
		{
			m_curFadeValue = 1.0f - (Real)fade/(Real)m_fadeFrames;
			m_curFadeFrame++;
		}
		else
		{	m_curFadeValue = 0.0f;
			TheTacticalView->setViewFilterMode(FM_NULL_MODE);
			TheTacticalView->setViewFilter(FT_NULL_FILTER);
			m_curFadeFrame = 0;
			m_fadeDirection = 0;
			return false;
		}
	}
	return true;
}

Bool ScreenCrossFadeFilter::preRender(Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	if (updateFadeLevel())
	{	//if fade has not completed
		W3DShaderManager::startRenderToTexture();
		scenePassMode=SCENE_PASS_ALPHA_MASK;
		skipRender = false;
		m_skipRender=true;	//tell the postRender function not to draw into framebuffer yet.
		return true;
	}
	//fade must have completed
	return true;
}

Bool ScreenCrossFadeFilter::postRender(FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender)
{
	IDirect3DTexture8 * tex;

	if (m_skipRender)
	{
		//don't render anything to frame buffer because we still need to draw the new scene
		//that we're fading into.  Okay to render on the next call.
		m_skipRender = false;
		doExtraRender = TRUE;
		tex =	W3DShaderManager::endRenderToTexture();
		return true;
	}

	tex=W3DShaderManager::getRenderTexture();

	DEBUG_ASSERTCRASH(tex, ("Require last rendered texture."));
	if (!tex) return false;
	if (!set(mode)) return false;

	LPDIRECT3DDEVICE8 pDev=DX8Wrapper::_Get_D3D_Device8();

	struct _TRANS_LIT_TEX_VERTEX {
		Vector4 p;
		DWORD color;   // diffuse color
		float	u;
		float	v;
		float	u1;
		float	v1;
	} v[4];

	Int xpos, ypos, width, height;
	Real radius = 0.0f;

	DX8Wrapper::_Get_D3D_Device8()->SetTexture(0,tex);	//previously rendered frame inside this texture
	if (mode == FM_VIEW_CROSSFADE_CIRCLE)
	{	DX8Wrapper::_Get_D3D_Device8()->SetTexture(1,m_fadePatternTexture->Peek_D3D_Texture());
		//Use the current fade level to scale the mask texture, for other modes the texture
		//comes pre-scaled so doesn't require uv scaling.
		radius = (1.0f-m_curFadeValue)*2.0f;
		if (radius <= 0)
			radius = 0.01f;
		radius = 0.5f/radius;
	}

	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

/*	Real radius = (1.0f-m_curFadeValue);
	if (radius <= 0)
		radius = 0.01f;
	radius = 25.0f-radius*24.75f;
*/
	//bottom right
	v[0].p = Vector4( xpos+width-0.5f, ypos+height-0.5f, 0.0f, 1.0f );
	v[0].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	v[0].u1 = 0.5f+radius;	v[0].v1 = 0.5f+radius;
	//top right
	v[1].p = Vector4( xpos+width-0.5f, ypos-0.5f, 0.0f, 1.0f );
	v[1].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	v[1].u1 = 0.5f+radius;	v[1].v1 = 0.5f-radius;
	//bottom left
	v[2].p = Vector4(  xpos-0.5f, ypos+height-0.5f, 0.0f, 1.0f );
	v[2].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	v[2].u1 = 0.5f-radius;	v[2].v1 = 0.5f+radius;
	//top left
	v[3].p = Vector4(  xpos-0.5f,  ypos-0.5f, 0.0f, 1.0f );
	v[3].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	v[3].u1 = 0.5f-radius;	v[3].v1 = 0.5f-radius;

	DWORD diffuse = 0xffffffff;//((Int)((m_curFadeValue) * 255.0f) << 24) | 0x00ffffff;	//store alpha value in vertex diffuse

	v[0].color = diffuse;
	v[1].color = diffuse;
	v[2].color = diffuse;
	v[3].color = diffuse;

	//draw polygons like this is very inefficient but for only 2 triangles, it's
	//not worth bothering with index/vertex buffers.
	DX8_SET_FVF(pDev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX2);

//		m_pDev->SetTextureStageState(0,D3DTSS_MAGFILTER,D3DTEXF_POINT);
//		m_pDev->SetTextureStageState(0,D3DTSS_MINFILTER,D3DTEXF_POINT);

	pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(_TRANS_LIT_TEX_VERTEX));

	reset();
	return true;
}

Int ScreenCrossFadeFilter::set(FilterModes mode)
{
	if (mode > FM_NULL_MODE)
	{	//rendering a quad with redirected rendering surface
		VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
		DX8Wrapper::Set_Material(vmat);
		REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.
		DX8Wrapper::Set_Shader(ShaderClass::_PresetAlphaShader);
		DX8Wrapper::Set_Texture(0,nullptr);
		DX8Wrapper::Set_Texture(1,nullptr);
		DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

		if (mode == FM_VIEW_CROSSFADE_CIRCLE)
		{	//cross-fading using circle mask stored in stage 1
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_MODULATE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXCOORDINDEX, 1 );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_MIPFILTER, D3DTEXF_NONE);
		}

		DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC,D3DCMP_ALWAYS);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE,FALSE);

		return true;
	}
	return false;
}

void ScreenCrossFadeFilter::reset()
{
	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(0,nullptr);	//previously rendered frame inside this texture
	DX8Wrapper::Invalidate_Cached_Render_States();
}

Int ScreenCrossFadeFilter::shutdown()
{
	REF_PTR_RELEASE(m_fadePatternTexture);

	return TRUE;
}

/*=========  ScreenMotionBlurFilter	=============================================================*/
///applies motion blur to viewport.

ScreenMotionBlurFilter screenMotionBlurFilter;

Coord3D ScreenMotionBlurFilter::m_zoomToPos;
Bool ScreenMotionBlurFilter::m_zoomToValid = false;

ScreenMotionBlurFilter::ScreenMotionBlurFilter():
m_decrement(false),
m_maxCount(0),
m_lastFrame(0),
m_skipRender(false)
{
}
///List of different motion blur implementations in order of preference
W3DFilterInterface *ScreenMotionBlurFilterList[]=
{
	&screenMotionBlurFilter,
	nullptr
};

Int ScreenMotionBlurFilter::init()
{
	if (!W3DShaderManager::canRenderToTexture()) {
		// Have to be able to render to texture.
		return false;
	}
	W3DFilters[FT_VIEW_MOTION_BLUR_FILTER]=this;
	return true;
}

Bool ScreenMotionBlurFilter::preRender(Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	skipRender = m_skipRender;
	W3DShaderManager::startRenderToTexture();
	return true;
}

Bool ScreenMotionBlurFilter::postRender(FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender)
{
	IDirect3DTexture8 * tex =	W3DShaderManager::endRenderToTexture();
	DEBUG_ASSERTCRASH(tex, ("Require rendered texture."));
	if (!tex) return false;
	if (!set(mode)) return false;

	LPDIRECT3DDEVICE8 pDev=DX8Wrapper::_Get_D3D_Device8();

	Bool continueEffect = true;
	struct _TRANS_LIT_TEX_VERTEX {
		Vector4 p;
		DWORD color;   // diffuse color
		float	u;
		float	v;
	} v[4];

	Int xpos, ypos, width, height;

	DX8Wrapper::_Get_D3D_Device8()->SetTexture(0,tex);	//previously rendered frame inside this texture
	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

	//bottom right
	v[0].p = Vector4( xpos+width-0.5f, ypos+height-0.5f, 0.0f, 1.0f );
	v[0].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top right
	v[1].p = Vector4( xpos+width-0.5f, ypos-0.5f, 0.0f, 1.0f );
	v[1].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	//bottom left
	v[2].p = Vector4(  xpos-0.5f, ypos+height-0.5f, 0.0f, 1.0f );
	v[2].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top left
	v[3].p = Vector4(  xpos-0.5f,  ypos-0.5f, 0.0f, 1.0f );
	v[3].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	v[0].color = 0xffffffff;
	v[1].color = 0xffffffff;
	v[2].color = 0xffffffff;
	v[3].color = 0xffffffff;


	if (m_additive) {
		DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND,D3DBLEND_ONE);
	} else {
		DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
	}
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE,false);
	//draw polygons like this is very inefficient but for only 2 triangles, it's
	//not worth bothering with index/vertex buffers.
	DX8Wrapper::Apply_Render_State_Changes();
	DX8_SET_FVF(pDev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);

	Coord2D center;
	center.x = 0.5f;
	center.y = 0.5f;
	Bool pan = false;
	if (mode>=FM_VIEW_MB_PAN_ALPHA) {
		Real len = sqrt(scrollDelta.x*scrollDelta.x + scrollDelta.y*scrollDelta.y);
		//center.x += 0.5f * (scrollDelta.x/len);
		center.y -= 0.5f; // * (scrollDelta.y/len);
		m_decrement = false;
		m_maxCount = (len*200*m_panFactor/(Real)DEFAULT_PAN_FACTOR);
		if (m_maxCount<m_panFactor/2)
			m_maxCount = m_panFactor/2;
		if (m_maxCount>m_panFactor)
			m_maxCount=m_panFactor;
		pan = true;
		m_priorDelta = scrollDelta;
	} else if (mode == FM_VIEW_MB_END_PAN_ALPHA) {
		Real len = sqrt(m_priorDelta.x*m_priorDelta.x + m_priorDelta.y*m_priorDelta.y);
		center.x += 0.5f * (m_priorDelta.x/len);
		center.y -= 0.5f * (m_priorDelta.y/len);
		m_decrement = false;
		m_maxCount--;
		if (m_maxCount<2) {
			continueEffect = false;
		}
		pan = true;
	}


	m_skipRender = false;
	if (!pan && m_lastFrame != TheGameLogic->getFrame()) {
		if (m_decrement) {
			m_maxCount-=COUNT_STEP;
			if (m_maxCount<1) {
				m_decrement = false;
				continueEffect = false;
			}	else {
				m_skipRender = true;
			}
		} else {
			m_maxCount+=COUNT_STEP;
			if (m_maxCount>=MAX_COUNT) {
				m_decrement = true;
				if (m_doZoomTo && m_zoomToValid) {
					TheTacticalView->lookAt(&m_zoomToPos);
				} else {
					continueEffect = false;
				}
			}	else {
				m_skipRender = true;
			}
		}
	}
	Int	 i, j;
	if (!pan) {
		for (i=0; i<4; i++) {
			Real factor = 1.0f - (m_maxCount/(Real)MAX_COUNT)*0.90f;
			factor = sqrt(factor);
			v[i].u = ((v[i].u-center.x)*factor) + center.x;
			v[i].v = ((v[i].v-center.y)*factor) + center.y;
		}
	}
	pDev->SetTextureStageState(0,D3DTSS_ALPHAARG1, D3DTA_CURRENT);
	pDev->SetTextureStageState(0,D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
	pDev->SetTextureStageState(0,D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(_TRANS_LIT_TEX_VERTEX));
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE,true);

	DX8Wrapper::Apply_Render_State_Changes();
	{
		Int limit = m_maxCount;
		if (m_maxCount>30) limit = 30;
		for (j=0; j<limit; j++) {
			for (i=0; i<4; i++) {
				Real factor = 0.99f;
				if (m_additive) factor = 0.98f;
				Int alpha = 0x15;
				if (m_additive) {
					alpha = 0x09;
					if (m_maxCount>limit) {
						alpha += (m_maxCount-limit)/5;
					}
					if (m_maxCount==MAX_COUNT) alpha += 60;
				}
				v[i].color = (alpha<<24)|0x00ffffff; //
				if (pan) {
					v[i].u = ((v[i].u-center.x)*(factor+.006)) + center.x;
					v[i].v = ((v[i].v-center.y)*factor) + center.y;
				} else {
					v[i].u = ((v[i].u-center.x)*factor) + center.x;
					v[i].v = ((v[i].v-center.y)*factor) + center.y;
				}
			}
			pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(_TRANS_LIT_TEX_VERTEX));

		}
	}
	m_lastFrame = TheGameLogic->getFrame();
	if (pan){
		m_skipRender = false;
	}
	reset();
	if (!continueEffect) {
		m_zoomToValid = false;
	}
	return continueEffect;
}

Bool ScreenMotionBlurFilter::setup(FilterModes mode)
{

	m_additive = false;

	if (mode == FM_VIEW_MB_IN_AND_OUT_SATURATE ||
			mode == FM_VIEW_MB_IN_SATURATE ||
			mode == FM_VIEW_MB_OUT_SATURATE) {
		m_additive = true;
	}

	m_doZoomTo = false;
	if (mode == FM_VIEW_MB_IN_AND_OUT_SATURATE ||
			mode == FM_VIEW_MB_IN_AND_OUT_ALPHA ) {
		m_doZoomTo = true;
	}
	if (mode >= FM_VIEW_MB_PAN_ALPHA)	{
		m_panFactor = (int)mode - FM_VIEW_MB_PAN_ALPHA;
		if (m_panFactor<1) m_panFactor = DEFAULT_PAN_FACTOR;
	}
	m_skipRender = false;
	if (mode != FM_VIEW_MB_END_PAN_ALPHA)
		m_maxCount = 0;
	m_decrement = false;
	m_skipRender = false;
	switch (mode) {
		case FM_VIEW_MB_OUT_SATURATE:
		case FM_VIEW_MB_OUT_ALPHA:
			m_maxCount = MAX_COUNT;
			m_decrement = TRUE;
			break;
	}
	return true;
}

Int ScreenMotionBlurFilter::set(FilterModes mode)
{
	if (mode > FM_NULL_MODE)
	{	//rendering a quad with redirected rendering surface motion blurred

		VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
		DX8Wrapper::Set_Material(vmat);
		REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.
		DX8Wrapper::Set_Shader(ShaderClass::_PresetOpaqueShader);
		DX8Wrapper::Set_Texture(0,nullptr);
		DX8Wrapper::Set_Texture(1,nullptr);
		DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

		DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC,D3DCMP_ALWAYS);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE,FALSE);
		DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices
	}
	return TRUE;
}

void ScreenMotionBlurFilter::reset()
{
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(0,nullptr);	//previously rendered frame inside this texture
	DX8Wrapper::Invalidate_Cached_Render_States();
}

Int ScreenMotionBlurFilter::shutdown()
{
	return TRUE;
}

/*===========================================================================================*/
/*=========      Shroud Shaders	=============================================================*/
/*===========================================================================================*/

///Shroud layer rendering shader
class ShroudTextureShader : public W3DShaderInterface
{
	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
	Int m_stageOfSet;
} shroudTextureShader;

///List of different shroud shader implementations in order of preference
W3DShaderInterface *ShroudShaderList[]=
{
	&shroudTextureShader,
	nullptr
};

//#define SHROUD_STRETCH_FACTOR	(1.0f/MAP_XY_FACTOR)	//1 texel per heightmap cell width

Int ShroudTextureShader::init()
{
	W3DShaders[W3DShaderManager::ST_SHROUD_TEXTURE]=&shroudTextureShader;
	W3DShadersPassCount[W3DShaderManager::ST_SHROUD_TEXTURE]=1;

	return TRUE;
}

//Setup a texture projection in the given stage that applies our shroud.
Int ShroudTextureShader::set(Int stage)
{
	//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vmat);
	REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.
	DX8Wrapper::Set_Texture(stage, W3DShaderManager::getShaderTexture(0));	//shroud always stored in texture 0

	if (stage == 0)
	{
#if defined(RTS_DEBUG)
	if (TheGlobalData && TheGlobalData->m_fogOfWarOn)
		DX8Wrapper::Set_Shader(ShaderClass::_PresetAlphaSpriteShader);
	else
		DX8Wrapper::Set_Shader(ShaderClass::_PresetMultiplicativeSpriteShader);
#else
	DX8Wrapper::Set_Shader(ShaderClass::_PresetMultiplicativeSpriteShader);
#endif
	}
	DX8Wrapper::Apply_Render_State_Changes();

	DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC,D3DCMP_EQUAL);

	//We need to scale so shroud texel stretches over one full terrain cell.  Each texel
	//is 1/128 the size of full texture. (assuming 128x128 vid-mem texture).
	W3DShroud *shroud;
	if ((shroud=TheTerrainRenderObject->getShroud()) != nullptr)
	{	///@todo: All this code really only need to be done once per camera/view.  Find a way to optimize it out.
		D3DMATRIX curView;
		DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

		D3DMATRIX inv;
		float det;
		Invert_D3DMATRIX(inv, &det, curView);

		D3DMATRIX scale,offset;

		//We need to make all world coordinates be relative to the heightmap data origin since that
		//is where the shroud begins.

		float xoffset = 0;
		float yoffset = 0;
		Real width=shroud->getCellWidth();
		Real height=shroud->getCellHeight();

		if (TheTerrainRenderObject->getMap())
		{	//subtract origin position from all coordinates.  Origin is shifted by 1 cell width/height to allow for unused border texels.
			xoffset = -(float)shroud->getDrawOriginX() + width;
			yoffset = -(float)shroud->getDrawOriginY() + height;
		}

		Set_D3DMATRIX_Translation(offset, xoffset, yoffset,0);

		width = 1.0f/(width*shroud->getTextureWidth());
		height = 1.0f/(height*shroud->getTextureHeight());
		Set_D3DMATRIX_Scaling(scale, width, height, 1);
		curView = (inv * offset) * scale;
		DX8Wrapper::_Set_DX8_Transform((D3DTRANSFORMSTATETYPE )(D3DTS_TEXTURE0+stage), curView);
	}
	m_stageOfSet=stage;
	return TRUE;
}

void ShroudTextureShader::reset()
{
	DX8Wrapper::Set_Texture(m_stageOfSet,nullptr);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
	DX8Wrapper::Set_DX8_Texture_Stage_State(m_stageOfSet,  D3DTSS_TEXCOORDINDEX, m_stageOfSet);
	DX8Wrapper::Set_DX8_Texture_Stage_State(m_stageOfSet,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
}

///Shroud layer rendering shader
class FlatShroudTextureShader : public W3DShaderInterface
{
	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
	Int m_stageOfSet;
} flatShroudTextureShader;

///List of different shroud shader implementations in order of preference
W3DShaderInterface *FlatShroudShaderList[]=
{
	&flatShroudTextureShader,
	nullptr
};

//#define SHROUD_STRETCH_FACTOR	(1.0f/MAP_XY_FACTOR)	//1 texel per heightmap cell width

Int FlatShroudTextureShader::init()
{
	W3DShaders[W3DShaderManager::ST_FLAT_SHROUD_TEXTURE]=&flatShroudTextureShader;
	W3DShadersPassCount[W3DShaderManager::ST_FLAT_SHROUD_TEXTURE]=1;

	return TRUE;
}

//Setup a texture projection in the given stage that applies our shroud.
Int FlatShroudTextureShader::set(Int stage)
{
	//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
	if (stage < 2)
		DX8Wrapper::Set_Texture(stage, W3DShaderManager::getShaderTexture(stage));
	else	//stages larger than 1 are not supported by W3D so set them directly
		DX8Wrapper::Set_DX8_Texture(stage, W3DShaderManager::getShaderTexture(stage)->Peek_D3D_Texture());

	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, D3DTSS_COLORARG2, D3DTA_CURRENT );
	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, D3DTSS_COLOROP,   D3DTOP_MODULATE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
	//DX8Wrapper::Apply_Render_State_Changes();

	DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

	//We need to scale so shroud texel stretches over one full terrain cell.  Each texel
	//is 1/128 the size of full texture. (assuming 128x128 vid-mem texture).
	W3DShroud *shroud;
	if ((shroud=TheTerrainRenderObject->getShroud()) != nullptr)
	{	///@todo: All this code really only need to be done once per camera/view.  Find a way to optimize it out.
		D3DMATRIX curView;
		DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

		D3DMATRIX inv;
		float det;
		Invert_D3DMATRIX(inv, &det, curView);

		D3DMATRIX scale,offset;

		//We need to make all world coordinates be relative to the heightmap data origin since that
		//is where the shroud begins.

		float xoffset = 0;
		float yoffset = 0;
		Real width=shroud->getCellWidth();
		Real height=shroud->getCellHeight();

		if (TheTerrainRenderObject->getMap())
		{	//subtract origin position from all coordinates.  Origin is shifted by 1 cell width/height to allow for unused border texels.
			xoffset = -(float)shroud->getDrawOriginX() + width;
			yoffset = -(float)shroud->getDrawOriginY() + height;
		}

		Set_D3DMATRIX_Translation(offset, xoffset, yoffset,0);

		width = 1.0f/(width*shroud->getTextureWidth());
		height = 1.0f/(height*shroud->getTextureHeight());
		Set_D3DMATRIX_Scaling(scale, width, height, 1);
		curView = (inv * offset) * scale;
		DX8Wrapper::_Set_DX8_Transform((D3DTRANSFORMSTATETYPE )(D3DTS_TEXTURE0+stage), curView);
	}
	m_stageOfSet=stage;
	return TRUE;
}

void FlatShroudTextureShader::reset()
{
	if (m_stageOfSet < MAX_TEXTURE_STAGES)
		DX8Wrapper::Set_Texture(m_stageOfSet,nullptr);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
	DX8Wrapper::Set_DX8_Texture_Stage_State(m_stageOfSet,  D3DTSS_TEXCOORDINDEX, m_stageOfSet);
	DX8Wrapper::Set_DX8_Texture_Stage_State(m_stageOfSet,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
}

///Mask layer rendering shader
class MaskTextureShader : public W3DShaderInterface
{
	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
} maskTextureShader;

///List of different shroud shader implementations in order of preference
W3DShaderInterface *MaskShaderList[]=
{
	&maskTextureShader,
	nullptr
};

Int MaskTextureShader::init()
{
	W3DShaders[W3DShaderManager::ST_MASK_TEXTURE]=&maskTextureShader;
	W3DShadersPassCount[W3DShaderManager::ST_MASK_TEXTURE]=1;

	return TRUE;
}

Int MaskTextureShader::set(Int pass)
{
	Real fadeLevel=ScreenCrossFadeFilter::getCurrentFadeValue();

	//Use the current fade level to scale the mask texture
	Real radius = (1.0f-fadeLevel)*2.0f;
	if (radius <= 0)
		radius = 0.01f;
	radius = 0.5f/radius;

	//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vmat);
	REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.

	//For now we're always going to project the texture coming from the crossfade effect
	DX8Wrapper::Set_Texture(0, ScreenCrossFadeFilter::getCurrentMaskTexture());
	ShaderClass shader=ShaderClass::_PresetOpaqueShader;
	shader.Set_Primary_Gradient(ShaderClass::GRADIENT_DISABLE);
	DX8Wrapper::Set_Shader(shader);
	DX8Wrapper::Apply_Render_State_Changes();

	D3DMATRIX curView;
	DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

	DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

	D3DMATRIX inv;
	float det;

	//Get inverse view matrix so we can transform camera space points back to world space
	Invert_D3DMATRIX(inv, &det, curView);

	D3DMATRIX scale,offset,offsetTextureCenter;
	Coord3D centerPos;
	centerPos.zero();

	//Find center of projection (this should be returned from some other filter, etc. but
	//for now assume terrain location at center of screen.
	if (TheTacticalView)
	{	Int xpos,ypos;

		TheTacticalView->getOrigin(&xpos,&ypos);

		ICoord2D screenPos;
		screenPos.x=(Real)TheTacticalView->getWidth()*0.5f;
		screenPos.y=(Real)TheTacticalView->getHeight()*0.5f;
		TheTacticalView->screenToTerrain(&screenPos,&centerPos);
	}

	Set_D3DMATRIX_Translation(offset, -centerPos.x, -centerPos.y,0);

	Set_D3DMATRIX_Translation(offsetTextureCenter, 0.5f, 0.5f, 0);	//shift coordinates so center of projection falls at uv 0.5,0.5

	Real worldTexelWidth=(1.0f-fadeLevel)*25.0f;	//9 worked well for circle but weird shape requires more stretch to cover.
	Real worldTexelHeight=(1.0f-fadeLevel)*25.0f;

	///@todo: Fix this to work with non 128x128 textures.
	if (worldTexelWidth != 0 && worldTexelHeight != 0)
	{
		Real widthScale = 1.0f/(worldTexelWidth*128.0f);
		Real heightScale = 1.0f/(worldTexelHeight*128.0f);
		Set_D3DMATRIX_Scaling(scale, widthScale, heightScale, 1);
		curView = ((inv * offset) * scale)*offsetTextureCenter;
	}
	else
	{
		Set_D3DMATRIX_Scaling(scale, 0, 0, 1);	//scaling by 0 will set uv coordinates to 0,0
		curView = ((inv * offset) * scale);
	}

	DX8Wrapper::_Set_DX8_Transform(D3DTS_TEXTURE0, curView);

	return TRUE;
}

void MaskTextureShader::reset()
{
	DX8Wrapper::Set_Texture(0,nullptr);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_TEXCOORDINDEX, 0);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
}

/*===========================================================================================*/
/*=========      Shadow Map Depth Shader	=================================================*/
/*===========================================================================================*/

// The sun the specular pass lights with, set once a frame by the scene.
static Vector3 SpecularToSun(0.0f, 0.0f, 1.0f);
static Vector3 SpecularColor(0.0f, 0.0f, 0.0f);
static Real SpecularPower = 24.0f;
static Bool SpecularDebug = FALSE;
static Int SpecularPassCount = 0;

// The bump detail the specular pass shades, set once a frame by the scene.
static Vector3 SpecularSunDiffuse(0.0f, 0.0f, 0.0f);
static Bool BumpEnabled = FALSE;
static Real BumpAmbient = 0.0f;
static Real BumpHeight = 0.0f;
static Real BumpNormalMapStrength = 1.0f;
static Bool BumpSupported = FALSE;
static Int BumpDerivedCount = 0;
static Int BumpNormalMapCount = 0;

// The glow masks the specular pass adds, set once a frame by the scene.
static Real EmissiveIntensity = 0.0f;
static Int EmissiveMapCount = 0;

// The point lights the terrain and specular shaders add, set once a frame by the scene.
static W3DShaderManager::PixelLight PixelLights[W3DShaderManager::MAX_PIXEL_LIGHTS];
static Int PixelLightCount = 0;
enum { PIXEL_LIGHT_REGISTERS = W3DShaderManager::MAX_PIXEL_LIGHTS * 2 + (W3DShaderManager::MAX_PIXEL_LIGHTS + 3) / 4 };
// The unit lights in camera space, packed once per set of lights and view.
static Vector4 UnitLightBlock[PIXEL_LIGHT_REGISTERS];
static D3DMATRIX UnitLightView;
static Bool UnitLightBlockValid = FALSE;
static Bool TerrainPixelLightsLoaded = FALSE;
static Bool UnitPixelLightsLoaded = FALSE;

// Bisects per-pixel light faults without a rebuild. CONTRA_PIXELLIGHTS=0 leaves every light
// to the vertex lighting, 1 draws them per pixel on the terrain only, 2 on units as well.
enum { PIXEL_LIGHTS_OFF = 0, PIXEL_LIGHTS_TERRAIN = 1, PIXEL_LIGHTS_ALL = 2 };

static Int Get_Pixel_Light_Mode()
{
	const char *value = getenv("CONTRA_PIXELLIGHTS");
	return (value != nullptr) ? atoi(value) : PIXEL_LIGHTS_ALL;
}

// Packs the lights as pointlights.hlsli reads them, all for the terrain in world space or the unit ones in view space.
static Int Pack_Pixel_Lights(Vector4 *constants, const D3DMATRIX *view, Bool terrain)
{
	const Int slots = terrain ? W3DShaderManager::MAX_PIXEL_LIGHTS : W3DShaderManager::MAX_UNIT_PIXEL_LIGHTS;
	memset(constants, 0, sizeof(Vector4) * PIXEL_LIGHT_REGISTERS);

	Int i = 0;
	for (Int index = 0; index < PixelLightCount && i < slots; index++)
	{
		const W3DShaderManager::PixelLight &light = PixelLights[index];
		if (!terrain && !light.unitLit)
		{
			continue;
		}

		Vector3 position = light.position;
		if (view != nullptr)
		{
			const Vector3 world = position;
			position.X = world.X * view->m[0][0] + world.Y * view->m[1][0] + world.Z * view->m[2][0] + view->m[3][0];
			position.Y = world.X * view->m[0][1] + world.Y * view->m[1][1] + world.Z * view->m[2][1] + view->m[3][1];
			position.Z = world.X * view->m[0][2] + world.Y * view->m[1][2] + world.Z * view->m[2][2] + view->m[3][2];
		}

		// Full strength inside the inner radius, falling to nothing at the outer one.
		const Real scale = 1.0f / (light.outerRadius - light.innerRadius);
		constants[i * 2].Set(position.X, position.Y, position.Z, scale);
		constants[i * 2 + 1].Set(light.diffuse.X, light.diffuse.Y, light.diffuse.Z, 1.0f + light.innerRadius * scale);
		(&constants[slots * 2 + i / 4].X)[i % 4] = light.ambientScale;
		i++;
	}

	return slots * 2 + (slots + 3) / 4;
}

static void Set_Pixel_Light_Constants(Int firstRegister, const D3DMATRIX *view, Bool terrain)
{
	Vector4 constants[PIXEL_LIGHT_REGISTERS];
	DX8Wrapper::Set_Pixel_Shader_Constant(firstRegister, constants, Pack_Pixel_Lights(constants, view, terrain));
}

// The terrain normal maps, set once a frame by the scene.
static Bool TerrainBumpEnabled = FALSE;
static Real TerrainBumpStrength = 1.0f;
static Bool TerrainBumpDebug = FALSE;
static Int TerrainBumpCount = 0;

#if defined(BUILD_WITH_D3D9)

///Writes caster depth into the shadow map. Only the D3D9 backend has the shader model for it.
///
///Casters draw with their own textures and shaders, and this overrides the state those
///shaders apply, so a cutout or blended mesh casts its shape rather than a solid block.
class ShadowDepthShader : public W3DShaderInterface
{
public:
	ShadowDepthShader() : m_dwPixelShader(0), m_dwInstanceShader(0), m_dwSkinShader(0) {}

	virtual Int set(Int pass) override;
	virtual Int init() override;
	virtual void reset() override;
	virtual Int shutdown() override;

	void applyOverride(const ShaderClass &shader);

protected:

	DWORD m_dwPixelShader;	///<packed path only; the hardware path writes depth with no shader.
	DWORD m_dwInstanceShader;	///<draws groups of identical casters in one call; the pass runs without it.
	DWORD m_dwSkinShader;	///<deforms skinned casters on the GPU; the pass runs without it.
} shadowDepthShader;

W3DShaderInterface *ShadowDepthShaderList[]=
{
	&shadowDepthShader,
	nullptr
};

static void Apply_Shadow_Depth_Override(const ShaderClass &shader)
{
	shadowDepthShader.applyOverride(shader);
}

// Matches the reference ShaderClass uses for its own alpha test, so a shadow's edge lines
// up with the cutout the player sees.
static const DWORD SHADOW_DEPTH_ALPHA_REFERENCE = 0x60;

Int ShadowDepthShader::init()
{
	// The map already checked the device for shader model 2, so it decides for both.
	if (TheW3DShadowMap == nullptr || !TheW3DShadowMap->isAvailable())
	{
		return FALSE;
	}

	const Bool packed = (TheW3DShadowMap->getDepthMode() == W3DShadowMap::DEPTH_MODE_PACKED);
	if (packed)
	{
		if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\shadowdepthpacked.pso",
				nullptr, 0, false, &m_dwPixelShader)))
		{
			return FALSE;
		}
	}

	// The instancing module builds its own declarations, so this one only has to be valid.
	DWORD declaration[] =
	{
		D3DVSD_STREAM(0),
		D3DVSD_REG(0, D3DVSDT_FLOAT3),
		D3DVSD_END()
	};
	if (FAILED(W3DShaderManager::LoadAndCreateD3DShader(packed ? "shaders\\instancedepthpacked.vso" : "shaders\\instancedepth.vso",
			declaration, 0, true, &m_dwInstanceShader)))
	{
		m_dwInstanceShader = 0;
	}
	if (FAILED(W3DShaderManager::LoadAndCreateD3DShader(packed ? "shaders\\skindepthpacked.vso" : "shaders\\skindepth.vso",
			declaration, 0, true, &m_dwSkinShader)))
	{
		m_dwSkinShader = 0;
	}

	W3DShaders[W3DShaderManager::ST_SHADOW_DEPTH]=&shadowDepthShader;
	W3DShadersPassCount[W3DShaderManager::ST_SHADOW_DEPTH]=1;

	return TRUE;
}

// Set once around the whole depth pass rather than per caster.
Int ShadowDepthShader::set(Int pass)
{
	DX8Wrapper::Set_Apply_Hook(&Apply_Shadow_Depth_Override);
	if (m_dwInstanceShader != 0)
	{
		DX8InstancingClass::Begin_Shadow_Depth_Pass(Peek_D3D9_Vertex_Shader(m_dwInstanceShader));
	}
	if (m_dwSkinShader != 0)
	{
		DX8SkinningClass::Begin_Shadow_Depth_Pass(Peek_D3D9_Vertex_Shader(m_dwSkinShader));
	}
	return TRUE;
}

void ShadowDepthShader::applyOverride(const ShaderClass &shader)
{
	// The sun winds triangles opposite to the camera, so each shader's cull mode is dropped.
	DX8Wrapper::Set_DX8_Render_State(D3DRS_CULLMODE, D3DCULL_NONE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, TRUE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, FALSE);

	const ShaderClass::SrcBlendFuncType src = shader.Get_Src_Blend_Func();
	const ShaderClass::DstBlendFuncType dst = shader.Get_Dst_Blend_Func();

	// Alpha decides the shape only where the mesh's own shader cuts or blends by it. Many
	// opaque textures carry masks in alpha, so testing those would punch holes in shadows.
	Bool casts = TRUE;
	Bool cutout = FALSE;
	Bool inverted = (src == ShaderClass::SRCBLEND_ONE_MINUS_SRC_ALPHA);

	if (shader.Uses_Alpha())
	{
		cutout = TRUE;
	}
	else if (src != ShaderClass::SRCBLEND_ONE || dst != ShaderClass::DSTBLEND_ZERO)
	{
		// Additive and multiplied passes are glows and effects, which cast nothing.
		casts = FALSE;
	}

	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE, casts);

	// Pushes steep caster surfaces back further than flat ones, which is what keeps them
	// from shadowing themselves without detaching shadows from their bases.
	const float constantBias = TheW3DShadowMap->getCasterDepthBias();
	const float slopeBias = W3DShadowMap::getCasterSlopeBias();
	DX8Wrapper::Set_DX8_Render_State(D3DRS_DEPTHBIAS, *(const DWORD *)&constantBias);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_SLOPESCALEDEPTHBIAS, *(const DWORD *)&slopeBias);

	if (m_dwPixelShader == 0)
	{
		// Only depth is read back, so colour writes are wasted bandwidth.
		DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE, 0);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, cutout);

		if (cutout)
		{
			DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAREF, inverted ? 0xff - SHADOW_DEPTH_ALPHA_REFERENCE : SHADOW_DEPTH_ALPHA_REFERENCE);
			DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAFUNC, inverted ? D3DCMP_LESSEQUAL : D3DCMP_GREATEREQUAL);
		}
		return;
	}

	// The packed shader writes depth into colour, so its output alpha is not the mesh's
	// and it cuts by the texture itself. Stage 1 hands it the sun view-space position,
	// whose depth maps linearly through the projection's z row because it is orthographic.
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, FALSE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_TEXCOORDINDEX, 0);
	DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
	DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3);

	D3DMATRIX identity;
	Set_D3DMATRIX_Identity(identity);
	DX8Wrapper::_Set_DX8_Transform(D3DTS_TEXTURE1, identity);

	const Matrix4x4 &projection = TheW3DShadowMap->getSunProjection();
	Vector4 depthRow(projection[2][2], projection[2][3], 0.0f, 0.0f);
	DX8Wrapper::Set_Pixel_Shader_Constant(0, &depthRow, 1);

	// x is the cutoff, negative for none, and y selects inverted alpha.
	Vector4 cutoff(cutout ? (Real)SHADOW_DEPTH_ALPHA_REFERENCE / 255.0f : -1.0f, inverted ? 1.0f : 0.0f, 0.0f, 0.0f);
	DX8Wrapper::Set_Pixel_Shader_Constant(1, &cutoff, 1);

	DX8Wrapper::Set_Pixel_Shader(m_dwPixelShader);
}

void ShadowDepthShader::reset()
{
	DX8Wrapper::Set_Apply_Hook(nullptr);
	DX8InstancingClass::End_Pass();
	DX8SkinningClass::End_Pass();
	DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE, 0x0000000f);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_DEPTHBIAS, 0);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_SLOPESCALEDEPTHBIAS, 0);

	if (m_dwPixelShader != 0)
	{
		DX8Wrapper::Set_Pixel_Shader(0);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_TEXCOORDINDEX, 1);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	}

	// The override changed state behind every shader's back, so they all reapply in full.
	ShaderClass::Invalidate();
}

Int ShadowDepthShader::shutdown()
{
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();

	if (device != nullptr)
	{
		DX8_DELETE_PIXEL_SHADER(device, m_dwPixelShader);
		DX8_DELETE_VERTEX_SHADER(device, m_dwInstanceShader);
		DX8_DELETE_VERTEX_SHADER(device, m_dwSkinShader);
	}

	m_dwPixelShader = 0;
	m_dwInstanceShader = 0;
	m_dwSkinShader = 0;

	// Cleared so a failed init after a device reset leaves the pass disabled.
	W3DShaders[W3DShaderManager::ST_SHADOW_DEPTH]=nullptr;
	W3DShadersPassCount[W3DShaderManager::ST_SHADOW_DEPTH]=0;

	return TRUE;
}

// CONTRA_UNITCLOUDS=0 keeps cloud shadows off objects without a rebuild.
static Bool Get_Unit_Clouds_Enabled()
{
	const char *value = getenv("CONTRA_UNITCLOUDS");
	return (value != nullptr) ? atoi(value) != 0 : TRUE;
}

static Bool bindCloudReceiver(Int stage);
static void unbindCloudReceiver(Int stage);

///Multiplies the shadow map into geometry that has already drawn, for fixed-function receivers.
class ShadowMultiplyShader : public W3DShaderInterface
{
public:
	ShadowMultiplyShader() : m_dwPixelShader(0), m_dwCloudPixelShader(0), m_cloudBound(FALSE) {}

	virtual Int set(Int pass) override;
	virtual Int init() override;
	virtual void reset() override;
	virtual Int shutdown() override;

protected:

	DWORD m_dwPixelShader;
	DWORD m_dwCloudPixelShader;
	Bool m_cloudBound;
} shadowMultiplyShader;

W3DShaderInterface *ShadowMultiplyShaderList[]=
{
	&shadowMultiplyShader,
	nullptr
};

Int ShadowMultiplyShader::init()
{
	if (TheW3DShadowMap == nullptr || !TheW3DShadowMap->isAvailable())
	{
		return FALSE;
	}

	const Bool packed = (TheW3DShadowMap->getDepthMode() == W3DShadowMap::DEPTH_MODE_PACKED);
	const char *file = packed ? "shaders\\shadowmultiplypacked.pso" : "shaders\\shadowmultiply.pso";

	if (FAILED(W3DShaderManager::LoadAndCreateD3DShader(file, nullptr, 0, false, &m_dwPixelShader)))
	{
		return FALSE;
	}

	// Without the cloud variant, receivers fall back to the shadow alone.
	const char *cloudFile = packed ? "shaders\\shadowmultiplycloudpacked.pso" : "shaders\\shadowmultiplycloud.pso";
	if (!Get_Unit_Clouds_Enabled() ||
		FAILED(W3DShaderManager::LoadAndCreateD3DShader(cloudFile, nullptr, 0, false, &m_dwCloudPixelShader)))
	{
		m_dwCloudPixelShader = 0;
	}

	W3DShaders[W3DShaderManager::ST_SHADOW_MULTIPLY]=&shadowMultiplyShader;
	W3DShadersPassCount[W3DShaderManager::ST_SHADOW_MULTIPLY]=1;

	return TRUE;
}

// Expects the receiver's geometry and transforms bound and applied, as for the shroud pass.
Int ShadowMultiplyShader::set(Int pass)
{
	if (TheW3DShadowMap == nullptr || !TheW3DShadowMap->bindReceiver(0))
	{
		return FALSE;
	}

	// The geometry is redrawn at the same depth, so EQUAL limits the pass to pixels the
	// first draw wrote and leaves alpha-tested holes alone.
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, TRUE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC, D3DCMP_EQUAL);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, TRUE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, D3DBLEND_ZERO);

	// Fog would pull the factor toward the fog colour and tint the shadow.
	DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGENABLE, FALSE);

	m_cloudBound = pass == W3DShaderManager::SHADOW_MULTIPLY_PASS_CLOUDS && m_dwCloudPixelShader != 0 &&
		bindCloudReceiver(1);

	DX8Wrapper::Set_Pixel_Shader(m_cloudBound ? m_dwCloudPixelShader : m_dwPixelShader);

	return TRUE;
}

void ShadowMultiplyShader::reset()
{
	DX8Wrapper::Set_Pixel_Shader(0);

	if (TheW3DShadowMap != nullptr)
	{
		TheW3DShadowMap->unbindReceiver(0);
	}

	if (m_cloudBound)
	{
		unbindCloudReceiver(1);
		m_cloudBound = FALSE;
	}

	// Z, blend and fog are ShaderClass state, so the next shader set restores them in full.
	ShaderClass::Invalidate();
}

Int ShadowMultiplyShader::shutdown()
{
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();

	if (device != nullptr)
	{
		DX8_DELETE_PIXEL_SHADER(device, m_dwPixelShader);
		DX8_DELETE_PIXEL_SHADER(device, m_dwCloudPixelShader);
	}

	m_dwPixelShader = 0;
	m_dwCloudPixelShader = 0;

	W3DShaders[W3DShaderManager::ST_SHADOW_MULTIPLY]=nullptr;
	W3DShadersPassCount[W3DShaderManager::ST_SHADOW_MULTIPLY]=0;

	return TRUE;
}

///Adds a per-pixel sun highlight and bump shading over geometry that has already drawn.
class SpecularShader : public W3DShaderInterface
{
public:
	enum Bump
	{
		BUMP_NONE,
		BUMP_DERIVED,		///<height taken from the texture's brightness
		BUMP_NORMAL_MAP,	///<the texture's own _nrm normal map
		BUMP_COUNT
	};

	SpecularShader() : m_shadowed(FALSE), m_lit(FALSE)
	{
		for (Int i = 0; i < BUMP_COUNT; i++)
		{
			m_dwShadowedShaders[i] = 0;
			m_dwUnshadowedShaders[i] = 0;
			m_dwLitShadowedShaders[i] = 0;
			m_dwLitUnshadowedShaders[i] = 0;
		}
	}

	virtual Int set(Int pass) override;
	virtual Int init() override;
	virtual void reset() override;
	virtual Int shutdown() override;

	/// Binds one polygon group's texture, and its normal map and shader when it is bumped.
	void setTexture(TextureClass *texture);

protected:

	DWORD m_dwShadowedShaders[BUMP_COUNT];		///<take the sun out in its shadow
	DWORD m_dwUnshadowedShaders[BUMP_COUNT];	///<for when the shadow map is off or holds no depth
	DWORD m_dwLitShadowedShaders[BUMP_COUNT];	///<the same two, also adding the point lights
	DWORD m_dwLitUnshadowedShaders[BUMP_COUNT];
	Bool m_shadowed;							///<which of the two the current pass uses
	Bool m_lit;									///<whether it uses the point light set
} specularShader;

W3DShaderInterface *SpecularShaderList[]=
{
	&specularShader,
	nullptr
};

// Stages the pass generates texcoords on. Each holds one set, in stage order, so the shader
// finds the normal, position and shadow coordinates on the matching registers.
#define SPECULAR_NORMAL_STAGE	1
#define SPECULAR_POSITION_STAGE	2
#define SPECULAR_SHADOW_STAGE	3
// The normal and glow maps come last and read the mesh's UVs from TEXCOORD0.
#define SPECULAR_NORMAL_MAP_STAGE	4
#define SPECULAR_EMISSIVE_STAGE		5

// The bumped shaders take screen-space derivatives, which only ps_2_a and up have.
static Bool Supports_Pixel_Shader_2_a(const DX8Caps *caps)
{
	const D3DCAPS8 &d3dCaps = caps->Get_DX8_Caps();
	return (d3dCaps.PS20Caps.Caps & D3DPS20CAPS_GRADIENTINSTRUCTIONS) != 0 &&
		d3dCaps.PS20Caps.NumTemps >= 22 && d3dCaps.PS20Caps.NumInstructionSlots >= 512;
}

Int SpecularShader::init()
{
	const DX8Caps *caps = DX8Wrapper::Get_Current_Caps();
	if (caps == nullptr || caps->Get_Pixel_Shader_Major_Version() < 2)
	{
		return FALSE;
	}

	static const char *const unshadowedFiles[BUMP_COUNT] =
	{
		"shaders\\specularnoshadow.pso", "shaders\\specularderivednoshadow.pso", "shaders\\specularnormalnoshadow.pso"
	};
	static const char *const shadowedFiles[BUMP_COUNT] =
	{
		"shaders\\specular.pso", "shaders\\specularderived.pso", "shaders\\specularnormal.pso"
	};
	static const char *const packedFiles[BUMP_COUNT] =
	{
		"shaders\\specularpacked.pso", "shaders\\specularderivedpacked.pso", "shaders\\specularnormalpacked.pso"
	};

	if (FAILED(W3DShaderManager::LoadAndCreateD3DShader(unshadowedFiles[BUMP_NONE],
			nullptr, 0, false, &m_dwUnshadowedShaders[BUMP_NONE])))
	{
		return FALSE;
	}

	// Without a shadowed variant the pass still draws, just through shadows too.
	const Bool shadowMap = (TheW3DShadowMap != nullptr && TheW3DShadowMap->isAvailable());
	const char *const *shadowedSet = (shadowMap && TheW3DShadowMap->getDepthMode() == W3DShadowMap::DEPTH_MODE_PACKED)
		? packedFiles : shadowedFiles;
	const Int bumpCount = Supports_Pixel_Shader_2_a(caps) ? BUMP_COUNT : BUMP_NONE + 1;

	for (Int bump = BUMP_NONE; bump < bumpCount; bump++)
	{
		if (bump != BUMP_NONE && FAILED(W3DShaderManager::LoadAndCreateD3DShader(unshadowedFiles[bump],
				nullptr, 0, false, &m_dwUnshadowedShaders[bump])))
		{
			m_dwUnshadowedShaders[bump] = 0;
		}
		if (shadowMap && FAILED(W3DShaderManager::LoadAndCreateD3DShader(shadowedSet[bump],
				nullptr, 0, false, &m_dwShadowedShaders[bump])))
		{
			m_dwShadowedShaders[bump] = 0;
		}
	}

	BumpSupported = (m_dwUnshadowedShaders[BUMP_DERIVED] != 0 || m_dwUnshadowedShaders[BUMP_NORMAL_MAP] != 0);

	// The point lights replace the mesh's fixed-function ones, so every variant has to be there or none is used.
	static const char *const litUnshadowedFiles[BUMP_COUNT] =
	{
		"shaders\\specularlitnoshadow.pso", "shaders\\specularlitderivednoshadow.pso", "shaders\\specularlitnormalnoshadow.pso"
	};
	static const char *const litShadowedFiles[BUMP_COUNT] =
	{
		"shaders\\specularlit.pso", "shaders\\specularlitderived.pso", "shaders\\specularlitnormal.pso"
	};
	static const char *const litPackedFiles[BUMP_COUNT] =
	{
		"shaders\\specularlitpacked.pso", "shaders\\specularlitderivedpacked.pso", "shaders\\specularlitnormalpacked.pso"
	};
	const char *const *litShadowedSet = (shadowMap && TheW3DShadowMap->getDepthMode() == W3DShadowMap::DEPTH_MODE_PACKED)
		? litPackedFiles : litShadowedFiles;

	UnitPixelLightsLoaded = FALSE;
	if (Get_Pixel_Light_Mode() >= PIXEL_LIGHTS_ALL && Supports_Pixel_Shader_2_a(caps))
	{
		UnitPixelLightsLoaded = TRUE;
		for (Int bump = BUMP_NONE; bump < BUMP_COUNT; bump++)
		{
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader(litUnshadowedFiles[bump],
					nullptr, 0, false, &m_dwLitUnshadowedShaders[bump])))
			{
				m_dwLitUnshadowedShaders[bump] = 0;
				UnitPixelLightsLoaded = FALSE;
			}
			if (m_dwShadowedShaders[BUMP_NONE] != 0 && FAILED(W3DShaderManager::LoadAndCreateD3DShader(litShadowedSet[bump],
					nullptr, 0, false, &m_dwLitShadowedShaders[bump])))
			{
				m_dwLitShadowedShaders[bump] = 0;
				UnitPixelLightsLoaded = FALSE;
			}
		}
	}

	W3DShaders[W3DShaderManager::ST_SPECULAR]=&specularShader;
	W3DShadersPassCount[W3DShaderManager::ST_SPECULAR]=1;

	return TRUE;
}

static void Set_Camera_Space_Texcoord(Int stage, DWORD source)
{
	D3DMATRIX identity;
	Set_D3DMATRIX_Identity(identity);
	DX8Wrapper::_Set_DX8_Transform((D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + stage), identity);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, source);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3);
}

// Expects the mesh bound and transformed. Its texture arrives per polygon group, through
// W3DSpecularMaterialPassClass::Install_Polygon_Materials.
Int SpecularShader::set(Int pass)
{
	m_shadowed = (m_dwShadowedShaders[BUMP_NONE] != 0 && TheW3DShadowMap != nullptr &&
		TheW3DShadowMap->bindReceiver(SPECULAR_SHADOW_STAGE));

	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_TEXCOORDINDEX, 0);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	Set_Camera_Space_Texcoord(SPECULAR_NORMAL_STAGE, D3DTSS_TCI_CAMERASPACENORMAL);
	Set_Camera_Space_Texcoord(SPECULAR_POSITION_STAGE, D3DTSS_TCI_CAMERASPACEPOSITION);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SPECULAR_NORMAL_MAP_STAGE, D3DTSS_TEXCOORDINDEX, 0);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SPECULAR_NORMAL_MAP_STAGE, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SPECULAR_EMISSIVE_STAGE, D3DTSS_TEXCOORDINDEX, 0);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SPECULAR_EMISSIVE_STAGE, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

	// The mesh is redrawn at the same depth, so EQUAL limits the pass to pixels the first
	// draw wrote. It adds its colour to them after scaling them by its alpha.
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, TRUE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC, D3DCMP_EQUAL);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, TRUE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND, D3DBLEND_ONE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, D3DBLEND_SRCALPHA);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGENABLE, FALSE);

	// The shader lights in camera space, so the sun is turned into it with the view's rotation.
	D3DMATRIX view;
	DX8Wrapper::_Get_D3D_Device8()->GetTransform(D3DTS_VIEW, &view);
	Vector3 toSun(
		SpecularToSun.X * view.m[0][0] + SpecularToSun.Y * view.m[1][0] + SpecularToSun.Z * view.m[2][0],
		SpecularToSun.X * view.m[0][1] + SpecularToSun.Y * view.m[1][1] + SpecularToSun.Z * view.m[2][1],
		SpecularToSun.X * view.m[0][2] + SpecularToSun.Y * view.m[1][2] + SpecularToSun.Z * view.m[2][2]);
	toSun.Normalize();

	Vector4 sunDirection(toSun.X, toSun.Y, toSun.Z, 0.0f);
	Vector4 sunColor(SpecularColor.X, SpecularColor.Y, SpecularColor.Z, 0.0f);
	Vector4 gloss(SpecularPower, SpecularDebug ? 1.0f : 0.0f, 0.0f, 0.0f);
	Vector4 bump(BumpHeight, BumpNormalMapStrength, BumpAmbient, 0.0f);
	Vector4 sunDiffuse(SpecularSunDiffuse.X, SpecularSunDiffuse.Y, SpecularSunDiffuse.Z, 0.0f);
	DX8Wrapper::Set_Pixel_Shader_Constant(1, &sunDirection, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(2, &sunColor, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(3, &gloss, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(5, &sunDiffuse, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(6, &bump, 1);

	m_lit = (UnitPixelLightsLoaded && PixelLightCount > 0);
	if (m_lit)
	{
		static Int unitLightRegisters = 0;
		if (!UnitLightBlockValid || memcmp(&view, &UnitLightView, sizeof(D3DMATRIX)) != 0)
		{
			unitLightRegisters = Pack_Pixel_Lights(UnitLightBlock, &view, FALSE);
			UnitLightView = view;
			UnitLightBlockValid = TRUE;
		}
		// The wrapper skips the upload while the registers still hold these values.
		DX8Wrapper::Set_Pixel_Shader_Constant(8, UnitLightBlock, unitLightRegisters);
	}

	DX8Wrapper::Set_Pixel_Shader(m_lit
		? (m_shadowed ? m_dwLitShadowedShaders[BUMP_NONE] : m_dwLitUnshadowedShaders[BUMP_NONE])
		: (m_shadowed ? m_dwShadowedShaders[BUMP_NONE] : m_dwUnshadowedShaders[BUMP_NONE]));
	++SpecularPassCount;
	return TRUE;
}

// Loads <name><suffix> beside the texture, or returns null. The caller releases it.
static TextureClass *Load_Companion_Texture(TextureClass *texture, const char *suffix)
{
	StringClass name(texture->Get_Texture_Name());
	const char *dot = strrchr(name.Peek_Buffer(), '.');
	if (dot != nullptr)
	{
		const Int start = (Int)(dot - name.Peek_Buffer());
		name.Erase(start, name.Get_Length() - start);
	}

	// A missing file would load as the missing-texture placeholder, so it is checked first.
	if (name.Is_Empty())
	{
		return nullptr;
	}
	name += suffix;
	file_auto_ptr file(_TheFileFactory, name.Peek_Buffer());
	if (!file->Is_Available())
	{
		return nullptr;
	}
	return WW3DAssetManager::Get_Instance()->Get_Texture(name.Peek_Buffer());
}

// Looks for <name>_nrm.dds beside the texture, once per texture.
static TextureClass *Find_Normal_Map(TextureClass *texture)
{
	if (!texture->Is_Normal_Map_Checked())
	{
		TextureClass *normalMap = Load_Companion_Texture(texture, "_nrm.dds");
		texture->Set_Normal_Map(normalMap);
		REF_PTR_RELEASE(normalMap);
	}
	return texture->Peek_Normal_Map();
}

// Looks for <name>_emi.dds beside the texture, once per texture.
static TextureClass *Find_Emissive_Map(TextureClass *texture)
{
	if (!texture->Is_Emissive_Map_Checked())
	{
		TextureClass *emissiveMap = Load_Companion_Texture(texture, "_emi.dds");
		texture->Set_Emissive_Map(emissiveMap);
		REF_PTR_RELEASE(emissiveMap);
	}
	return texture->Peek_Emissive_Map();
}

void SpecularShader::setTexture(TextureClass *texture)
{
	DX8Wrapper::Set_Texture(0, texture);

	Int bump = BUMP_NONE;
	TextureClass *normalMap = nullptr;
	if (BumpEnabled && texture != nullptr)
	{
		normalMap = Find_Normal_Map(texture);
		if (normalMap != nullptr)
		{
			bump = BUMP_NORMAL_MAP;
		}
		else if (BumpHeight > 0.0f)
		{
			bump = BUMP_DERIVED;
		}
	}

	const DWORD *shaders = m_lit
		? (m_shadowed ? m_dwLitShadowedShaders : m_dwLitUnshadowedShaders)
		: (m_shadowed ? m_dwShadowedShaders : m_dwUnshadowedShaders);
	if (shaders[bump] == 0)
	{
		bump = BUMP_NONE;
	}

	DX8Wrapper::Set_Texture(SPECULAR_NORMAL_MAP_STAGE, (bump == BUMP_NORMAL_MAP) ? normalMap : nullptr);
	DX8Wrapper::Set_Pixel_Shader(shaders[bump]);

	TextureClass *emissiveMap = (EmissiveIntensity > 0.0f && texture != nullptr) ? Find_Emissive_Map(texture) : nullptr;
	DX8Wrapper::Set_Texture(SPECULAR_EMISSIVE_STAGE, emissiveMap);

	// Derived bumps step at least a texel, so they need its size. The loaded level is read, since the size can still change.
	Real texelU = 1.0f / 256.0f;
	Real texelV = 1.0f / 256.0f;
	IDirect3DTexture8 *meshTexture = (bump == BUMP_DERIVED) ? texture->Peek_D3D_Texture() : nullptr;
	if (meshTexture != nullptr)
	{
		D3DSURFACE_DESC desc;
		if (SUCCEEDED(meshTexture->GetLevelDesc(0, &desc)) && desc.Width > 0 && desc.Height > 0)
		{
			texelU = 1.0f / (Real)desc.Width;
			texelV = 1.0f / (Real)desc.Height;
		}
	}
	Vector4 textureInfo((emissiveMap != nullptr) ? EmissiveIntensity : 0.0f, texelU, texelV, 0.0f);
	DX8Wrapper::Set_Pixel_Shader_Constant(7, &textureInfo, 1);
	if (emissiveMap != nullptr)
	{
		++EmissiveMapCount;
	}

	if (bump == BUMP_DERIVED)
	{
		++BumpDerivedCount;
	}
	else if (bump == BUMP_NORMAL_MAP)
	{
		++BumpNormalMapCount;
	}
}

void SpecularShader::reset()
{
	DX8Wrapper::Set_Pixel_Shader(0);
	DX8Wrapper::Set_Texture(SPECULAR_NORMAL_MAP_STAGE, nullptr);
	DX8Wrapper::Set_Texture(SPECULAR_EMISSIVE_STAGE, nullptr);

	if (m_shadowed && TheW3DShadowMap != nullptr)
	{
		TheW3DShadowMap->unbindReceiver(SPECULAR_SHADOW_STAGE);
	}
	m_shadowed = FALSE;
	m_lit = FALSE;

	for (Int stage = SPECULAR_NORMAL_STAGE; stage <= SPECULAR_POSITION_STAGE; stage++)
	{
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU | stage);
	}
	DX8Wrapper::Set_DX8_Texture_Stage_State(SPECULAR_NORMAL_MAP_STAGE, D3DTSS_TEXCOORDINDEX,
		D3DTSS_TCI_PASSTHRU | SPECULAR_NORMAL_MAP_STAGE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(SPECULAR_EMISSIVE_STAGE, D3DTSS_TEXCOORDINDEX,
		D3DTSS_TCI_PASSTHRU | SPECULAR_EMISSIVE_STAGE);

	// Z, blend and fog are ShaderClass state, so the next shader set restores them in full.
	ShaderClass::Invalidate();
}

Int SpecularShader::shutdown()
{
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();

	for (Int bump = BUMP_NONE; bump < BUMP_COUNT; bump++)
	{
		if (device != nullptr)
		{
			DX8_DELETE_PIXEL_SHADER(device, m_dwShadowedShaders[bump]);
			DX8_DELETE_PIXEL_SHADER(device, m_dwUnshadowedShaders[bump]);
			DX8_DELETE_PIXEL_SHADER(device, m_dwLitShadowedShaders[bump]);
			DX8_DELETE_PIXEL_SHADER(device, m_dwLitUnshadowedShaders[bump]);
		}
		m_dwShadowedShaders[bump] = 0;
		m_dwUnshadowedShaders[bump] = 0;
		m_dwLitShadowedShaders[bump] = 0;
		m_dwLitUnshadowedShaders[bump] = 0;
	}

	BumpSupported = FALSE;
	UnitPixelLightsLoaded = FALSE;

	W3DShaders[W3DShaderManager::ST_SPECULAR]=nullptr;
	W3DShadersPassCount[W3DShaderManager::ST_SPECULAR]=0;

	return TRUE;
}

#endif	// BUILD_WITH_D3D9

///Adds the specular pass to one mesh, with that mesh's own texture on stage 0.
class W3DSpecularMaterialPassClass : public MaterialPassClass
{
public:

	virtual void Install_Materials() const override
	{
		W3DShaderManager::setShader(W3DShaderManager::ST_SPECULAR, 0);
	}

	virtual void UnInstall_Materials() const override
	{
		W3DShaderManager::resetShader(W3DShaderManager::ST_SPECULAR);
	}

	virtual void Install_Polygon_Materials(DX8PolygonRendererClass *renderer) const override
	{
		DX8TextureCategoryClass *category = renderer->Get_Texture_Category();
		TextureClass *texture = (category != nullptr) ? category->Peek_Texture(0) : nullptr;
#if defined(BUILD_WITH_D3D9)
		specularShader.setTexture(texture);
#else
		DX8Wrapper::Set_Texture(0, texture);
#endif
	}
};

static W3DSpecularMaterialPassClass SpecularMaterialPass;

void W3DShaderManager::setSpecularLight(const Vector3 &toSun, const Vector3 &color, Real intensity, Real power, Bool debug)
{
	SpecularToSun = toSun;
	SpecularToSun.Normalize();
	SpecularColor = color * intensity;
	SpecularSunDiffuse = color;
	SpecularPower = power;
	SpecularDebug = debug;
}

void W3DShaderManager::setSurfaceBumps(Bool enabled, const Vector3 &ambient, Real height, Real normalMapStrength)
{
	BumpEnabled = enabled;
	BumpAmbient = ambient.X * 0.3f + ambient.Y * 0.59f + ambient.Z * 0.11f;
	BumpHeight = height;
	BumpNormalMapStrength = normalMapStrength;
}

void W3DShaderManager::setEmissive(Real intensity)
{
	EmissiveIntensity = intensity;
	DX8MeshRendererClass::Set_Bloom_Emissive_Intensity(intensity);
}

void W3DShaderManager::setPixelLights(const PixelLight *lights, Int count)
{
	PixelLightCount = min(count, (Int)MAX_PIXEL_LIGHTS);
	for (Int i = 0; i < PixelLightCount; i++)
	{
		PixelLights[i] = lights[i];
	}
	UnitLightBlockValid = FALSE;
}

Bool W3DShaderManager::supportsTerrainPixelLights()
{
	return TerrainPixelLightsLoaded;
}

Bool W3DShaderManager::supportsUnitPixelLights()
{
	return UnitPixelLightsLoaded;
}

void W3DShaderManager::setTerrainBumps(Bool enabled, Real strength, Bool debug)
{
	TerrainBumpEnabled = enabled;
	TerrainBumpStrength = strength;
	TerrainBumpDebug = debug;
}

Bool W3DShaderManager::supportsPixelShader2a()
{
#if defined(BUILD_WITH_D3D9)
	const DX8Caps *caps = DX8Wrapper::Get_Current_Caps();
	return caps != nullptr && Supports_Pixel_Shader_2_a(caps);
#else
	return FALSE;
#endif
}

TextureClass *W3DShaderManager::findNormalMap(TextureClass *texture)
{
#if defined(BUILD_WITH_D3D9)
	return (texture != nullptr) ? Find_Normal_Map(texture) : nullptr;
#else
	return nullptr;
#endif
}

Int W3DShaderManager::takeTerrainBumpCount()
{
	const Int count = TerrainBumpCount;
	TerrainBumpCount = 0;
	return count;
}

void W3DShaderManager::takeSpecularCounts(Int &meshes, Int &derived, Int &normalMapped, Int &emissive)
{
	meshes = SpecularPassCount;
	derived = BumpDerivedCount;
	normalMapped = BumpNormalMapCount;
	emissive = EmissiveMapCount;
	SpecularPassCount = 0;
	BumpDerivedCount = 0;
	BumpNormalMapCount = 0;
	EmissiveMapCount = 0;
}

MaterialPassClass *W3DShaderManager::getSpecularPass()
{
	const Bool bumps = (BumpEnabled && BumpSupported);
	const Bool pointLights = (UnitPixelLightsLoaded && PixelLightCount > 0);
	if (W3DShadersPassCount[ST_SPECULAR] == 0 ||
		(SpecularColor.Length2() <= 0.0f && !bumps && EmissiveIntensity <= 0.0f && !pointLights))
	{
		return nullptr;
	}
	return &SpecularMaterialPass;
}

/*===========================================================================================*/
/*=========      Terrain Shaders	=========================================================*/
/*===========================================================================================*/

///regular terrain shader that should work on all multi-texture video cards (slowest version)
class TerrainShader2Stage : public W3DShaderInterface
{
public:
	float m_xSlidePerSecond ;	 ///< How far the clouds move per second.
	float m_ySlidePerSecond ;	 ///< How far the clouds move per second.
	float m_xOffset;
	float m_yOffset;

	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.

	void updateCloud();
	void updateNoise1 (D3DMATRIX *destMatrix,D3DMATRIX *curViewInverse, Bool doUpdate=true);	///<generate the uv coordinates for Noise1 (i.e clouds)
	void updateNoise2 (D3DMATRIX *destMatrix,D3DMATRIX *curViewInverse, Bool doUpdate=true);	///<generate the uv coordinates for Noise2 (i.e lightmap)
} terrainShader2Stage;

///regular terrain shader that should work on all multi-texture video cards (slowest version)
class FlatTerrainShader2Stage : public W3DShaderInterface
{
public:
	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
} flatTerrainShader2Stage;

///regular terrain shader that should work on all multi-texture video cards (slowest version)
class FlatTerrainShaderPixelShader : public W3DShaderInterface
{
public:
	DWORD					m_dwBasePixelShader;	///<handle to terrain D3D pixel shader
	DWORD					m_dwBaseNoise1PixelShader;	///<handle to terrain/single noise D3D pixel shader
	DWORD					m_dwBaseNoise2PixelShader;	///<handle to terrain/double noise D3D pixel shader
	DWORD					m_dwBase0PixelShader;	///<handle to terrain only pixel shader
	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
	virtual Int shutdown() override;			///<release resources used by shader
} flatTerrainShaderPixelShader;

///8 stage terrain shader which only works on certain Nvidia cards.
class TerrainShader8Stage : public W3DShaderInterface
{
	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
	virtual Int init() override;			///<perform any one time initialization and validation
} terrainShader8Stage;

//Offsets into constant register pool used by vertex shader
#define CV_WORLDVIEWPROJ_0	0	//4 vectors for transform of world->clip space.

///Pixel shader based terrain shader - fastest method for the newest cards.
class TerrainShaderPixelShader : public W3DShaderInterface
{
public:
	TerrainShaderPixelShader() : m_shadowStage(-1), m_bumpStage(-1), m_lightStage(-1) {}

private:
	DWORD					m_dwBasePixelShader;	///<handle to terrain D3D pixel shader
	DWORD					m_dwBaseNoise1PixelShader;	///<handle to terrain/single noise D3D pixel shader
	DWORD					m_dwBaseNoise2PixelShader;	///<handle to terrain/double noise D3D pixel shader
	DWORD					m_dwShadowPixelShader[3];	///<the same three, also receiving the shadow map, indexed by noise texture count
	Int						m_shadowStage;	///<stage the shadow map is bound to, or -1
	DWORD					m_dwBumpPixelShader[2][3];	///<the same three with the normal atlas, unshadowed then shadowed
	Int						m_bumpStage;	///<stage the world position is generated on, with the normal atlas on the next, or -1
	DWORD					m_dwLitPixelShader[2][2][3];	///<the same again adding the point lights, by bump, shadow and noise count
	Int						m_lightStage;	///<stage the world position is generated on for unbumped point lights, or -1

	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual Int shutdown() override;			///<release resources used by shader

	void initShadowReceiver();
	Bool setShadowReceiver(Int noiseCount);
	void initBump();
	Bool setBump(Int noiseCount, Bool shadowed);
	void initPixelLights();
	Bool setPixelLights(Int noiseCount, Bool shadowed, Bool bumped);
} terrainShaderPixelShader;

///List of different terrain shader implementations in order of preference
W3DShaderInterface *TerrainShaderList[]=
{
	&terrainShaderPixelShader,
	&terrainShader8Stage,
	&terrainShader2Stage,
	nullptr
};

///List of different terrain shader implementations in order of preference
W3DShaderInterface *FlatTerrainShaderList[]=
{
	&flatTerrainShaderPixelShader,
	&flatTerrainShader2Stage,
	nullptr
};

Int TerrainShader2Stage::init()
{
	//initialize settings for uv animated clouds
	m_xSlidePerSecond = -0.02f;
	m_ySlidePerSecond =  1.50f * m_xSlidePerSecond;
	m_xOffset = 0;
	m_yOffset = 0;

	//no special device validation needed - anything in our min spec should handle this.

	W3DShaders[W3DShaderManager::ST_TERRAIN_BASE]=&terrainShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE]=2;
	W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE1]=&terrainShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE1]=3;
	W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE2]=&terrainShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE2]=3;
	W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE12]=&terrainShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE12]=3;

	return TRUE;
}

void TerrainShader2Stage::reset()
{
	ShaderClass::Invalidate();

	//Free references to textures
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, nullptr);
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(1, nullptr);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|0);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|1);
}

void TerrainShader2Stage::updateCloud()
{
	const float frame_time = WW3D::Get_Logic_Frame_Time_Seconds();
	m_xOffset += m_xSlidePerSecond * frame_time;
	m_yOffset += m_ySlidePerSecond * frame_time;

	// This moves offsets towards zero when smaller -1.0 or larger 1.0
	m_xOffset -= (Int)m_xOffset;
	m_yOffset -= (Int)m_yOffset;
}

void TerrainShader2Stage::updateNoise1(D3DMATRIX *destMatrix,D3DMATRIX *curViewInverse, Bool doUpdate)
{
	#define STRETCH_FACTOR ((float)(1/(63.0*MAP_XY_FACTOR/2))) /* covers 63/2 tiles */

	D3DMATRIX scale;

	Set_D3DMATRIX_Scaling(scale, STRETCH_FACTOR, STRETCH_FACTOR,1);
	*destMatrix = *curViewInverse * scale;

	D3DMATRIX offset;
	Set_D3DMATRIX_Translation(offset, m_xOffset, m_yOffset,0);
	*destMatrix *= offset;
}

void TerrainShader2Stage::updateNoise2(D3DMATRIX *destMatrix,D3DMATRIX *curViewInverse, Bool doUpdate)
{

	D3DMATRIX scale;

	Set_D3DMATRIX_Scaling(scale, STRETCH_FACTOR, STRETCH_FACTOR,1);
	*destMatrix = *curViewInverse * scale;
}

Int TerrainShader2Stage::set(Int pass)
{
	//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
	DX8Wrapper::Apply_Render_State_Changes();

	if (TheGlobalData && (TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex)) {
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	} else {
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_POINT);
	}
	if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex) {
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
	} else {
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
	}

	switch (pass)
	{
		case 0:
			DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, W3DShaderManager::getShaderTexture(0)->Peek_D3D_Texture());
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

			// Modulate the diffuse color with the texture as lighting comes from diffuse.
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, 0 );
			DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE,false);
			break;
		case 1:
			DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, W3DShaderManager::getShaderTexture(1)->Peek_D3D_Texture());
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

			// Modulate the diffuse color with the texture as lighting comes from diffuse.
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, 1 );
			// Blend the result using the alpha. (came from diffuse mod texture)
			DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE,true);
			DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
			DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
			// Disable stage 2.
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
			break;
		case 2:
			// Noise/cloud pass
			D3DMATRIX curView;
			DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

			//these states apply to all noise/cloud combination passes
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

			DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
			// Two output coordinates are used.
			DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
			DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
			DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);

			//blend into frame buffer
			DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE,true);
			DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND,D3DBLEND_DESTCOLOR);
			DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND,D3DBLEND_ZERO);

			D3DMATRIX inv;
			float det;
			Invert_D3DMATRIX(inv, &det, curView);

			if (W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_TERRAIN_BASE_NOISE12)
			{
				//setup cloud pass
				DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, W3DShaderManager::getShaderTexture(2)->Peek_D3D_Texture());

				updateNoise1(&curView,&inv);	//update curView with texture matrix
				DX8Wrapper::_Set_DX8_Transform(D3DTS_TEXTURE0, curView);
				//clouds always need bilinear filtering
				DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
				DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);

				//setup noise pass
				DX8Wrapper::_Get_D3D_Device8()->SetTexture(1, W3DShaderManager::getShaderTexture(3)->Peek_D3D_Texture());

				updateNoise2(&curView,&inv);
				DX8Wrapper::_Set_DX8_Transform(D3DTS_TEXTURE1, curView);
				//noise always needs point/linear filtering.  Why point!?
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_POINT);
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);

				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_MODULATE );
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
				// Two output coordinates are used.
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
			}
			else
			{	//only 1 noise or cloud texture
				// Now setup the texture pipeline.
				if (W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_TERRAIN_BASE_NOISE1)
				{	//setup cloud pass
					DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, W3DShaderManager::getShaderTexture(2)->Peek_D3D_Texture());
					updateNoise1(&curView,&inv);	//update curView with texture matrix
					DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
					DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
				}
				else
				{
					//setup noise pass
					DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, W3DShaderManager::getShaderTexture(3)->Peek_D3D_Texture());
					updateNoise2(&curView,&inv);	//update curView with texture matrix
					DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_POINT);
					DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
				}

				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
				DX8Wrapper::_Set_DX8_Transform(D3DTS_TEXTURE0, curView);
			}
			break;
	}

	return TRUE;
}

Int TerrainShader8Stage::init()
{
	ChipsetType res;

	//this shader will also use the 2Stage shader for some of the passes so initialize it too.
	if (terrainShader2Stage.init() && (res=W3DShaderManager::getChipset()) >= DC_TNT && res <= DC_GEFORCE2)
	{
		W3DShaders[W3DShaderManager::ST_TERRAIN_BASE]=&terrainShader8Stage;
		W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE]=1;
		W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE1]=&terrainShader8Stage;
		W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE1]=2;
		W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE2]=&terrainShader8Stage;
		W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE2]=2;
		W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE12]=&terrainShader8Stage;
		W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE12]=2;
		return TRUE;
	}

	return FALSE;
}

Int TerrainShader8Stage::set(Int pass)
{
	if (pass == 0)
	{
		//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
		DX8Wrapper::Apply_Render_State_Changes();

		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

		if (TheGlobalData && (TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex)) {
			DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
			DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
			DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
			DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
		} else {
			DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_POINT);
			DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
			DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_POINT);
			DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_POINT);
		}
		if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex) {
			DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
			DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
		} else {
			DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_POINT);
			DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
		}

		DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, W3DShaderManager::getShaderTexture(0)->Peek_D3D_Texture());
		DX8Wrapper::_Get_D3D_Device8()->SetTexture(1, W3DShaderManager::getShaderTexture(1)->Peek_D3D_Texture());

		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, 0);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP, D3DTOP_ADD);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXCOORDINDEX, 1);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG1, D3DTA_DIFFUSE | D3DTA_COMPLEMENT | D3DTA_ALPHAREPLICATE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_ADD);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAARG1, D3DTA_TFACTOR | D3DTA_COMPLEMENT);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);

		DX8Wrapper::Set_DX8_Texture(2, nullptr);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 2, D3DTSS_COLOROP, D3DTOP_MODULATE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 2, D3DTSS_TEXCOORDINDEX, 2);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 2, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 2, D3DTSS_COLORARG2, D3DTA_TEXTURE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 2, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 2, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 2, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);

		DX8Wrapper::Set_DX8_Texture(3, nullptr);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 3, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 3, D3DTSS_TEXCOORDINDEX, 3);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 3, D3DTSS_COLORARG1, D3DTA_DIFFUSE | 0 | D3DTA_ALPHAREPLICATE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 3, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 3, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 3, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 3, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);

		DX8Wrapper::Set_DX8_Texture(4, nullptr);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 4, D3DTSS_COLOROP, D3DTOP_MODULATE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 4, D3DTSS_TEXCOORDINDEX, 4);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 4, D3DTSS_COLORARG1, D3DTA_CURRENT);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 4, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 4, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 4, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 4, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

		DX8Wrapper::Set_DX8_Texture(5, nullptr);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 5, D3DTSS_COLOROP, D3DTOP_ADD);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 5, D3DTSS_TEXCOORDINDEX, 5);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 5, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 5, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 5, D3DTSS_ALPHAOP,   D3DTOP_ADD);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 5, D3DTSS_ALPHAARG1, D3DTA_TFACTOR | D3DTA_COMPLEMENT);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 5, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);

		DX8Wrapper::Set_DX8_Texture(6, nullptr);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 6, D3DTSS_COLOROP, D3DTOP_MODULATE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 6, D3DTSS_TEXCOORDINDEX, 6);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 6, D3DTSS_COLORARG1, D3DTA_TFACTOR);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 6, D3DTSS_COLORARG2, D3DTA_TFACTOR);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 6, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 6, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 6, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);

		DX8Wrapper::Set_DX8_Texture(7, nullptr);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 7, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 7, D3DTSS_TEXCOORDINDEX, 7);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 7, D3DTSS_COLORARG1, D3DTA_TFACTOR);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 7, D3DTSS_COLORARG2, D3DTA_TFACTOR);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 7, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 7, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 7, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
	}
	else
	{	//setup cloud noise/pass
		DX8Wrapper::Set_DX8_Texture_Stage_State( 2, D3DTSS_COLOROP, D3DTOP_DISABLE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 3, D3DTSS_COLOROP, D3DTOP_DISABLE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 3, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
		DX8Wrapper::Invalidate_Cached_Render_States();

		terrainShader2Stage.set(2);
	}
	return TRUE;
}

void TerrainShader8Stage::reset()
{
	DX8Wrapper::Set_DX8_Texture_Stage_State( 2, D3DTSS_COLOROP, D3DTOP_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 3, D3DTSS_COLOROP, D3DTOP_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 3, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 4, D3DTSS_COLOROP, D3DTOP_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 4, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, nullptr);
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(1, nullptr);
	DX8Wrapper::Invalidate_Cached_Render_States();
}

Int TerrainShaderPixelShader::shutdown()
{
	if (m_dwBasePixelShader)
		DX8_DELETE_PIXEL_SHADER(DX8Wrapper::_Get_D3D_Device8(), m_dwBasePixelShader);

	if (m_dwBaseNoise1PixelShader)
		DX8_DELETE_PIXEL_SHADER(DX8Wrapper::_Get_D3D_Device8(), m_dwBaseNoise1PixelShader);

	if (m_dwBaseNoise2PixelShader)
		DX8_DELETE_PIXEL_SHADER(DX8Wrapper::_Get_D3D_Device8(), m_dwBaseNoise2PixelShader);

	m_dwBasePixelShader=0;
	m_dwBaseNoise1PixelShader=0;
	m_dwBaseNoise2PixelShader=0;

	for (Int i=0; i<3; i++)
	{
		if (m_dwShadowPixelShader[i])
			DX8_DELETE_PIXEL_SHADER(DX8Wrapper::_Get_D3D_Device8(), m_dwShadowPixelShader[i]);
		m_dwShadowPixelShader[i]=0;
	}

	for (Int s=0; s<2; s++)
	{
		for (Int i=0; i<3; i++)
		{
			if (m_dwBumpPixelShader[s][i])
			{
				DX8_DELETE_PIXEL_SHADER(DX8Wrapper::_Get_D3D_Device8(), m_dwBumpPixelShader[s][i]);
			}
			m_dwBumpPixelShader[s][i]=0;
		}
	}

	for (Int b=0; b<2; b++)
	{
		for (Int s=0; s<2; s++)
		{
			for (Int i=0; i<3; i++)
			{
				if (m_dwLitPixelShader[b][s][i])
				{
					DX8_DELETE_PIXEL_SHADER(DX8Wrapper::_Get_D3D_Device8(), m_dwLitPixelShader[b][s][i]);
				}
				m_dwLitPixelShader[b][s][i]=0;
			}
		}
	}
	TerrainPixelLightsLoaded = FALSE;

	return TRUE;
}

void TerrainShaderPixelShader::initShadowReceiver()
{
	for (Int i=0; i<3; i++)
		m_dwShadowPixelShader[i]=0;
	m_shadowStage = -1;

#if defined(BUILD_WITH_D3D9)
	if (TheW3DShadowMap == nullptr || !TheW3DShadowMap->isAvailable())
		return;

	const Bool packed = TheW3DShadowMap->getDepthMode() == W3DShadowMap::DEPTH_MODE_PACKED;
	const char *files[3][2] =
	{
		{ "shaders\\terrainshadow.pso",       "shaders\\terrainshadowpacked.pso" },
		{ "shaders\\terrainshadownoise.pso",  "shaders\\terrainshadownoisepacked.pso" },
		{ "shaders\\terrainshadownoise2.pso", "shaders\\terrainshadownoise2packed.pso" }
	};

	for (Int i=0; i<3; i++)
	{
		if (FAILED(W3DShaderManager::LoadAndCreateD3DShader(files[i][packed ? 1 : 0], nullptr, 0, false, &m_dwShadowPixelShader[i])))
		{
			// Terrain still draws without shadows, so a missing variant only turns them off.
			for (Int j=0; j<i; j++)
				DX8_DELETE_PIXEL_SHADER(DX8Wrapper::_Get_D3D_Device8(), m_dwShadowPixelShader[j]);
			for (Int j=0; j<3; j++)
				m_dwShadowPixelShader[j]=0;
			return;
		}
	}
#endif
}

Bool TerrainShaderPixelShader::setShadowReceiver(Int noiseCount)
{
	if (m_dwShadowPixelShader[noiseCount] == 0 || TheW3DShadowMap == nullptr)
		return FALSE;

	// The first stage after the base, blend and noise textures. Fixed-function vertex
	// processing hands out texcoord sets in stage order, so this is the set the shader reads.
	const Int stage = 2 + noiseCount;
	if (!TheW3DShadowMap->bindReceiver(stage))
		return FALSE;

	m_shadowStage = stage;
	DX8Wrapper::Set_Pixel_Shader(m_dwShadowPixelShader[noiseCount]);
	return TRUE;
}

void TerrainShaderPixelShader::initBump()
{
	for (Int s=0; s<2; s++)
	{
		for (Int i=0; i<3; i++)
		{
			m_dwBumpPixelShader[s][i]=0;
		}
	}
	m_bumpStage = -1;

#if defined(BUILD_WITH_D3D9)
	const DX8Caps *caps = DX8Wrapper::Get_Current_Caps();
	if (caps == nullptr || !Supports_Pixel_Shader_2_a(caps))
	{
		return;
	}

	const char *unshadowedFiles[3] =
	{
		"shaders\\terrainbumpnoshadow.pso", "shaders\\terrainbumpnoisenoshadow.pso", "shaders\\terrainbumpnoise2noshadow.pso"
	};
	const char *shadowedFiles[3][2] =
	{
		{ "shaders\\terrainbump.pso",       "shaders\\terrainbumppacked.pso" },
		{ "shaders\\terrainbumpnoise.pso",  "shaders\\terrainbumpnoisepacked.pso" },
		{ "shaders\\terrainbumpnoise2.pso", "shaders\\terrainbumpnoise2packed.pso" }
	};

	// Shadowed variants only go with the shadow receivers they replace.
	const Bool shadowMap = (m_dwShadowPixelShader[0] != 0 && TheW3DShadowMap != nullptr);
	const Bool packed = shadowMap && TheW3DShadowMap->getDepthMode() == W3DShadowMap::DEPTH_MODE_PACKED;

	for (Int i=0; i<3; i++)
	{
		if (FAILED(W3DShaderManager::LoadAndCreateD3DShader(unshadowedFiles[i], nullptr, 0, false, &m_dwBumpPixelShader[0][i])))
		{
			m_dwBumpPixelShader[0][i]=0;
		}
		if (shadowMap && FAILED(W3DShaderManager::LoadAndCreateD3DShader(shadowedFiles[i][packed ? 1 : 0], nullptr, 0, false, &m_dwBumpPixelShader[1][i])))
		{
			m_dwBumpPixelShader[1][i]=0;
		}
	}
#endif
}

// World position is camera space taken back through the view, because the terrain has no world transform.
static void Set_Terrain_World_Position(Int stage)
{
	D3DMATRIX view;
	D3DMATRIX inv;
	float det;
	DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, view);
	Invert_D3DMATRIX(inv, &det, view);
	DX8Wrapper::_Set_DX8_Transform((D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + stage), inv);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3);
}

// Expects the shadow map bound already when shadowed, since the stages after it are this one's.
Bool TerrainShaderPixelShader::setBump(Int noiseCount, Bool shadowed)
{
	TextureClass *normalAtlas = W3DShaderManager::getShaderTexture(W3DShaderManager::TERRAIN_NORMAL_TEXTURE);
	const DWORD shader = m_dwBumpPixelShader[shadowed ? 1 : 0][noiseCount];
	if (!TerrainBumpEnabled || shader == 0 || normalAtlas == nullptr || normalAtlas->Peek_D3D_Texture() == nullptr)
	{
		return FALSE;
	}

	const Int stage = 2 + noiseCount + (shadowed ? 1 : 0);
	Set_Terrain_World_Position(stage);

	// The atlas is read with the base and blend UVs, so its own texcoords go unused.
	const Int atlasStage = stage + 1;
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(atlasStage, normalAtlas->Peek_D3D_Texture());
	DX8Wrapper::Set_DX8_Texture_Stage_State(atlasStage, D3DTSS_TEXCOORDINDEX, 0);
	DX8Wrapper::Set_DX8_Texture_Stage_State(atlasStage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(atlasStage, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(atlasStage, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(atlasStage, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(atlasStage, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(atlasStage, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);

	// The vertex lighting's first light is the sun, so the bump redoes that one.
	const Coord3D &lightPos = TheGlobalData->m_terrainLightPos[0];
	Vector3 toSun(-lightPos.x, -lightPos.y, -lightPos.z);
	toSun.Normalize();
	const RGBColor &sunColor = TheGlobalData->m_terrainDiffuse[0];
	Vector4 sunDirection(toSun.X, toSun.Y, toSun.Z, 0.0f);
	Vector4 sunDiffuse(sunColor.red, sunColor.green, sunColor.blue, 0.0f);
	Vector4 bumpParams(TerrainBumpStrength, TerrainBumpDebug ? 1.0f : 0.0f, 0.0f, 0.0f);
	DX8Wrapper::Set_Pixel_Shader_Constant(1, &sunDirection, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(2, &sunDiffuse, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(3, &bumpParams, 1);

	m_bumpStage = stage;
	DX8Wrapper::Set_Pixel_Shader(shader);
	++TerrainBumpCount;
	return TRUE;
}

void TerrainShaderPixelShader::initPixelLights()
{
	for (Int b=0; b<2; b++)
	{
		for (Int s=0; s<2; s++)
		{
			for (Int i=0; i<3; i++)
			{
				m_dwLitPixelShader[b][s][i]=0;
			}
		}
	}
	m_lightStage = -1;
	TerrainPixelLightsLoaded = FALSE;

#if defined(BUILD_WITH_D3D9)
	const DX8Caps *caps = DX8Wrapper::Get_Current_Caps();
	if (caps == nullptr || !Supports_Pixel_Shader_2_a(caps) || Get_Pixel_Light_Mode() < PIXEL_LIGHTS_TERRAIN)
	{
		return;
	}

	// Shadowed variants only go with the shadow receivers they replace.
	const Bool shadowMap = (m_dwShadowPixelShader[0] != 0 && TheW3DShadowMap != nullptr);
	const Bool packed = shadowMap && TheW3DShadowMap->getDepthMode() == W3DShadowMap::DEPTH_MODE_PACKED;
	static const char *const noiseNames[3] = { "", "noise", "noise2" };

	// Lights handed to the shader leave the vertex lighting, so every variant has to be there or none is used.
	Bool complete = TRUE;
	for (Int b=0; b<2; b++)
	{
		for (Int s=0; s<(shadowMap ? 2 : 1); s++)
		{
			for (Int i=0; i<3; i++)
			{
				char file[64];
				snprintf(file, sizeof(file), "shaders\\terrainlit%s%s%s.pso", b ? "bump" : "", noiseNames[i],
					s == 0 ? "noshadow" : (packed ? "packed" : ""));
				if (FAILED(W3DShaderManager::LoadAndCreateD3DShader(file, nullptr, 0, false, &m_dwLitPixelShader[b][s][i])))
				{
					m_dwLitPixelShader[b][s][i]=0;
					complete = FALSE;
				}
			}
		}
	}
	TerrainPixelLightsLoaded = complete;
#endif
}

// Expects the shadow map and bump already bound, since the stages after them are this one's.
Bool TerrainShaderPixelShader::setPixelLights(Int noiseCount, Bool shadowed, Bool bumped)
{
	const DWORD shader = m_dwLitPixelShader[bumped ? 1 : 0][shadowed ? 1 : 0][noiseCount];
	if (!TerrainPixelLightsLoaded || PixelLightCount == 0 || shader == 0)
	{
		return FALSE;
	}

	// Bumped terrain already has the world position.
	if (!bumped)
	{
		m_lightStage = 2 + noiseCount + (shadowed ? 1 : 0);
		Set_Terrain_World_Position(m_lightStage);
	}

	Set_Pixel_Light_Constants(5, nullptr, TRUE);
	DX8Wrapper::Set_Pixel_Shader(shader);
	return TRUE;
}

Int TerrainShaderPixelShader::init()
{
	Int res;
#ifdef DISABLE_PIXEL_SHADERS
	return false;
#endif
	//this shader will also use the 2Stage shader for some of the passes so initialize it too.
	if (terrainShader2Stage.init() && (res=W3DShaderManager::getChipset()) >= DC_GENERIC_PIXEL_SHADER_1_1)
	{
		if (res >= DC_GENERIC_PIXEL_SHADER_1_1)
		{
			//this shader needs some assets that need to be loaded
			//shader decleration
			DWORD Declaration[]=
			{
				(D3DVSD_STREAM(0)),
				(D3DVSD_REG(0, D3DVSDT_FLOAT3)), // Position
				(D3DVSD_REG(1, D3DVSDT_D3DCOLOR)), // Diffuse
				(D3DVSD_REG(2, D3DVSDT_FLOAT2)), //  Texture Coordinates
				(D3DVSD_REG(3, D3DVSDT_FLOAT2)), //  Texture Coordinates
				(D3DVSD_END())
			};

			//base version which doesn't apply any noise textures.
			HRESULT hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\terrain.pso", &Declaration[0], 0, false, &m_dwBasePixelShader);
			if (FAILED(hr))
				return FALSE;

			//version which blends 1 noise texture.
			hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\terrainnoise.pso", &Declaration[0], 0, false, &m_dwBaseNoise1PixelShader);
			if (FAILED(hr))
				return FALSE;

			//version which blends 2 noise textures.
			hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\terrainnoise2.pso", &Declaration[0], 0, false, &m_dwBaseNoise2PixelShader);
			if (FAILED(hr))
				return FALSE;

			initShadowReceiver();
			initBump();
			initPixelLights();

			W3DShaders[W3DShaderManager::ST_TERRAIN_BASE]=&terrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE1]=&terrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE2]=&terrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE12]=&terrainShaderPixelShader;
			W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE]=1;
			W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE1]=1;
			W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE2]=1;
			W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE12]=1;
			return TRUE;
		}
	}
	return FALSE;
}

Int TerrainShaderPixelShader::set(Int pass)
{
	//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
	DX8Wrapper::Apply_Render_State_Changes();

	//setup base pass
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, W3DShaderManager::getShaderTexture(0)->Peek_D3D_Texture());
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(1, W3DShaderManager::getShaderTexture(1)->Peek_D3D_Texture());

	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

	//tell pixel shader which UV set to use for each stage
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, 0 );
	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXCOORDINDEX, 1 );

	if (TheGlobalData && (TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex)) {
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	} else {
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_POINT);
	}
	if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex) {
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
	} else {
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
	}

	if (W3DShaderManager::getCurrentShader() >= W3DShaderManager::ST_TERRAIN_BASE_NOISE1)
	{
		D3DMATRIX curView;
		DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

		D3DMATRIX inv;
		float det;
		Invert_D3DMATRIX(inv, &det, curView);

		DX8Wrapper::Set_DX8_Texture_Stage_State(2,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
		// Two output coordinates are used.
		DX8Wrapper::Set_DX8_Texture_Stage_State(2,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

		DX8Wrapper::Set_DX8_Texture_Stage_State(2,  D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
		DX8Wrapper::Set_DX8_Texture_Stage_State(2,  D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);

		if (W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_TERRAIN_BASE_NOISE12)
		{	//full shader
			DX8Wrapper::Set_DX8_Texture_Stage_State(3,  D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
			DX8Wrapper::Set_DX8_Texture_Stage_State(3,  D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
			DX8Wrapper::_Get_D3D_Device8()->SetTexture(2, W3DShaderManager::getShaderTexture(2)->Peek_D3D_Texture());
			DX8Wrapper::_Get_D3D_Device8()->SetTexture(3, W3DShaderManager::getShaderTexture(3)->Peek_D3D_Texture());
			DX8Wrapper::Set_Pixel_Shader(m_dwBaseNoise2PixelShader);

			DX8Wrapper::Set_DX8_Texture_Stage_State(2, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
			DX8Wrapper::Set_DX8_Texture_Stage_State(2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);

			DX8Wrapper::Set_DX8_Texture_Stage_State(3, D3DTSS_MINFILTER, D3DTEXF_POINT);
			DX8Wrapper::Set_DX8_Texture_Stage_State(3, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);

			terrainShader2Stage.updateNoise1(&curView,&inv);	//update curView with texture matrix
			DX8Wrapper::_Set_DX8_Transform(D3DTS_TEXTURE2, curView);

			terrainShader2Stage.updateNoise2(&curView,&inv);	//update curView with texture matrix
			DX8Wrapper::_Set_DX8_Transform(D3DTS_TEXTURE3, curView);

			DX8Wrapper::Set_DX8_Texture_Stage_State(3,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
			// Two output coordinates are used.
			DX8Wrapper::Set_DX8_Texture_Stage_State(3,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
		}
		else
		{	//single noise texture shader
			DX8Wrapper::Set_Pixel_Shader(m_dwBaseNoise1PixelShader);

			if (W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_TERRAIN_BASE_NOISE1)
			{	//cloud map
				DX8Wrapper::_Get_D3D_Device8()->SetTexture(2, W3DShaderManager::getShaderTexture(2)->Peek_D3D_Texture());
				terrainShader2Stage.updateNoise1(&curView,&inv);	//update curView with texture matrix
				DX8Wrapper::Set_DX8_Texture_Stage_State(2, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
				DX8Wrapper::Set_DX8_Texture_Stage_State(2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
			}
			else
			{	//light map
				DX8Wrapper::_Get_D3D_Device8()->SetTexture(2, W3DShaderManager::getShaderTexture(3)->Peek_D3D_Texture());
				terrainShader2Stage.updateNoise2(&curView,&inv);	//update curView with texture matrix
				DX8Wrapper::Set_DX8_Texture_Stage_State(2, D3DTSS_MINFILTER, D3DTEXF_POINT);
				DX8Wrapper::Set_DX8_Texture_Stage_State(2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
			}
			DX8Wrapper::_Set_DX8_Transform(D3DTS_TEXTURE2, curView);
		}
	}
	else
	{	//just base texturing
		DX8Wrapper::Set_Pixel_Shader(m_dwBasePixelShader);
	}

	// Swap in the matching variant that also receives the shadow map.
	Int noiseCount = 0;
	if (W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_TERRAIN_BASE_NOISE12)
		noiseCount = 2;
	else if (W3DShaderManager::getCurrentShader() >= W3DShaderManager::ST_TERRAIN_BASE_NOISE1)
		noiseCount = 1;
	const Bool shadowed = setShadowReceiver(noiseCount);
	const Bool bumped = setBump(noiseCount, shadowed);
	setPixelLights(noiseCount, shadowed, bumped);

	return TRUE;
}

void TerrainShaderPixelShader::reset()
{
	if (TheW3DShadowMap != nullptr && m_shadowStage >= 0)
		TheW3DShadowMap->unbindReceiver(m_shadowStage);
	m_shadowStage = -1;

	if (m_bumpStage >= 0)
	{
		for (Int stage = m_bumpStage; stage <= m_bumpStage + 1; stage++)
		{
			DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
			DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|stage);
		}
		DX8Wrapper::_Get_D3D_Device8()->SetTexture(m_bumpStage + 1, nullptr);
	}
	m_bumpStage = -1;

	if (m_lightStage >= 0)
	{
		DX8Wrapper::Set_DX8_Texture_Stage_State(m_lightStage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		DX8Wrapper::Set_DX8_Texture_Stage_State(m_lightStage, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|m_lightStage);
	}
	m_lightStage = -1;

	DX8Wrapper::_Get_D3D_Device8()->SetTexture(2,nullptr);	//release reference to any texture
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(3,nullptr);	//release reference to any texture

	DX8Wrapper::Set_Pixel_Shader(0);	//turn off pixel shader

	DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, nullptr);
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(1, nullptr);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|0);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|1);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 2, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 2, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|2);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 3, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 3, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|3);


	DX8Wrapper::Invalidate_Cached_Render_States();
}

///Cloud layer rendering shader - used for objects similar to terrain which only need the cloud layer.
class CloudTextureShader : public W3DShaderInterface
{
	virtual Int set(Int stage) override;		///<setup shader for the specified rendering pass.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
	Int m_stageOfSet;
} cloudTextureShader;

///List of different cloud shader implementations in order of preference
W3DShaderInterface *CloudShaderList[]=
{
	&cloudTextureShader,
	nullptr
};

Int CloudTextureShader::init()
{
	W3DShaders[W3DShaderManager::ST_CLOUD_TEXTURE]=&cloudTextureShader;
	W3DShadersPassCount[W3DShaderManager::ST_CLOUD_TEXTURE]=1;

	return TRUE;
}

/**Setup a certain texture stage to project our cloud texture*/
Int CloudTextureShader::set(Int stage)
{
	D3DMATRIX curView;
	DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

	D3DMATRIX inv;
	float det;

	Invert_D3DMATRIX(inv, &det, curView);

	//Get a texture matrix that applies the current cloud position
	terrainShader2Stage.updateNoise1(&curView,&inv,false);	//update curView with texture matrix

	DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	DX8Wrapper::_Set_DX8_Transform((D3DTRANSFORMSTATETYPE )(D3DTS_TEXTURE0+stage), curView);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);

	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, D3DTSS_COLORARG2, D3DTA_CURRENT );
	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, D3DTSS_COLOROP,   D3DTOP_MODULATE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );

	DX8Wrapper::_Get_D3D_Device8()->SetTexture(stage, W3DShaderManager::getShaderTexture(stage)->Peek_D3D_Texture());

	m_stageOfSet=stage;
	return TRUE;
}

void CloudTextureShader::reset()
{
	//Free reference to texture
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(m_stageOfSet, nullptr);
	//Turn off texture projection
	DX8Wrapper::Set_DX8_Texture_Stage_State( m_stageOfSet, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( m_stageOfSet, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|m_stageOfSet);

	DX8Wrapper::Set_DX8_Texture_Stage_State( m_stageOfSet, D3DTSS_COLOROP,   D3DTOP_DISABLE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( m_stageOfSet, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
}

// Projects the terrain's cloud map onto a pixel shader receiver, the way the terrain samples it.
static Bool bindCloudReceiver(Int stage)
{
	TextureClass *cloudTexture = (TheTerrainRenderObject != nullptr) ? TheTerrainRenderObject->getCloudTexture() : nullptr;
	if (cloudTexture == nullptr)
	{
		return FALSE;
	}

	// Applied through the wrapper before the sampler state, as bindWorldReceiver explains.
	DX8Wrapper::Set_Texture(stage, nullptr);
	DX8Wrapper::Set_Texture(stage, cloudTexture);
	DX8Wrapper::Apply_Render_State_Changes();

	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);

	// The wrapper's view copy is zeroed by every invalidate, so the device's is read.
	D3DMATRIX view;
	DX8Wrapper::_Get_D3D_Device8()->GetTransform(D3DTS_VIEW, &view);

	D3DMATRIX inverseView;
	float det;
	Invert_D3DMATRIX(inverseView, &det, view);

	D3DMATRIX textureTransform;
	terrainShader2Stage.updateNoise1(&textureTransform, &inverseView, false);

	DX8Wrapper::_Set_DX8_Transform((D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + stage), textureTransform);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

	return TRUE;
}

static void unbindCloudReceiver(Int stage)
{
	DX8Wrapper::Set_Texture(stage, nullptr);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU | stage);
}

/*===========================================================================================*/
/*=========      Road Shaders	=========================================================*/
/*===========================================================================================*/
class RoadShaderPixelShader : public W3DShaderInterface
{
	friend class RoadShader2Stage;	//the two-stage path hands its passes over when roads receive shadows.

public:
	RoadShaderPixelShader() : m_shadowStage(-1) {}

private:

	DWORD					m_dwBaseNoise2PixelShader;	///<handle to road/double noise D3D pixel shader
	DWORD					m_dwShadowPixelShader[3];	///<every road mode, also receiving the shadow map, indexed by noise texture count
	Int						m_shadowStage;	///<stage the shadow map is bound to, or -1

	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual Int shutdown() override;			///<release resources used by shader

	void initShadowReceiver();
	Bool setShadowReceiver();
} roadShaderPixelShader;

class RoadShader2Stage : public W3DShaderInterface
{	friend class RoadShaderPixelShader;	//pixel shader version uses some of the same features.

	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual void reset() override;
} roadShader2Stage;

///List of different terrain shader implementations in order of preference
W3DShaderInterface *RoadShaderList[]=
{
	&roadShaderPixelShader,
	&roadShader2Stage,
	nullptr
};

Int RoadShaderPixelShader::shutdown()
{
	if (m_dwBaseNoise2PixelShader)
		DX8_DELETE_PIXEL_SHADER(DX8Wrapper::_Get_D3D_Device8(), m_dwBaseNoise2PixelShader);

	m_dwBaseNoise2PixelShader=0;

	for (Int i=0; i<3; i++)
	{
		if (m_dwShadowPixelShader[i])
			DX8_DELETE_PIXEL_SHADER(DX8Wrapper::_Get_D3D_Device8(), m_dwShadowPixelShader[i]);
		m_dwShadowPixelShader[i]=0;
	}

	return TRUE;
}

void RoadShaderPixelShader::initShadowReceiver()
{
	for (Int i=0; i<3; i++)
		m_dwShadowPixelShader[i]=0;
	m_shadowStage = -1;

#if defined(BUILD_WITH_D3D9)
	if (TheW3DShadowMap == nullptr || !TheW3DShadowMap->isAvailable())
		return;

	const Bool packed = TheW3DShadowMap->getDepthMode() == W3DShadowMap::DEPTH_MODE_PACKED;
	const char *files[3][2] =
	{
		{ "shaders\\roadshadow.pso",       "shaders\\roadshadowpacked.pso" },
		{ "shaders\\roadshadownoise.pso",  "shaders\\roadshadownoisepacked.pso" },
		{ "shaders\\roadshadownoise2.pso", "shaders\\roadshadownoise2packed.pso" }
	};

	for (Int i=0; i<3; i++)
	{
		if (FAILED(W3DShaderManager::LoadAndCreateD3DShader(files[i][packed ? 1 : 0], nullptr, 0, false, &m_dwShadowPixelShader[i])))
		{
			// Roads still draw without shadows, so a missing variant only turns them off.
			for (Int j=0; j<i; j++)
				DX8_DELETE_PIXEL_SHADER(DX8Wrapper::_Get_D3D_Device8(), m_dwShadowPixelShader[j]);
			for (Int j=0; j<3; j++)
				m_dwShadowPixelShader[j]=0;
			return;
		}
	}
#endif
}

Bool RoadShaderPixelShader::setShadowReceiver()
{
	const W3DShaderManager::ShaderTypes shader = W3DShaderManager::getCurrentShader();
	const Bool cloudMap = (shader == W3DShaderManager::ST_ROAD_BASE_NOISE1 || shader == W3DShaderManager::ST_ROAD_BASE_NOISE12);
	const Bool lightMap = (shader == W3DShaderManager::ST_ROAD_BASE_NOISE2 || shader == W3DShaderManager::ST_ROAD_BASE_NOISE12);
	const Int noiseCount = (cloudMap ? 1 : 0) + (lightMap ? 1 : 0);

	if (m_dwShadowPixelShader[noiseCount] == 0 || TheW3DShadowMap == nullptr || !TheW3DShadowMap->hasDepth())
		return FALSE;

	DX8Wrapper::Set_Texture(0,W3DShaderManager::getShaderTexture(0));
	//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
	DX8Wrapper::Apply_Render_State_Changes();

	// The first stage after the road and noise textures. Fixed-function vertex processing
	// hands out texcoord sets in stage order, so this is the set the shader reads.
	const Int stage = 1 + noiseCount;
	if (!TheW3DShadowMap->bindReceiver(stage))
		return FALSE;
	m_shadowStage = stage;

	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, 0 );

	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE,FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_LIGHTING, FALSE);

	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE,true);	//blend roads into terrain
	DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);

	const DWORD mipFilter = (TheGlobalData && TheGlobalData->m_trilinearTerrainTex) ? D3DTEXF_LINEAR : D3DTEXF_POINT;
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, mipFilter);

	D3DMATRIX curView;
	DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

	D3DMATRIX inv;
	float det;
	Invert_D3DMATRIX(inv, &det, curView);

	// Cloud map first, then light map, matching the order roadnoise2 applies them.
	Int noiseStage = 1;
	for (Int map=0; map<2; map++)
	{
		const Bool isCloud = (map == 0);
		if (isCloud ? !cloudMap : !lightMap)
			continue;

		D3DMATRIX textureTransform = curView;
		if (isCloud)
			terrainShader2Stage.updateNoise1(&textureTransform, &inv, false);
		else
			terrainShader2Stage.updateNoise2(&textureTransform, &inv, false);

		DX8Wrapper::Set_Texture(noiseStage, W3DShaderManager::getShaderTexture(isCloud ? 1 : 2));
		DX8Wrapper::_Set_DX8_Transform((D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + noiseStage), textureTransform);

		DX8Wrapper::Set_DX8_Texture_Stage_State(noiseStage, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
		DX8Wrapper::Set_DX8_Texture_Stage_State(noiseStage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
		DX8Wrapper::Set_DX8_Texture_Stage_State(noiseStage, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
		DX8Wrapper::Set_DX8_Texture_Stage_State(noiseStage, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
		DX8Wrapper::Set_DX8_Texture_Stage_State(noiseStage, D3DTSS_MIPFILTER, mipFilter);
		DX8Wrapper::Set_DX8_Texture_Stage_State(noiseStage, D3DTSS_MINFILTER, isCloud ? D3DTEXF_LINEAR : D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(noiseStage, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
		noiseStage++;
	}

	DX8Wrapper::Set_Pixel_Shader(m_dwShadowPixelShader[noiseCount]);
	return TRUE;
}

Int RoadShaderPixelShader::init()
{
	Int res;

	//this shader will also use the 2Stage shader for some of the passes so initialize it too.
	if (roadShader2Stage.init() && (res=W3DShaderManager::getChipset()) >= DC_GENERIC_PIXEL_SHADER_1_1)
	{
		if (res >= DC_GENERIC_PIXEL_SHADER_1_1)
		{
			//this shader needs some assets that need to be loaded
			//shader declaration
			DWORD Declaration[]=
			{
				(D3DVSD_STREAM(0)),
				(D3DVSD_REG(0, D3DVSDT_FLOAT3)), // Position
				(D3DVSD_REG(1, D3DVSDT_D3DCOLOR)), // Diffuse
				(D3DVSD_REG(2, D3DVSDT_FLOAT2)), //  Texture Coordinates
				(D3DVSD_END())
			};

			//version which blends 2 noise textures.
			HRESULT hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\roadnoise2.pso", &Declaration[0], 0, false, &m_dwBaseNoise2PixelShader);
			if (FAILED(hr))
				return FALSE;

			initShadowReceiver();

			//Only set this shader for use in dual noise mode.  The 2Stage shader will take care of
			//all the other modes.
			W3DShaders[W3DShaderManager::ST_ROAD_BASE_NOISE12]=&roadShaderPixelShader;
			W3DShadersPassCount[W3DShaderManager::ST_ROAD_BASE_NOISE12]=1;
			return TRUE;
		}
	}
	return FALSE;
}

Int RoadShaderPixelShader::set(Int pass)
{
	if (setShadowReceiver())
		return TRUE;

	DX8Wrapper::Set_Texture(0,W3DShaderManager::getShaderTexture(0));
	//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
	DX8Wrapper::Apply_Render_State_Changes();

	//tell pixel shader which UV set to use for each stage
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, 0 );

	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE,FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_LIGHTING, FALSE);

	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE,true);	//blend roads into terrain
	DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);

	D3DMATRIX curView;
	DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

	D3DMATRIX inv;
	float det;
	Invert_D3DMATRIX(inv, &det, curView);

	if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex)
	{	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
	}
	else
	{	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MIPFILTER, D3DTEXF_POINT);
	}

	DX8Wrapper::Set_DX8_Texture_Stage_State(1,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
	// Two output coordinates are used.
	DX8Wrapper::Set_DX8_Texture_Stage_State(1,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

	DX8Wrapper::Set_DX8_Texture_Stage_State(1,  D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(1,  D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);

	DX8Wrapper::Set_DX8_Texture_Stage_State(2,  D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(2,  D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);

	DX8Wrapper::Set_Texture(1,W3DShaderManager::getShaderTexture(1));
	DX8Wrapper::Set_Texture(2,W3DShaderManager::getShaderTexture(2));

	DX8Wrapper::Set_Pixel_Shader(m_dwBaseNoise2PixelShader);

	DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);

	DX8Wrapper::Set_DX8_Texture_Stage_State(2, D3DTSS_MINFILTER, D3DTEXF_POINT);
	DX8Wrapper::Set_DX8_Texture_Stage_State(2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);

	terrainShader2Stage.updateNoise1(&curView,&inv, false);	//get texture projection matrix
	DX8Wrapper::_Set_DX8_Transform(D3DTS_TEXTURE1, curView);

	terrainShader2Stage.updateNoise2(&curView,&inv, false);	//get texture projection matrix
	DX8Wrapper::_Set_DX8_Transform(D3DTS_TEXTURE2, curView);

	DX8Wrapper::Set_DX8_Texture_Stage_State(2,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
	// Two output coordinates are used.
	DX8Wrapper::Set_DX8_Texture_Stage_State(2,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

	return TRUE;
}

void RoadShaderPixelShader::reset()
{
	if (TheW3DShadowMap != nullptr && m_shadowStage >= 0)
		TheW3DShadowMap->unbindReceiver(m_shadowStage);
	m_shadowStage = -1;

	DX8Wrapper::Set_Pixel_Shader(0);	//turn off pixel shader

	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|0);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|1);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 2, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 2, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|2);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 3, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 3, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|3);


	DX8Wrapper::Invalidate_Cached_Render_States();
}

Int RoadShader2Stage::init()
{
	//no special device validation needed - anything in our min spec should handle this.
	W3DShaders[W3DShaderManager::ST_ROAD_BASE]=&roadShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_ROAD_BASE]=1;
	W3DShaders[W3DShaderManager::ST_ROAD_BASE_NOISE1]=&roadShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_ROAD_BASE_NOISE1]=1;
	W3DShaders[W3DShaderManager::ST_ROAD_BASE_NOISE2]=&roadShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_ROAD_BASE_NOISE2]=1;
	W3DShaders[W3DShaderManager::ST_ROAD_BASE_NOISE12]=&roadShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_ROAD_BASE_NOISE12]=2;

	return TRUE;
}

Int RoadShader2Stage::set(Int pass)
{
	// Receiving the shadow map needs a pixel shader, which covers every mode in one pass.
	if (pass == 0 && roadShaderPixelShader.setShadowReceiver())
		return TRUE;

	//First stage always contains base texture.
	DX8Wrapper::Set_Texture(0,W3DShaderManager::getShaderTexture(0));
	//Force system to apply world/view transforms.
	DX8Wrapper::Apply_Render_State_Changes();

	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE,FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_LIGHTING, FALSE);

	// Modulate the diffuse color with the texture as lighting comes from diffuse.
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );

	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, 0 );
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE,true);	//blend roads into terrain

	if (pass == 0)
	{
		DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);

		if (W3DShaderManager::getCurrentShader() >= W3DShaderManager::ST_ROAD_BASE_NOISE1)
		{	//second texture unit will contain a noise pass
			D3DMATRIX curView;
			DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

			D3DMATRIX inv;
			float det;
			Invert_D3DMATRIX(inv, &det, curView);

			if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex)
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
			else
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MIPFILTER, D3DTEXF_POINT);

			DX8Wrapper::Set_DX8_Texture_Stage_State(1,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
			// Two output coordinates are used.
			DX8Wrapper::Set_DX8_Texture_Stage_State(1,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

			DX8Wrapper::Set_DX8_Texture_Stage_State(1,  D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
			DX8Wrapper::Set_DX8_Texture_Stage_State(1,  D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);

			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_MODULATE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );

			if (W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_ROAD_BASE_NOISE12)
			{	//full shader, apply noise 1 in pass 0.
				DX8Wrapper::Set_Texture(1,W3DShaderManager::getShaderTexture(1));
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);

				terrainShader2Stage.updateNoise1(&curView, &inv, false);	//get texture projection matrix
				DX8Wrapper::_Set_DX8_Transform(D3DTS_TEXTURE1, curView);
			}
			else
			{	//single noise texture shader
				if (W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_ROAD_BASE_NOISE1)
				{	//cloud map
					DX8Wrapper::Set_Texture(1,W3DShaderManager::getShaderTexture(1));
					terrainShader2Stage.updateNoise1(&curView, &inv, false);	//update curView with texture matrix
					DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
					DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
				}
				else
				{	//light map
					DX8Wrapper::Set_Texture(1,W3DShaderManager::getShaderTexture(2));
					terrainShader2Stage.updateNoise2(&curView,&inv, false);	//update curView with texture matrix
					DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_POINT);
					DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
				}
				DX8Wrapper::_Set_DX8_Transform(D3DTS_TEXTURE1, curView);
			}
		}
		else
		{	//just base texturing
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
		}
	}
	else
	{	//pass 1, apply additional noise pass
		D3DMATRIX curView;
		DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

		D3DMATRIX inv;
		float det;
		Invert_D3DMATRIX(inv, &det, curView);

		if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex)
			DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
		else
			DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MIPFILTER, D3DTEXF_POINT);

		DX8Wrapper::Set_Texture(1,W3DShaderManager::getShaderTexture(2));

		terrainShader2Stage.updateNoise2(&curView, &inv, false);	//update curView with texture matrix
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);

		DX8Wrapper::Set_DX8_Texture_Stage_State(1,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
		// Two output coordinates are used.
		DX8Wrapper::Set_DX8_Texture_Stage_State(1,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

		DX8Wrapper::Set_DX8_Texture_Stage_State(1,  D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1,  D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);

		//Copy alpha channel into stage 1 but mask out color channel by replacing with white.
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		//Force color channel to white by copying the alpha into RGB
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE|D3DTA_ALPHAREPLICATE);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG2);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );

		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_BLENDCURRENTALPHA);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
		DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

		//Modulate into existing roads with clouds applied. - only apply where roads are transparent by
		//using road texture as a mask.
		DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND,D3DBLEND_ZERO);
		DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND,D3DBLEND_SRCCOLOR);

		DX8Wrapper::_Set_DX8_Transform(D3DTS_TEXTURE0, curView);
	}

	return TRUE;
}

void RoadShader2Stage::reset()
{
	if (roadShaderPixelShader.m_shadowStage >= 0)
	{
		roadShaderPixelShader.reset();
		return;
	}

	ShaderClass::Invalidate();

	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|0);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|1);
}

/** List of all custom shader lists - each list in this list contains variations of the same
	shader to allow it to work on different hardware configurations.
*/
#if defined(BUILD_WITH_D3D9)
static DWORD InstancedMainShader = 0;	///<main scene meshes and their passes drawn in groups; the scene draws without it.
static DWORD SkinnedMainShader = 0;	///<main scene skins and their passes deformed on the GPU; the scene draws without it.
#endif

W3DShaderInterface **MasterShaderList[]=
{
	TerrainShaderList,
	ShroudShaderList,
	FlatShroudShaderList,
	RoadShaderList,
	MaskShaderList,
	CloudShaderList,
	FlatTerrainShaderList,
#if defined(BUILD_WITH_D3D9)
	ShadowDepthShaderList,
	ShadowMultiplyShaderList,
	SpecularShaderList,
#endif
	nullptr
};

/** List of all custom filter lists - each list in this list contains variations of the same
	filter to allow it to work on different hardware configurations.
*/
W3DFilterInterface **MasterFilterList[]=
{
	ScreenDefaultFilterList,
	ScreenBWFilterList,
	ScreenMotionBlurFilterList,
	ScreenCrossFadeFilterList,
	nullptr
};

// W3DShaderManager::W3DShaderManager =========================================
/** Constructor - just clears some variables */
//=============================================================================
W3DShaderManager::W3DShaderManager()
{
	m_currentShader = ST_INVALID;
	m_currentFilter = FT_NULL_FILTER;
	m_oldRenderSurface = nullptr;
	m_renderTexture = nullptr;
	m_newRenderSurface = nullptr;
	m_oldDepthSurface = nullptr;
	m_renderingToTexture = false;
	Int i;
	for (i=0; i<W3DShaderManager::ST_MAX; i++)
	{	W3DShaders[i]=nullptr;
		W3DShadersPassCount[i]=0;
	}
	for (i=0; i<FT_MAX; i++)
	{	W3DFilters[i]=nullptr;
	}
	for (i=0; i<8; i++)
	{
		m_Textures[i]=nullptr;
	}
	m_currentShader=(W3DShaderManager::ShaderTypes)-1;
}

// W3DShaderManager::init =======================================================
/** Walk through all shaders and find versions suitable for current hardware */
//=============================================================================
void W3DShaderManager::init()
{
	int i,j;

	D3DSURFACE_DESC desc;
	// For now, check & see if we are gf3 or higher on the food chain.

	ChipsetType res=DC_UNKNOWN;
	if ((res=W3DShaderManager::getChipset()) != 0)
	{
		m_currentChipset = res;	//cache the current chipset.

		//Some of our effects require an offscreen render target, so try creating it here.
		HRESULT hr=DX8Wrapper::_Get_D3D_Device8()->GetRenderTarget(DX8_SWAPCHAIN &m_oldRenderSurface);

		if (hr != S_OK || !m_oldRenderSurface)
			return;

		m_oldRenderSurface->GetDesc(&desc);
		
		// TheSuperHackers @bugfix Redirecting rendering to a non-multisampled texture
		// while using a multisampled depth buffer is an API violation in DX8.
		if (desc.MultiSampleType == D3DMULTISAMPLE_NONE)
		{
			hr=DX8_CREATE_TEXTURE(DX8Wrapper::_Get_D3D_Device8(),desc.Width,desc.Height,1,D3DUSAGE_RENDERTARGET,desc.Format,D3DPOOL_DEFAULT,&m_renderTexture);
		}
		else
		{
			// Force failure path to avoid MSAA mismatch
			hr = E_FAIL;
		}

		if (hr != S_OK)
		{
			SAFE_RELEASE(m_oldRenderSurface);
			m_renderTexture = nullptr;
		} else {
			hr = m_renderTexture->GetSurfaceLevel(0, &m_newRenderSurface);
			if (hr != S_OK)
			{
				SAFE_RELEASE(m_renderTexture);
				m_newRenderSurface = nullptr;
			}	else {
				hr = DX8Wrapper::_Get_D3D_Device8()->GetDepthStencilSurface(&m_oldDepthSurface);
				if (hr != S_OK)
				{
					SAFE_RELEASE(m_newRenderSurface);
					SAFE_RELEASE(m_renderTexture);
					m_oldDepthSurface = nullptr;
				}
			}
		}
	}

	W3DShaderInterface **shaders;

	for (i=0; MasterShaderList[i] != nullptr; i++)
	{
		shaders=MasterShaderList[i];
		for (j=0; shaders[j] != nullptr; j++)
		{
			if (shaders[j]->init())
				break;	//found a working shader
		}
	}
#if defined(BUILD_WITH_D3D9)
	// The instancing module builds its own declarations, so this one only has to be valid.
	DWORD instanceDeclaration[] =
	{
		D3DVSD_STREAM(0),
		D3DVSD_REG(0, D3DVSDT_FLOAT3),
		D3DVSD_END()
	};
	if (SUCCEEDED(LoadAndCreateD3DShader("shaders\\instancemain.vso", instanceDeclaration, 0, true, &InstancedMainShader)))
	{
		DX8InstancingClass::Set_Main_Shader(Peek_D3D9_Vertex_Shader(InstancedMainShader));
	}
	else
	{
		InstancedMainShader = 0;
	}
	if (SUCCEEDED(LoadAndCreateD3DShader("shaders\\skinmain.vso", instanceDeclaration, 0, true, &SkinnedMainShader)))
	{
		DX8SkinningClass::Set_Main_Shader(Peek_D3D9_Vertex_Shader(SkinnedMainShader));
	}
	else
	{
		SkinnedMainShader = 0;
	}
#endif

	W3DFilterInterface **filters;

	for (i=0; MasterFilterList[i] != nullptr; i++)
	{
		filters=MasterFilterList[i];
		for (j=0; filters[j] != nullptr; j++)
		{
			if (filters[j]->init())
				break;	//found a working shader
		}
	}

	DEBUG_LOG(("ShaderManager ChipsetID %d", res));
}

// W3DShaderManager::shutdown =======================================================
/** Any shaders which allocate resources will be allowed to free them */
//=============================================================================
void W3DShaderManager::shutdown()
{
	SAFE_RELEASE(m_newRenderSurface);
	SAFE_RELEASE(m_renderTexture);
	SAFE_RELEASE(m_oldRenderSurface);
	SAFE_RELEASE(m_oldDepthSurface);
	m_currentShader = ST_INVALID;
	m_currentFilter = FT_NULL_FILTER;
	//release any assets associated with a shader (vertex/pixel shaders, textures, etc.)
	Int i=0;
	for (; i<W3DShaderManager::ST_MAX; i++) {
		if (W3DShaders[i]) {
			W3DShaders[i]->shutdown();
		}
	}

	for (i=0; i < FT_MAX; i++)
	{
		if (W3DFilters[i])
		{
			W3DFilters[i]->shutdown();
		}
	}

#if defined(BUILD_WITH_D3D9)
	DX8InstancingClass::Set_Main_Shader(nullptr);
	if (InstancedMainShader != 0)
	{
		DX8_DELETE_VERTEX_SHADER(DX8Wrapper::_Get_D3D_Device8(), InstancedMainShader);
		InstancedMainShader = 0;
	}
	DX8SkinningClass::Set_Main_Shader(nullptr);
	if (SkinnedMainShader != 0)
	{
		DX8_DELETE_VERTEX_SHADER(DX8Wrapper::_Get_D3D_Device8(), SkinnedMainShader);
		SkinnedMainShader = 0;
	}
#endif
}

//=============================================================================
void W3DShaderManager::updateCloud()
{
	terrainShader2Stage.updateCloud();
}

// W3DShaderManager::getShaderPasses =======================================================
/** Return number of renderig passes required in perform the desired shader on current
	hardware.  App will need to re-render the polygons this many times to complete the
	effect.
 */
//=============================================================================
Int W3DShaderManager::getShaderPasses(ShaderTypes shader)
{
	return W3DShadersPassCount[shader];
}

// W3DShaderManager::setShader =======================================================
/** Must call this method before each rendering pass in order to perform proper D3D
	setup for each shader.
 */
//=============================================================================
Int W3DShaderManager::setShader(ShaderTypes shader, Int pass)
{
	if (shader == m_currentShader && pass == m_currentShaderPass)
		return TRUE;	//shader is already set
	m_currentShader=shader;
	m_currentShaderPass = pass;
	if (W3DShaders[shader])
		return W3DShaders[shader]->set(pass);
	return FALSE;
}

// W3DShaderManager::resetShader =======================================================
/** Must call this method after all polygons and rendering passes have been submitted.
	This method allows D3D to reset itself to a default state that doesn't conflict
	with the WW3D2 Shader system.
 */
//=============================================================================
void W3DShaderManager::resetShader(ShaderTypes shader)
{
	if (m_currentShader == ST_INVALID)
		return;	//last shader is already reset.
	if (W3DShaders[shader])
		W3DShaders[shader]->reset();
	m_currentShader = ST_INVALID;
}
// W3DShaderManager::filterPreRender =======================================================
/** Call to view filter shaders before rendering starts.
 */
//=============================================================================
Bool W3DShaderManager::filterPreRender(FilterTypes filter, Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	if (W3DFilters[filter])
	{	Bool result=W3DFilters[filter]->preRender(skipRender,scenePassMode);
		if (result)
			m_currentFilter = filter;
		return result;
	}
	return FALSE;
}

// W3DShaderManager::filterPostRender =======================================================
/** Call to view filter shaders after rendering is complete.
 */
//=============================================================================
Bool W3DShaderManager::filterPostRender(FilterTypes filter, FilterModes mode, Coord2D &scrollDelta, Bool &doExtraRender)
{
	if (W3DFilters[filter])
		return W3DFilters[filter]->postRender(mode, scrollDelta,doExtraRender);

	m_currentFilter = FT_NULL_FILTER;
	return FALSE;
}

// W3DShaderManager::filterPostRender =======================================================
/** Call to view filter shaders after rendering is complete.
 */
//=============================================================================
	static Bool filterSetup(FilterTypes filter, FilterModes mode);
Bool W3DShaderManager::filterSetup(FilterTypes filter, FilterModes mode)
{
	if (W3DFilters[filter])
		return W3DFilters[filter]->setup(mode);
	return FALSE;
}

/*Draws 2 triangles covering the viewport given the current render states*/
void W3DShaderManager::drawViewport(Int color)
{
	LPDIRECT3DDEVICE8 pDev=DX8Wrapper::_Get_D3D_Device8();

	struct _TRANS_LIT_TEX_VERTEX {
		Vector4 p;
		DWORD color;   // diffuse color
		float	u;
		float	v;
	} v[4];

	Int xpos, ypos, width, height;

	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

	//bottom right
	v[0].p = Vector4( xpos+width-0.5f, ypos+height-0.5f, 0.0f, 1.0f );
	v[0].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top right
	v[1].p = Vector4( xpos+width-0.5f, ypos-0.5f, 0.0f, 1.0f );
	v[1].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	//bottom left
	v[2].p = Vector4(  xpos-0.5f, ypos+height-0.5f, 0.0f, 1.0f );
	v[2].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top left
	v[3].p = Vector4(  xpos-0.5f,  ypos-0.5f, 0.0f, 1.0f );
	v[3].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	v[0].color = color;
	v[1].color = color;
	v[2].color = color;
	v[3].color = color;

	//draw polygons like this is very inefficient but for only 2 triangles, it's
	//not worth bothering with index/vertex buffers.
	DX8_SET_FVF(pDev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);

	pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(_TRANS_LIT_TEX_VERTEX));
}

// W3DShaderManager::startRenderToTexture =======================================================
/** Starts rendering to a texture.
 */
//=============================================================================
void W3DShaderManager::startRenderToTexture()
{
	DEBUG_ASSERTCRASH(!m_renderingToTexture, ("Already rendering to texture - cannot nest calls."));

	if (m_renderingToTexture || m_newRenderSurface==nullptr || m_oldDepthSurface==nullptr) return;

	// The flip model rotates the back buffers, so what is bound now is what endRenderToTexture restores.
	IDirect3DSurface8 *currentTarget = nullptr;
	IDirect3DSurface8 *currentDepth = nullptr;
	if (FAILED(DX8Wrapper::_Get_D3D_Device8()->GetRenderTarget(DX8_SWAPCHAIN &currentTarget)) ||
		FAILED(DX8Wrapper::_Get_D3D_Device8()->GetDepthStencilSurface(&currentDepth)))
	{
		SAFE_RELEASE(currentTarget);
		return;
	}
	SAFE_RELEASE(m_oldRenderSurface);
	SAFE_RELEASE(m_oldDepthSurface);
	m_oldRenderSurface = currentTarget;
	m_oldDepthSurface = currentDepth;

	HRESULT hr = DX8Wrapper::Set_DX8_Render_Target_Surfaces(m_newRenderSurface,m_oldDepthSurface);

	// TheSuperHackers @bugfix If SetRenderTarget fails (e.g. due to MSAA forced by driver
	// profile causing a depth buffer mismatch that D3DSURFACE_DESC doesn't report), permanently
	// disable RTT to prevent repeated failures and accidental backbuffer clears.
	if (hr != S_OK)
	{
		// Permanently disable RTT
		SAFE_RELEASE(m_newRenderSurface);
		SAFE_RELEASE(m_renderTexture);
		SAFE_RELEASE(m_oldRenderSurface);
		SAFE_RELEASE(m_oldDepthSurface);
		return;
	}

	m_renderingToTexture = true;
	if (TheGlobalData->m_showSoftWaterEdge)
	{	//Soft water edges use frame buffer destination alpha so we must clear it to a known value.
		if (m_currentFilter == FT_VIEW_MOTION_BLUR_FILTER || m_currentFilter == FT_VIEW_CROSSFADE)
		{	//these filters rely on the previous frame being visible so we must be careful about clearing
			//frame buffer.  Only clear the alpha channel
			DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_ALPHA);	//only clear alpha
			ShaderClass shader=ShaderClass::_PresetOpaqueSolidShader;
			shader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);
			shader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
			DX8Wrapper::Set_Shader(shader);

			VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
			DX8Wrapper::Set_Material(vmat);
			REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.

			drawViewport(0x00ffffff | (((Int)(TheWaterTransparency->m_minWaterOpacity*255.0f)) <<24));
			DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_RED|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_BLUE);	//disable writes to alpha
		}
		else	//normal clear that overwrites everything.
			DX8Wrapper::Clear(true, false, Vector3( 0.0f, 0.0f, 0.0f ), TheWaterTransparency->m_minWaterOpacity);
	}
}

// W3DShaderManager::startRenderToTexture =======================================================
/** Ends rendering to a texture.
 */
//=============================================================================
IDirect3DTexture8 *W3DShaderManager::endRenderToTexture()
{
	DEBUG_ASSERTCRASH(m_renderingToTexture, ("Not rendering to texture."));
	if (!m_renderingToTexture) return nullptr;
	HRESULT hr = DX8Wrapper::Set_DX8_Render_Target_Surfaces(m_oldRenderSurface,m_oldDepthSurface);	//restore original render target
	DEBUG_ASSERTCRASH(hr==S_OK, ("Set target failed unexpectedly."));
	if (hr == S_OK)
	{
		//assume render target texture will be in stage 0.  Most hardware has "conditional" support for
		//non-power-of-2 textures so we must force some required states:
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSW, D3DTADDRESS_CLAMP);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_MIPFILTER, D3DTEXF_NONE);

		m_renderingToTexture = false;
	}
	return m_renderTexture;
}

/**Returns texture containing the image that was last rendered using any of the effects requiring render target
textures.  Used mostly for cross-fading effects that need an unmodified version of the view before the effect
was applied.  NOTE: This texture does not survive device reset.. so quit effect on reset!*/
IDirect3DTexture8 *W3DShaderManager::getRenderTexture()
{
	return m_renderTexture;
}

Bool W3DShaderManager::copyRenderTarget(IDirect3DTexture8 *&copy)
{
#if defined(BUILD_WITH_D3D9)
	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DSurface8 *target = nullptr;
	if (FAILED(device->GetRenderTarget(0, &target)))
	{
		return FALSE;
	}

	D3DSURFACE_DESC targetDesc;
	target->GetDesc(&targetDesc);
	if (copy != nullptr)
	{
		D3DSURFACE_DESC copyDesc;
		copy->GetLevelDesc(0, &copyDesc);
		if (copyDesc.Width != targetDesc.Width || copyDesc.Height != targetDesc.Height || copyDesc.Format != targetDesc.Format)
		{
			SAFE_RELEASE(copy);
		}
	}
	if (copy == nullptr &&
		FAILED(device->CreateTexture(targetDesc.Width, targetDesc.Height, 1, D3DUSAGE_RENDERTARGET, targetDesc.Format, D3DPOOL_DEFAULT, &copy, nullptr)))
	{
		copy = nullptr;
		target->Release();
		return FALSE;
	}

	// StretchRect also resolves a multisampled target.
	IDirect3DSurface8 *copySurface = nullptr;
	Bool copied = FALSE;
	if (SUCCEEDED(copy->GetSurfaceLevel(0, &copySurface)))
	{
		copied = SUCCEEDED(device->StretchRect(target, nullptr, copySurface, nullptr, D3DTEXF_NONE));
		copySurface->Release();
	}
	target->Release();
	return copied;
#else
	(void)copy;
	return FALSE;
#endif
}

Vector4 W3DShaderManager::getClipToTargetMapping(Real width, Real height)
{
	D3DVIEWPORT8 viewport;
	DX8Wrapper::_Get_D3D_Device8()->GetViewport(&viewport);
	return Vector4(0.5f * viewport.Width / width, -0.5f * viewport.Height / height,
		(viewport.X + 0.5f * viewport.Width + 0.5f) / width, (viewport.Y + 0.5f * viewport.Height + 0.5f) / height);
}

enum GraphicsVenderID CPP_11(: Int)
{
	DC_NVIDIA_VENDOR_ID	= 0x10DE,
	DC_3DFX_VENDOR_ID	= 0x121A,
	DC_ATI_VENDOR_ID	= 0x1002
};

// W3DShaderManager::ChipsetType =======================================================
/** Returns the chipset used by the currently active rendering device.  Can be useful
	for coding around specific driver bugs.
 */
//=============================================================================
ChipsetType W3DShaderManager::getChipset()
{
	//check if globaldata has an override for current chipset
	if (TheGlobalData && TheGlobalData->m_chipSetType != DC_UNKNOWN)
		return (ChipsetType)TheGlobalData->m_chipSetType;

	ChipsetType chip=DC_UNKNOWN;
	IDirect3D8* d3d8Interface=DX8Wrapper::_Get_D3D8();

	if (d3d8Interface && DX8Wrapper::_Get_D3D_Device8())
	{

		D3DADAPTER_IDENTIFIER8 did;
		::ZeroMemory(&did, sizeof(D3DADAPTER_IDENTIFIER8));
	/*	HRESULT res = */ d3d8Interface->GetAdapterIdentifier(0,D3DENUM_NO_WHQL_LEVEL,&did);
		*((LARGE_INTEGER*)&m_driverVersion) = did.DriverVersion;

		if(did.VendorId == DC_NVIDIA_VENDOR_ID)
		{
			m_currentVendor = DC_NVIDIA_VENDOR_ID;

			if (did.DeviceId == 0x20)
				return DC_TNT;

			if (did.DeviceId >= 0x28 && did.DeviceId < 0x100)
				return DC_TNT2;

			if ( (did.DeviceId >= 0x100 && did.DeviceId <= 0x103) ||	//GeForce
				 (did.DeviceId >= 0x110 && did.DeviceId <= 0x113) ||	//GeForce2 MX
						 (did.DeviceId >= 0x150 && did.DeviceId <= 0x153) )	//GeForce2
           		return DC_GEFORCE2;

			if (did.DeviceId >= 0x200 && did.DeviceId < 0x250)
				return DC_GEFORCE3;

			if (did.DeviceId >= 0x250)
				return DC_GEFORCE4;
		}
		else
		if(did.VendorId == DC_3DFX_VENDOR_ID)
		{
			m_currentVendor = DC_3DFX_VENDOR_ID;

			if (did.DeviceId == 0x0002)
				return DC_VOODOO2;
			if (did.DeviceId == 0x0005)
				return DC_VOODOO3;
			if (did.DeviceId == 0x0008)	///@todo: Just guessing on this one - find actual Voodoo4 deviceID.
				return DC_VOODOO4;
			if (did.DeviceId == 0x0009)
				return DC_VOODOO5;
		}
		else
		if(did.VendorId == DC_ATI_VENDOR_ID)
		{
			m_currentVendor = DC_ATI_VENDOR_ID;

			if (did.DeviceId == 0x5144)
				return DC_RADEON;
			if (did.DeviceId == 0x514C)
				return DC_RADEON_8500;
			if (did.DeviceId == 0x4e44)
				return DC_RADEON_9700;
		}

		//None of the vendor specific ID's matched so use generic means to classify the card
		Int maxTextures=DX8Wrapper::Get_Current_Caps()->Get_Max_Simultaneous_Textures();
		Real pixelShaderVersion;

		char buf[256];

		//Convert version to Real
		sprintf(buf,"%d.%d",DX8Wrapper::Get_Current_Caps()->Get_Pixel_Shader_Major_Version(),DX8Wrapper::Get_Current_Caps()->Get_Pixel_Shader_Minor_Version());
		sscanf(buf,"%f",&pixelShaderVersion);

		if (maxTextures >= 4)
		{	if (pixelShaderVersion >= 1.1f)
				chip=DC_GENERIC_PIXEL_SHADER_1_1;
			if (pixelShaderVersion >= 1.4f)
				chip=DC_GENERIC_PIXEL_SHADER_1_4;
			if (maxTextures >= 8 && pixelShaderVersion >= 2.0f)
				chip=DC_GENERIC_PIXEL_SHADER_2_0;
		}
	}

	return chip;
}

//=============================================================================
// WaterRenderObjClass::LoadAndCreateShader
//=============================================================================
/** Loads and creates a D3D pixel or vertex shader.*/
//=============================================================================
HRESULT W3DShaderManager::LoadAndCreateD3DShader(const char* strFilePath, const DWORD* pDeclaration, DWORD Usage, Bool ShaderType, DWORD* pHandle)
{
	if (getChipset() < DC_GENERIC_PIXEL_SHADER_1_1)
		return E_FAIL;	//don't allow loading any shaders if hardware can't handle it.

	try
	{
		File *file = nullptr;
		HRESULT hr;

		file = TheFileSystem->openFile(strFilePath, File::READ | File::BINARY);
		if (file == nullptr)
		{
			DEBUG_LOG(("LoadAndCreateD3DShader: could not open %s", strFilePath));
			return E_FAIL;
		}

		FileInfo fileInfo;
		TheFileSystem->getFileInfo(AsciiString(strFilePath), &fileInfo);
		DWORD dwFileSize = fileInfo.sizeLow;

		const DWORD* pShader = (DWORD*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwFileSize);
		if (!pShader)
		{
			DEBUG_LOG(("LoadAndCreateD3DShader: out of memory for %s", strFilePath));
			return E_FAIL;
		}

		file->read((void *)pShader, dwFileSize);

		file->close();
		file = nullptr;

#if defined(BUILD_WITH_D3D9)
			// D3D9 separates the declaration from the shader and hands back COM objects,
			// so both go into the table the DWORD handle indexes.
			if (ShaderType) // SHADERTYPE_VERTEX
			{
				IDirect3DVertexDeclaration9* declaration = nullptr;
				hr = Create_D3D9_Declaration_From_D3D8(pDeclaration, &declaration);
				if (SUCCEEDED(hr))
				{
					IDirect3DVertexShader9* vertex_shader = nullptr;
					hr = DX8Wrapper::_Get_D3D_Device8()->CreateVertexShader(pShader, &vertex_shader);
					if (SUCCEEDED(hr))
					{
						*pHandle = Register_D3D9_Vertex_Shader(vertex_shader, declaration);
						if (*pHandle == 0)
						{
							vertex_shader->Release();
							declaration->Release();
							hr = E_OUTOFMEMORY;
						}
					}
					else
					{
						declaration->Release();
					}
				}
			}
			else // SHADERTYPE_PIXEL
			{
				IDirect3DPixelShader9* pixel_shader = nullptr;
				hr = DX8Wrapper::_Get_D3D_Device8()->CreatePixelShader(pShader, &pixel_shader);
				if (SUCCEEDED(hr))
				{
					*pHandle = Register_D3D9_Pixel_Shader(pixel_shader);
					if (*pHandle == 0)
					{
						pixel_shader->Release();
						hr = E_OUTOFMEMORY;
					}
				}
			}
#else
			if (ShaderType) // SHADERTYPE_VERTEX
			{
				hr = DX8Wrapper::_Get_D3D_Device8()->CreateVertexShader(pDeclaration, pShader, pHandle, Usage);
			}
			else // SHADERTYPE_PIXEL
			{
				hr = DX8Wrapper::_Get_D3D_Device8()->CreatePixelShader(pShader, pHandle);
			}
#endif

		HeapFree(GetProcessHeap(), 0, (void*)pShader);

		if (FAILED(hr))
		{
			DEBUG_LOG(("LoadAndCreateD3DShader: failed to create %s, hr=0x%08X", strFilePath, hr));
			return E_FAIL;
		}
	}
	catch(...)
	{
		DEBUG_LOG(("LoadAndCreateD3DShader: exception loading %s", strFilePath));
		return E_FAIL;
	}

	DEBUG_LOG(("LoadAndCreateD3DShader: loaded %s -> handle 0x%08X", strFilePath, *pHandle));
	return S_OK;
}

//For the MP test, we're enforcing high min-spec requirements that need to be verified.
#define MIN_INTEL_CPU_FREQ	1300
#define MIN_AMD_CPU_FREQ	1100
#define MIN_ACCEPTED_FREQUENCY	1300
#define MIN_ACCEPTED_MEMORY	(1024*1024*256)	//256 MB
#define MIN_ACCEPTED_TEXTURE_MEMORY	(1024*1024*30)	//30 MB

/**Hack to give gameengine access to this function*/
Bool testMinimumRequirements(ChipsetType *videoChipType, CpuType *cpuType, Int *cpuFreq, MemValueType *numRAM, Real *intBenchIndex, Real *floatBenchIndex, Real *memBenchIndex)
{
	return W3DShaderManager::testMinimumRequirements(videoChipType,cpuType,cpuFreq,numRAM,intBenchIndex,floatBenchIndex,memBenchIndex);
}

Bool W3DShaderManager::testMinimumRequirements(ChipsetType *videoChipType, CpuType *cpuType, Int *cpuFreq, MemValueType *numRAM, Real *intBenchIndex, Real *floatBenchIndex, Real *memBenchIndex)
{
	if (videoChipType)
		*videoChipType = getChipset();

	if (cpuType)
	{
		*cpuType = XX;	//unknown

		//Check if it's an Athlon
		if (CPUDetectClass::Get_Processor_Manufacturer() == CPUDetectClass::MANUFACTURER_AMD &&
				CPUDetectClass::Get_AMD_Processor() >= CPUDetectClass::AMD_PROCESSOR_ATHLON_025)
				*cpuType = K7;

		//Check if it's a P3
		if (CPUDetectClass::Get_Processor_Manufacturer() == CPUDetectClass::MANUFACTURER_INTEL &&
				CPUDetectClass::Get_Intel_Processor() >= CPUDetectClass::INTEL_PROCESSOR_PENTIUM_III_MODEL_7)
				*cpuType = P3;
		//Check if it's a P4
		if (CPUDetectClass::Get_Processor_Manufacturer() == CPUDetectClass::MANUFACTURER_INTEL &&
				CPUDetectClass::Get_Intel_Processor() >= CPUDetectClass::INTEL_PROCESSOR_PENTIUM4)
				*cpuType = P4;
	}

	if (cpuFreq)
		*cpuFreq=CPUDetectClass::Get_Processor_Speed();

	if (numRAM)
		*numRAM=CPUDetectClass::Get_Total_Physical_Memory();

	if (intBenchIndex && floatBenchIndex && memBenchIndex)
	{
		// TheSuperHackers @tweak Aliendroid1 19/06/2025 Legacy benchmarking code was removed.
		// Since modern hardware always meets the minimum requirements, we preset the benchmark "results" to a high value.
		*intBenchIndex = 10.0f;
		*floatBenchIndex = 10.0f;
		*memBenchIndex = 10.0f;
	}

	return TRUE;
}

/**Try to guess how well the video card will handle the game assuming very fast CPU*/
StaticGameLODLevel W3DShaderManager::getGPUPerformanceIndex()
{
	ChipsetType	chipType;
	StaticGameLODLevel detailSetting=STATIC_GAME_LOD_LOW;	//assume lowest settings for now.

	if ((chipType=getChipset()) != DC_UNKNOWN)
	{	//a known video card so we can make some assumptions
		if (chipType >=	DC_GEFORCE2)
			detailSetting=STATIC_GAME_LOD_LOW;	//these cards need multiple terrain passes.
		if (chipType >= DC_GENERIC_PIXEL_SHADER_1_1)	//these cards can do terrain in single pass.
			detailSetting=STATIC_GAME_LOD_VERY_HIGH;
	}

	return detailSetting;
}

/**We need a hardware independent method to compare different CPU's.  For lack of anything better, we'll
use time to calculate PIE using a slow random number algorithm.*/

/**Used to test function call overhead*/
void add(float *sum,float *addend)
{
	*sum = *sum + *addend;
}

/**Returns seconds needed to run the test*/
Real W3DShaderManager::GetCPUBenchTime()
{
	float ztot, yran, ymult, ymod, x, y, z, pi, prod;
    long int low, ixran, itot, j, iprod;

  	__int64 endTime64,freq64,startTime64;
	QueryPerformanceFrequency((LARGE_INTEGER *)&freq64);
	QueryPerformanceCounter((LARGE_INTEGER *)&startTime64);

    ztot = 0.0;
    low = 1;
    ixran = 1907;
    yran = 5813.0;
    ymult = 1307.0;
    ymod = 5471.0;
    itot = 560000;	//total iterations. This value ends up running at ~30 fps on our P4-2.2Ghz.

    for(j=1; j<=itot; j++)
    {
		iprod = 27611 * ixran;
		ixran = iprod - 74383*(long int)(iprod/74383);
		x = (float)ixran / 74383.0;
		prod = ymult * yran;
		yran = (prod - ymod*(long int)(prod/ymod));
		y = yran / ymod;
		z = x*x + y*y;
		add(&ztot,&z);
		if ( z <= 1.0 )
		{
		  low = low + 1;
		}
	}
	pi = 4.0 * (float)low/(float)itot;

	QueryPerformanceCounter((LARGE_INTEGER *)&endTime64);
	return ((double)(endTime64-startTime64)/(double)(freq64));
}


// W3DShaderManager::setShroudTex =======================================================
/** Puts the shroud texture into a texture stage.
 */
//=============================================================================
Int W3DShaderManager::setShroudTex(Int stage)
{
	//We need to scale so shroud texel stretches over one full terrain cell.  Each texel
	//is 1/128 the size of full texture. (assuming 128x128 vid-mem texture).
	W3DShroud *shroud;
	if ((shroud=TheTerrainRenderObject->getShroud()) != nullptr)
	{
		DX8Wrapper::Set_Texture(stage, shroud->getShroudTexture());

		DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
		DX8Wrapper::Set_DX8_Texture_Stage_State( stage, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		DX8Wrapper::Set_DX8_Texture_Stage_State( stage, D3DTSS_COLORARG2, D3DTA_CURRENT );
		DX8Wrapper::Set_DX8_Texture_Stage_State( stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		DX8Wrapper::Set_DX8_Texture_Stage_State( stage, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
		DX8Wrapper::Set_DX8_Texture_Stage_State( stage, D3DTSS_COLOROP,   D3DTOP_MODULATE );
		DX8Wrapper::Set_DX8_Texture_Stage_State( stage, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2 );

		D3DMATRIX curView;
		DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

		D3DMATRIX inv;
		float det;
		Invert_D3DMATRIX(inv, &det, curView);

		D3DMATRIX scale,offset;

		//We need to make all world coordinates be relative to the heightmap data origin since that
		//is where the shroud begins.

		float xoffset = 0;
		float yoffset = 0;
		Real width=shroud->getCellWidth();
		Real height=shroud->getCellHeight();

		if (TheTerrainRenderObject->getMap())
		{	//subtract origin position from all coordinates.  Origin is shifted by 1 cell width/height to allow for unused border texels.
			xoffset = -(float)shroud->getDrawOriginX() + width;
			yoffset = -(float)shroud->getDrawOriginY() + height;
		}

		Set_D3DMATRIX_Translation(offset, xoffset, yoffset,0);

		width = 1.0f/(width*shroud->getTextureWidth());
		height = 1.0f/(height*shroud->getTextureHeight());
		Set_D3DMATRIX_Scaling(scale, width, height, 1);
		curView = (inv * offset) * scale;
		DX8Wrapper::_Set_DX8_Transform((D3DTRANSFORMSTATETYPE )(D3DTS_TEXTURE0+stage), curView);
		return TRUE;
	}
	return FALSE;
}



Int FlatTerrainShader2Stage::init()
{
	//no special device validation needed - anything in our min spec should handle this.

	W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE]=&flatTerrainShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE]=1;
	W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE1]=&flatTerrainShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE1]=2;
	W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE2]=&flatTerrainShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE2]=2;
	W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE12]=&flatTerrainShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE12]=2;

	return TRUE;
}

void FlatTerrainShader2Stage::reset()
{
	ShaderClass::Invalidate();

	//Free references to textures
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, nullptr);
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(1, nullptr);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|0);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|1);
}


Int FlatTerrainShader2Stage::set(Int pass)
{
	//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
	DX8Wrapper::Apply_Render_State_Changes();

	if (TheGlobalData && (TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex)) {
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	} else {
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_POINT);
	}
	if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex) {
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
	} else {
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MIPFILTER, D3DTEXF_POINT);
	}

	switch (pass)
	{
		case 0:

			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

			// Modulate the diffuse color with the texture as lighting comes from diffuse.
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			if (W3DShaderManager::getShaderTexture(0)) {
				DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, W3DShaderManager::getShaderTexture(0)->Peek_D3D_Texture());
				DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
				DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG2, D3DTA_CURRENT );
				DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
				DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

				DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
				DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

				//We need to scale so shroud texel stretches over one full terrain cell.  Each texel
				//is 1/128 the size of full texture. (assuming 128x128 vid-mem texture).
				W3DShroud *shroud;
				if ((shroud=TheTerrainRenderObject->getShroud()) != nullptr)
				{
					D3DMATRIX curView;
					DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

					D3DMATRIX inv;
					float det;
					Invert_D3DMATRIX(inv, &det, curView);

					D3DMATRIX scale,offset;

					//We need to make all world coordinates be relative to the heightmap data origin since that
					//is where the shroud begins.

					float xoffset = 0;
					float yoffset = 0;
					Real width=shroud->getCellWidth();
					Real height=shroud->getCellHeight();

					if (TheTerrainRenderObject->getMap())
					{	//subtract origin position from all coordinates.  Origin is shifted by 1 cell width/height to allow for unused border texels.
						xoffset = -(float)shroud->getDrawOriginX() + width;
						yoffset = -(float)shroud->getDrawOriginY() + height;
					}

					Set_D3DMATRIX_Translation(offset, xoffset, yoffset,0);

					width = 1.0f/(width*shroud->getTextureWidth());
					height = 1.0f/(height*shroud->getTextureHeight());
					Set_D3DMATRIX_Scaling(scale, width, height, 1);
					curView = (inv * offset) * scale;
					DX8Wrapper::_Set_DX8_Transform((D3DTRANSFORMSTATETYPE )(D3DTS_TEXTURE0), curView);
				}
			}	else {
				DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG2 );
				DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, 0 );
			}
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

			// Modulate the diffuse color with the texture as lighting comes from diffuse.
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_MODULATE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXCOORDINDEX, 0 );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|0);
			DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE,false);
			break;
		case 1:
			// Noise/cloud pass
			D3DMATRIX curView;
			DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

			//these states apply to all noise/cloud combination passes
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

			DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
			// Two output coordinates are used.
			DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
			DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
			DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);

			//blend into frame buffer
			DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE,true);
			DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND,D3DBLEND_DESTCOLOR);
			DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND,D3DBLEND_ZERO);

			D3DMATRIX inv;
			float det;
			Invert_D3DMATRIX(inv, &det, curView);

			if (W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE12)
			{
				//setup cloud pass

				terrainShader2Stage.updateNoise1(&curView,&inv);	//update curView with texture matrix
				DX8Wrapper::_Set_DX8_Transform(D3DTS_TEXTURE0, curView);
				//clouds always need bilinear filtering
				DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
				DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
				DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, W3DShaderManager::getShaderTexture(2)->Peek_D3D_Texture());

				//setup noise pass

				terrainShader2Stage.updateNoise2(&curView,&inv);
				DX8Wrapper::_Set_DX8_Transform(D3DTS_TEXTURE1, curView);
				//noise always needs point/linear filtering.  Why point!?
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_POINT);
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);

				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_MODULATE );
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
				// Two output coordinates are used.
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
				DX8Wrapper::_Get_D3D_Device8()->SetTexture(1, W3DShaderManager::getShaderTexture(3)->Peek_D3D_Texture());
			}
			else
			{	//only 1 noise or cloud texture
				// Now setup the texture pipeline.
				if (W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE1)
				{	//setup cloud pass
					DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, W3DShaderManager::getShaderTexture(2)->Peek_D3D_Texture());
					terrainShader2Stage.updateNoise1(&curView,&inv);	//update curView with texture matrix
					DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
					DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
				}
				else
				{
					//setup noise pass
					DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, W3DShaderManager::getShaderTexture(3)->Peek_D3D_Texture());
					terrainShader2Stage.updateNoise2(&curView,&inv);	//update curView with texture matrix
					DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_POINT);
					DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
				}

				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
				DX8Wrapper::_Set_DX8_Transform(D3DTS_TEXTURE0, curView);
			}
			break;
	}

	return TRUE;
}






Int FlatTerrainShaderPixelShader::shutdown()
{
	if (m_dwBasePixelShader)
		DX8_DELETE_PIXEL_SHADER(DX8Wrapper::_Get_D3D_Device8(), m_dwBasePixelShader);

	if (m_dwBase0PixelShader)
		DX8_DELETE_PIXEL_SHADER(DX8Wrapper::_Get_D3D_Device8(), m_dwBase0PixelShader);

	if (m_dwBaseNoise1PixelShader)
		DX8_DELETE_PIXEL_SHADER(DX8Wrapper::_Get_D3D_Device8(), m_dwBaseNoise1PixelShader);

	if (m_dwBaseNoise2PixelShader)
		DX8_DELETE_PIXEL_SHADER(DX8Wrapper::_Get_D3D_Device8(), m_dwBaseNoise2PixelShader);

	m_dwBasePixelShader=0;
	m_dwBase0PixelShader=0;
	m_dwBaseNoise1PixelShader=0;
	m_dwBaseNoise2PixelShader=0;

	return TRUE;
}

Int FlatTerrainShaderPixelShader::init()
{
	Int res;

#ifdef DISABLE_PIXEL_SHADERS
	return false;
#endif

	//this shader will also use the 2Stage shader for some of the passes so initialize it too.
	if ((res=W3DShaderManager::getChipset()) >= DC_GENERIC_PIXEL_SHADER_1_1)
	{
		if (res >= DC_GENERIC_PIXEL_SHADER_1_1)
		{
			//this shader needs some assets that need to be loaded
			//shader decleration
			DWORD Declaration[]=
			{
				(D3DVSD_STREAM(0)),
				(D3DVSD_REG(0, D3DVSDT_FLOAT3)), // Position
				(D3DVSD_REG(1, D3DVSDT_D3DCOLOR)), // Diffuse
				(D3DVSD_REG(2, D3DVSDT_FLOAT2)), //  Texture Coordinates
				(D3DVSD_REG(3, D3DVSDT_FLOAT2)), //  Texture Coordinates
				(D3DVSD_END())
			};

			//base version which doesn't apply any noise textures.
			HRESULT hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\fterrain.pso", &Declaration[0], 0, false, &m_dwBasePixelShader);
			if (FAILED(hr))
				return FALSE;

			//base version which doesn't apply any shroud textures.
			hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\fterrain0.pso", &Declaration[0], 0, false, &m_dwBase0PixelShader);
			if (FAILED(hr))
				return FALSE;

			//version which blends 1 noise texture.
			hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\fterrainnoise.pso", &Declaration[0], 0, false, &m_dwBaseNoise1PixelShader);
			if (FAILED(hr))
				return FALSE;

			//version which blends 2 noise textures.
			hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\fterrainnoise2.pso", &Declaration[0], 0, false, &m_dwBaseNoise2PixelShader);
			if (FAILED(hr))
				return FALSE;

			W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE]=&flatTerrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE1]=&flatTerrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE2]=&flatTerrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE12]=&flatTerrainShaderPixelShader;
			W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE]=1;
			W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE1]=1;
			W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE2]=1;
			W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE12]=1;
			return TRUE;
		}
	}
	return FALSE;
}

Int FlatTerrainShaderPixelShader::set(Int pass)
{
	//setup base pass
	Int curStage = 1;
	// setup terrain [3/31/2003]

	DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_Texture(0, W3DShaderManager::getShaderTexture(2));
	DX8Wrapper::Set_Texture(1, W3DShaderManager::getShaderTexture(2));
	//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
	DX8Wrapper::Apply_Render_State_Changes();




	DX8Wrapper::Set_DX8_Texture_Stage_State( curStage, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Texture_Stage_State( curStage, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	//tell pixel shader which UV set to use for each stage
	DX8Wrapper::Set_DX8_Texture_Stage_State( curStage, D3DTSS_TEXCOORDINDEX, 0 );
	DX8Wrapper::Set_DX8_Texture_Stage_State(curStage,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

	if (TheGlobalData && (TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex)) {
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	} else {
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage, D3DTSS_MINFILTER, D3DTEXF_POINT);
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage, D3DTSS_MAGFILTER, D3DTEXF_POINT);
	}
	if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex) {
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
	} else {
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage, D3DTSS_MIPFILTER, D3DTEXF_POINT);
	}

	curStage = 0;

	W3DShroud *shroud = TheTerrainRenderObject->getShroud();
	if (shroud) {

		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

		//We need to scale so shroud texel stretches over one full terrain cell.  Each texel
		//is 1/128 the size of full texture. (assuming 128x128 vid-mem texture).
		{
			D3DMATRIX curView;
			DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

			D3DMATRIX inv;
			float det;
			Invert_D3DMATRIX(inv, &det, curView);

			D3DMATRIX scale,offset;

			//We need to make all world coordinates be relative to the heightmap data origin since that
			//is where the shroud begins.

			float xoffset = 0;
			float yoffset = 0;
			Real width=shroud->getCellWidth();
			Real height=shroud->getCellHeight();

			if (TheTerrainRenderObject->getMap())
			{	//subtract origin position from all coordinates.  Origin is shifted by 1 cell width/height to allow for unused border texels.
				xoffset = -(float)shroud->getDrawOriginX() + width;
				yoffset = -(float)shroud->getDrawOriginY() + height;
			}

			Set_D3DMATRIX_Translation(offset, xoffset, yoffset,0);

			width = 1.0f/(width*shroud->getTextureWidth());
			height = 1.0f/(height*shroud->getTextureHeight());
			Set_D3DMATRIX_Scaling(scale, width, height, 1);
			curView = (inv * offset) * scale;
			DX8Wrapper::_Set_DX8_Transform((D3DTRANSFORMSTATETYPE )(D3DTS_TEXTURE0+curStage), curView);
		}
		DX8Wrapper::Set_DX8_Texture_Stage_State( curStage, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
		DX8Wrapper::Set_DX8_Texture_Stage_State( curStage, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
		DX8Wrapper::Set_DX8_Texture_Stage_State( curStage, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State( curStage, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::_Get_D3D_Device8()->SetTexture(curStage, shroud->getShroudTexture()->Peek_D3D_Texture());
		curStage++;
		if (curStage==1) curStage++;
	}

	Bool doNoise1 = (W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE1 ||
						W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE12);
	if (doNoise1) {	 // Cloud pass.
		D3DMATRIX curView;
		DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

		D3DMATRIX inv;
		float det;
		Invert_D3DMATRIX(inv, &det, curView);

		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
		// Two output coordinates are used.
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage,  D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage,  D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
		DX8Wrapper::_Get_D3D_Device8()->SetTexture(curStage, W3DShaderManager::getShaderTexture(2)->Peek_D3D_Texture());
		terrainShader2Stage.updateNoise1(&curView,&inv);	//update curView with texture matrix
		DX8Wrapper::_Set_DX8_Transform((D3DTRANSFORMSTATETYPE )(D3DTS_TEXTURE0+curStage), curView);
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);

		curStage++;
		if (curStage==1) curStage++;
	}

	Bool doNoise2 = (W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE2 ||
						W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE12);
	if (doNoise2)
	{
		D3DMATRIX curView;
		DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

		D3DMATRIX inv;
		float det;
		Invert_D3DMATRIX(inv, &det, curView);

		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
		// Two output coordinates are used.
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage,  D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage,  D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
		DX8Wrapper::_Get_D3D_Device8()->SetTexture(curStage, W3DShaderManager::getShaderTexture(3)->Peek_D3D_Texture());
		terrainShader2Stage.updateNoise2(&curView,&inv);	//update curView with texture matrix
		DX8Wrapper::_Set_DX8_Transform((D3DTRANSFORMSTATETYPE )(D3DTS_TEXTURE0+curStage), curView);
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);

		curStage++;
		if (curStage==1) curStage++;
	}
	if (curStage<2) {
		DX8Wrapper::Set_Pixel_Shader(m_dwBase0PixelShader);
	}	else if (curStage==2) {
		DX8Wrapper::Set_Pixel_Shader(m_dwBasePixelShader);
	}	else if (curStage==3) {
		DX8Wrapper::Set_Pixel_Shader(m_dwBaseNoise1PixelShader);
	}else if (curStage==4) {
		DX8Wrapper::Set_Pixel_Shader(m_dwBaseNoise2PixelShader);
	}
	DX8Wrapper::_Get_D3D_Device8()->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
	DX8Wrapper::Apply_Render_State_Changes();
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(curStage, W3DShaderManager::getShaderTexture(3)->Peek_D3D_Texture());
	return TRUE;
}

void FlatTerrainShaderPixelShader::reset()
{
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(2,nullptr);	//release reference to any texture
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(3,nullptr);	//release reference to any texture

	DX8Wrapper::Set_Pixel_Shader(0);	//turn off pixel shader

	DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, nullptr);
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(1, nullptr);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|0);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|1);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 2, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 2, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|2);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 3, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 3, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|3);


	DX8Wrapper::Invalidate_Cached_Render_States();
}





