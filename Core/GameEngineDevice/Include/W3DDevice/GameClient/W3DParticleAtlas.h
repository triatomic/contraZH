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

// FILE: W3DParticleAtlas.h //////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"
#include "WW3D2/texture.h"
#include "WWLib/ref_ptr.h"
#include "WWMath/vector4.h"
#include <map>
#include <utility>
#include <vector>

// Packs particle textures into shared pages for premultiplied blending (ONE, INV_SRC_ALPHA), so alpha
// and additive systems with different textures draw from one texture and one blend state.
// Alpha art keeps its alpha and has its colour multiplied by it; additive art keeps its colour and drops alpha.
// Textures are packed the first time they draw, so a page holds only what has been seen since the last reset.
class W3DParticleAtlas
{
public:
	W3DParticleAtlas();
	~W3DParticleAtlas();

	/// the page holding the texture's converted art and its rectangle in uv, or null when it has no place
	TextureClass *find(TextureClass *source, Bool additive, Vector4 &rect);

	/// drops every page, so the next map packs only its own textures
	void reset();

private:
	// Each entry keeps LEVELS mips. Its gutter shrinks to one texel at the last, so filtering never reads a neighbour.
	enum { PAGE_SIZE = 2048, LEVELS = 5, GUTTER = 1 << (LEVELS - 1), MAX_PAGES = 4 };

	struct Shelf
	{
		Int y;
		Int height;
		Int used;
	};

	struct Page
	{
		RefCountPtr<TextureClass> texture;
		std::vector<Shelf> shelves;
		Int bottom;
	};

	struct Entry
	{
		RefCountPtr<TextureClass> source;	///< keeps the key's texture alive
		Int page;							///< -1 when the texture has no place
		Vector4 rect;
	};

	typedef std::vector<UnsignedInt> Pixels;
	typedef std::map<std::pair<TextureClass *, Bool>, Entry> EntryMap;

	Bool place(Int width, Int height, Int &page, Int &x, Int &y);
	Bool addPage();
	Bool readLevels(TextureClass *source, Bool additive, std::vector<Pixels> &levels);
	Bool writeEntry(const Page &page, const std::vector<Pixels> &levels, Int x, Int y, Int width, Int height, Bool wrapU, Bool wrapV);

	std::vector<Page> m_pages;
	EntryMap m_entries;
};
