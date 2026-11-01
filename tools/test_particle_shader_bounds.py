"""Check disruption fallback and particle shader bounds without starting the game.

Run after building d3d9 Release. Requires Visual Studio and a D3D9-capable GPU.
"""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
VCVARS = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")

HARNESS = r'''
#include <d3d9.h>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

typedef bool Bool;
const int DEFAULT_VOLUME_PARTICLE_DEPTH = 1;
enum { DRAW_DISRUPT };
struct ParticleName {
    const char* value;
    bool isEmpty() const { return value[0] == 0; }
};
struct ParticleSystem {
    ParticleName name;
    bool isUsingParticles() const { return true; }
    bool isUsingVolumeParticles() const { return false; }
    int getVolumeParticleDepth() const { return 1; }
    const ParticleName& getParticleTypeName() const { return name; }
};
struct SoftParticles {
    bool enabled;
    bool disruptionEnabled() const { return enabled; }
};
static SoftParticles softParticles = { true };
static SoftParticles* TheW3DSoftParticles = &softParticles;
struct W3DParticleSystemManager {
    int systemEffects(ParticleSystem&, int) { return 1; }
    Bool disrupts(ParticleSystem&);
};

// DISRUPTS_FUNCTION

static void check(HRESULT result)
{
    if (FAILED(result))
    {
        fprintf(stderr, "D3D9 failure 0x%08lx\n", result);
        exit(1);
    }
}

static IDirect3DPixelShader9* loadShader(IDirect3DDevice9* device, const std::string& path)
{
    std::ifstream input(path.c_str(), std::ios::binary | std::ios::ate);
    assert(input.good());
    const size_t size = static_cast<size_t>(input.tellg());
    assert(size != 0 && size % sizeof(DWORD) == 0);
    std::vector<DWORD> code(size / sizeof(DWORD));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(&code[0]), size);
    assert(input.good());
    IDirect3DPixelShader9* shader = NULL;
    check(device->CreatePixelShader(&code[0], &shader));
    return shader;
}

struct Vertex {
    float x, y, z, rhw;
    DWORD color;
    float u, v, px, py, pz;
};

static DWORD draw(IDirect3DDevice9* device, IDirect3DSurface9* target, IDirect3DSurface9* readback,
                  IDirect3DPixelShader9* shader, float u, float v)
{
    Vertex vertices[4] = {
        { -0.5f, -0.5f, 0.0f, 1.0f, 0xffffffff, u, v, 0.0f, 0.0f, 100.0f },
        { 3.5f, -0.5f, 0.0f, 1.0f, 0xffffffff, u, v, 0.0f, 0.0f, 100.0f },
        { -0.5f, 3.5f, 0.0f, 1.0f, 0xffffffff, u, v, 0.0f, 0.0f, 100.0f },
        { 3.5f, 3.5f, 0.0f, 1.0f, 0xffffffff, u, v, 0.0f, 0.0f, 100.0f }
    };
    check(device->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0));
    check(device->SetPixelShader(shader));
    check(device->BeginScene());
    check(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex)));
    check(device->EndScene());
    check(device->GetRenderTargetData(target, readback));
    D3DLOCKED_RECT locked;
    check(readback->LockRect(&locked, NULL, D3DLOCK_READONLY));
    const DWORD color = reinterpret_cast<DWORD*>(static_cast<char*>(locked.pBits) + 2 * locked.Pitch)[2] & 0xffffff;
    check(readback->UnlockRect());
    return color;
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    W3DParticleSystemManager manager;
    ParticleSystem particle = { { "" } };
    assert(!manager.disrupts(particle));
    particle.name.value = "pool.tga";
    assert(manager.disrupts(particle));
    softParticles.enabled = false;
    assert(!manager.disrupts(particle));

    HWND window = CreateWindowA("STATIC", "Particle shader test", WS_POPUP, 0, 0, 16, 16, NULL, NULL, NULL, NULL);
    assert(window != NULL);
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    assert(d3d != NULL);
    D3DPRESENT_PARAMETERS presentation = {};
    presentation.Windowed = TRUE;
    presentation.SwapEffect = D3DSWAPEFFECT_DISCARD;
    presentation.hDeviceWindow = window;
    presentation.BackBufferWidth = presentation.BackBufferHeight = 16;
    IDirect3DDevice9* device = NULL;
    check(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &presentation, &device));
    IDirect3DSurface9* target = NULL;
    IDirect3DSurface9* readback = NULL;
    check(device->CreateRenderTarget(4, 4, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &target, NULL));
    check(device->CreateOffscreenPlainSurface(4, 4, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &readback, NULL));
    check(device->SetRenderTarget(0, target));
    check(device->SetRenderState(D3DRS_LIGHTING, FALSE));
    check(device->SetRenderState(D3DRS_ZENABLE, FALSE));
    check(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE));
    check(device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
    check(device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX2 | D3DFVF_TEXCOORDSIZE3(1)));
    IDirect3DTexture9* texture = NULL;
    IDirect3DTexture9* noise = NULL;
    check(device->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, NULL));
    check(device->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &noise, NULL));
    D3DLOCKED_RECT locked;
    check(texture->LockRect(0, &locked, NULL, 0));
    *static_cast<DWORD*>(locked.pBits) = 0xffffffff;
    check(texture->UnlockRect(0));
    check(device->SetTexture(0, texture));
    check(device->SetTexture(1, texture));
    check(device->SetTexture(2, noise));
    for (DWORD stage = 0; stage < 3; ++stage)
    {
        check(device->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_POINT));
        check(device->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
    }
    const char* effects[] = { "flame", "electric" };
    for (int effect = 0; effect < 2; ++effect)
    {
        const std::string base = std::string(argv[1]) + "/particle" + effects[effect];
        IDirect3DPixelShader9* plain = loadShader(device, base + ".pso");
        IDirect3DPixelShader9* terrain = loadShader(device, base + "terrain.pso");
        float constants[12][4] = {};
        constants[5][1] = constants[5][3] = 1.0f;
        if (effect == 0)
        {
            constants[6][1] = 0.2f;
            constants[6][2] = 0.05f;
            constants[7][0] = constants[8][1] = constants[9][2] = 1.0f;
            constants[10][2] = 1000.0f;
            constants[11][0] = constants[11][1] = -1000000.0f;
            constants[11][2] = constants[11][3] = 1000000.0f;
        }
        else
        {
            constants[6][2] = 0.05f;
            constants[6][3] = 0.2f;
            constants[7][1] = 10.0f;
            constants[7][3] = 1000.0f;
            constants[8][0] = constants[8][1] = -1000000.0f;
            constants[8][2] = constants[8][3] = 1000000.0f;
            constants[9][0] = 1.0f;
        }
        check(device->SetPixelShaderConstantF(0, &constants[0][0], 12));
        for (int side = 0; side < 2; ++side)
        {
            check(noise->LockRect(0, &locked, NULL, 0));
            *static_cast<DWORD*>(locked.pBits) = side == 0 ? 0xff404040 : 0xffc0c0c0;
            check(noise->UnlockRect(0));
            assert(draw(device, target, readback, terrain, 0.5f, 0.5f) != 0);
            const float outside = side == 0 ? 1.013353f : -0.013353f;
            for (int axis = 0; axis < 2; ++axis)
            {
                const float u = axis == 0 ? outside : 0.5f;
                const float v = axis == 1 ? outside : 0.5f;
                assert(draw(device, target, readback, plain, u, v) != 0);
                assert(draw(device, target, readback, terrain, u, v) == 0);
            }
        }
        terrain->Release();
        plain->Release();
    }
    device->SetTexture(0, NULL);
    device->SetTexture(1, NULL);
    device->SetTexture(2, NULL);
    noise->Release();
    texture->Release();
    readback->Release();
    target->Release();
    device->Release();
    d3d->Release();
    DestroyWindow(window);
    puts("PASS: untextured disruption fallback and all four flame/electric footprint edges on D3D9");
}
'''


def main():
    source = (ROOT / "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DParticleSys.cpp").read_text(encoding="utf-8")
    start = source.index("Bool W3DParticleSystemManager::disrupts(")
    end = source.index("\n}", start) + 2
    harness = HARNESS.replace("// DISRUPTS_FUNCTION", source[start:end])
    with tempfile.TemporaryDirectory(prefix="particle-shader-bounds-", dir=ROOT / "build") as directory:
        work = Path(directory)
        (work / "test.cpp").write_text(harness, encoding="utf-8")
        (work / "run.bat").write_text(
            f'@echo off\ncall "{VCVARS}" x86 >nul\n'
            'if errorlevel 1 exit /b 1\n'
            'cl /nologo /EHsc /Fe:test.exe test.cpp d3d9.lib user32.lib\n'
            'if errorlevel 1 exit /b 1\n'
            f'test.exe "{ROOT / "build/d3d9/shaders"}"\n', encoding="utf-8")
        subprocess.run(["cmd", "/d", "/c", str(work / "run.bat")], cwd=work, check=True)


if __name__ == "__main__":
    main()
