# Compiles the HLSL shaders the shadow map needs into the same .vso/.pso blobs the
# engine already loads at runtime. Compiling here rather than at runtime keeps
# d3dcompiler out of the 32-bit process and turns shader errors into build errors.

find_program(RTS_FXC_EXECUTABLE
    NAMES fxc
    HINTS
        "$ENV{WindowsSdkVerBinPath}/x64"
        "$ENV{WindowsSdkVerBinPath}/x86"
        "$ENV{WindowsSdkDir}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
        "$ENV{WindowsSdkDir}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x86"
    PATHS
        "C:/Program Files (x86)/Windows Kits/10/bin"
    PATH_SUFFIXES
        x64 x86
)

if(NOT RTS_FXC_EXECUTABLE)
    # Globbing the versioned SDK directories is the fallback when the environment
    # does not name one.
    file(GLOB RTS_SDK_BIN_DIRS "C:/Program Files (x86)/Windows Kits/10/bin/10.*")
    list(REVERSE RTS_SDK_BIN_DIRS)
    foreach(dir ${RTS_SDK_BIN_DIRS})
        if(EXISTS "${dir}/x64/fxc.exe")
            set(RTS_FXC_EXECUTABLE "${dir}/x64/fxc.exe")
            break()
        endif()
    endforeach()
endif()

if(NOT RTS_FXC_EXECUTABLE)
    message(STATUS "fxc not found; shadow mapping will be unavailable and the legacy shadows used instead")
    set(RTS_SHADERS_AVAILABLE FALSE CACHE INTERNAL "")
    return()
endif()

message(STATUS "Shader compiler: ${RTS_FXC_EXECUTABLE}")
set(RTS_SHADERS_AVAILABLE TRUE CACHE INTERNAL "")

# Shaders land beside the binary, where the engine finds loose files before archives.
set(RTS_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${RTS_SHADER_OUTPUT_DIR}")

# rts_add_shader(<source.hlsl> <profile> <entry point> <output name>)
function(rts_add_shader SOURCE PROFILE ENTRY OUTPUT)
    set(src "${CMAKE_SOURCE_DIR}/${SOURCE}")
    set(dst "${RTS_SHADER_OUTPUT_DIR}/${OUTPUT}")

    add_custom_command(
        OUTPUT "${dst}"
        COMMAND "${RTS_FXC_EXECUTABLE}" /nologo /T ${PROFILE} /E ${ENTRY} /Fo "${dst}" "${src}"
        DEPENDS "${src}"
        COMMENT "Compiling ${OUTPUT} (${PROFILE})"
        VERBATIM
    )

    set(RTS_SHADER_OUTPUTS ${RTS_SHADER_OUTPUTS} "${dst}" PARENT_SCOPE)
endfunction()

set(RTS_SHADER_DIR "Core/GameEngineDevice/Source/W3DDevice/GameClient/Shaders")

rts_add_shader("${RTS_SHADER_DIR}/shadowdepth.hlsl"   vs_2_0 mainVS        shadowdepth.vso)
rts_add_shader("${RTS_SHADER_DIR}/shadowdepth.hlsl"   ps_2_0 mainPS        shadowdepth.pso)
rts_add_shader("${RTS_SHADER_DIR}/shadowdepth.hlsl"   ps_2_0 mainPackedPS  shadowdepthpacked.pso)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_0 main          terrainshadow.pso)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_0 mainPacked    terrainshadowpacked.pso)

add_custom_target(rts_shaders ALL DEPENDS ${RTS_SHADER_OUTPUTS})
