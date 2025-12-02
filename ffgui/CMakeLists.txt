########################################################################################################################
#
# flipflip's gui (ffgui)
#
# Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)
#
# This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
# warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with this program.
# If not, see <https://www.gnu.org/licenses/>.
#
########################################################################################################################

# GENERAL ==============================================================================================================

cmake_minimum_required(VERSION 3.16)
include(../cmake/setup.cmake)

project(ffgui
    LANGUAGES C CXX
    VERSION ${FF_VERSION_NUMBER}
    DESCRIPTION "flipflip's gui (ffgui)"
)

# COMPILER SETUP =======================================================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror \
    -Wshadow -Wunused-parameter -Wformat -Wpointer-arith -Woverloaded-virtual")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror \
    -Wshadow -Wunused-parameter -Wformat -Wpointer-arith -Wundef")
set(CMAKE_C_FLAGS_RELEASE "-O3")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(NDEBUG)
endif()


# DEPENDENCIES =========================================================================================================

if(NOT TARGET ubloxcfg)
    find_package(ubloxcfg REQUIRED)
endif()
if(NOT TARGET fpsdk_common)
    set(FPSDK_USE_PROJ ON)
    find_package(fpsdk_common REQUIRED)
endif()
if(NOT TARGET ffxx)
    find_package(ffxx REQUIRED)
endif()

find_package(PkgConfig REQUIRED)
find_package(X11 REQUIRED)

find_package(     Boost         REQUIRED CONFIG)                     # libboost-all-dev
find_package(     ZLIB          REQUIRED)                            # zlib1g-dev
find_package(     glfw3         REQUIRED)                            # libglfw3-dev
find_package(     glm           REQUIRED)                            # libglm-dev
find_package(     nlohmann_json REQUIRED)                            # nlohmann-json3-dev
pkg_search_module(gl            REQUIRED IMPORTED_TARGET gl)         # libgl-dev
pkg_search_module(ft2           REQUIRED IMPORTED_TARGET freetype2)  # libfreetype-dev
pkg_search_module(curl          REQUIRED IMPORTED_TARGET libcurl)    # libcurl4-gnutls-dev

message(STATUS "ff: Boost         ${Boost_VERSION}")
message(STATUS "ff: zlib          ${ZLIB_VERSION}")
message(STATUS "ff: glfw3         ${glfw3_VERSION}")
message(STATUS "ff: glm           ${glm_VERSION}")
message(STATUS "ff: nlohmann_json ${nlohmann_json_VERSION}")
message(STATUS "ff: gl            ${gl_VERSION}")
message(STATUS "ff: ft2           ${ft2_VERSION}")
message(STATUS "ff: curl          ${curl_VERSION}")


# EXECUTABLES ==========================================================================================================

# https://gitlab.kitware.com/cmake/community/-/wikis/doc/cmake/RPATH-handling#recommendations
list(APPEND CMAKE_INSTALL_RPATH $ORIGIN)
list(APPEND CMAKE_INSTALL_RPATH $ORIGIN/../lib)

file(GLOB SRC_FILES
    ../3rdparty/stuff/*.c
    ../3rdparty/stb/*.c
    ../3rdparty/stuff/*.cpp
    ../3rdparty/imgui/*.cpp
    ../3rdparty/implot/*.cpp
    # ../3rdparty/implot3d/*.cpp
    *.cpp
    stuff/*.cpp
    gui/*.cpp
    gui_msg/*.cpp
    gui_widget/*.cpp
    gui_win/*.cpp
    gui_win_data/*.cpp
    gui_win_input/*.cpp
)

set(SRC_DIRS "${PROJECT_SOURCE_DIR}/../3rdparty/fonts;${PROJECT_SOURCE_DIR}/input")
foreach(_file ${SRC_FILES})
    get_filename_component(_incdir ${_file} DIRECTORY)
    list(APPEND SRC_DIRS ${_incdir})
endforeach()
list(REMOVE_DUPLICATES SRC_DIRS)
message(STATUS "ff: SRC_DIRS: ${SRC_DIRS}")

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${SRC_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    PRIVATE
        ubloxcfg
        ffxx
        fpsdk_common
        X11
        Boost::boost
        ZLIB::ZLIB
        glfw
        glm::glm
        nlohmann_json::nlohmann_json
        PkgConfig::gl
        PkgConfig::ft2
        PkgConfig::curl
)


# INSTALL ==============================================================================================================

include(GNUInstallDirs) # Provides nice relative paths wrt CMAKE_INSTALL_PREFIX
set(PROJECT_RUNTIME_DIR ${CMAKE_INSTALL_FULL_BINDIR})
set(PROJECT_LIBRARY_DIR ${CMAKE_INSTALL_FULL_LIBDIR})
set(PROJECT_INCLUDE_DIR ${CMAKE_INSTALL_FULL_INCLUDEDIR}/${PROJECT_NAME})
set(PROJECT_DATA_DIR    ${CMAKE_INSTALL_FULL_DATAROOTDIR}/${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-targets
    LIBRARY DESTINATION ${PROJECT_LIBRARY_DIR}
    RUNTIME DESTINATION ${PROJECT_RUNTIME_DIR}
)

# ======================================================================================================================
