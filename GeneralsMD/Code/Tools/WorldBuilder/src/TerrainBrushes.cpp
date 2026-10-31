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

// TerrainBrushes.cpp
// WorldBuilder-only terrain brush definitions read from WorldBuilderQt\*.ini.

#include "StdAfx.h"
#include "TerrainBrushes.h"
#include "WHeightMapEdit.h"
#include "Common/Debug.h"
#include "Common/File.h"
#include "Common/FileSystem.h"
#include "Common/MapReaderWriterInfo.h"
#include "Common/TerrainTypes.h"

#include <stdio.h>
#include <string>

static const char *const BRUSH_DIR = "WorldBuilderQt\\";
static const char *const BRUSH_FILE = "WorldBuilderQt\\TerrainBrushes.ini";
static const char *const GAME_TERRAIN_FILE = "Data\\INI\\Terrain.ini";
static const char *const DEFAULT_CATEGORY = "Custom";

std::vector<TerrainBrushes::Alias> TerrainBrushes::s_aliases;
std::vector<TerrainBrushes::CategoryEntry> TerrainBrushes::s_categories;

namespace
{
	char *trimInPlace(char *s)
	{
		while (*s == ' ' || *s == '\t')
		{
			s++;
		}
		Int len = (Int)strlen(s);
		while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n'))
		{
			s[--len] = 0;
		}
		return s;
	}

	// Forward slashes become backslashes, and empty segments and outer separators are dropped.
	AsciiString normalizeCategory(const char *src)
	{
		AsciiString out;
		AsciiString segment;
		const char *p = src;
		for (;;)
		{
			if (*p == 0 || *p == '\\' || *p == '/')
			{
				segment.trim();
				if (!segment.isEmpty())
				{
					if (!out.isEmpty())
					{
						out.concat('\\');
					}
					out.concat(segment);
				}
				segment.clear();
				if (*p == 0)
				{
					break;
				}
			}
			else
			{
				segment.concat(*p);
			}
			p++;
		}
		return out;
	}

	// Names are single INI tokens.
	Bool isValidName(const char *name)
	{
		if (name == nullptr || name[0] == 0)
		{
			return false;
		}
		for (const char *p = name; *p; p++)
		{
			char c = *p;
			Bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
				|| c == '_' || c == '-' || c == '.';
			if (!ok)
			{
				return false;
			}
		}
		return true;
	}

	// A new loose Terrain.ini would hide the archive copy and every terrain in it.
	Bool hasLooseTerrainIni(AsciiString &error)
	{
		DWORD attributes = GetFileAttributes(GAME_TERRAIN_FILE);
		if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			error.format("%s is not a loose file. Extract Terrain.ini from the mod's .big into Data\\INI\\ and try again.", GAME_TERRAIN_FILE);
			return false;
		}
		return true;
	}

	// The line's first two whitespace-separated words.
	void lineWords(const std::string &line, std::string &first, std::string &second)
	{
		first.clear();
		second.clear();
		size_t pos = line.find_first_not_of(" \t");
		while (pos != std::string::npos && pos < line.size() && line[pos] != ' ' && line[pos] != '\t' && line[pos] != ';' && line[pos] != '=')
		{
			first += line[pos++];
		}
		pos = (pos == std::string::npos) ? pos : line.find_first_not_of(" \t", pos);
		while (pos != std::string::npos && pos < line.size() && line[pos] != ' ' && line[pos] != '\t' && line[pos] != ';')
		{
			second += line[pos++];
		}
	}

	// Sets or adds "key = value" inside the block lines [start, end). Returns the new end.
	size_t setBlockKey(std::vector<std::string> &lines, size_t start, size_t end, const char *key, const std::string &value)
	{
		Bool found = false;
		size_t i;
		for (i = start; i < end; i++)
		{
			std::string first, second;
			lineWords(lines[i], first, second);
			if (stricmp(first.c_str(), key) == 0)
			{
				size_t indent = lines[i].find_first_not_of(" \t");
				lines[i] = lines[i].substr(0, indent) + key + " = " + value;
				found = true;
			}
		}
		if (!found)
		{
			lines.insert(lines.begin() + end, std::string("  ") + key + " = " + value);
			end++;
		}
		return end;
	}

	// A file under Data\INI\Terrain\ with a block for the name; the game loads those after Terrain.ini.
	AsciiString findLaterDefinition(const char *name)
	{
		FilenameList files;
		TheFileSystem->getFileListInDirectory("Data\\INI\\Terrain\\", "*.ini", files, TRUE);
		FilenameList::const_iterator it;
		for (it = files.begin(); it != files.end(); ++it)
		{
			File *file = TheFileSystem->openFile(it->str(), File::READ);
			if (file == nullptr)
			{
				continue;
			}
			Int size = file->size();
			char *data = file->readEntireAndClose();
			if (data == nullptr)
			{
				continue;
			}
			std::string text(data, size);
			delete[] data;
			size_t pos = 0;
			while (pos < text.size())
			{
				size_t nl = text.find('\n', pos);
				size_t stop = (nl == std::string::npos) ? text.size() : nl;
				std::string first, second;
				lineWords(text.substr(pos, stop - pos), first, second);
				if (!second.empty() && second[second.size() - 1] == '\r')
				{
					second.erase(second.size() - 1);
				}
				if (stricmp(first.c_str(), "Terrain") == 0 && second == name)
				{
					return *it;
				}
				pos = (nl == std::string::npos) ? text.size() : nl + 1;
			}
		}
		return AsciiString::TheEmptyString;
	}

	std::string formatReal(Real value)
	{
		char buf[32];
		snprintf(buf, ARRAY_SIZE(buf), "%g", value);
		return buf;
	}

	Bool textureHasTiles(const AsciiString &texture)
	{
		AsciiString path;
		path.format("%s%s", TERRAIN_TGA_DIR_PATH, texture.str());
		CachedFileInputStream stream;
		if (!stream.open(path))
		{
			return false;
		}
		return WorldHeightMap::countTiles(&stream, nullptr, path.str()) >= 1;
	}
}

void TerrainBrushes::load()
{
	s_aliases.clear();
	s_categories.clear();

	char pattern[_MAX_PATH];
	snprintf(pattern, ARRAY_SIZE(pattern), "%s*.ini", BRUSH_DIR);

	WIN32_FIND_DATA findData;
	HANDLE hFind = FindFirstFile(pattern, &findData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return;
	}
	do
	{
		if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		{
			char path[_MAX_PATH];
			snprintf(path, ARRAY_SIZE(path), "%s%s", BRUSH_DIR, findData.cFileName);
			loadFile(path);
		}
	} while (FindNextFile(hFind, &findData));
	FindClose(hFind);
}

void TerrainBrushes::loadFile(const char *path)
{
	FILE *fp = fopen(path, "r");
	if (fp == nullptr)
	{
		return;
	}

	Block block;
	Bool inBlock = false;
	Int lineNum = 0;
	char line[1024];
	while (fgets(line, sizeof(line), fp))
	{
		lineNum++;
		char *comment = strchr(line, ';');
		if (comment)
		{
			*comment = 0;
		}
		comment = strstr(line, "//");
		if (comment)
		{
			*comment = 0;
		}
		char *text = trimInPlace(line);
		if (text[0] == 0)
		{
			continue;
		}

		char *equals = strchr(text, '=');
		if (equals == nullptr)
		{
			char *rest = text;
			while (*rest && *rest != ' ' && *rest != '\t')
			{
				rest++;
			}
			if (*rest)
			{
				*rest++ = 0;
			}
			rest = trimInPlace(rest);

			if (stricmp(text, "Brush") == 0)
			{
				if (inBlock)
				{
					DEBUG_LOG(("%s(%d): Brush '%s' has no End.", path, lineNum, block.name.str()));
				}
				block = Block();
				block.name = rest;
				inBlock = true;
			}
			else if (stricmp(text, "End") == 0 && inBlock)
			{
				AsciiString error;
				if (!apply(block, error))
				{
					DEBUG_LOG(("%s(%d): %s", path, lineNum, error.str()));
				}
				inBlock = false;
			}
			else
			{
				DEBUG_LOG(("%s(%d): unexpected '%s'.", path, lineNum, text));
			}
			continue;
		}

		*equals = 0;
		char *key = trimInPlace(text);
		char *value = trimInPlace(equals + 1);
		if (!inBlock)
		{
			DEBUG_LOG(("%s(%d): '%s' outside a Brush block.", path, lineNum, key));
		}
		else if (stricmp(key, "Category") == 0)
		{
			block.category = normalizeCategory(value);
		}
		else if (stricmp(key, "Terrain") == 0)
		{
			block.terrain = value;
		}
		else
		{
			DEBUG_LOG(("%s(%d): unknown key '%s'.", path, lineNum, key));
		}
	}
	fclose(fp);
}

Bool TerrainBrushes::apply(const Block &block, AsciiString &error)
{
	if (!isValidName(block.name.str()))
	{
		error.format("Brush name '%s' must be letters, digits, '_', '-' or '.'.", block.name.str());
		return false;
	}

	AsciiString terrainName = block.terrain.isEmpty() ? block.name : block.terrain;
	if (TheTerrainTypes->findTerrain(terrainName) == nullptr)
	{
		error.format("Brush '%s': terrain '%s' is not defined.", block.name.str(), terrainName.str());
		return false;
	}

	if (terrainName == block.name)
	{
		if (!block.category.isEmpty())
		{
			setCategory(terrainName, block.category);
		}
		return true;
	}

	if (TheTerrainTypes->findTerrain(block.name) != nullptr || isAlias(block.name))
	{
		error.format("Brush '%s' is already defined.", block.name.str());
		return false;
	}
	Alias alias;
	alias.name = block.name;
	alias.terrain = terrainName;
	alias.category = block.category.isEmpty() ? AsciiString(DEFAULT_CATEGORY) : block.category;
	s_aliases.push_back(alias);
	return true;
}

Int TerrainBrushes::create(const char *name, const char *category, const char *terrain, const char *texture, const TerrainKeys &keys, AsciiString &error)
{
	Block block;
	block.name = name ? name : "";
	block.name.trim();
	block.category = normalizeCategory(category ? category : "");
	block.terrain = terrain ? terrain : "";
	block.terrain.trim();
	AsciiString textureName = texture ? texture : "";
	textureName.trim();

	if (!isValidName(block.name.str()))
	{
		error.format("Brush name '%s' must be letters, digits, '_', '-' or '.'.", block.name.str());
		return -1;
	}

	// Both INI readers cut a value at these characters.
	if (strchr(block.category.str(), ';') != nullptr)
	{
		error.format("Category '%s' must not contain ';'.", block.category.str());
		return -1;
	}
	if (strpbrk(textureName.str(), " \t;=\"") != nullptr)
	{
		error.format("Texture '%s' must not contain spaces, ';', '=' or quotes. Rename the file.", textureName.str());
		return -1;
	}

	if (!textureName.isEmpty())
	{
		return createTerrain(block, textureName, keys, error);
	}

	AsciiString terrainName = block.terrain.isEmpty() ? block.name : block.terrain;
	if (WorldHeightMapEdit::findTexClass(terrainName) < 0)
	{
		error.format("Terrain '%s' has no loaded texture.", terrainName.str());
		return -1;
	}
	if (block.terrain == block.name)
	{
		block.terrain.clear();
	}
	if (block.terrain.isEmpty() && block.category.isEmpty())
	{
		error.format("Give '%s' a category, or a different name to make a new brush.", block.name.str());
		return -1;
	}

	if (!block.terrain.isEmpty() && (TheTerrainTypes->findTerrain(block.name) != nullptr || isAlias(block.name)))
	{
		error.format("Brush '%s' is already defined.", block.name.str());
		return -1;
	}

	if (!appendToFile(block, error) || !apply(block, error))
	{
		return -1;
	}
	return WorldHeightMapEdit::findTexClass(terrainName);
}

// The terrain goes to the game's Terrain.ini, so maps using it load in-game; only the category stays WorldBuilder-only.
Int TerrainBrushes::createTerrain(Block &block, const AsciiString &texture, const TerrainKeys &keys, AsciiString &error)
{
	if (TheTerrainTypes->findTerrain(block.name) != nullptr || isAlias(block.name))
	{
		error.format("Brush '%s' is already defined.", block.name.str());
		return -1;
	}
	if (!textureHasTiles(texture))
	{
		error.format("'%s%s' is missing or is not a terrain texture.", TERRAIN_TGA_DIR_PATH, texture.str());
		return -1;
	}

	if (!hasLooseTerrainIni(error))
	{
		return -1;
	}
	FILE *fp = fopen(GAME_TERRAIN_FILE, "a");
	if (fp == nullptr)
	{
		error.format("Cannot write '%s'.", GAME_TERRAIN_FILE);
		return -1;
	}
	fprintf(fp, "\nTerrain %s\n  Texture = %s\n", block.name.str(), texture.str());
	if (keys.blendEdges)
	{
		fprintf(fp, "  BlendEdges = Yes\n");
	}
	if (keys.restrictConstruction)
	{
		fprintf(fp, "  RestrictConstruction = Yes\n");
	}
	if (keys.glintStrength >= 0.0f)
	{
		fprintf(fp, "  GlintStrength = %g\n", keys.glintStrength);
	}
	if (keys.glintGloss > 0.0f)
	{
		fprintf(fp, "  GlintGloss = %g\n", keys.glintGloss);
	}
	fprintf(fp, "End\n");
	Bool ok = (ferror(fp) == 0);
	fclose(fp);
	if (!ok)
	{
		error.format("Cannot write '%s'.", GAME_TERRAIN_FILE);
		return -1;
	}

	// Same steps as the game's Terrain block parser: DefaultTerrain values, then the written keys.
	TerrainType *terrain = TheTerrainTypes->newTerrain(block.name);
	terrain->friend_setTexture(texture);
	if (keys.blendEdges)
	{
		terrain->friend_setBlendEdge(true);
	}
	if (keys.restrictConstruction)
	{
		terrain->friend_setRestrictConstruction(true);
	}
	terrain->friend_setGlint(keys.glintStrength >= 0.0f ? keys.glintStrength : terrain->getGlintStrength(),
		keys.glintGloss > 0.0f ? keys.glintGloss : terrain->getGlintGloss());
	Int texClass = WorldHeightMapEdit::addTerrainTexture(terrain);
	if (texClass < 0)
	{
		error.format("Saved '%s' to %s, but its texture did not load.", block.name.str(), GAME_TERRAIN_FILE);
		return -1;
	}

	if (block.category.isEmpty())
	{
		block.category = DEFAULT_CATEGORY;
	}
	setCategory(block.name, block.category);
	appendToFile(block, error);
	return texClass;
}

// BlendEdges is left alone: the loaded texture class already decided how its tiles are used.
Bool TerrainBrushes::setTerrainKeys(const char *name, const TerrainKeys &keys, UnsignedInt changedKeys, AsciiString &error)
{
	TerrainType *terrain = TheTerrainTypes->findTerrain(name ? name : "");
	if (terrain == nullptr)
	{
		error.format("Terrain '%s' is not defined.", name ? name : "");
		return false;
	}
	if (!hasLooseTerrainIni(error))
	{
		return false;
	}
	AsciiString laterFile = findLaterDefinition(name);
	if (!laterFile.isEmpty())
	{
		error.format("%s also defines Terrain %s and loads after Terrain.ini, so edit it there.", laterFile.str(), name);
		return false;
	}

	FILE *fp = fopen(GAME_TERRAIN_FILE, "rb");
	if (fp == nullptr)
	{
		error.format("Cannot read '%s'.", GAME_TERRAIN_FILE);
		return false;
	}
	std::string text;
	char buf[4096];
	size_t got;
	while ((got = fread(buf, 1, sizeof(buf), fp)) > 0)
	{
		text.append(buf, got);
	}
	fclose(fp);

	std::string eol = (text.find("\r\n") != std::string::npos) ? "\r\n" : "\n";
	std::vector<std::string> lines;
	size_t pos = 0;
	while (pos < text.size())
	{
		size_t nl = text.find('\n', pos);
		size_t stop = (nl == std::string::npos) ? text.size() : nl;
		std::string line = text.substr(pos, stop - pos);
		if (!line.empty() && line[line.size() - 1] == '\r')
		{
			line.erase(line.size() - 1);
		}
		lines.push_back(line);
		pos = (nl == std::string::npos) ? text.size() : nl + 1;
	}

	// The last block for the name wins, as in the game.
	size_t start = lines.size();
	size_t end = lines.size();
	size_t i;
	for (i = 0; i < lines.size(); i++)
	{
		std::string first, second;
		lineWords(lines[i], first, second);
		if (stricmp(first.c_str(), "Terrain") == 0 && second == name)
		{
			start = i + 1;
		}
	}
	if (start < lines.size())
	{
		for (end = start; end < lines.size(); end++)
		{
			std::string first, second;
			lineWords(lines[end], first, second);
			if (stricmp(first.c_str(), "End") == 0)
			{
				break;
			}
		}
	}
	else
	{
		// Defined only in another file: a block of the same name overrides the keys it lists.
		lines.push_back("");
		lines.push_back(std::string("Terrain ") + name);
		lines.push_back("End");
		start = lines.size() - 1;
		end = start;
	}

	// An unset key is written as its default, since an earlier block may still set it.
	Real strength = (keys.glintStrength >= 0.0f) ? keys.glintStrength : 1.0f;
	Real gloss = (keys.glintGloss > 0.0f) ? keys.glintGloss : 0.0f;
	if (changedKeys & KEY_RESTRICT_CONSTRUCTION)
	{
		end = setBlockKey(lines, start, end, "RestrictConstruction", keys.restrictConstruction ? "Yes" : "No");
	}
	if (changedKeys & KEY_GLINT_STRENGTH)
	{
		end = setBlockKey(lines, start, end, "GlintStrength", formatReal(strength));
	}
	if (changedKeys & KEY_GLINT_GLOSS)
	{
		end = setBlockKey(lines, start, end, "GlintGloss", formatReal(gloss));
	}

	std::string out;
	for (i = 0; i < lines.size(); i++)
	{
		out += lines[i];
		if (i + 1 < lines.size() || (!text.empty() && text[text.size() - 1] == '\n'))
		{
			out += eol;
		}
	}

	// Write beside the file and swap it in, so a failed write leaves Terrain.ini whole.
	AsciiString tempPath;
	tempPath.format("%s.tmp", GAME_TERRAIN_FILE);
	fp = fopen(tempPath.str(), "wb");
	if (fp == nullptr)
	{
		error.format("Cannot write '%s'.", tempPath.str());
		return false;
	}
	Bool ok = (fwrite(out.data(), 1, out.size(), fp) == out.size());
	ok = (fclose(fp) == 0) && ok;
	if (!ok || !MoveFileEx(tempPath.str(), GAME_TERRAIN_FILE, MOVEFILE_REPLACE_EXISTING))
	{
		DeleteFile(tempPath.str());
		error.format("Cannot write '%s'.", GAME_TERRAIN_FILE);
		return false;
	}

	if (changedKeys & KEY_RESTRICT_CONSTRUCTION)
	{
		terrain->friend_setRestrictConstruction(keys.restrictConstruction);
	}
	terrain->friend_setGlint((changedKeys & KEY_GLINT_STRENGTH) ? strength : terrain->getGlintStrength(),
		(changedKeys & KEY_GLINT_GLOSS) ? gloss : terrain->getGlintGloss());
	return true;
}

Bool TerrainBrushes::appendToFile(const Block &block, AsciiString &error)
{
	CreateDirectory(BRUSH_DIR, nullptr);
	FILE *fp = fopen(BRUSH_FILE, "a");
	if (fp == nullptr)
	{
		error.format("Cannot write '%s'.", BRUSH_FILE);
		return false;
	}
	fprintf(fp, "\nBrush %s\n", block.name.str());
	if (!block.terrain.isEmpty())
	{
		fprintf(fp, "  Terrain = %s\n", block.terrain.str());
	}
	if (!block.category.isEmpty())
	{
		fprintf(fp, "  Category = %s\n", block.category.str());
	}
	fprintf(fp, "End\n");
	Bool ok = (ferror(fp) == 0);
	fclose(fp);
	if (!ok)
	{
		error.format("Cannot write '%s'.", BRUSH_FILE);
	}
	return ok;
}

AsciiString TerrainBrushes::getCategory(const AsciiString &terrainName)
{
	size_t i;
	for (i = 0; i < s_categories.size(); i++)
	{
		if (s_categories[i].terrain == terrainName)
		{
			return s_categories[i].category;
		}
	}
	return AsciiString::TheEmptyString;
}

void TerrainBrushes::setCategory(const AsciiString &terrainName, const AsciiString &category)
{
	size_t i;
	for (i = 0; i < s_categories.size(); i++)
	{
		if (s_categories[i].terrain == terrainName)
		{
			s_categories[i].category = category;
			return;
		}
	}
	CategoryEntry entry;
	entry.terrain = terrainName;
	entry.category = category;
	s_categories.push_back(entry);
}

Bool TerrainBrushes::isAlias(const AsciiString &name)
{
	size_t i;
	for (i = 0; i < s_aliases.size(); i++)
	{
		if (s_aliases[i].name == name)
		{
			return true;
		}
	}
	return false;
}
