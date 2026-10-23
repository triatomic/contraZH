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

#pragma once

// ScriptJson.h : converts script exports between .scb and an editable JSON layout

#include <string>

bool ScriptJson_ScbToJson(const std::string &scb, std::string &json, std::string &error);
bool ScriptJson_JsonToScb(const std::string &json, std::string &scb, std::string &error);
bool ScriptJson_IsJsonPath(const char *path);

// Hands the import a .scb to parse; a .json input is converted into a temporary .scb first
class ScriptJsonImportFile
{
public:
	ScriptJsonImportFile();
	~ScriptJsonImportFile();

	bool open(const char *path);
	const char *scbPath() const { return m_scbPath.c_str(); }
	const char *error() const { return m_error.c_str(); }

private:
	std::string m_scbPath;
	std::string m_tempPath;
	std::string m_error;
};

// Gives the export a .scb to write; for a .json target, finish() converts it into the target
class ScriptJsonExportFile
{
public:
	explicit ScriptJsonExportFile(const char *path);
	~ScriptJsonExportFile();

	bool isValid() const { return !m_scbPath.empty(); }
	const char *scbPath() const { return m_scbPath.c_str(); }
	bool finish();
	const char *error() const { return m_error.c_str(); }

private:
	std::string m_targetPath;
	std::string m_scbPath;
	std::string m_tempPath;
	std::string m_error;
};
