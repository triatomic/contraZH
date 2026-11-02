"""Compare bounded and unbounded lighting on D3D9 using the actual batch callback."""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
VCVARS = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")

HARNESS = r'''
#define NOMINMAX
#include <d3d9.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <float.h>
#include <fstream>
#include <map>
#include <string>
#include <vector>
using std::min;
using std::max;
typedef int Int;
typedef float Real;
typedef bool Bool;
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
template<class T> void release(T *&pointer)
{
    if (pointer != NULL)
    {
        pointer->Release();
        pointer = NULL;
    }
}
#define SAFE_RELEASE(x) release(x)
struct Vector3
{
    float X, Y, Z;
    Vector3(float x = 0, float y = 0, float z = 0) : X(x), Y(y), Z(z) {}
};
struct Vector4
{
    float X, Y, Z, W;
    void Set(float x, float y, float z, float w) { X=x; Y=y; Z=z; W=w; }
};
struct W3DShaderManager
{
    struct PixelLight
    {
        Vector3 position, diffuse;
        float innerRadius, outerRadius, ambientScale;
    };
};
static std::vector<W3DShaderManager::PixelLight> PixelLights;
static int PixelLightCount;
enum { PIXEL_LIGHT_REGISTERS = 18 };
static IDirect3DDevice9 *device;
struct DX8Wrapper
{
    static IDirect3DDevice9 *_Get_D3D_Device8() { return device; }
};
struct AdditionalLightShaders
{
    IDirect3DPixelShader9 *normal, *debug;
};
static std::map<DWORD, AdditionalLightShaders> AdditionalShaders;
static Bool Get_Lights_Debug() { return FALSE; }
static D3DMATRIX operator*(const D3DMATRIX &a, const D3DMATRIX &b)
{
    D3DMATRIX result = {};
    for (int row = 0; row < 4; row++)
    {
        for (int column = 0; column < 4; column++)
        {
            for (int k = 0; k < 4; k++)
            {
                result.m[row][column] += a.m[row][k] * b.m[k][column];
            }
        }
    }
    return result;
}

// ENGINE_CODE

static void check(HRESULT result)
{
    if (FAILED(result))
    {
        fprintf(stderr, "D3D9 failure 0x%08lx\n", result);
        exit(1);
    }
}
static IDirect3DPixelShader9 *load(const std::string &path)
{
    std::ifstream input(path.c_str(), std::ios::binary | std::ios::ate);
    assert(input.good());
    std::vector<DWORD> code((size_t)input.tellg() / sizeof(DWORD));
    input.seekg(0);
    input.read((char *)&code[0], code.size() * sizeof(DWORD));
    IDirect3DPixelShader9 *shader = NULL;
    check(device->CreatePixelShader(&code[0], &shader));
    return shader;
}
static void draw(const D3DVIEWPORT9 &viewport, float cameraOffset, bool perspective)
{
    struct Vertex { float x,y,z,w,u,v,px,py,pz; };
    const float x = (float)viewport.X - .5f, y = (float)viewport.Y - .5f;
    const float scale = perspective ? 5.0f : 1.0f;
    const float depth = perspective ? 5.0f : 0.0f;
    Vertex vertices[4] = {
        {x,y,0,1,.5f,.5f,-scale+cameraOffset,scale,depth},
        {x+viewport.Width,y,0,1,.5f,.5f,scale+cameraOffset,scale,depth},
        {x,y+viewport.Height,0,1,.5f,.5f,-scale+cameraOffset,-scale,depth},
        {x+viewport.Width,y+viewport.Height,0,1,.5f,.5f,scale+cameraOffset,-scale,depth}
    };
    check(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex)));
}
static std::vector<DWORD> render(IDirect3DSurface9 *target, IDirect3DSurface9 *readback,
    IDirect3DPixelShader9 *shader, bool bounded, bool alpha, bool clipped, float cameraOffset, bool perspective, int &draws)
{
    const D3DVIEWPORT9 viewport = { 13, 7, 129, 81, 0, 1 };
    const RECT scissor = { 20, 12, 125, 72 };
    check(device->SetRenderTarget(0, target));
    check(device->SetViewport(&viewport));
    check(device->Clear(0, NULL, D3DCLEAR_TARGET, 0x80201008, 1, 0));
    check(device->SetScissorRect(&scissor));
    check(device->SetRenderState(D3DRS_SCISSORTESTENABLE, clipped));
    check(device->SetRenderState(D3DRS_COLORWRITEENABLE, alpha ? 15 : 7));
    check(device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
    check(device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE));
    check(device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO));
    check(device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE));
    check(device->SetRenderState(D3DRS_FOGCOLOR, 0x123456));
    D3DMATRIX view = {}, projection = {};
    for (int i = 0; i < 4; i++)
    {
        view.m[i][i] = projection.m[i][i] = 1;
    }
    view.m[3][0] = cameraOffset;
    projection.m[3][0] = -cameraOffset;
    projection.m[2][2] = .1f;
    projection.m[3][2] = .5f;
    if (perspective)
    {
        projection.m[2][2] = 100.0f/99.0f;
        projection.m[2][3] = 1.0f;
        projection.m[3][2] = -100.0f/99.0f;
        projection.m[3][3] = 0;
    }
    check(device->SetTransform(D3DTS_VIEW, &view));
    check(device->SetTransform(D3DTS_PROJECTION, &projection));
    AdditionalLightsInView = cameraOffset != 0;
    AdditionalLightView = view;
    check(device->SetPixelShader(shader));
    float eye[4] = {0,0,10,0};
    check(device->SetPixelShaderConstantF(1, eye, 1));
    Vector4 base[PIXEL_LIGHT_REGISTERS];
    Pack_Pixel_Lights(base, 8, &AdditionalLightIndices[0], PixelLightCount, AdditionalLightsInView ? &view : NULL);
    check(device->SetPixelShaderConstantF(5, &base[0].X, PIXEL_LIGHT_REGISTERS));
    check(device->BeginScene());
    draw(viewport, cameraOffset, perspective);
    draws = 0;
    if (bounded)
    {
        for (unsigned pass = 1; Draw_Additional_Light_Pass(pass); pass++)
        {
            draw(viewport, cameraOffset, perspective);
            draws++;
        }
        DWORD state;
        check(device->GetRenderState(D3DRS_ALPHABLENDENABLE, &state)); assert(state == FALSE);
        check(device->GetRenderState(D3DRS_DESTBLEND, &state)); assert(state == D3DBLEND_ZERO);
        check(device->GetRenderState(D3DRS_ZWRITEENABLE, &state)); assert(state == TRUE);
        check(device->GetRenderState(D3DRS_FOGCOLOR, &state)); assert(state == 0x123456);
        check(device->GetRenderState(D3DRS_SCISSORTESTENABLE, &state)); assert(state == (DWORD)clipped);
        RECT restored;
        check(device->GetScissorRect(&restored)); assert(memcmp(&restored, &scissor, sizeof(scissor)) == 0);
        Vector4 restoredConstants[PIXEL_LIGHT_REGISTERS];
        check(device->GetPixelShaderConstantF(5, &restoredConstants[0].X, PIXEL_LIGHT_REGISTERS));
        assert(memcmp(base, restoredConstants, sizeof(base)) == 0);
        IDirect3DPixelShader9 *restoredShader = NULL;
        check(device->GetPixelShader(&restoredShader)); assert(restoredShader == shader);
        restoredShader->Release();
        assert(SavedLightShader == NULL);
    }
    else
    {
        check(device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
        check(device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE));
        check(device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE));
        check(device->SetRenderState(D3DRS_FOGCOLOR, 0));
        check(device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD));
        for (int first = 8; first < PixelLightCount; first += 8)
        {
            Vector4 constants[PIXEL_LIGHT_REGISTERS];
            Pack_Pixel_Lights(constants, 8, &AdditionalLightIndices[first], PixelLightCount-first, AdditionalLightsInView ? &view : NULL);
            check(device->SetPixelShaderConstantF(5, &constants[0].X, PIXEL_LIGHT_REGISTERS));
            draw(viewport, cameraOffset, perspective);
            draws++;
        }
    }
    check(device->EndScene());
    check(device->GetRenderTargetData(target, readback));
    D3DLOCKED_RECT lock;
    check(readback->LockRect(&lock, NULL, D3DLOCK_READONLY));
    std::vector<DWORD> image(160*96);
    for (int y = 0; y < 96; y++)
    {
        memcpy(&image[y*160], (char *)lock.pBits + y*lock.Pitch, 160*4);
    }
    check(readback->UnlockRect());
    return image;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    HWND window = CreateWindowA("STATIC", "Light scissor test", WS_POPUP, 0,0,160,96,NULL,NULL,NULL,NULL);
    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS presentation = {};
    presentation.Windowed = TRUE;
    presentation.SwapEffect = D3DSWAPEFFECT_DISCARD;
    presentation.hDeviceWindow = window;
    presentation.BackBufferWidth = 160; presentation.BackBufferHeight = 96;
    check(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&presentation,&device));
    IDirect3DSurface9 *target = NULL, *readback = NULL;
    check(device->CreateRenderTarget(160,96,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,NULL));
    check(device->CreateOffscreenPlainSurface(160,96,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,NULL));
    check(device->SetDepthStencilSurface(NULL));
    check(device->SetRenderState(D3DRS_ZENABLE,FALSE));
    check(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));
    check(device->SetRenderState(D3DRS_LIGHTING,FALSE));
    check(device->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX2|D3DFVF_TEXCOORDSIZE3(1)));
    IDirect3DTexture9 *texture = NULL;
    check(device->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,NULL));
    D3DLOCKED_RECT lock;
    check(texture->LockRect(0,&lock,NULL,0)); *(DWORD *)lock.pBits = 0xffffffff;
    check(texture->UnlockRect(0)); check(device->SetTexture(0,texture));
    IDirect3DPixelShader9 *shader = load(std::string(argv[1])+"/pointlightpass.pso");
    AdditionalLightShader = 1;
    AdditionalShaders[1].normal = shader; AdditionalShaders[1].debug = NULL;
    AdditionalLightSlots = 8; AdditionalLightRegister = 5;
    for (int scenario = 0; scenario < 5; scenario++)
    {
        PixelLightCount = scenario == 4 ? 8 : 25;
        PixelLights.resize(PixelLightCount); AdditionalLightIndices.resize(PixelLightCount);
        for (int i = 0; i < PixelLightCount; i++)
        {
            W3DShaderManager::PixelLight &light = PixelLights[i];
            light.position = Vector3(-.55f+(i%4)*.05f, .3f+(i%3)*.02f, .05f);
            light.diffuse = Vector3(.02f,.01f,.015f);
            light.innerRadius = .01f; light.outerRadius = .25f; light.ambientScale = .1f;
            AdditionalLightIndices[i] = i;
            if (scenario == 1 && i >= 8 && i < 16)
            {
                light.position.X = 10;
            }
            if (scenario == 2 && i >= 8)
            {
                light.position.X = -10;
            }
            if (scenario == 3)
            {
                light.outerRadius = 6;
            }
        }
        for (int alpha = 0; alpha < 2; alpha++)
        {
            for (int clipped = 0; clipped < 2; clipped++)
            {
                for (int camera = 0; camera < 4; camera++)
                {
                    int referenceDraws, boundedDraws;
                    const float offset = (camera & 1) ? .125f : 0;
                    const bool perspective = camera >= 2;
                    for (int light = 0; light < PixelLightCount; light++)
                    {
                        PixelLights[light].position.Z = perspective ? 5.05f : .05f;
                    }
                    const std::vector<DWORD> reference = render(target,readback,shader,false,alpha!=0,clipped!=0,offset,perspective,referenceDraws);
                    const std::vector<DWORD> bounded = render(target,readback,shader,true,alpha!=0,clipped!=0,offset,perspective,boundedDraws);
                    assert(reference == bounded);
                    if (alpha)
                    {
                        assert(referenceDraws == boundedDraws);
                    }
                    else if (scenario == 1 || scenario == 2)
                    {
                        assert(boundedDraws < referenceDraws);
                    }
                    if (scenario == 3 && !alpha)
                    {
                        assert(referenceDraws == boundedDraws);
                    }
                }
            }
        }
    }
    device->SetTexture(0,NULL); shader->Release(); texture->Release(); target->Release(); readback->Release();
    device->Release(); d3d->Release(); DestroyWindow(window);
    puts("PASS 80 pixel-identical D3D9 perspective/orthographic comparisons, offscreen batch skipping, near-plane fallback, camera-space lights, alpha writes and restored GPU state");
}
'''


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2]


def main():
    source = (ROOT / "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp").read_text(encoding="utf-8")
    start = source.index("static std::vector<Int> AdditionalLightIndices;")
    engine = source[start:source.index("\n#endif", start)]
    for signature in ["static Int Pack_Pixel_Lights(", "static bool Pixel_Light_Screen_Rect(",
                      "static bool Additional_Light_Screen_Rect(", "static bool Draw_Additional_Light_Pass("]:
        engine += "\n" + function(source, signature)
    with tempfile.TemporaryDirectory(prefix="light-scissor-", dir=ROOT / "build") as directory:
        work = Path(directory)
        (work / "test.cpp").write_text(HARNESS.replace("// ENGINE_CODE", engine), encoding="utf-8")
        (work / "run.bat").write_text(
            f'@echo off\ncall "{VCVARS}" x86 >nul\n'
            'if errorlevel 1 exit /b 1\n'
            'cl /nologo /EHsc /Fe:test.exe test.cpp d3d9.lib user32.lib\n'
            'if errorlevel 1 exit /b 1\n'
            f'test.exe "{ROOT / "build/d3d9/shaders"}"\n', encoding="utf-8")
        subprocess.run(["cmd", "/d", "/c", str(work / "run.bat")], cwd=work, check=True)


if __name__ == "__main__":
    main()
