cmake_minimum_required(VERSION 3.16)
include(cmake/setup.cmake)

project(everything
    LANGUAGES C CXX
    VERSION ${FF_VERSION_NUMBER}
    DESCRIPTION "everything"
)
if(NOT EXISTS 3rdparty/fixposition-sdk/fpsdk_common/CMakeLists.txt)
    message(FATAL_ERROR "Please clone the fixposition-sdk repository to 3rdparty/fixposition-sdk")
endif()
if(NOT EXISTS 3rdparty/ubloxcfg/ubloxcfg/CMakeLists.txt)
    message(FATAL_ERROR "Please clone the ubloxcfg repository to 3rdparty/fixposition-sdk")
endif()

set(FPSDK_BUILD_TESTING OFF)
if (BUILD_FFGUI STREQUAL "ON")
    set(FPSDK_USE_PROJ ON)
endif()

add_subdirectory(3rdparty/ubloxcfg/ubloxcfg             ubloxcfg)
add_subdirectory(3rdparty/ubloxcfg/ff                   ff)
add_subdirectory(3rdparty/ubloxcfg/cfgtool              cfgtool)
add_subdirectory(3rdparty/fixposition-sdk/fpsdk_common  fpsdk_common)
add_subdirectory(3rdparty/fixposition-sdk/fpsdk_apps    fpsdk_apps)
add_subdirectory(ffxx)
add_subdirectory(ffapps)

if (BUILD_FFGUI STREQUAL "ON")
   add_subdirectory(ffgui)
endif()
