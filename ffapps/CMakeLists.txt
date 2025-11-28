########################################################################################################################
#
# flipflip's c++ apps (ffapps)
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

project(ffapps
    LANGUAGES CXX
    VERSION ${FF_VERSION_NUMBER}
    DESCRIPTION "flipflip's c++ apps (ffapps)"
)


# COMPILER SETUP =======================================================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror \
    -Wshadow -Wunused-parameter -Wformat -Wpointer-arith -Woverloaded-virtual")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
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
    find_package(fpsdk_common REQUIRED)
endif()
if(NOT TARGET ffxx)
    find_package(ffxx REQUIRED)
endif()

find_package(Boost         REQUIRED CONFIG)                     # libboost-all-dev
find_package(nlohmann_json REQUIRED)                            # nlohmann-json3-dev

message(STATUS "ff: Boost         ${Boost_VERSION}")
message(STATUS "ff: nlohmann_json ${nlohmann_json_VERSION}")


# SHARED LIBRARY =======================================================================================================

# https://gitlab.kitware.com/cmake/community/-/wikis/doc/cmake/RPATH-handling#recommendations
list(APPEND CMAKE_INSTALL_RPATH $ORIGIN)
list(APPEND CMAKE_INSTALL_RPATH $ORIGIN/../lib)


# EXECUTABLES ==========================================================================================================

file(GLOB STREAMTOOL_CPP_FILES streamtool*.cpp)
add_executable(streamtool ${STREAMTOOL_CPP_FILES} ${COMMON_CPP_FILES})
target_link_libraries(streamtool
    PRIVATE
        fpsdk_common
        ffxx
)

# ----------------------------------------------------------------------------------------------------------------------

add_custom_command(
    OUTPUT streammux_html.hpp
    COMMAND xxd -i -C -c 32 -n streammux_html ${PROJECT_SOURCE_DIR}/streammux.html streammux_html.hpp
    DEPENDS ${PROJECT_SOURCE_DIR}/streammux.html
)
add_custom_command(
    OUTPUT streammux_css.hpp
    COMMAND xxd -i -C -c 32 -n streammux_css ${PROJECT_SOURCE_DIR}/streammux.css streammux_css.hpp
    DEPENDS ${PROJECT_SOURCE_DIR}/streammux.html
)
add_custom_command(
    OUTPUT streammux_js.hpp
    COMMAND xxd -i -C -c 32 -n streammux_js ${PROJECT_SOURCE_DIR}/streammux.js streammux_js.hpp
    DEPENDS ${PROJECT_SOURCE_DIR}/streammux.html
)
add_custom_target(streammux-generated DEPENDS streammux_html.hpp streammux_css.hpp streammux_js.hpp)

file(GLOB STREAMMUX_CPP_FILES streammux*.cpp)
add_executable(streammux ${STREAMMUX_CPP_FILES} ${COMMON_CPP_FILES})
target_link_libraries(streammux
    PRIVATE
        fpsdk_common
        ffxx
        Boost::boost
        nlohmann_json::nlohmann_json
)
target_include_directories(streammux PRIVATE ${PROJECT_BINARY_DIR})
add_dependencies(streammux streammux-generated)

# GENERATED HELP =======================================================================================================

add_custom_command(
    OUTPUT ${PROJECT_BINARY_DIR}/streamtool.txt
    COMMAND streamtool -h > ${PROJECT_BINARY_DIR}/streamtool.txt
    DEPENDS streamtool ${STREAMTOOL_CPP_FILES}
)

add_custom_target(streamtool-help ALL DEPENDS ${PROJECT_BINARY_DIR}/streamtool.txt)

add_custom_command(
    OUTPUT ${PROJECT_BINARY_DIR}/streammux.txt
    COMMAND streammux -h > ${PROJECT_BINARY_DIR}/streammux.txt
    DEPENDS streammux ${STREAMMUX_CPP_FILES}
)

add_custom_target(streammux-help ALL DEPENDS ${PROJECT_BINARY_DIR}/streammux.txt)


# INSTALL ==============================================================================================================

include(GNUInstallDirs) # Provides nice relative paths wrt CMAKE_INSTALL_PREFIX
set(PROJECT_RUNTIME_DIR ${CMAKE_INSTALL_FULL_BINDIR})
set(PROJECT_LIBRARY_DIR ${CMAKE_INSTALL_FULL_LIBDIR})
set(PROJECT_INCLUDE_DIR ${CMAKE_INSTALL_FULL_INCLUDEDIR}/${PROJECT_NAME})
set(PROJECT_DATA_DIR    ${CMAKE_INSTALL_FULL_DATAROOTDIR}/${PROJECT_NAME})
set(PROJECT_DOC_DIR     ${CMAKE_INSTALL_FULL_DOCDIR})


install(TARGETS streamtool streammux
    EXPORT ${PROJECT_NAME}-targets
    LIBRARY DESTINATION ${PROJECT_LIBRARY_DIR}
    RUNTIME DESTINATION ${PROJECT_RUNTIME_DIR}
)

install(PROGRAMS streamtool-plot
    DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})

# Documentation
install(
    FILES
        ${PROJECT_SOURCE_DIR}/README.md
        ${PROJECT_SOURCE_DIR}/LICENSE
        ${PROJECT_BINARY_DIR}/streamtool.txt
        ${PROJECT_BINARY_DIR}/streammux.txt
    DESTINATION ${PROJECT_DOC_DIR}
)


########################################################################################################################
