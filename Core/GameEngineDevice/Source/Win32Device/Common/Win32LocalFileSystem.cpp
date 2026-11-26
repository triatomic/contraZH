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

///////// Win32LocalFileSystem.cpp /////////////////////////
// Bryan Cleveland, August 2002
////////////////////////////////////////////////////////////

#include <windows.h>
#include "Common/AsciiString.h"
#include "Common/GameMemory.h"
#include "Common/PerfTimer.h"
#include "Win32Device/Common/Win32LocalFileSystem.h"
#include "Win32Device/Common/Win32LocalFile.h"
#include <io.h>

static const UnsignedInt MISSES_BEFORE_DIRECTORY_LISTING = 8;
static const size_t MAX_DIRECTORY_LISTINGS = 64;

static Bool copyFileDirectory(Char *directory, size_t directorySize, const Char *path)
{
	const Char *separator = getLastPathSeparator(path);
	if (separator == nullptr) {
		return FALSE;
	}

	size_t length = separator - path;
	while (length > 0 && isPathSeparator(path[length - 1])) {
		--length;
	}

	if (length == 0 || length >= directorySize) {
		return FALSE;
	}

	memcpy(directory, path, length);
	directory[length] = '\0';
	return TRUE;
}

Win32LocalFileSystem::Win32LocalFileSystem(Bool cacheMissingDirectories) : LocalFileSystem(), m_cacheMissingDirectories(cacheMissingDirectories), m_ownerThreadId(GetCurrentThreadId())
{
}

Win32LocalFileSystem::~Win32LocalFileSystem() {
	for (std::map<AsciiString, void *>::iterator it = m_treeWatches.begin(); it != m_treeWatches.end(); ++it) {
		FindCloseChangeNotification(it->second);
	}
	for (std::map<AsciiString, DirectoryListing>::iterator it = m_directoryListings.begin(); it != m_directoryListings.end(); ++it) {
		FindCloseChangeNotification(it->second.m_watch);
	}
}

//DECLARE_PERF_TIMER(Win32LocalFileSystem_openFile)
File * Win32LocalFileSystem::openFile(const Char *filename, Int access, size_t bufferSize)
{
	//USE_PERF_TIMER(Win32LocalFileSystem_openFile)

	// sanity check
	if (*filename == '\0') {
		return nullptr;
	}

	const Bool isRead = (access & (File::WRITE | File::CREATE)) == 0;
	if (isRead && (isInMissingDirectory(filename) || isKnownMissingFile(filename))) {
		return nullptr;
	}

	if (access & File::WRITE) {
		// if opening the file for writing, we need to make sure the directory is there
		// before we try to create the file.
		AsciiString string;
		string = filename;
		AsciiString token;
		AsciiString dirName;
		string.nextToken(&token, "\\/");
		dirName = token;
		while (!string.isEmpty()) {
			createDirectory(dirName);
			string.nextToken(&token, "\\/");
			dirName.concat('\\');
			dirName.concat(token);
		}
	}

	// TheSuperHackers @fix Mauller 21/04/2025 Create new file handle when necessary to prevent memory leak
	Win32LocalFile *file = newInstance( Win32LocalFile );

	if (file->open(filename, access, bufferSize) == FALSE) {
		deleteInstance(file);
		file = nullptr;

		// The probe tells a missing directory from a missing file and remembers the directory
		if (isRead && isMissingDirectoryCacheActive()) {
			doesFileExist(filename);
		}
	} else {
		file->deleteOnClose();
	}

// this will also need to play nice with the STREAMING type that I added, if we ever enable this

// srj sez: this speeds up INI loading, but makes BIG files unusable.
// don't enable it without further tweaking.
//
// unless you like running really slowly.
//	if (!(access&File::WRITE)) {
//		// Return a ramfile.
//		RAMFile *ramFile = newInstance( RAMFile );
//		if (ramFile->open(file)) {
//			file->close(); // is deleteonclose, so should delete.
//			ramFile->deleteOnClose();
//			return ramFile;
//		}	else {
//			ramFile->close();
//			deleteInstance(ramFile);
//		}
//	}

	return file;
}

void Win32LocalFileSystem::update()
{
}

void Win32LocalFileSystem::init()
{
}

void Win32LocalFileSystem::reset()
{
}

//DECLARE_PERF_TIMER(Win32LocalFileSystem_doesFileExist)
Bool Win32LocalFileSystem::doesFileExist(const Char *filename) const
{
	//USE_PERF_TIMER(Win32LocalFileSystem_doesFileExist)
	if (isInMissingDirectory(filename) || isKnownMissingFile(filename)) {
		return FALSE;
	}

	if (GetFileAttributesA(filename) != INVALID_FILE_ATTRIBUTES) {
		return TRUE;
	}

	const DWORD error = GetLastError();
	if (error == ERROR_PATH_NOT_FOUND) {
		// A directory the missing-directory cache refuses counts as a miss in its parent's listing
		Char directory[_MAX_PATH];
		if (!noteMissingDirectory(filename) && copyFileDirectory(directory, ARRAY_SIZE(directory), filename)) {
			noteMissingFile(directory);
		}
	} else if (error == ERROR_FILE_NOT_FOUND) {
		noteMissingFile(filename);
	}

	return FALSE;
}

Bool Win32LocalFileSystem::makeDirectoryKey(Char *key, size_t keySize, const Char *path, Bool pathIsFile)
{
	if (isPathSeparator(path[0]) || (path[0] != '\0' && path[1] == ':')) {
		return FALSE;
	}

	size_t length = strlen(path);
	if (pathIsFile) {
		const Char *separator = getLastPathSeparator(path);
		if (separator == nullptr) {
			return FALSE;
		}
		length = separator - path;
	}

	while (length > 0 && isPathSeparator(path[length - 1])) {
		--length;
	}

	Char directory[_MAX_PATH];
	if (length == 0 || length >= ARRAY_SIZE(directory)) {
		return FALSE;
	}

	memcpy(directory, path, length);
	directory[length] = '\0';
	return FileSystem::makePathKey(key, keySize, directory);
}

Bool Win32LocalFileSystem::isMissingDirectoryCacheActive() const
{
	return m_cacheMissingDirectories && !FileSystem::isLiveLooseFiles();
}

Bool Win32LocalFileSystem::isInMissingDirectory(const Char *filename) const
{
	Char key[_MAX_PATH];
	if (!isMissingDirectoryCacheActive() || !makeDirectoryKey(key, ARRAY_SIZE(key), filename, TRUE)) {
		return FALSE;
	}

	FastCriticalSectionClass::LockClass lock(m_missingDirectoriesMutex);
	return m_missingDirectories.find(AsciiString(key)) != m_missingDirectories.end();
}

Bool Win32LocalFileSystem::noteMissingDirectory(const Char *filename) const
{
	Char key[_MAX_PATH];
	if (!isMissingDirectoryCacheActive() || !makeDirectoryKey(key, ARRAY_SIZE(key), filename, TRUE)) {
		return FALSE;
	}

	// AsciiString reference counts are not atomic, so the key string lives only under the lock
	FastCriticalSectionClass::LockClass lock(m_missingDirectoriesMutex);
	m_missingDirectories.insert(AsciiString(key));
	return TRUE;
}

void Win32LocalFileSystem::forgetMissingDirectory(const Char *directory)
{
	Char key[_MAX_PATH];
	if (!m_cacheMissingDirectories || !makeDirectoryKey(key, ARRAY_SIZE(key), directory, FALSE)) {
		return;
	}

	FastCriticalSectionClass::LockClass lock(m_missingDirectoriesMutex);
	m_missingDirectories.erase(AsciiString(key));
}

Bool Win32LocalFileSystem::makeFileKeys(Char *directoryKey, size_t directoryKeySize, Char *nameKey, size_t nameKeySize, const Char *filename)
{
	Char directory[_MAX_PATH];
	if (!copyFileDirectory(directory, ARRAY_SIZE(directory), filename) || directory[strlen(directory) - 1] == ':') {
		return FALSE;
	}

	const Char *name = getLastPathSeparator(filename) + 1;
	const size_t nameLength = strlen(name);
	if (nameLength == 0 || name[nameLength - 1] == '.') {
		return FALSE;
	}

	// Short-name aliases, streams, wildcards and non-ASCII case rules need the disk
	for (const Char *ch = name; *ch != '\0'; ++ch) {
		const UnsignedByte c = (UnsignedByte)*ch;
		if (c <= 0x20 || c >= 0x7F || strchr("~:*?\"<>|", c) != nullptr) {
			return FALSE;
		}
	}

	return FileSystem::makePathKey(directoryKey, directoryKeySize, directory) && FileSystem::makePathKey(nameKey, nameKeySize, name);
}

Bool Win32LocalFileSystem::isAbsentFromListing(const Char *path, Bool &unlisted) const
{
	unlisted = FALSE;
	Char directoryKey[_MAX_PATH];
	Char nameKey[_MAX_PATH];
	if (!makeFileKeys(directoryKey, ARRAY_SIZE(directoryKey), nameKey, ARRAY_SIZE(nameKey), path)) {
		return FALSE;
	}

	const AsciiString directoryString(directoryKey);
	std::map<AsciiString, DirectoryListing>::iterator it = m_directoryListings.find(directoryString);
	if (it == m_directoryListings.end()) {
		unlisted = TRUE;
		return FALSE;
	}

	if (WaitForSingleObject(it->second.m_watch, 0) != WAIT_TIMEOUT) {
		FindCloseChangeNotification(it->second.m_watch);
		m_directoryListings.erase(it);
		m_directoryMissCounts.erase(directoryString);
		unlisted = TRUE;
		return FALSE;
	}

	return it->second.m_names.find(AsciiString(nameKey)) == it->second.m_names.end();
}

Bool Win32LocalFileSystem::isKnownMissingFile(const Char *filename) const
{
	if (!isMissingDirectoryCacheActive()) {
		return FALSE;
	}

	FastCriticalSectionClass::LockClass lock(m_watchMutex);
	Bool unlisted;
	if (isAbsentFromListing(filename, unlisted)) {
		return TRUE;
	}

	// A directory absent from its parent's listing holds no files
	Char directory[_MAX_PATH];
	if (!unlisted || !copyFileDirectory(directory, ARRAY_SIZE(directory), filename)) {
		return FALSE;
	}

	return isAbsentFromListing(directory, unlisted);
}

void Win32LocalFileSystem::noteMissingFile(const Char *filename) const
{
	Char directoryKey[_MAX_PATH];
	Char nameKey[_MAX_PATH];
	if (!isMissingDirectoryCacheActive() || !makeFileKeys(directoryKey, ARRAY_SIZE(directoryKey), nameKey, ARRAY_SIZE(nameKey), filename)) {
		return;
	}

	const Bool isOwnerThread = GetCurrentThreadId() == m_ownerThreadId;
	FastCriticalSectionClass::LockClass lock(m_watchMutex);
	const AsciiString directoryString(directoryKey);
	if (m_directoryListings.find(directoryString) != m_directoryListings.end()) {
		return;
	}

	UnsignedInt &missCount = m_directoryMissCounts[directoryString];
	if (missCount < MISSES_BEFORE_DIRECTORY_LISTING) {
		++missCount;
	}
	if (missCount < MISSES_BEFORE_DIRECTORY_LISTING || !isOwnerThread || m_directoryListings.size() >= MAX_DIRECTORY_LISTINGS) {
		return;
	}

	// The watch exists before the enumeration so no change falls between them
	HANDLE watch = FindFirstChangeNotificationA(directoryKey, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME);
	if (watch == INVALID_HANDLE_VALUE) {
		m_directoryMissCounts.erase(directoryString);
		return;
	}

	AsciiString search = directoryString;
	search.concat("\\*");
	std::set<AsciiString> names;
	WIN32_FIND_DATAA findData;
	HANDLE findHandle = FindFirstFileA(search.str(), &findData);
	Bool listed = (findHandle != INVALID_HANDLE_VALUE);
	if (listed) {
		for (;;) {
			if (strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0) {
				Char entryKey[_MAX_PATH];
				if (!FileSystem::makePathKey(entryKey, ARRAY_SIZE(entryKey), findData.cFileName)) {
					listed = FALSE;
					break;
				}
				names.insert(AsciiString(entryKey));
			}
			if (!FindNextFileA(findHandle, &findData)) {
				listed = (GetLastError() == ERROR_NO_MORE_FILES);
				break;
			}
		}
		FindClose(findHandle);
	}

	if (!listed) {
		FindCloseChangeNotification(watch);
		m_directoryMissCounts.erase(directoryString);
		return;
	}

	DirectoryListing &listing = m_directoryListings[directoryString];
	listing.m_watch = watch;
	listing.m_names.swap(names);
}

Bool Win32LocalFileSystem::hasDirectoryTreeChanged(const AsciiString& directory)
{
	Char path[_MAX_PATH];
	size_t length = directory.getLength();
	if (length >= ARRAY_SIZE(path)) {
		return TRUE;
	}

	memcpy(path, directory.str(), length);
	while (length > 0 && isPathSeparator(path[length - 1])) {
		--length;
	}
	path[length] = '\0';

	Char key[_MAX_PATH];
	if (!FileSystem::makePathKey(key, ARRAY_SIZE(key), path)) {
		return TRUE;
	}

	// A thread's exit cancels the notifications it armed, so only the owner thread arms them
	const Bool isOwnerThread = GetCurrentThreadId() == m_ownerThreadId;
	FastCriticalSectionClass::LockClass lock(m_watchMutex);
	const AsciiString keyString(key);
	std::map<AsciiString, void *>::iterator it = m_treeWatches.find(keyString);
	if (it == m_treeWatches.end()) {
		if (isOwnerThread) {
			HANDLE watch = FindFirstChangeNotificationA(key, TRUE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE);
			if (watch != INVALID_HANDLE_VALUE) {
				m_treeWatches[keyString] = watch;
			}
		}
		return TRUE;
	}

	if (WaitForSingleObject(it->second, 0) == WAIT_TIMEOUT) {
		return FALSE;
	}

	if (!isOwnerThread || !FindNextChangeNotification(it->second)) {
		FindCloseChangeNotification(it->second);
		m_treeWatches.erase(it);
	}
	return TRUE;
}

void Win32LocalFileSystem::getFileListInDirectory(const AsciiString& currentDirectory, const AsciiString& originalDirectory, const AsciiString& searchName, FilenameList & filenameList, Bool searchSubdirectories) const
{
	AsciiString directory = originalDirectory;
	directory.concat(currentDirectory);

	AsciiString asciisearch = directory;
	asciisearch.concat(searchName);

	WIN32_FIND_DATA findData;
	HANDLE fileHandle = FindFirstFile(asciisearch.str(), &findData);
	Bool done = (fileHandle == INVALID_HANDLE_VALUE);

	for (; !done; done = (FindNextFile(fileHandle, &findData) == 0))	{
		if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
				(strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0)) {
				// if we haven't already, add this filename to the list.
				// a stl set should only allow one copy of each filename
				AsciiString newFilename = directory;
				newFilename.concat(findData.cFileName);
				if (filenameList.find(newFilename) == filenameList.end()) {
					filenameList.insert(newFilename);
				}
		}
	}
	FindClose(fileHandle);

	if (searchSubdirectories) {
		AsciiString subdirsearch = directory;
		subdirsearch.concat("*");
		fileHandle = FindFirstFile(subdirsearch.str(), &findData);
		done = (fileHandle == INVALID_HANDLE_VALUE);

		for (; !done; done = (FindNextFile(fileHandle, &findData) == 0)) {
			// A leading dot covers . and .. and hidden folders; a "*." search would also drop "Map v1.2"
			if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && findData.cFileName[0] != '.') {

					AsciiString tempsearchstr;
					tempsearchstr.concat(currentDirectory);
					tempsearchstr.concat(findData.cFileName);
					tempsearchstr.concat('\\');

					// recursively add files in subdirectories if required.
					getFileListInDirectory(tempsearchstr, originalDirectory, searchName, filenameList, searchSubdirectories);
			}
		}

		FindClose(fileHandle);
	}

}

Bool Win32LocalFileSystem::getFileInfo(const AsciiString& filename, FileInfo *fileInfo) const
{
	WIN32_FIND_DATA findData;
	HANDLE findHandle = nullptr;
	findHandle = FindFirstFile(filename.str(), &findData);

	if (findHandle == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	fileInfo->sizeHigh = findData.nFileSizeHigh;
	fileInfo->sizeLow = findData.nFileSizeLow;
	fileInfo->timestampHigh = findData.ftLastWriteTime.dwHighDateTime;
	fileInfo->timestampLow = findData.ftLastWriteTime.dwLowDateTime;

	FindClose(findHandle);

	return TRUE;
}

Bool Win32LocalFileSystem::createDirectory(AsciiString directory)
{
	forgetMissingDirectory(directory.str());

	if ((!directory.isEmpty()) && (directory.getLength() < _MAX_DIR)) {
		return (CreateDirectory(directory.str(), nullptr) != 0);
	}
	return FALSE;
}

AsciiString Win32LocalFileSystem::normalizePath(const AsciiString& filePath) const
{
	DWORD retval = GetFullPathNameA(filePath.str(), 0, nullptr, nullptr);
	if (retval == 0)
	{
		DEBUG_LOG(("Unable to determine buffer size for normalized file path. Error=(%u).", GetLastError()));
		return AsciiString::TheEmptyString;
	}

	AsciiString normalizedFilePath;
	retval = GetFullPathNameA(filePath.str(), retval, normalizedFilePath.getBufferForRead(retval - 1), nullptr);
	if (retval == 0)
	{
		DEBUG_LOG(("Unable to normalize file path '%s'. Error=(%u).", filePath.str(), GetLastError()));
		return AsciiString::TheEmptyString;
	}

	return normalizedFilePath;
}
