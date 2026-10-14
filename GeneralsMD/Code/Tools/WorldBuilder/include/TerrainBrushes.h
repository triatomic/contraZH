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

// TerrainBrushes.h
// WorldBuilder-only terrain brush definitions read from WorldBuilderQt\*.ini.

#pragma once

#include "Lib/BaseType.h"
#include "Common/AsciiString.h"

#include <vector>

/// WorldBuilder-only terrain brushes: categories and aliases.
/** Each "Brush <Name>" block holds Category and optionally Terrain. With Terrain it is an
alias that paints that terrain; without it, it re-files the terrain of the same name. The game
never reads these files. A brush with a new texture also gets a Terrain block in the game's
loose Data\INI\Terrain.ini. */
class TerrainBrushes
{
public:
	struct Alias
	{
		AsciiString name;
		AsciiString terrain;
		AsciiString category;
	};

	/// Reads every .ini in the brush folder. Call after TheTerrainTypes, before WorldHeightMapEdit::init.
	static void load();

	/// Category path for a terrain type, "\"-separated, or empty for the Class folder.
	static AsciiString getCategory(const AsciiString &terrainName);

	static Int getNumAliases() { return (Int)s_aliases.size(); }
	static const Alias &getAlias(Int ndx) { return s_aliases[ndx]; }

	/// Optional Terrain.ini keys for a brush with a new texture.
	struct TerrainKeys
	{
		Bool blendEdges;
		Bool restrictConstruction;
		Real glintStrength;	///< Negative leaves the key out.
		Real glintGloss;	///< Zero or less leaves the key out, as the game reads it.

		TerrainKeys() : blendEdges(false), restrictConstruction(false), glintStrength(-1.0f), glintGloss(0.0f) {}
	};

	/// Defines a brush, appends it to the brush files and loads it. Returns its texture class or -1.
	/** A texture brush can succeed with a warning in error when only its category failed to save. */
	static Int create(const char *name, const char *category, const char *terrain, const char *texture, const TerrainKeys &keys, AsciiString &error);

	enum
	{
		KEY_RESTRICT_CONSTRUCTION = 1,
		KEY_GLINT_STRENGTH = 2,
		KEY_GLINT_GLOSS = 4
	};

	/// Rewrites the changedKeys of an existing terrain in the loose Terrain.ini.
	static Bool setTerrainKeys(const char *name, const TerrainKeys &keys, UnsignedInt changedKeys, AsciiString &error);

private:
	struct CategoryEntry
	{
		AsciiString terrain;
		AsciiString category;
	};

	struct Block
	{
		AsciiString name;
		AsciiString category;
		AsciiString terrain;
	};

	static void loadFile(const char *path);
	static Bool apply(const Block &block, AsciiString &error);
	static Int createTerrain(Block &block, const AsciiString &texture, const TerrainKeys &keys, AsciiString &error);
	static Bool appendToFile(const Block &block, AsciiString &error);
	static void setCategory(const AsciiString &terrainName, const AsciiString &category);
	static Bool isAlias(const AsciiString &name);

	static std::vector<Alias> s_aliases;
	static std::vector<CategoryEntry> s_categories;
};
