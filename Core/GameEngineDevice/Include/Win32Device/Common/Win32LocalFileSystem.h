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

///// Win32LocalFileSystem.h //////////////////////////////////
// Bryan Cleveland, August 2002
///////////////////////////////////////////////////////////////

#pragma once

#include "Common/LocalFileSystem.h"

#include <map>
#include <set>

class Win32LocalFileSystem : public LocalFileSystem
{
public:
	explicit Win32LocalFileSystem(Bool cacheMissingDirectories = FALSE);
	virtual ~Win32LocalFileSystem() override;

	virtual void init() override;
	virtual void reset() override;
	virtual void update() override;

	virtual File * openFile(const Char *filename, Int access = File::NONE, size_t bufferSize = File::BUFFERSIZE) override;	///< open the given file.
	virtual Bool doesFileExist(const Char *filename) const override;								///< does the given file exist?

	virtual void getFileListInDirectory(const AsciiString& currentDirectory, const AsciiString& originalDirectory, const AsciiString& searchName, FilenameList &filenameList, Bool searchSubdirectories) const override; ///< search the given directory for files matching the searchName (egs. *.ini, *.rep).  Possibly search subdirectories.
	virtual Bool getFileInfo(const AsciiString& filename, FileInfo *fileInfo) const override;

	virtual Bool createDirectory(AsciiString directory) override;
	virtual AsciiString normalizePath(const AsciiString& filePath) const override;
	virtual Bool hasDirectoryTreeChanged(const AsciiString& directory) override;

protected:
	static Bool makeDirectoryKey(Char *key, size_t keySize, const Char *path, Bool pathIsFile);	///< key of the file's directory or of a directory path; FALSE when the path is absolute, has no directory or the key does not fit
	Bool isMissingDirectoryCacheActive() const;
	Bool isInMissingDirectory(const Char *filename) const;	///< is the file's directory known to be missing?
	void noteMissingDirectory(const Char *filename) const;	///< remember that the file's directory is missing
	void forgetMissingDirectory(const Char *directory);

	// Only relative directories are cached because other code creates absolute ones without this class
	Bool m_cacheMissingDirectories;
	mutable std::set<AsciiString> m_missingDirectories;
	mutable FastCriticalSectionClass m_missingDirectoriesMutex;

private:
	struct DirectoryListing
	{
		void *m_watch;
		std::set<AsciiString> m_names;
	};

	static Bool makeFileKeys(Char *directoryKey, size_t directoryKeySize, Char *nameKey, size_t nameKeySize, const Char *filename);	///< keys of the file's directory and name; FALSE when the listing must not answer for the path
	Bool isKnownMissingFile(const Char *filename) const;	///< is the file absent from its directory's watched listing?
	void noteMissingFile(const Char *filename) const;	///< count a miss in an existing directory and list it after enough misses

	unsigned long m_ownerThreadId;
	std::map<AsciiString, void *> m_treeWatches;
	mutable std::map<AsciiString, DirectoryListing> m_directoryListings;
	mutable std::map<AsciiString, UnsignedInt> m_directoryMissCounts;
	mutable FastCriticalSectionClass m_watchMutex;
};
