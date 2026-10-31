"""Run the terrain readers, game path resolver and engine DDS block decoder.

The harness checks file validation and tiling without starting Direct3D or the game.
Requires the same Visual Studio installation as the build skill.
"""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
VCVARS = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")
SOURCE = ROOT / "Core/GameEngineDevice/Source/W3DDevice/GameClient/WorldHeightMap.cpp"
DDS_HEADER = ROOT / "GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/ddsfile.h"
DDS_SOURCE = DDS_HEADER.with_suffix(".cpp")
FILE_SOURCE = ROOT / "GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DFileSystem.cpp"

PREAMBLE = r"""
#include <algorithm>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>
using std::min;
typedef bool Bool;
typedef int Int;
typedef short Short;
typedef unsigned UnsignedInt;
typedef unsigned char UnsignedByte;
#define RTS_ZEROHOUR 1
#define FALSE false
#define TILE_PIXEL_EXTENT 64
#define TILE_BYTES_PER_PIXEL 4
#define MSGNEW(tag) new
#define DEBUG_ASSERTCRASH(test, message) assert(test)
#define stricmp _stricmp
enum { D3DFMT_DXT1=0x31545844, D3DFMT_DXT2=0x32545844, D3DFMT_DXT3=0x33545844,
    D3DFMT_DXT4=0x34545844, D3DFMT_DXT5=0x35545844, WW3D_FORMAT_A8R8G8B8=1 };
class InputStream {
public:
    virtual Int read(void *out, Int size) = 0;
};
struct MemoryStream : InputStream {
    std::vector<unsigned char> bytes;
    size_t position = 0;
    Int read(void *out, Int size) override {
        size = min(size, (Int)(bytes.size()-position));
        if (size == 0) {
            return 0;
        }
        memcpy(out, bytes.data()+position, size);
        position += size;
        return size;
    }
};
struct TileData {
    unsigned char pixels[64*64*4];
    bool mips = false;
    unsigned char *getDataPtr() { return pixels; }
    void updateMips() { mips = true; }
};
struct WorldHeightMap {
    static Int countTiles(InputStream *, Bool * = nullptr, const char * = nullptr);
    static Bool readTiles(InputStream *, TileData **, Int, const char * = nullptr);
};
"""

FILE_ADAPTER = r"""
#define _MAX_PATH 260
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
#define TGA_DIR_PATH "Art/Textures/"
#define TERRAIN_TGA_DIR_PATH "Art/Terrain/"
#define W3D_DIR_PATH "Art/W3D/"
#define USER_W3D_DIR_PATH "%sW3D/"
#define USER_TGA_DIR_PATH "%sTextures/"
#define MAP_PREVIEW_DIR_PATH "%sMapPreviews/"
#define strnicmp _strnicmp
static void strlcpy(char *out, const char *in, size_t size) { strcpy_s(out,size,in); }
static void strlcat(char *out, const char *in, size_t size) { strcat_s(out,size,in); }
struct Text { const char *str() const { return "English"; } };
static Text GetRegistryLanguage() { return Text(); }
struct GlobalData { Text getPath_UserData() { return Text(); } };
static GlobalData *TheGlobalData=nullptr;
struct FileSystem {
    bool doesFileExist(const char *path) {
        return _stricmp(path,"Art/Terrain/test.dds")==0 || _stricmp(path,"Art/Textures/model.dds")==0;
    }
};
static FileSystem files;
static FileSystem *TheFileSystem=&files;
enum GameFileType { FILE_TYPE_COMPLETELY_UNKNOWN, FILE_TYPE_W3D, FILE_TYPE_TGA, FILE_TYPE_DDS };
class GameFileClass {
    bool m_fileExists=false;
    char m_filePath[_MAX_PATH]={}, m_filename[_MAX_PATH]={};
public:
    bool Is_Open() const { return false; }
    void Close() {}
    bool Is_Available() const { return m_fileExists; }
    const char *Set_Name(const char *filename);
};
"""

DECODER_ADAPTER = r"""
static_assert(sizeof(LegacyDDSURFACEDESC2)==124, "DDS header layout");
static LegacyDDSURFACEDESC2 image;
static std::vector<unsigned char> encoded;
typedef int WW3DFormat;
enum { WW3D_FORMAT_DXT1=D3DFMT_DXT1, WW3D_FORMAT_DXT2=D3DFMT_DXT2,
    WW3D_FORMAT_DXT3=D3DFMT_DXT3, WW3D_FORMAT_DXT4=D3DFMT_DXT4, WW3D_FORMAT_DXT5=D3DFMT_DXT5 };
#define WWINLINE inline
#define WWASSERT(test) assert(test)
struct Vector3 {
    float values[3]={};
    float operator[](int index) const { return values[index]; }
};
static unsigned Get_Bytes_Per_Pixel(WW3DFormat format) { assert(format==WW3D_FORMAT_A8R8G8B8); return 4; }
static void Recolor(unsigned &, const Vector3 &) { assert(false); }
struct BitmapHandlerClass {
    static void Write_B8G8R8A8(unsigned char *out, WW3DFormat format, unsigned color) {
        assert(format==WW3D_FORMAT_A8R8G8B8);
        memcpy(out,&color,4);
    }
};
class DDSFileClass {
    WW3DFormat Format;
    unsigned MipLevels=1;
    bool available;
public:
    DDSFileClass(const char *name, unsigned) : Format(image.PixelFormat.FourCC) {
        GameFileClass file;
        file.Set_Name(name);
        available=file.Is_Available();
    }
    bool Is_Available() const { return available; }
    DDSType Get_Type() const { return DDS_TEXTURE; }
    unsigned Get_Width(unsigned) const { return image.Width; }
    unsigned Get_Height(unsigned) const { return image.Height; }
    bool Load() { return true; }
    const unsigned char *Get_Memory_Pointer(unsigned) const { return encoded.data()+128; }
    bool Get_4x4_Block(unsigned char *, unsigned, WW3DFormat, unsigned, unsigned, unsigned, const Vector3 &) const;
    void Copy_Level_To_Surface(unsigned, int, unsigned width, unsigned height, unsigned char *out, unsigned pitch) {
        for (unsigned y=0; y<height; y+=4) {
            for (unsigned x=0; x<width; x+=4) {
                Get_4x4_Block(out+y*pitch+x*4,pitch,WW3D_FORMAT_A8R8G8B8,0,x,y,Vector3());
            }
        }
    }
};
"""

CASES = r"""
static MemoryStream dds(unsigned width, unsigned height, unsigned format) {
    image = {};
    image.Size=sizeof(image); image.Width=width; image.Height=height;
    image.PixelFormat.Size=sizeof(image.PixelFormat); image.PixelFormat.Flags=4;
    image.PixelFormat.FourCC=format;
    MemoryStream stream;
    stream.bytes.resize(128+(width/4)*(height/4)*(format==D3DFMT_DXT1 ? 8 : 16));
    memcpy(stream.bytes.data(), "DDS ", 4);
    memcpy(stream.bytes.data()+4, &image, sizeof(image));
    unsigned blockBytes=format==D3DFMT_DXT1 ? 8 : 16;
    for (unsigned y=0; y<height/4; y++) {
        for (unsigned x=0; x<width/4; x++) {
            unsigned char *block=stream.bytes.data()+128+(y*(width/4)+x)*blockBytes;
            if (blockBytes==16) {
                block[0]=200;
                block+=8;
            }
            unsigned bottomY=height/4-1-y;
            unsigned short color=((bottomY%32)<<11)|((x%64)<<5)|((bottomY+x)%32);
            memcpy(block,&color,2);
        }
    }
    encoded=stream.bytes;
    return stream;
}
static MemoryStream tga(unsigned width, unsigned height) {
    TTargaHeader header = {};
    header.imageType=2; header.imageWidth=width; header.imageHeight=height; header.pixelDepth=32;
    MemoryStream stream;
    stream.bytes.resize(sizeof(header)+width*height*4);
    memcpy(stream.bytes.data(), &header, sizeof(header));
    for (unsigned y=0; y<height; y++) {
        for (unsigned x=0; x<width; x++) {
            unsigned char *pixel=stream.bytes.data()+sizeof(header)+(y*width+x)*4;
            pixel[0]=(((y/4)+(x/4))%32)<<3; pixel[1]=((x/4)%64)<<2;
            pixel[2]=((y/4)%32)<<3; pixel[3]=image.PixelFormat.FourCC==D3DFMT_DXT1 ? 255 : 200;
        }
    }
    return stream;
}
int main() {
    GameFileClass model;
    model.Set_Name("model.dds");
    assert(model.Is_Available());
    const unsigned formats[]={D3DFMT_DXT1,D3DFMT_DXT5};
    for (unsigned format : formats) {
        for (unsigned size : {64u,128u,512u,640u}) {
            MemoryStream compressed=dds(size,size,format), plain=tga(size,size);
            int rows=size/64;
            assert(WorldHeightMap::countTiles(&compressed,nullptr,"test.DDS")==rows*rows);
            assert(WorldHeightMap::countTiles(&plain,nullptr,"test.tga")==rows*rows);
            compressed.position=plain.position=0;
            TileData *actual[100]={}, *expected[100]={};
            assert(WorldHeightMap::readTiles(&compressed,actual,rows,"test.DDS"));
            assert(WorldHeightMap::readTiles(&plain,expected,rows,"test.tga"));
            for (int i=0; i<rows*rows; i++) {
                assert(actual[i]->mips && expected[i]->mips);
                assert(memcmp(actual[i]->pixels,expected[i]->pixels,sizeof(actual[i]->pixels))==0);
                delete actual[i]; delete expected[i];
            }
        }
    }
    MemoryStream rectangular=dds(128,256,D3DFMT_DXT1);
    assert(WorldHeightMap::countTiles(&rectangular,nullptr,"test.dds")==4);
    rectangular.position=0;
    TileData *cropped[100]={};
    assert(WorldHeightMap::readTiles(&rectangular,cropped,2,"test.dds"));
    assert(cropped[0]->pixels[0]==0 && cropped[2]->pixels[0]==128);
    for (auto tile : cropped) {
        delete tile;
    }
    for (int invalid=0; invalid<9; invalid++) {
        MemoryStream stream=dds(64,64,D3DFMT_DXT1);
        switch (invalid) {
        case 0: image.Width=1024; break;
        case 1: image.Height=32; break;
        case 2: image.PixelFormat.FourCC=0x30315844; break;
        case 3: image.Caps.Caps2=0x200; break;
        case 4: image.MipMapCount=100; break;
        case 5: image.PixelFormat.Flags=0; break;
        case 6: image.Size=0; break;
        case 7: image.Width=65; break;
        case 8: image.Depth=2; break;
        }
        memcpy(stream.bytes.data()+4,&image,sizeof(image));
        assert(WorldHeightMap::countTiles(&stream,nullptr,"test.dds")==0);
    }
    for (unsigned length : {0u,4u,127u,128u,130u}) {
        MemoryStream stream=dds(64,64,D3DFMT_DXT1);
        stream.bytes.resize(length);
        TileData *tiles[100]={};
        assert(!WorldHeightMap::readTiles(&stream,tiles,1,"test.dds"));
        assert(tiles[0]==nullptr);
    }
    for (unsigned format : {D3DFMT_DXT2,D3DFMT_DXT3,D3DFMT_DXT4}) {
        MemoryStream stream=dds(64,64,format);
        assert(WorldHeightMap::countTiles(&stream,nullptr,"test.dds")==0);
        stream.position=0;
        TileData *tiles[100]={};
        assert(!WorldHeightMap::readTiles(&stream,tiles,1,"test.dds"));
        assert(tiles[0]==nullptr);
    }
    puts("Terrain reader checks passed: real game path lookup, DXT1/DXT5 block decoding, TGA parity and invalid DDS rejection.");
}
"""


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
    source = SOURCE.read_text()
    readers = source[source.index("// Targa format:  Header"):source.index("/** updateTileTexturePositions")]
    header = DDS_HEADER.read_text()
    structs = header[header.index("struct LegacyDDCOLORKEY"):header.index("// Utility class for loading DDS files.")]
    file_source = FILE_SOURCE.read_text()
    lookup = "".join(function(file_source, signature) for signature in (
        "inline static Bool isImageFileType(", "static GameFileType getFileType(",
        "char const * GameFileClass::Set_Name("))
    decoder_source = DDS_SOURCE.read_text()
    decoder = "".join(function(decoder_source, signature) for signature in (
        "WWINLINE static unsigned RGB565_To_ARGB8888(",
        "WWINLINE static unsigned Combine_Colors(", "bool DDSFileClass::Get_4x4_Block("))
    cases = CASES.replace('"test.DDS"', '"Art/Terrain/test.DDS"').replace('"test.dds"', '"Art/Terrain/test.dds"')
    with tempfile.TemporaryDirectory(prefix="terrain-tiles-") as directory:
        work = Path(directory)
        (work / "test.cpp").write_text(PREAMBLE + structs + FILE_ADAPTER + lookup + DECODER_ADAPTER + decoder + readers + cases)
        (work / "run.bat").write_text(
            f'@echo off\ncall "{VCVARS}" x86 >nul\n'
            'if errorlevel 1 exit /b 1\n'
            'cl /nologo /EHsc /std:c++20 /Fe:test.exe test.cpp\n'
            'if errorlevel 1 exit /b 1\n'
            'test.exe\n'
        )
        subprocess.run(["cmd", "/d", "/c", str(work / "run.bat")], cwd=work, check=True)


if __name__ == "__main__":
    main()
