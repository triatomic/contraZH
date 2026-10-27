# Building the Qt WorldBuilder

This repo is the **full MFC->Qt inversion** of the Zero Hour WorldBuilder map editor. The Qt
build is **off by default** -- a plain configure gives you the classic MFC WorldBuilder. To get
the Qt version you must install a 32-bit Qt5 and turn the option on.

> For the base game/tools prerequisites (DirectX SDK, STLport, Miles, Bink, GameSpy, ZLib, etc.)
> and the general Win32 build, see **[README.md](../../../../../README.md)** first. This document only covers the
> Qt-specific delta on top of that.

## Prerequisites (Qt-specific)

- **Visual Studio 2022** with the **MSVC v14x x86 (32-bit)** toolset. WorldBuilder is a 32-bit app.
- **Qt 5.15.2, 32-bit (`msvc2019` build)**. The 64-bit Qt will NOT link -- it must be the x86 build.
  - Install via the Qt Online Installer (pick "MSVC 2019 32-bit" under Qt 5.15.2), or `aqtinstall`:
    ```
    pip install aqtinstall
    aqt install-qt windows desktop 5.15.2 win32_msvc2019 -O C:\Qt
    ```
  - You then have e.g. `C:\Qt\5.15.2\msvc2019` -- that path is your `CMAKE_PREFIX_PATH`.
- **Ninja** (ships with VS) and **CMake 3.25+** for the preset-based build below.

## Configure + build (Ninja, recommended)

The project ships `CMakePresets.json`. Use the **`win32-qt`** preset (the `win32` preset with
`RTS_ENABLE_WORLDBUILDER_QT=ON`) and point CMake at your Qt install. Run inside an **x86** MSVC
environment (`vcvarsall.bat x86`):

```
cmake --preset win32-qt -DCMAKE_PREFIX_PATH="C:/Qt/5.15.2/msvc2019"

cmake --build --preset win32-qt --target z_worldbuilder
```

Output: `build/win32-qt/GeneralsMD/Release/WorldBuilderZH_Qt.exe`

The build automatically deploys the needed Qt runtime DLLs (via `windeployqt`) next to the exe.

## D3D9 backend (experimental, side by side)

The engine can render through Direct3D 9 (`RTS_D3D_BACKEND=D3D9`, D3D9Ex + FLIPEX where
available). The `win32-qt-d3d9` preset builds that variant as **`WorldBuilderZH_Qt_D3D9.exe`**,
which ships next to the D3D8 exe and shares its Qt runtime:

```
cmake --preset win32-qt-d3d9 -DCMAKE_PREFIX_PATH="C:/Qt/5.15.2/msvc2019"

cmake --build --preset win32-qt-d3d9 --target z_worldbuilder
cmake --build --preset win32-qt-d3d9 --target rts_shaders
```

Output: `build/win32-qt-d3d9/GeneralsMD/Release/WorldBuilderZH_Qt_D3D9.exe`

The D3D9 renderer loads its shader blobs at runtime from `shaders\*.pso|*.vso` in the game
folder. `rts_shaders` compiles them into `build/win32-qt-d3d9/shaders/`; copy that folder into the
install you run the exe from (a vanilla install has none of them). The tree shader is not part of
that set: copy `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shaders/Trees.vso`
alongside too, otherwise the D3D8 blob from `ShadersZH.big` is used and trees draw black.

The blobs and the engine share one constant-register layout, so they must come from the same
source tree as the exe. This repo tracks contraZH's `feat/d3d9-port`, the branch the Contra D3D9
game build ships with, and the blobs it produces are byte-identical to that install's `shaders/`.
Blobs from another branch still load, but every shader whose layout moved (lit, bump and seabed
terrain, lit roads, point lights, water swell) renders wrong while the rest look fine.

Differences from the D3D8 build: no D3DX, so the "Old" label renderer (ID3DXFont) is unavailable
and maps to the atlas renderer; HUD/ruler/tooltip text draws from a glyph atlas; PNG tracing
overlays are decoded with `stb_image`.

The game's D3D9 effects are switched from **Level Of Detail > FX Shaders**: shadow mapping,
bloom, the particle effect shaders (flame, electric, laser, cryo, with soft particles), HQ sky
cloud shadows, terrain normal maps, terrain height blend, specular/glint, dynamic lights, pixel
lights, and shoreline foam. Soft particles and each effect shader also have
individual switches. Each item starts
from the value in the player's `Options.ini` and is remembered in `WorldBuilder.ini`. Shadow
mapping still needs View > Show Shadows, and HQ sky needs View > Show Clouds. Objects cast into
the shadow map and the terrain receives it; the objects themselves keep their classic shadows.
Water reflections, ambient occlusion, laser ground glow and planar mirrors are listed as
unavailable because their rendering passes are not connected to the editor.
In the D3D8 exe Bloom and Dynamic Lights are available.

## Configure + build (Visual Studio generator, alternative)

```
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32 ^
  -DRTS_BUILD_ZEROHOUR=ON -DRTS_BUILD_GENERALS=OFF ^
  -DRTS_BUILD_ZEROHOUR_TOOLS=ON -DRTS_BUILD_GENERALS_TOOLS=OFF ^
  -DRTS_BUILD_OPTION_INTERNAL=ON ^
  -DRTS_ENABLE_WORLDBUILDER_QT=ON ^
  -DCMAKE_PREFIX_PATH="C:/Qt/5.15.2/msvc2019"

cmake --build build --target z_worldbuilder --config Release
```

## Debugging the Qt inversion

Keyboard / focus routing under the inversion (which HWND owns focus, whether a key was posted
as a command or passed to a Qt widget) can be traced with an opt-in facility. Configure with
`-DWB_QT_KEYDEBUG=ON` and rebuild:

```
cmake --preset win32-qt -DWB_QT_KEYDEBUG=ON -DCMAKE_PREFIX_PATH="C:/Qt/5.15.2/msvc2019"
cmake --build --preset win32-qt --target z_worldbuilder
```

It emits `[WBDBG] ...` lines via `OutputDebugString` -- read them with DebugView, cdb, or the
Visual Studio Output window. It compiles to nothing when the option is OFF (the default), so
normal builds pay zero cost and print no spam. Turn it back off with `-DWB_QT_KEYDEBUG=OFF`.
The macro (`WBQT_DBGLOG`, in `qt/WBQtDebug.h`) is reusable across the Qt sources.

## Notes

- `RTS_ENABLE_WORLDBUILDER_QT=OFF` (the default) builds the original MFC WorldBuilder with **zero**
  Qt dependency -- the Qt code is entirely `#ifdef RTS_HAS_QT`-guarded, so the OFF binary is
  byte-identical to the pre-Qt build. Leave it OFF if you don't want Qt.
- If CMake can't find Qt (`Could NOT find Qt5`), your `CMAKE_PREFIX_PATH` is wrong or points at a
  64-bit Qt. It must be the **32-bit** `msvc2019` Qt 5.15.2 directory.
- To run WorldBuilder it must sit beside the game data (it `SetCurrentDirectory`s to its own exe
  folder at startup), so deploy the exe + the auto-copied Qt DLLs into a Zero Hour install.
