/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

// W3DParticleAtlas.cpp ///////////////////////////////////////////////////////////////////////////
// Shared premultiplied pages for particle textures, after stephanmeesters' texture atlas prototype
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Lib/BaseType.h"
#include "WWLib/always.h"
#include "W3DDevice/GameClient/W3DParticleAtlas.h"
#include "Common/Debug.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/formconv.h"
#include <string.h>

namespace
{

Bool Is_Power_Of_Two(UnsignedInt value)
{
	return value != 0 && (value & (value - 1)) == 0;
}

// A gutter texel repeats the opposite edge where the source tiles and the nearest edge where it clamps.
Int Address(Int coordinate, Int size, Bool wrap)
{
	if (wrap)
	{
		return (coordinate % size + size) % size;
	}
	return (coordinate < 0) ? 0 : (coordinate >= size) ? size - 1 : coordinate;
}

UnsignedInt Premultiply(UnsignedInt pixel)
{
	const UnsignedInt alpha = pixel >> 24;
	UnsignedInt result = pixel & 0xFF000000u;
	for (UnsignedInt shift = 0; shift < 24; shift += 8)
	{
		result |= ((((pixel >> shift) & 255) * alpha + 127) / 255) << shift;
	}
	return result;
}

// Averages converted texels, so a missing tail filters premultiplied colour as the pages do.
std::vector<UnsignedInt> Downsample(const std::vector<UnsignedInt> &input, Int width, Int height)
{
	const Int outWidth = (width > 1) ? width / 2 : 1;
	const Int outHeight = (height > 1) ? height / 2 : 1;
	std::vector<UnsignedInt> output(outWidth * outHeight);
	for (Int y = 0; y < outHeight; ++y)
	{
		for (Int x = 0; x < outWidth; ++x)
		{
			UnsignedInt result = 0;
			for (UnsignedInt shift = 0; shift < 32; shift += 8)
			{
				UnsignedInt sum = 0;
				for (Int dy = 0; dy < 2; ++dy)
				{
					for (Int dx = 0; dx < 2; ++dx)
					{
						const Int sx = min(x * 2 + dx, width - 1);
						const Int sy = min(y * 2 + dy, height - 1);
						sum += (input[sy * width + sx] >> shift) & 255;
					}
				}
				result |= ((sum + 2) / 4) << shift;
			}
			output[y * outWidth + x] = result;
		}
	}
	return output;
}

}

W3DParticleAtlas *TheW3DParticleAtlas = nullptr;

W3DParticleAtlas::W3DParticleAtlas()
{
	m_packBudget = PACKS_PER_FRAME;
}

W3DParticleAtlas::~W3DParticleAtlas()
{
	reset();
}

void W3DParticleAtlas::reset()
{
	m_entries.clear();
	m_pages.clear();
}

TextureClass *W3DParticleAtlas::find(TextureClass *source, Bool additive, Vector4 &rect)
{
	if (source == nullptr)
	{
		return nullptr;
	}

	const std::pair<TextureClass *, Bool> key(source, additive);
	EntryMap::iterator found = m_entries.find(key);
	if (found != m_entries.end())
	{
		// a source reloaded at another size packs again, and its old cell stays unused until the reset
		if (found->second.width != source->Get_Width() || found->second.height != source->Get_Height())
		{
			m_entries.erase(found);
		}
		else if (found->second.page < 0)
		{
			return nullptr;
		}
		else
		{
			rect = found->second.rect;
			return m_pages[found->second.page].texture.Peek();
		}
	}

	if (m_packBudget <= 0)
	{
		return nullptr;
	}

	// A texture still loading is asked about again later, not refused for good.
	source->Init();
	if (!source->Is_Initialized() || source->Peek_D3D_Texture() == nullptr)
	{
		return nullptr;
	}
	--m_packBudget;

	Entry entry;
	entry.source = source;
	entry.page = -1;
	entry.width = source->Get_Width();
	entry.height = source->Get_Height();
	entry.rect.Set(0.0f, 0.0f, 1.0f, 1.0f);

	std::vector<Pixels> levels;
	if (readLevels(source, additive, levels))
	{
		const Int width = source->Get_Width();
		const Int height = source->Get_Height();
		const Bool wrapU = source->Get_Filter().Get_U_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_REPEAT;
		const Bool wrapV = source->Get_Filter().Get_V_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_REPEAT;
		Int page = 0;
		Int shelf = 0;
		Int x = 0;
		Int y = 0;
		if (place(width + 2 * GUTTER, height + 2 * GUTTER, page, shelf, x, y))
		{
			// a failed upload gives its cell back and is tried again later
			if (!writeEntry(m_pages[page], levels, x, y, width, height, wrapU, wrapV))
			{
				unplace(width + 2 * GUTTER, page, shelf);
				return nullptr;
			}
			entry.page = page;
			const Real scale = 1.0f / (Real)PAGE_SIZE;
			entry.rect.Set((x + GUTTER) * scale, (y + GUTTER) * scale, (x + GUTTER + width) * scale, (y + GUTTER + height) * scale);
		}
	}

	m_entries[key] = entry;
	if (entry.page < 0)
	{
		return nullptr;
	}
	rect = entry.rect;
	return m_pages[entry.page].texture.Peek();
}

// Same-height shelves first, then any taller one with room, then a new shelf, then a new page.
Bool W3DParticleAtlas::place(Int width, Int height, Int &page, Int &shelf, Int &x, Int &y)
{
	for (Int pass = 0; pass < 2; ++pass)
	{
		for (size_t p = 0; p < m_pages.size(); ++p)
		{
			std::vector<Shelf> &shelves = m_pages[p].shelves;
			for (size_t s = 0; s < shelves.size(); ++s)
			{
				Shelf &candidate = shelves[s];
				const Bool fits = (pass == 0) ? (candidate.height == height) : (candidate.height > height);
				if (fits && candidate.used + width <= PAGE_SIZE)
				{
					page = (Int)p;
					shelf = (Int)s;
					x = candidate.used;
					y = candidate.y;
					candidate.used += width;
					return TRUE;
				}
			}
		}
	}

	for (size_t p = 0; p <= m_pages.size(); ++p)
	{
		if (p == m_pages.size() && !addPage())
		{
			return FALSE;
		}
		Page &candidate = m_pages[p];
		if (candidate.bottom + height <= PAGE_SIZE)
		{
			Shelf added;
			added.y = candidate.bottom;
			added.height = height;
			added.used = width;
			candidate.shelves.push_back(added);
			candidate.bottom += height;
			page = (Int)p;
			shelf = (Int)candidate.shelves.size() - 1;
			x = 0;
			y = added.y;
			return TRUE;
		}
	}
	return FALSE;
}

// The cell just placed is always the last on its shelf, and an emptied last shelf frees its rows too.
void W3DParticleAtlas::unplace(Int width, Int page, Int shelf)
{
	Page &owner = m_pages[page];
	owner.shelves[shelf].used -= width;
	if (owner.shelves[shelf].used == 0 && shelf == (Int)owner.shelves.size() - 1)
	{
		owner.bottom -= owner.shelves[shelf].height;
		owner.shelves.pop_back();
	}
}

Bool W3DParticleAtlas::addPage()
{
	if (m_pages.size() >= MAX_PAGES)
	{
		return FALSE;
	}

	IDirect3DTexture8 *texture = DX8Wrapper::_Create_DX8_Texture(PAGE_SIZE, PAGE_SIZE, WW3D_FORMAT_A8R8G8B8, (MipCountType)LEVELS, D3DPOOL_MANAGED, false);
	if (texture == nullptr)
	{
		return FALSE;
	}

	D3DSURFACE_DESC desc;
	if (texture->GetLevelCount() != LEVELS || FAILED(texture->GetLevelDesc(0, &desc)) || desc.Format != D3DFMT_A8R8G8B8)
	{
		texture->Release();
		return FALSE;
	}

	// Space no entry holds is left unwritten, since filtering never reads past an entry's gutter.
	Page page;
	page.texture.Assign_No_Add_Ref(NEW_REF(TextureClass, (texture)));
	texture->Release();
	page.bottom = 0;

	// Anisotropic taps reach past the one-texel gutter of the last level, so pages filter bilinear or trilinear only.
	TextureFilterClass &filter = page.texture->Get_Filter();
	filter.Set_Min_Filter(TextureFilterClass::FILTER_TYPE_FAST);
	filter.Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_FAST);
	filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_DEFAULT);
	filter.Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	filter.Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);

	m_pages.push_back(page);
	DEBUG_LOG(("Particle atlas: page %d of %d, %dx%d, %d mips", (Int)m_pages.size(), (Int)MAX_PAGES, (Int)PAGE_SIZE, (Int)PAGE_SIZE, (Int)LEVELS));
	return TRUE;
}

// The source's own mips are kept, since rebuilding them from the top level shifts legacy sprites.
Bool W3DParticleAtlas::readLevels(TextureClass *source, Bool additive, std::vector<Pixels> &levels)
{
	IDirect3DTexture8 *texture = DX8Wrapper::_Peek_Lockable_Texture(source->Peek_D3D_Texture());
	D3DSURFACE_DESC desc;
	if (texture == nullptr || FAILED(texture->GetLevelDesc(0, &desc)))
	{
		return FALSE;
	}

	const Int width = (Int)desc.Width;
	const Int height = (Int)desc.Height;
	if (!Is_Power_Of_Two(desc.Width) || !Is_Power_Of_Two(desc.Height) || width < GUTTER || height < GUTTER ||
		width + 2 * GUTTER > PAGE_SIZE || height + 2 * GUTTER > PAGE_SIZE ||
		width != source->Get_Width() || height != source->Get_Height())
	{
		return FALSE;
	}

	const Int available = (Int)texture->GetLevelCount();
	for (Int level = 0; level < LEVELS; ++level)
	{
		const Int levelWidth = width >> level;
		const Int levelHeight = height >> level;
		if (level >= available)
		{
			levels.push_back(Downsample(levels.back(), levelWidth * 2, levelHeight * 2));
			continue;
		}

		IDirect3DSurface8 *surface = nullptr;
		IDirect3DSurface8 *decoded = DX8Wrapper::_Create_DX8_Surface(levelWidth, levelHeight, WW3D_FORMAT_A8R8G8B8);
		HRESULT result = (decoded != nullptr) ? texture->GetSurfaceLevel(level, &surface) : E_FAIL;
		if (SUCCEEDED(result))
		{
			result = Load_Surface_From_Surface(decoded, nullptr, surface, nullptr);
		}
		if (surface != nullptr)
		{
			surface->Release();
		}

		D3DLOCKED_RECT locked;
		if (SUCCEEDED(result))
		{
			result = decoded->LockRect(&locked, nullptr, D3DLOCK_READONLY);
		}
		if (FAILED(result))
		{
			if (decoded != nullptr)
			{
				decoded->Release();
			}
			return FALSE;
		}

		levels.push_back(Pixels(levelWidth * levelHeight));
		Pixels &pixels = levels.back();
		for (Int row = 0; row < levelHeight; ++row)
		{
			memcpy(&pixels[row * levelWidth], (const UnsignedByte *)locked.pBits + row * locked.Pitch, levelWidth * sizeof(UnsignedInt));
		}
		decoded->UnlockRect();
		decoded->Release();

		for (size_t i = 0; i < pixels.size(); ++i)
		{
			pixels[i] = additive ? (pixels[i] & 0x00FFFFFFu) : Premultiply(pixels[i]);
		}
	}
	return TRUE;
}

// The cell at (x, y) is the art plus its gutter on every side, at every level.
Bool W3DParticleAtlas::writeEntry(const Page &page, const std::vector<Pixels> &levels, Int x, Int y, Int width, Int height, Bool wrapU, Bool wrapV)
{
	IDirect3DTexture8 *texture = page.texture->Peek_D3D_Texture();
	IDirect3DTexture8 *lockable = DX8Wrapper::_Peek_Lockable_Texture(texture);
	for (Int level = 0; level < LEVELS; ++level)
	{
		const Int border = GUTTER >> level;
		const Int levelWidth = width >> level;
		const Int levelHeight = height >> level;
		const Int cellWidth = levelWidth + 2 * border;
		const Int cellHeight = levelHeight + 2 * border;
		RECT area;
		area.left = x >> level;
		area.top = y >> level;
		area.right = area.left + cellWidth;
		area.bottom = area.top + cellHeight;

		D3DLOCKED_RECT locked;
		if (FAILED(lockable->LockRect(level, &locked, &area, 0)))
		{
			return FALSE;
		}
		const Pixels &pixels = levels[level];
		for (Int row = 0; row < cellHeight; ++row)
		{
			UnsignedInt *dest = (UnsignedInt *)((UnsignedByte *)locked.pBits + row * locked.Pitch);
			const Int sourceRow = Address(row - border, levelHeight, wrapV) * levelWidth;
			for (Int column = 0; column < cellWidth; ++column)
			{
				dest[column] = pixels[sourceRow + Address(column - border, levelWidth, wrapU)];
			}
		}
		lockable->UnlockRect(level);
	}

	// The top level's lock marks the whole cell dirty at every level.
	DX8Wrapper::_Upload_Lockable_Texture(texture, false);
	return TRUE;
}
