"""Check the engine's existence cache when files are created or overwritten."""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Core/GameEngine/Source/Common/System/FileSystem.cpp"
HEADER = ROOT / "Core/GameEngine/Include/Common/FileSystem.h"
VCVARS = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")

PREAMBLE = r'''
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
using std::min;
using std::max;
typedef bool Bool;
typedef int Int;
typedef char Char;
typedef unsigned char FileInstance;
#define TRUE true
#define FALSE false
#define ENABLE_FILESYSTEM_EXISTENCE_CACHE 1
#define USE_PERF_TIMER(name)
struct FastCriticalSectionClass
{
    struct LockClass { LockClass(FastCriticalSectionClass &) {} };
};
struct File
{
    enum { NONE = 0, READ = 1, WRITE = 2, CREATE = 4 };
    Int access;
    Int getAccess() const { return access; }
} testFile;
struct LocalFiles
{
    Bool exists = false;
    File *openFile(const Char *, Int access, size_t)
    {
        if (access & File::CREATE)
        {
            exists = true;
        }
        testFile.access = access;
        return exists ? &testFile : nullptr;
    }
    Bool doesFileExist(const Char *) const { return exists; }
} localFiles;
LocalFiles *TheLocalFileSystem = &localFiles;
struct ArchiveFiles
{
    FileInstance copies = 0;
    File *openFile(const Char *, Int, FileInstance instance)
    {
        return doesFileExist("", instance) ? &testFile : nullptr;
    }
    Bool doesFileExist(const Char *, FileInstance instance) const { return instance < copies; }
} archiveFiles;
ArchiveFiles *TheArchiveFileSystem = &archiveFiles;
class FileSystem
{
public:
'''

ADAPTER = r'''
    struct FileExistMap : std::map<std::string, FileExistData>
    {
        struct key_type
        {
            static std::string temporary(const Char *filename) { return filename; }
        };
    };
    mutable FileExistMap m_fileExist;
    mutable FastCriticalSectionClass m_fileExistMutex;
    static Bool s_liveLooseFiles;
    File *openFile(const Char *, Int, size_t = 0, FileInstance = 0);
    Bool doesFileExist(const Char *, FileInstance = 0) const;
    Bool isKnownArchiveOnlyRead(const Char *, Int) const;
};
Bool FileSystem::s_liveLooseFiles = false;
'''

CASES = r'''
int main()
{
    const Char *path = "Hotkeys.ini";
    const Int write = File::WRITE | File::CREATE;
    {
        FileSystem files;
        localFiles.exists = true;
        assert(files.doesFileExist(path));
        assert(files.openFile(path, write));
        assert(files.doesFileExist(path));
        assert(!files.doesFileExist(path, 1));
        for (Int save = 0; save < 300; ++save)
        {
            assert(files.openFile(path, write));
            assert(files.doesFileExist(path));
            assert(!files.doesFileExist(path, 1));
        }
    }
    {
        FileSystem files;
        localFiles.exists = false;
        assert(!files.doesFileExist(path));
        assert(files.openFile(path, write));
        assert(files.doesFileExist(path));
        assert(!files.doesFileExist(path, 1));
    }
    {
        FileSystem files;
        localFiles.exists = false;
        archiveFiles.copies = 2;
        assert(files.doesFileExist(path));
        assert(files.isKnownArchiveOnlyRead(path, File::READ));
        assert(!files.doesFileExist(path, 2));
        assert(files.openFile(path, write));
        assert(!files.isKnownArchiveOnlyRead(path, File::READ));
        assert(files.doesFileExist(path, 2));
        assert(!files.doesFileExist(path, 3));
    }
    puts("Passed file creation cache checks, including first save and 300 overwrites.");
}
'''


def function(source, signature):
    start = source.index(signature)
    body = source.index("{", start)
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


def main():
    header = HEADER.read_text()
    cache = header[header.index("\tstruct FileExistData"):header.index("\ttypedef std::hash_map<")]
    source = SOURCE.read_text()
    functions = "".join(function(source, signature) for signature in (
        "File*\t\tFileSystem::openFile(", "Bool FileSystem::isKnownArchiveOnlyRead(",
        "Bool FileSystem::doesFileExist("))
    with tempfile.TemporaryDirectory(prefix="file-cache-", dir=ROOT / "build") as directory:
        work = Path(directory)
        (work / "test.cpp").write_text(PREAMBLE + cache + ADAPTER + functions + CASES)
        (work / "run.bat").write_text(
            f'@echo off\ncall "{VCVARS}" x86 >nul\n'
            'if errorlevel 1 exit /b 1\n'
            'cl /nologo /EHsc /std:c++17 /Fe:test.exe test.cpp\n'
            'if errorlevel 1 exit /b 1\n'
            'test.exe\n')
        subprocess.run(["cmd", "/d", "/c", str(work / "run.bat")], cwd=work, check=True)


if __name__ == "__main__":
    main()
