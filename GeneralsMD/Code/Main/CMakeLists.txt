add_executable(z_generals WIN32)

# Use a binary name that doesn't conflict with original game.
# TheSuperHackers @feature A cheats enabled build gets a _c suffix, so a build that can grant
# money and reveal the map is never mistaken for a normal one sitting in the same folder.
if(RTS_DEBUG_CHEATS)
    set_target_properties(z_generals PROPERTIES OUTPUT_NAME "generalszh_c${RTS_BUILD_OUTPUT_SUFFIX}")
else()
    set_target_properties(z_generals PROPERTIES OUTPUT_NAME "generalszh${RTS_BUILD_OUTPUT_SUFFIX}")
endif()

if(RTS_BUILD_GENERALS_ONLINE AND WIN32)
    # The Generals Online vendor libraries are import libs; their DLLs must sit
    # beside the exe. discord-rpc's license ships alongside per its terms.
    set(GO_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../GameEngine/Source/GameNetwork/GeneralsOnline/Vendor")
    add_custom_command(TARGET z_generals POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GO_VENDOR_DIR}/DiscordRPC/LICENSE.txt"
            "$<TARGET_FILE_DIR:z_generals>/discord-rpc-LICENSE.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GO_VENDOR_DIR}/DiscordRPC/discord-rpc.dll"
            "${GO_VENDOR_DIR}/libcurl/libcurl.dll"
            "${GO_VENDOR_DIR}/libcurl/libcrypto-3.dll"
            "${GO_VENDOR_DIR}/libcurl/libssl-3.dll"
            "${GO_VENDOR_DIR}/libcurl/zlib1.dll"
            "${GO_VENDOR_DIR}/ValveNetworkingSockets/GameNetworkingSockets.dll"
            "${GO_VENDOR_DIR}/ValveNetworkingSockets/abseil_dll.dll"
            "${GO_VENDOR_DIR}/ValveNetworkingSockets/libprotobuf.dll"
            "$<TARGET_FILE_DIR:z_generals>"
    )
endif()

target_link_libraries(z_generals PRIVATE
    binkstub
    comctl32
    d3d8
    d3dx8
    dinput8
    dxguid
    imm32
    milesstub
    vfw32
    winmm
    z_gameengine
    z_gameenginedevice
    zi_always
)

# TODO Originally referred to build host and user, replace with git info perhaps?
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/GeneratedVersion.h
"#pragma once

#define VERSION_LOCALBUILDNUM 0
#define VERSION_BUILDUSER \"\"
#define VERSION_BUILDLOC \"\"
"
)

# Based on original binary values for these variables.
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/BuildVersion.h
"#pragma once

#define VERSION_MAJOR 1
#define VERSION_MINOR 4
#define VERSION_BUILDNUM 601
"
)

if(MSVC)
    target_link_options(z_generals PRIVATE "/NODEFAULTLIB:libci.lib")

    # Raise the 2 GB address space limit to 4 GB on 64 bit Windows. Contra's asset load
    # gets close enough to the default ceiling that a large texture allocation can fail;
    # the engine's texture loader does not check for that and crashes on the null
    # surface. Costs nothing on a 32 bit OS, where the limit stays 2 GB.
    target_link_options(z_generals PRIVATE "/LARGEADDRESSAWARE")
endif()

target_include_directories(z_generals PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_sources(z_generals PRIVATE
    WinMain.cpp
    WinMain.h
)

# RC files optional for MinGW builds
# Icon: LoadIcon() handles missing resource gracefully (uses default system icon)
# Manifest: DPI awareness metadata (nice to have but not essential)
if(MSVC)
    # VS2005 and later adds default manifest, we need to turn it off to prevent conflict with custom manifest
    if(NOT IS_VS6_BUILD)
        target_link_options(z_generals PRIVATE
            "/MANIFEST:NO"
        )
    endif()

    target_sources(z_generals PRIVATE
        RTS.RC
    )
endif()

# Strip debug symbols to separate file for MinGW Release builds
# This creates generalszh${RTS_BUILD_OUTPUT_SUFFIX}.exe.debug (similar to MSVC .pdb files)
if(MINGW AND COMMAND add_debug_strip_target)
    add_debug_strip_target(z_generals)
endif()
