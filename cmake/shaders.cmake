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

# rts_add_shader(<source.hlsl> <profile> <entry point> <output name> [NAME=VALUE ...])
function(rts_add_shader SOURCE PROFILE ENTRY OUTPUT)
    set(src "${CMAKE_SOURCE_DIR}/${SOURCE}")
    set(dst "${RTS_SHADER_OUTPUT_DIR}/${OUTPUT}")

    set(defines "")
    foreach(define ${ARGN})
        list(APPEND defines /D ${define})
    endforeach()

    add_custom_command(
        OUTPUT "${dst}"
        COMMAND "${RTS_FXC_EXECUTABLE}" /nologo /T ${PROFILE} /E ${ENTRY} ${defines} /Fo "${dst}" "${src}"
        DEPENDS "${src}" ${RTS_SHADER_INCLUDES}
        COMMENT "Compiling ${OUTPUT} (${PROFILE})"
        VERBATIM
    )

    set(RTS_SHADER_OUTPUTS ${RTS_SHADER_OUTPUTS} "${dst}" PARENT_SCOPE)
endfunction()

set(RTS_SHADER_DIR "Core/GameEngineDevice/Source/W3DDevice/GameClient/Shaders")

# Every shader rebuilds when a shared include changes, since fxc reports no dependencies.
set(RTS_SHADER_INCLUDES
    "${CMAKE_SOURCE_DIR}/${RTS_SHADER_DIR}/shadowreceive.hlsli"
    "${CMAKE_SOURCE_DIR}/${RTS_SHADER_DIR}/pointlights.hlsli")

rts_add_shader("${RTS_SHADER_DIR}/shadowdepth.hlsl"   ps_2_0 mainPackedPS  shadowdepthpacked.pso)
rts_add_shader("${RTS_SHADER_DIR}/instancedepth.hlsl" vs_2_0 main instancedepth.vso             PACKED=0)
rts_add_shader("${RTS_SHADER_DIR}/instancedepth.hlsl" vs_2_0 main instancedepthpacked.vso       PACKED=1)
rts_add_shader("${RTS_SHADER_DIR}/instancemain.hlsl"  vs_2_0 main instancemain.vso)
rts_add_shader("${RTS_SHADER_DIR}/instancedepth.hlsl" vs_2_0 main skindepth.vso                 PACKED=0 SKINNED=1)
rts_add_shader("${RTS_SHADER_DIR}/instancedepth.hlsl" vs_2_0 main skindepthpacked.vso           PACKED=1 SKINNED=1)
rts_add_shader("${RTS_SHADER_DIR}/instancemain.hlsl"  vs_2_0 main skinmain.vso                  SKINNED=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_0 main terrainshadow.pso             NOISE_COUNT=0 PACKED=0)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_0 main terrainshadownoise.pso        NOISE_COUNT=1 PACKED=0)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_0 main terrainshadownoise2.pso       NOISE_COUNT=2 PACKED=0)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_0 main terrainshadowpacked.pso       NOISE_COUNT=0 PACKED=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_0 main terrainshadownoisepacked.pso  NOISE_COUNT=1 PACKED=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_0 main terrainshadownoise2packed.pso NOISE_COUNT=2 PACKED=1)
# The terrain normal map variants take derivatives, which ps_2_0 lacks.
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainbump.pso                 NOISE_COUNT=0 SHADOWED=1 PACKED=0 BUMP=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainbumpnoise.pso            NOISE_COUNT=1 SHADOWED=1 PACKED=0 BUMP=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainbumpnoise2.pso           NOISE_COUNT=2 SHADOWED=1 PACKED=0 BUMP=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainbumppacked.pso           NOISE_COUNT=0 SHADOWED=1 PACKED=1 BUMP=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainbumpnoisepacked.pso      NOISE_COUNT=1 SHADOWED=1 PACKED=1 BUMP=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainbumpnoise2packed.pso     NOISE_COUNT=2 SHADOWED=1 PACKED=1 BUMP=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainbumpnoshadow.pso         NOISE_COUNT=0 SHADOWED=0 PACKED=0 BUMP=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainbumpnoisenoshadow.pso    NOISE_COUNT=1 SHADOWED=0 PACKED=0 BUMP=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainbumpnoise2noshadow.pso   NOISE_COUNT=2 SHADOWED=0 PACKED=0 BUMP=1)
# The point light variants need ps_2_a for their length.
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlitnoshadow.pso         NOISE_COUNT=0 SHADOWED=0 PACKED=0 BUMP=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlit.pso                 NOISE_COUNT=0 SHADOWED=1 PACKED=0 BUMP=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlitpacked.pso           NOISE_COUNT=0 SHADOWED=1 PACKED=1 BUMP=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlitnoisenoshadow.pso    NOISE_COUNT=1 SHADOWED=0 PACKED=0 BUMP=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlitnoise.pso            NOISE_COUNT=1 SHADOWED=1 PACKED=0 BUMP=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlitnoisepacked.pso      NOISE_COUNT=1 SHADOWED=1 PACKED=1 BUMP=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlitnoise2noshadow.pso   NOISE_COUNT=2 SHADOWED=0 PACKED=0 BUMP=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlitnoise2.pso           NOISE_COUNT=2 SHADOWED=1 PACKED=0 BUMP=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlitnoise2packed.pso     NOISE_COUNT=2 SHADOWED=1 PACKED=1 BUMP=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlitbumpnoshadow.pso     NOISE_COUNT=0 SHADOWED=0 PACKED=0 BUMP=1 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlitbump.pso             NOISE_COUNT=0 SHADOWED=1 PACKED=0 BUMP=1 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlitbumppacked.pso       NOISE_COUNT=0 SHADOWED=1 PACKED=1 BUMP=1 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlitbumpnoisenoshadow.pso NOISE_COUNT=1 SHADOWED=0 PACKED=0 BUMP=1 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlitbumpnoise.pso        NOISE_COUNT=1 SHADOWED=1 PACKED=0 BUMP=1 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlitbumpnoisepacked.pso  NOISE_COUNT=1 SHADOWED=1 PACKED=1 BUMP=1 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlitbumpnoise2noshadow.pso NOISE_COUNT=2 SHADOWED=0 PACKED=0 BUMP=1 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlitbumpnoise2.pso       NOISE_COUNT=2 SHADOWED=1 PACKED=0 BUMP=1 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/terrainshadow.hlsl" ps_2_a main terrainlitbumpnoise2packed.pso NOISE_COUNT=2 SHADOWED=1 PACKED=1 BUMP=1 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_0 main roadshadow.pso                NOISE_COUNT=0 PACKED=0)
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_0 main roadshadownoise.pso           NOISE_COUNT=1 PACKED=0)
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_0 main roadshadownoise2.pso          NOISE_COUNT=2 PACKED=0)
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_0 main roadshadowpacked.pso          NOISE_COUNT=0 PACKED=1)
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_0 main roadshadownoisepacked.pso     NOISE_COUNT=1 PACKED=1)
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_0 main roadshadownoise2packed.pso    NOISE_COUNT=2 PACKED=1)
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_0 main roadnoshadow.pso              NOISE_COUNT=0 SHADOWED=0 PACKED=0)
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_0 main roadnoisenoshadow.pso         NOISE_COUNT=1 SHADOWED=0 PACKED=0)
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_0 main roadnoise2noshadow.pso        NOISE_COUNT=2 SHADOWED=0 PACKED=0)
# The point light variants need ps_2_a for their length.
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_a main roadlitnoshadow.pso           NOISE_COUNT=0 SHADOWED=0 PACKED=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_a main roadlit.pso                   NOISE_COUNT=0 SHADOWED=1 PACKED=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_a main roadlitpacked.pso             NOISE_COUNT=0 SHADOWED=1 PACKED=1 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_a main roadlitnoisenoshadow.pso      NOISE_COUNT=1 SHADOWED=0 PACKED=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_a main roadlitnoise.pso              NOISE_COUNT=1 SHADOWED=1 PACKED=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_a main roadlitnoisepacked.pso        NOISE_COUNT=1 SHADOWED=1 PACKED=1 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_a main roadlitnoise2noshadow.pso     NOISE_COUNT=2 SHADOWED=0 PACKED=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_a main roadlitnoise2.pso             NOISE_COUNT=2 SHADOWED=1 PACKED=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/roadshadow.hlsl"    ps_2_a main roadlitnoise2packed.pso       NOISE_COUNT=2 SHADOWED=1 PACKED=1 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/flatterrain.hlsl"   ps_2_a main flatterrainlit1.pso           TEXTURE_COUNT=1)
rts_add_shader("${RTS_SHADER_DIR}/flatterrain.hlsl"   ps_2_a main flatterrainlit2.pso           TEXTURE_COUNT=2)
rts_add_shader("${RTS_SHADER_DIR}/flatterrain.hlsl"   ps_2_a main flatterrainlit3.pso           TEXTURE_COUNT=3)
rts_add_shader("${RTS_SHADER_DIR}/flatterrain.hlsl"   ps_2_a main flatterrainlit4.pso           TEXTURE_COUNT=4)
rts_add_shader("${RTS_SHADER_DIR}/pointlightpass.hlsl" ps_2_a main pointlightpass.pso)
rts_add_shader("${RTS_SHADER_DIR}/shadowmultiply.hlsl" ps_2_0 main shadowmultiply.pso           PACKED=0 CLOUD=0)
rts_add_shader("${RTS_SHADER_DIR}/shadowmultiply.hlsl" ps_2_0 main shadowmultiplypacked.pso     PACKED=1 CLOUD=0)
rts_add_shader("${RTS_SHADER_DIR}/shadowmultiply.hlsl" ps_2_0 main shadowmultiplycloud.pso      PACKED=0 CLOUD=1)
rts_add_shader("${RTS_SHADER_DIR}/shadowmultiply.hlsl" ps_2_0 main shadowmultiplycloudpacked.pso PACKED=1 CLOUD=1)
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_0 main specular.pso                 SHADOWED=1 PACKED=0)
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_0 main specularpacked.pso           SHADOWED=1 PACKED=1)
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_0 main specularnoshadow.pso         SHADOWED=0 PACKED=0)
# The bumped variants take derivatives, which ps_2_0 lacks.
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_a main specularderived.pso          SHADOWED=1 PACKED=0 BUMP=1)
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_a main specularderivedpacked.pso    SHADOWED=1 PACKED=1 BUMP=1)
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_a main specularderivednoshadow.pso  SHADOWED=0 PACKED=0 BUMP=1)
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_a main specularnormal.pso           SHADOWED=1 PACKED=0 BUMP=2)
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_a main specularnormalpacked.pso     SHADOWED=1 PACKED=1 BUMP=2)
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_a main specularnormalnoshadow.pso   SHADOWED=0 PACKED=0 BUMP=2)
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_a main specularlit.pso                SHADOWED=1 PACKED=0 BUMP=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_a main specularlitpacked.pso          SHADOWED=1 PACKED=1 BUMP=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_a main specularlitnoshadow.pso        SHADOWED=0 PACKED=0 BUMP=0 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_a main specularlitderived.pso         SHADOWED=1 PACKED=0 BUMP=1 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_a main specularlitderivedpacked.pso   SHADOWED=1 PACKED=1 BUMP=1 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_a main specularlitderivednoshadow.pso SHADOWED=0 PACKED=0 BUMP=1 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_a main specularlitnormal.pso          SHADOWED=1 PACKED=0 BUMP=2 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_a main specularlitnormalpacked.pso    SHADOWED=1 PACKED=1 BUMP=2 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/specular.hlsl"       ps_2_a main specularlitnormalnoshadow.pso  SHADOWED=0 PACKED=0 BUMP=2 LIGHTS=1)
rts_add_shader("${RTS_SHADER_DIR}/bloomblur.hlsl"      ps_2_0 main bloomblur.pso)
rts_add_shader("${RTS_SHADER_DIR}/softparticle.hlsl"   ps_2_0 main softparticledepth.pso        DEPTH=1)
rts_add_shader("${RTS_SHADER_DIR}/softparticle.hlsl"   ps_2_0 main softparticleheight.pso       DEPTH=0)
# Fading and flame shading together run past ps_2_0's 64 arithmetic slots.
rts_add_shader("${RTS_SHADER_DIR}/softparticle.hlsl"   ps_2_a main softparticleflamedepth.pso   DEPTH=1 FLAME=1)
rts_add_shader("${RTS_SHADER_DIR}/softparticle.hlsl"   ps_2_a main softparticleflameheight.pso  DEPTH=0 FLAME=1)
rts_add_shader("${RTS_SHADER_DIR}/softparticle.hlsl"   ps_2_0 main particleflame.pso            SOFT=0 FLAME=1)
rts_add_shader("${RTS_SHADER_DIR}/softparticle.hlsl"   ps_2_0 main softparticleelectricdepth.pso   DEPTH=1 ELECTRIC=1)
rts_add_shader("${RTS_SHADER_DIR}/softparticle.hlsl"   ps_2_0 main softparticleelectricheight.pso  DEPTH=0 ELECTRIC=1)
rts_add_shader("${RTS_SHADER_DIR}/softparticle.hlsl"   ps_2_0 main particleelectric.pso            SOFT=0 ELECTRIC=1)
rts_add_shader("${RTS_SHADER_DIR}/softparticle.hlsl"   ps_2_0 main softparticlelaserdepth.pso      DEPTH=1 LASER=1)
rts_add_shader("${RTS_SHADER_DIR}/softparticle.hlsl"   ps_2_0 main softparticlelaserheight.pso     DEPTH=0 LASER=1)
rts_add_shader("${RTS_SHADER_DIR}/softparticle.hlsl"   ps_2_0 main particlelaser.pso               SOFT=0 LASER=1)
rts_add_shader("${RTS_SHADER_DIR}/laserglow.hlsl"      ps_2_0 main laserglow.pso)
rts_add_shader("${RTS_SHADER_DIR}/heathaze.hlsl"       ps_2_0 main heathaze.pso)
rts_add_shader("${RTS_SHADER_DIR}/shockwave.hlsl"      ps_2_0 main shockwave.pso)
rts_add_shader("${RTS_SHADER_DIR}/ambientocclusion.hlsl" ps_2_a main ambientocclusion.pso      BLUR=0)
rts_add_shader("${RTS_SHADER_DIR}/ambientocclusion.hlsl" ps_2_a main ambientocclusionblur.pso  BLUR=1)
# The water shaders outgrow ps_2_0's instruction limit.
rts_add_shader("${RTS_SHADER_DIR}/shaderwater.hlsl"    ps_2_a main shaderwater.pso              RIVER=0 PACKED=0)
rts_add_shader("${RTS_SHADER_DIR}/shaderwater.hlsl"    ps_2_a main shaderwaterpacked.pso        RIVER=0 PACKED=1)
rts_add_shader("${RTS_SHADER_DIR}/shaderwater.hlsl"    ps_2_a main shaderriver.pso              RIVER=1 PACKED=0)
rts_add_shader("${RTS_SHADER_DIR}/shaderwater.hlsl"    ps_2_a main shaderriverpacked.pso        RIVER=1 PACKED=1)
# Vertex waves read a texture in the vertex shader, which needs shader model 3 on both ends.
rts_add_shader("${RTS_SHADER_DIR}/shaderwaterswell.hlsl" vs_3_0 main shaderwaterswell.vso)
rts_add_shader("${RTS_SHADER_DIR}/shaderwater.hlsl"    ps_3_0 main shaderwaterswell.pso         RIVER=0 SWELL=1 PACKED=0)
rts_add_shader("${RTS_SHADER_DIR}/shaderwater.hlsl"    ps_3_0 main shaderwaterswellpacked.pso   RIVER=0 SWELL=1 PACKED=1)
rts_add_shader("${RTS_SHADER_DIR}/shaderwaterswell.hlsl" vs_3_0 main shaderwaterradial.vso RADIAL=1)
rts_add_shader("${RTS_SHADER_DIR}/shaderwater.hlsl"    ps_3_0 main shaderwaterradial.pso        RIVER=0 SWELL=1 RADIAL=1 PACKED=0)
rts_add_shader("${RTS_SHADER_DIR}/shaderwater.hlsl"    ps_3_0 main shaderwaterradialpacked.pso  RIVER=0 SWELL=1 RADIAL=1 PACKED=1)

add_custom_target(rts_shaders ALL DEPENDS ${RTS_SHADER_OUTPUTS})
