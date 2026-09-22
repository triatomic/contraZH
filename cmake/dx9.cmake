# Direct3D 9 comes from the Windows SDK, so unlike dx8.cmake there is nothing to fetch.
# D3DX is deliberately not used; its only distribution is an import library needing
# the deprecated d3dx9_43.dll at runtime.

add_library(d3d9lib INTERFACE)

target_link_libraries(d3d9lib INTERFACE d3d9 dinput8 dxguid)

if(MSVC)
    target_link_options(d3d9lib INTERFACE /NODEFAULTLIB:libci.lib)

    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "12.0.8804")
        target_link_libraries(d3d9lib INTERFACE legacy_stdio_definitions)
        target_link_options(d3d9lib INTERFACE /SAFESEH:NO)
    endif()
endif()

target_compile_definitions(d3d9lib INTERFACE -DBUILD_WITH_D3D9)

# Consumers keep naming the D3D8 targets so this backend switch stays a one-line
# change and the per-target CMakeLists continue to merge cleanly from upstream.
add_library(d3d8lib ALIAS d3d9lib)
add_library(d3d8 ALIAS d3d9lib)
add_library(d3dx8 INTERFACE)
