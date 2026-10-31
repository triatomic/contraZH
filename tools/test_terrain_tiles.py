"""Run the actual terrain readers with only the existing DDS decompressor stubbed.

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

DECODER_STUB = r"""
static_assert(sizeof(LegacyDDSURFACEDESC2)==124, "DDS header layout");
static LegacyDDSURFACEDESC2 image;
class DDSFileClass {
public:
    DDSFileClass(const char *, unsigned) {}
    bool Is_Available() const { return true; }
    DDSType Get_Type() const { return DDS_TEXTURE; }
    unsigned Get_Width(unsigned) const { return image.Width; }
    unsigned Get_Height(unsigned) const { return image.Height; }
    bool Load() { return true; }
    const unsigned char *Get_Memory_Pointer(unsigned) const { return (const unsigned char *)&image; }
    void Copy_Level_To_Surface(unsigned, int, unsigned width, unsigned height, unsigned char *out, unsigned pitch) {
        for (unsigned y=0; y<height; y++) {
            for (unsigned x=0; x<width; x++) {
                unsigned char *pixel = out+y*pitch+x*4;
                pixel[0]=(height-1-y)%251; pixel[1]=x%251; pixel[2]=255; pixel[3]=200;
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
            pixel[0]=y%251; pixel[1]=x%251; pixel[2]=255; pixel[3]=200;
        }
    }
    return stream;
}
int main() {
    const unsigned formats[]={D3DFMT_DXT1,D3DFMT_DXT2,D3DFMT_DXT3,D3DFMT_DXT4,D3DFMT_DXT5};
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
    assert(cropped[0]->pixels[0]==0 && cropped[2]->pixels[0]==64);
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
    puts("Terrain reader checks passed: TGA parity, DXT1-DXT5 dispatch, tile order, cropping and invalid/truncated DDS.");
}
"""


def main():
    source = SOURCE.read_text()
    readers = source[source.index("// Targa format:  Header"):source.index("/** updateTileTexturePositions")]
    header = DDS_HEADER.read_text()
    structs = header[header.index("struct LegacyDDCOLORKEY"):header.index("// Utility class for loading DDS files.")]
    with tempfile.TemporaryDirectory(prefix="terrain-tiles-") as directory:
        work = Path(directory)
        (work / "test.cpp").write_text(PREAMBLE + structs + DECODER_STUB + readers + CASES)
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
