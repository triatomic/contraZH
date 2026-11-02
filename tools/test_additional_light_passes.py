"""Render additional production light batches on D3D9 and check state restoration."""

from pathlib import Path
import subprocess
import tempfile

from test_emissive_lights import function


ROOT = Path(__file__).resolve().parents[1]
VCVARS = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")

HARNESS = r'''
#define NOMINMAX
#include <d3d9.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
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
#define BUILD_WITH_D3D9 1
#define RTS_ZEROHOUR 1
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p)=NULL; } } while(0)
struct Vector3
{
    float X, Y, Z;
    Vector3(float x=0, float y=0, float z=0) : X(x), Y(y), Z(z) {}
};
struct Vector4
{
    float X, Y, Z, W;
    void Set(float x, float y, float z, float w) { X=x; Y=y; Z=z; W=w; }
};
struct AABoxClass { Vector3 Center, Extent; };
struct W3DShaderManager
{
    enum { MAX_PIXEL_LIGHTS = 8 };
    static void setDrawPixelLights(const Int*, Int);
    static void filterPixelLights(const AABoxClass&,std::vector<Int>&);
    // PIXEL_LIGHT
};
static std::vector<W3DShaderManager::PixelLight> PixelLights;
static int PixelLightCount;
static const int PIXEL_LIGHT_REGISTERS = 18;
struct GlobalData { bool m_usePathTracing; };
static GlobalData globals = { true };
static GlobalData* TheGlobalData = &globals;
static bool Get_Lights_Debug() { return false; }
// SHADER_PAIR
static std::map<DWORD, AdditionalLightShaders> AdditionalShaders;
// PASS_STATE
static IDirect3DDevice9* device;
static int extraDraws;
static DWORD DrawLitShader=1, DrawUnlitShader=0;
static const int GROUND_POINT_LIGHT_REGISTER=5;
static IDirect3DPixelShader9* GroundShader=NULL;
static void check(HRESULT hr)
{
    if (FAILED(hr))
    {
        fprintf(stderr, "D3D9 failure 0x%08lx\n", hr);
        exit(1);
    }
}
#define DX8CALL(call) check(device->call)
#define DX8_RECORD_DRAW_CALLS() (++extraDraws)
struct DX8Wrapper
{
    static bool (*AdditionalPassCallback)(unsigned);
    static unsigned CurrentBaseVertexIndex;
    static IDirect3DDevice9* _Get_D3D_Device8() { return device; }
    static void Set_Pixel_Shader(DWORD) { check(device->SetPixelShader(GroundShader)); }
    static void Set_Pixel_Shader_Constant(int reg,const Vector4* values,int count)
    { check(device->SetPixelShaderConstantF(reg,&values[0].X,count)); }
    static void Set_Additional_Pass_Callback(bool (*p)(unsigned)) { AdditionalPassCallback=p; }
    static void Draw_DX8_Indexed_Primitive(D3DPRIMITIVETYPE, UINT, UINT, UINT, UINT);
};
bool (*DX8Wrapper::AdditionalPassCallback)(unsigned) = NULL;
unsigned DX8Wrapper::CurrentBaseVertexIndex = 0;
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
// PACK_LIGHTS
// EXTRA_PASS
// CONFIGURE_PASS
// GROUND_LIGHTS
// FILTER_LIGHTS
// INDEXED_DRAW

static IDirect3DPixelShader9* loadShader(const std::string& path)
{
    std::ifstream input(path.c_str(), std::ios::binary | std::ios::ate);
    assert(input.good());
    std::vector<DWORD> code(static_cast<size_t>(input.tellg())/4);
    input.seekg(0);
    input.read(reinterpret_cast<char*>(&code[0]), code.size()*4);
    IDirect3DPixelShader9* shader = NULL;
    check(device->CreatePixelShader(&code[0], &shader));
    return shader;
}
struct Vertex
{
    float x,y,z,w;
    DWORD color;
    float u0,v0,u1,v1,nz,px,py,pz,wx,wy,wz,vx,vy,vz;
};
static DWORD draw(int count, int slots, IDirect3DPixelShader9* base, IDirect3DSurface9* target, IDirect3DSurface9* readback, int firstRegister=5, bool groundLights=false, const AABoxClass* bounds=NULL)
{
    check(device->Clear(0,NULL,D3DCLEAR_TARGET,0,1,0));
    std::vector<Int> selected;
    if (bounds != NULL)
    {
        for (int i=0; i<count; i++)
        {
            selected.push_back(i);
        }
        W3DShaderManager::filterPixelLights(*bounds,selected);
        count=(int)selected.size();
    }
    const Int* indices=selected.empty() ? NULL : &selected[0];
    Vector4 constants[32] = {};
    Pack_Pixel_Lights(constants+firstRegister, slots, indices, count, NULL);
    check(device->SetPixelShaderConstantF(0, &constants[0].X, 32));
    check(device->SetPixelShader(base));
    check(device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO));
    check(device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
    check(device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE));
    check(device->SetRenderState(D3DRS_FOGCOLOR, 0xff123456));
    check(device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD));
    if (groundLights)
    {
        GroundShader=base;
        W3DShaderManager::setDrawPixelLights(NULL,count);
        int selected=0;
        for (int i=0; i<count; i++)
        {
            selected+=!PixelLights[i].objectsOnly;
        }
        count=selected;
        check(device->GetPixelShaderConstantF(firstRegister,&constants[firstRegister].X,PIXEL_LIGHT_REGISTERS));
    }
    else
    {
        Set_Additional_Pixel_Lights(1, indices, count, slots, firstRegister, NULL);
    }
    extraDraws = 0;
    check(device->BeginScene());
    DX8Wrapper::Draw_DX8_Indexed_Primitive(D3DPT_TRIANGLELIST,0,4,0,2);
    check(device->EndScene());
    assert(extraDraws == (TheGlobalData->m_usePathTracing ? (max(count,1)-1)/slots : 0));
    IDirect3DPixelShader9* bound = NULL;
    check(device->GetPixelShader(&bound));
    assert(bound == base);
    bound->Release();
    const DWORD states[] = { TRUE, D3DBLEND_ZERO, TRUE, 0xff123456, D3DBLENDOP_ADD };
    for (int i=0; i<ARRAY_SIZE(states); i++)
    {
        DWORD value;
        check(device->GetRenderState(AdditionalLightStates[i],&value));
        assert(value == states[i]);
    }
    Vector4 restored[18];
    const int registers = slots*2+(slots+3)/4;
    check(device->GetPixelShaderConstantF(firstRegister,&restored[0].X,registers));
    assert(memcmp(restored,constants+firstRegister,registers*sizeof(Vector4)) == 0);
    check(device->GetRenderTargetData(target,readback));
    D3DLOCKED_RECT locked;
    check(readback->LockRect(&locked,NULL,D3DLOCK_READONLY));
    const DWORD pixel = *(reinterpret_cast<DWORD*>(static_cast<char*>(locked.pBits)+4*locked.Pitch)+4);
    check(readback->UnlockRect());
    return pixel & 255;
}
int main(int argc, char** argv)
{
    assert(argc == 2);
    HWND window = CreateWindowA("STATIC","Light batch test",WS_POPUP,0,0,16,16,NULL,NULL,GetModuleHandle(NULL),NULL);
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    assert(window && d3d);
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed=TRUE;
    pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow=window;
    pp.BackBufferWidth=pp.BackBufferHeight=16;
    check(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device));
    IDirect3DSurface9 *target=NULL, *readback=NULL;
    check(device->CreateRenderTarget(8,8,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,NULL));
    check(device->CreateOffscreenPlainSurface(8,8,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,NULL));
    check(device->SetRenderTarget(0,target));
    IDirect3DTexture9* white=NULL;
    check(device->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&white,NULL));
    D3DLOCKED_RECT lock;
    check(white->LockRect(0,&lock,NULL,0));
    *(DWORD*)lock.pBits=0xffffffff;
    check(white->UnlockRect(0));
    check(device->SetTexture(0,white));
    check(device->SetTexture(1,white));
    check(device->SetTexture(10,white));
    const Vertex vertices[] = {
        {-0.5f,-0.5f,0,1,0xff404040,0.5f,0.5f,0.5f,0.5f,1,-1,1,0,-1,1,0,-1,1,0},
        {7.5f,-0.5f,0,1,0xff404040,0.5f,0.5f,0.5f,0.5f,1,1,1,0,1,1,0,1,1,0},
        {-0.5f,7.5f,0,1,0xff404040,0.5f,0.5f,0.5f,0.5f,1,-1,-1,0,-1,-1,0,-1,-1,0},
        {7.5f,7.5f,0,1,0xff404040,0.5f,0.5f,0.5f,0.5f,1,1,-1,0,1,-1,0,1,-1,0}
    };
    const WORD indices[] = {0,1,2,2,1,3};
    IDirect3DVertexBuffer9* vb=NULL;
    IDirect3DIndexBuffer9* ib=NULL;
    const DWORD fvf=D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX5|D3DFVF_TEXCOORDSIZE3(1)|D3DFVF_TEXCOORDSIZE3(2)|D3DFVF_TEXCOORDSIZE3(3)|D3DFVF_TEXCOORDSIZE3(4);
    check(device->CreateVertexBuffer(sizeof(vertices),0,fvf,D3DPOOL_MANAGED,&vb,NULL));
    void* bits;
    check(vb->Lock(0,0,&bits,0));
    memcpy(bits,vertices,sizeof(vertices));
    check(vb->Unlock());
    check(device->CreateIndexBuffer(sizeof(indices),0,D3DFMT_INDEX16,D3DPOOL_MANAGED,&ib,NULL));
    check(ib->Lock(0,0,&bits,0));
    memcpy(bits,indices,sizeof(indices));
    check(ib->Unlock());
    check(device->SetStreamSource(0,vb,0,sizeof(Vertex)));
    check(device->SetIndices(ib));
    check(device->SetFVF(fvf));
    check(device->SetRenderState(D3DRS_ZENABLE,FALSE));
    check(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));
    check(device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE));
    check(device->SetRenderState(D3DRS_FOGENABLE,FALSE));
    IDirect3DPixelShader9* base=loadShader(std::string(argv[1])+"/terrainlitnoshadow.pso");
    AdditionalShaders[1].normal=loadShader(std::string(argv[1])+"/terrainlitnoshadowextralights.pso");
    PixelLightCount=257;
    PixelLights.resize(PixelLightCount);
    for (auto& light : PixelLights)
    {
        light.position=Vector3(0,0,10);
        light.innerRadius=0.5f;
        light.outerRadius=100;
        light.diffuse=Vector3(0.003f,0.003f,0.003f);
    }
    const DWORD zero=draw(0,8,base,target,readback);
    const DWORD eight=draw(8,8,base,target,readback);
    const DWORD nine=draw(9,8,base,target,readback);
    const DWORD many=draw(73,8,base,target,readback);
    assert(zero>=63 && zero<=65 && eight>zero && nine>eight && nine-eight<10 && many>nine+30);
    TheGlobalData->m_usePathTracing=false;
    assert(draw(73,8,base,target,readback)==eight);
    TheGlobalData->m_usePathTracing=true;
    for (int i=0; i<73; i++)
    {
        PixelLights[i].objectsOnly=true;
    }
    assert(draw(73,8,base,target,readback,5,true)==zero);
    for (int i=8; i<17; i++)
    {
        PixelLights[i].objectsOnly=false;
    }
    assert(draw(73,8,base,target,readback,5,true)==nine);
    TheGlobalData->m_usePathTracing=false;
    assert(draw(73,8,base,target,readback,5,true)==eight);
    TheGlobalData->m_usePathTracing=true;
    for (int i=0; i<73; i++)
    {
        PixelLights[i].objectsOnly=false;
    }
    assert(draw(73,8,base,target,readback,5,true)==many);
    // Partial final batches and a second draw must restore the first batch each time.
    assert(draw(73,8,base,target,readback)==many);
    assert(draw(10,3,base,target,readback)>draw(3,3,base,target,readback));
    AdditionalShaders[1].normal->Release();
    base->Release();
    base=loadShader(std::string(argv[1])+"/specularlitnoshadow.pso");
    AdditionalShaders[1].normal=loadShader(std::string(argv[1])+"/specularlitnoshadowextralights.pso");
    const DWORD modelEight=draw(8,8,base,target,readback,8);
    assert(draw(73,8,base,target,readback,8)>modelEight+25);
    AABoxClass meshBounds;
    meshBounds.Extent=Vector3(2,2,0);
    for (int i=9; i<73; i++)
    {
        PixelLights[i].position=Vector3(1000,1000,1000);
    }
    const DWORD beforeCull=draw(73,8,base,target,readback,8);
    const int beforeDraws=extraDraws;
    assert(draw(73,8,base,target,readback,8,false,&meshBounds)==beforeCull);
    assert(beforeDraws==9 && extraDraws==1);
    for (auto& light : PixelLights)
    {
        light.position=Vector3(1000,1000,1000);
    }
    assert(draw(73,8,base,target,readback,8,false,&meshBounds)==0 && extraDraws==0);
    for (auto& light : PixelLights)
    {
        light.position=Vector3(0,0,10);
    }
    AdditionalShaders[1].normal->Release();
    base->Release();
    IDirect3DTexture9* modulation=NULL;
    check(device->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&modulation,NULL));
    const char* variants[] = { "terrainlitnoisenoshadow", "terrainlitnoise2noshadow", "roadlitnoise2noshadow" };
    for (int variant=0; variant<3; variant++)
    {
        base=loadShader(std::string(argv[1])+"/"+variants[variant]+".pso");
        AdditionalShaders[1].normal=loadShader(std::string(argv[1])+"/"+variants[variant]+"extralights.pso");
        const int cloudStage=variant==2 ? 1 : 2;
        const int noiseStage=cloudStage+1;
        for (int stage=0; stage<5; stage++)
        {
            check(device->SetTexture(stage,white));
        }
        for (int map=0; map<(variant==0 ? 1 : 2); map++)
        {
            check(device->SetTexture(cloudStage,white));
            check(device->SetTexture(noiseStage,white));
            check(device->SetTexture(map==0 ? cloudStage : noiseStage,modulation));
            int cloudContribution=0;
            for (int shade=0; shade<2; shade++)
            {
                check(modulation->LockRect(0,&lock,NULL,0));
                *(DWORD*)lock.pBits=map==0 ? (shade==0 ? 0xff000000 : 0xff808080) : (shade==0 ? 0x80555555 : 0x80808080);
                check(modulation->UnlockRect(0));
                for (auto& light : PixelLights)
                {
                    light.diffuse=Vector3();
                }
                const DWORD unlit=draw(0,8,base,target,readback);
                PixelLights[0].diffuse=Vector3(0.2f,0.2f,0.2f);
                const DWORD firstBatch=draw(8,8,base,target,readback);
                PixelLights[8].diffuse=PixelLights[0].diffuse;
                PixelLights[0].diffuse=Vector3();
                const DWORD extraBatch=draw(9,8,base,target,readback);
                assert(abs((int)firstBatch-(int)extraBatch)<=1);
                if (map==0)
                {
                    const int contribution=(int)firstBatch-(int)unlit;
                    assert(contribution>5);
                    if (shade==0)
                    {
                        cloudContribution=contribution;
                    }
                    else
                    {
                        assert(abs(contribution-cloudContribution)<=1);
                    }
                }
                else if (shade==0)
                {
                    assert(firstBatch<=1 && extraBatch<=1);
                }
            }
        }
        for (int stage=0; stage<5; stage++)
        {
            check(device->SetTexture(stage,white));
        }
        check(modulation->LockRect(0,&lock,NULL,0));
        *(DWORD*)lock.pBits=0xff808080;
        check(modulation->UnlockRect(0));
        check(device->SetTexture(0,modulation));
        if (variant<2)
        {
            check(device->SetTexture(1,modulation));
        }
        for (auto& light : PixelLights)
        {
            light.diffuse=Vector3();
        }
        PixelLights[0].diffuse=Vector3(0.9f,0.9f,0.9f);
        const DWORD brightFirst=draw(8,8,base,target,readback);
        PixelLights[8].diffuse=PixelLights[0].diffuse;
        PixelLights[0].diffuse=Vector3();
        const DWORD brightExtra=draw(9,8,base,target,readback);
        assert(abs((int)brightFirst-(int)brightExtra)<=1);
        if (variant!=2)
        {
            AdditionalShaders[1].normal->Release();
            base->Release();
        }
    }
    AdditionalShaders[1].normal->Release();
    base->Release();
    base=loadShader(std::string(argv[1])+"/flatterrainlit1.pso");
    AdditionalShaders[1].normal=loadShader(std::string(argv[1])+"/flatterrainlit1extralights.pso");
    check(device->SetTexture(1,modulation));
    PixelLights[0].diffuse=PixelLights[8].diffuse;
    PixelLights[8].diffuse=Vector3();
    const DWORD flatFirst=draw(8,8,base,target,readback);
    PixelLights[8].diffuse=PixelLights[0].diffuse;
    PixelLights[0].diffuse=Vector3();
    assert(abs((int)flatFirst-(int)draw(9,8,base,target,readback))<=1);
    modulation->Release();
    puts("PASS: mesh bounds cut nine extra draws to one, empty batches skipped, tile seams, road filtering and shader/state restoration");
    AdditionalShaders[1].normal->Release();
    base->Release();
    ib->Release();
    vb->Release();
    white->Release();
    target->Release();
    readback->Release();
    device->Release();
    d3d->Release();
    DestroyWindow(window);
}
'''


def main():
    shader = (ROOT / "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp").read_text(encoding="utf-8")
    header = (ROOT / "Core/GameEngineDevice/Include/W3DDevice/GameClient/W3DShaderManager.h").read_text(encoding="utf-8")
    wrapper = (ROOT / "Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.h").read_text(encoding="utf-8")
    start = header.index("\tstruct PixelLight\n")
    harness = HARNESS.replace("// PIXEL_LIGHT", header[start:header.index("\n\t};",start)+4])
    start = shader.index("struct AdditionalLightShaders\n")
    harness = harness.replace("// SHADER_PAIR", shader[start:shader.index("\n};",start)+3])
    start = shader.index("static std::vector<Int> AdditionalLightIndices;")
    harness = harness.replace("// PASS_STATE", shader[start:shader.index("\n#endif",start)])
    harness = harness.replace("// PACK_LIGHTS", function(shader,"static Int Pack_Pixel_Lights("))
    extra_pass = "\n".join(function(shader, signature) for signature in (
        "static bool Pixel_Light_Screen_Rect(",
        "static bool Additional_Light_Screen_Rect(",
        "static bool Draw_Additional_Light_Pass("))
    harness = harness.replace("// EXTRA_PASS", extra_pass)
    harness = harness.replace("// CONFIGURE_PASS", function(shader,"static void Set_Additional_Pixel_Lights("))
    harness = harness.replace("// GROUND_LIGHTS", function(shader,"void W3DShaderManager::setDrawPixelLights("))
    harness = harness.replace("// FILTER_LIGHTS", function(shader,"void W3DShaderManager::filterPixelLights("))
    harness = harness.replace("// INDEXED_DRAW", function(wrapper,"WWINLINE void DX8Wrapper::Draw_DX8_Indexed_Primitive(").replace("WWINLINE ",""))
    with tempfile.TemporaryDirectory(prefix="additional-light-passes-",dir=ROOT / "build") as directory:
        work=Path(directory)
        (work / "test.cpp").write_text(harness,encoding="utf-8")
        (work / "run.bat").write_text(
            f'@echo off\ncall "{VCVARS}" x86 >nul\n'
            'if errorlevel 1 exit /b 1\n'
            'cl /nologo /EHsc /std:c++17 /Fe:test.exe test.cpp d3d9.lib user32.lib\n'
            'if errorlevel 1 exit /b 1\n'
            f'test.exe "{ROOT / "build/d3d9/shaders"}"\n',encoding="utf-8")
        subprocess.run(["cmd","/d","/c",str(work / "run.bat")],cwd=work,check=True)


if __name__ == "__main__":
    main()
