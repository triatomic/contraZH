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

// W3DColorLut.cpp ////////////////////////////////////////////////////////////////////////////////
// Colour grade over a view's finished 3D scene, through a colour table a map can choose
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Lib/BaseType.h"
#include "WWLib/always.h"
#include "W3DDevice/GameClient/W3DColorLut.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "Common/GlobalData.h"
#include "Common/Debug.h"
#include "Common/FileSystem.h"
#include "Common/LocalFileSystem.h"
#include "WWLib/TARGA.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/shader.h"
#include "WW3D2/vertmaterial.h"
#include "WW3D2/camera.h"
#include "WWMath/wwmath.h"

W3DColorLut *TheW3DColorLut = nullptr;

// CONTRA_COLORLUT bisects faults: 0 no grade, 1 grade.
static const Int ColorLutMode = (getenv("CONTRA_COLORLUT") != nullptr) ? atoi(getenv("CONTRA_COLORLUT")) : 1;

// A table's side, so its strip is at most 4096 by 64.
enum { MAX_TABLE_SIZE = 64 };

// How often a loose table's file is looked at for a newer save.
static const UnsignedInt TABLE_CHECK_MS = 500;

// Floors that keep the levels and the vignette from dividing by nothing.
static const Real MIN_LEVELS_SPAN = 0.0001f;
static const Real MIN_GAMMA = 0.01f;
static const Real MIN_VIGNETTE_RADIUS = 0.05f;

// When a loose table was last saved, or 0 for one that is missing or sits in an archive.
static Int64 Table_File_Stamp(const AsciiString &name)
{
	AsciiString path(TGA_DIR_PATH);
	path.concat(name.str());
	FileInfo info;
	if (TheFileSystem == nullptr || !TheFileSystem->getLooseFileInfo(path, &info))
	{
		return 0;
	}
	return info.timestamp();
}

W3DColorLut::W3DColorLut()
	: m_sceneCopy(nullptr),
	  m_table(nullptr),
	  m_tableStamp(0),
	  m_tableCheckTime(0),
	  m_tableSize(0),
	  m_tableShader(0),
	  m_gradeShader(0),
	  m_frame(0),
	  m_loaded(FALSE)
{
}

W3DColorLut::~W3DColorLut()
{
	ReleaseResources();
}

void W3DColorLut::ReleaseResources()
{
	if (m_sceneCopy != nullptr)
	{
		m_sceneCopy->Release();
		m_sceneCopy = nullptr;
	}
	if (m_table != nullptr)
	{
		m_table->Release();
		m_table = nullptr;
	}
	m_tableName.clear();
	m_tableSize = 0;

	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	if (device != nullptr)
	{
		DX8_DELETE_PIXEL_SHADER(device, m_tableShader);
		DX8_DELETE_PIXEL_SHADER(device, m_gradeShader);
	}
	m_tableShader = 0;
	m_gradeShader = 0;
	m_loaded = FALSE;
}

// The file is read here, texel for texel, because the texture loader would shrink or compress it with the detail settings.
void W3DColorLut::loadTable(const AsciiString &name)
{
#if defined(BUILD_WITH_D3D9)
	if (m_table != nullptr)
	{
		m_table->Release();
		m_table = nullptr;
	}
	m_tableName = name;
	m_tableStamp = Table_File_Stamp(name);
	m_tableCheckTime = timeGetTime();
	m_tableSize = 0;

	Targa targa;
	if (targa.Load(name.str(), TGAF_IMAGE, false) != 0)
	{
		RENDER_LOG(("ColorLut '%s' could not be read", name.str()));
		return;
	}

	const Int size = targa.Header.Height;
	const Int bytes = TGA_BytesPerPixel(targa.Header.PixelDepth);
	const Bool trueColor = targa.Header.ImageType == TGA_TRUECOLOR || targa.Header.ImageType == TGA_TRUECOLOR_ENCODED;
	if (!trueColor || (bytes != 3 && bytes != 4) || size < 2 || size > MAX_TABLE_SIZE || targa.Header.Width != size * size)
	{
		RENDER_LOG(("ColorLut '%s' is %d by %d at %d bits; a table is a 24 or 32 bit strip of N slices, N*N wide and N tall, N up to %d",
			name.str(), (Int)targa.Header.Width, size, (Int)targa.Header.PixelDepth, (Int)MAX_TABLE_SIZE));
		return;
	}

	IDirect3DTexture8 *table = DX8Wrapper::_Create_DX8_Texture(size * size, size, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1, D3DPOOL_MANAGED, false);
	if (table == nullptr)
	{
		return;
	}

	IDirect3DTexture8 *lockable = DX8Wrapper::_Peek_Lockable_Texture(table);
	D3DLOCKED_RECT locked;
	if (FAILED(lockable->LockRect(0, &locked, nullptr, 0)))
	{
		table->Release();
		return;
	}

	// The loaded image starts at its bottom row.
	const UnsignedByte *image = (const UnsignedByte *)targa.GetImage();
	for (Int y = 0; y < size; y++)
	{
		const UnsignedByte *source = image + (size - 1 - y) * size * size * bytes;
		UnsignedInt *row = (UnsignedInt *)((UnsignedByte *)locked.pBits + y * locked.Pitch);
		for (Int x = 0; x < size * size; x++)
		{
			row[x] = 0xff000000u | ((UnsignedInt)source[2] << 16) | ((UnsignedInt)source[1] << 8) | (UnsignedInt)source[0];
			source += bytes;
		}
	}

	lockable->UnlockRect(0);
	DX8Wrapper::_Upload_Lockable_Texture(table);
	m_table = table;
	m_tableSize = size;
#else
	(void)name;
#endif
}

void W3DColorLut::render(CameraClass &camera)
{
#if defined(BUILD_WITH_D3D9)
	if (ColorLutMode == 0)
	{
		return;
	}

	const GlobalData *data = TheGlobalData;
	const Real strength = WWMath::Clamp(data->m_colorLutStrength, 0.0f, 1.0f);
	const AsciiString &name = data->m_colorLut;
	const Bool wantsTable = strength > 0.0f && name.isNotEmpty() && name.compareNoCase("None") != 0;
	if (wantsTable)
	{
		Bool stale = name.compareNoCase(m_tableName) != 0;
		if (!stale && timeGetTime() - m_tableCheckTime >= TABLE_CHECK_MS)
		{
			m_tableCheckTime = timeGetTime();
			stale = Table_File_Stamp(name) != m_tableStamp;
		}
		if (stale)
		{
			loadTable(name);
		}
	}

	const Bool useTable = wantsTable && m_table != nullptr;
	const RGBColor &tint = data->m_colorLutTint;
	const Bool graded = data->m_colorLutBrightness != 1.0f || data->m_colorLutContrast != 1.0f || data->m_colorLutSaturation != 1.0f ||
		tint.red != 1.0f || tint.green != 1.0f || tint.blue != 1.0f ||
		data->m_colorLutVibrance != 0.0f || data->m_colorLutTechnicolor > 0.0f ||
		data->m_colorLutBlackPoint != 0.0f || data->m_colorLutWhitePoint != 1.0f || data->m_colorLutGamma != 1.0f ||
		data->m_colorLutOutputBlack != 0.0f || data->m_colorLutOutputWhite != 1.0f ||
		data->m_colorLutVignette > 0.0f || data->m_colorLutGrain > 0.0f;
	if (!useTable && !graded)
	{
		return;
	}

	if (!m_loaded)
	{
		m_loaded = TRUE;
		if (W3DShaderManager::supportsPixelShader2a())
		{
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\colorlut.pso", nullptr, 0, false, &m_tableShader)))
			{
				m_tableShader = 0;
			}
			if (FAILED(W3DShaderManager::LoadAndCreateD3DShader("shaders\\colorgrade.pso", nullptr, 0, false, &m_gradeShader)))
			{
				m_gradeShader = 0;
			}
		}
	}
	const DWORD pixelShader = useTable ? m_tableShader : m_gradeShader;
	if (pixelShader == 0)
	{
		return;
	}

	// The copy's size follows the target's, and the quad reads it through the camera's viewport.
	camera.Apply();
	if (!W3DShaderManager::copyRenderTarget(m_sceneCopy))
	{
		return;
	}
	D3DSURFACE_DESC copyDesc;
	m_sceneCopy->GetLevelDesc(0, &copyDesc);
	const Vector4 screenMap = W3DShaderManager::getClipToTargetMapping((Real)copyDesc.Width, (Real)copyDesc.Height);

	IDirect3DDevice8 *device = DX8Wrapper::_Get_D3D_Device8();
	DX8Wrapper::Set_Texture(0, nullptr);
	DX8Wrapper::Set_Texture(1, nullptr);
	VertexMaterialClass *material = VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(material);
	REF_PTR_RELEASE(material);
	Matrix4x4 identity(true);
	DX8Wrapper::Set_World_Identity();
	DX8Wrapper::Set_View_Identity();
	DX8Wrapper::Set_Transform(D3DTS_PROJECTION, identity);

	ShaderClass replace = ShaderClass::_PresetOpaque2DShader;
	replace.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);
	DX8Wrapper::Set_Shader(replace);
	DX8Wrapper::Apply_Render_State_Changes();

	// The device is read because the wrapper's cache can hold a placeholder after an invalidate.
	DWORD depthTest = D3DZB_TRUE;
	DWORD stencil = FALSE;
	device->GetRenderState(D3DRS_ZENABLE, &depthTest);
	device->GetRenderState(D3DRS_STENCILENABLE, &stencil);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGENABLE, FALSE);

	// The scene maps texel to pixel. The table filters across a slice, and the shader blends between slices.
	for (Int stage = 0; stage < 2; stage++)
	{
		const DWORD filter = (stage == 0) ? D3DTEXF_POINT : D3DTEXF_LINEAR;
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MINFILTER, filter);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MAGFILTER, filter);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MIPFILTER, D3DTEXF_NONE);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	}
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_TEXCOORDINDEX, 0);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

	const Vector4 grade(data->m_colorLutBrightness, data->m_colorLutContrast, data->m_colorLutSaturation, strength);
	const Vector4 tintConstant(tint.red, tint.green, tint.blue, WWMath::Clamp(data->m_colorLutVibrance, -1.0f, 1.0f));
	const Real size = (Real)m_tableSize;
	const Vector4 tableSize(size - 1.0f, useTable ? 1.0f / size : 0.0f, useTable ? 1.0f / (size * size) : 0.0f,
		WWMath::Clamp(data->m_colorLutTechnicolor, 0.0f, 1.0f));
	const Vector4 tableMix(WWMath::Clamp(data->m_colorLutChroma, 0.0f, 1.0f), WWMath::Clamp(data->m_colorLutLuma, 0.0f, 1.0f),
		max(data->m_colorLutGrain, 0.0f), max(data->m_colorLutDither, 0.0f) / 255.0f);

	const Real blackPoint = data->m_colorLutBlackPoint;
	const Vector4 levelsIn(blackPoint, 1.0f / max(data->m_colorLutWhitePoint - blackPoint, MIN_LEVELS_SPAN),
		1.0f / max(data->m_colorLutGamma, MIN_GAMMA), 0.0f);
	const Real vignetteRadius = max(data->m_colorLutVignetteRadius, MIN_VIGNETTE_RADIUS);
	const Vector4 levelsOut(data->m_colorLutOutputBlack, data->m_colorLutOutputWhite - data->m_colorLutOutputBlack,
		WWMath::Clamp(data->m_colorLutVignette, 0.0f, 1.0f), 1.0f / (vignetteRadius * vignetteRadius));

	// The quad's uv back to clip space, with x stretched so a circle stays round.
	D3DVIEWPORT8 viewport;
	device->GetViewport(&viewport);
	const Real aspect = (Real)viewport.Width / (Real)max(viewport.Height, (DWORD)1);
	const Vector4 viewMap(aspect / screenMap.X, 1.0f / screenMap.Y, -screenMap.Z * aspect / screenMap.X, -screenMap.W / screenMap.Y);

	// Steps by irrational shares never repeat, so the grain and dither never settle into a loop.
	m_frame++;
	const Vector4 noise(fmod(m_frame * 0.6180340f, 1.0f), fmod(m_frame * 0.7548777f, 1.0f),
		fmod(m_frame * 0.5698403f, 1.0f), fmod(m_frame * 0.3247180f, 1.0f));

	device->SetTexture(0, m_sceneCopy);
	device->SetTexture(1, useTable ? m_table : nullptr);
	DX8Wrapper::Set_Pixel_Shader(pixelShader);
	DX8Wrapper::Set_Pixel_Shader_Constant(0, &grade, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(1, &tintConstant, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(2, &tableSize, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(3, &tableMix, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(4, &levelsIn, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(5, &levelsOut, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(6, &viewMap, 1);
	DX8Wrapper::Set_Pixel_Shader_Constant(7, &noise, 1);
	W3DShaderManager::drawClipQuad(screenMap);

	DX8Wrapper::Set_Pixel_Shader(0);
	device->SetTexture(0, nullptr);
	device->SetTexture(1, nullptr);
	DX8Wrapper::Set_Vertex_Buffer(nullptr);
	DX8Wrapper::Set_Index_Buffer(nullptr, 0);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, depthTest);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, stencil);

	// Blend, cull and fog are ShaderClass state, so the next shader set restores them in full.
	ShaderClass::Invalidate();
	camera.Apply();
	DX8Wrapper::Apply_Render_State_Changes();
#else
	(void)camera;
#endif
}
