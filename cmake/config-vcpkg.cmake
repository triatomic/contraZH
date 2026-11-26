# Contains options for VCPKG features

option(RTS_BUILD_OPTION_FFMPEG "Enable FFmpeg support" OFF)
if(RTS_BUILD_OPTION_FFMPEG)
    list(APPEND VCPKG_MANIFEST_FEATURES "ffmpeg")
endif()

if(RTS_BUILD_GENERALS_ONLINE)
    list(APPEND VCPKG_MANIFEST_FEATURES "genonline")
    list(APPEND VCPKG_OVERLAY_PORTS "${CMAKE_SOURCE_DIR}/GenOnline/cmake/ports")
    include(${CMAKE_SOURCE_DIR}/GenOnline/cmake/config-vcpkg.cmake)
endif()
