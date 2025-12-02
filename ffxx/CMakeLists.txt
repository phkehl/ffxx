########################################################################################################################
#
# flipflip's c++ library (ffxx)
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

project(ffxx
    LANGUAGES CXX
    VERSION ${FF_VERSION_NUMBER}
    DESCRIPTION "flipflip's c++ library (ffxx)"
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

find_package(PkgConfig REQUIRED)
find_package(Boost         REQUIRED CONFIG)                     # libboost-all-dev
find_package(nlohmann_json REQUIRED)                            # nlohmann-json3-dev
find_package(Eigen3        REQUIRED)                            # libeigen3-dev
pkg_search_module(OpenSSL REQUIRED IMPORTED_TARGET openssl)


message(STATUS "ff: Boost         ${Boost_VERSION}")
message(STATUS "ff: nlohmann_json ${nlohmann_json_VERSION}")

# SHARED LIBRARY =======================================================================================================

file(GLOB CPP_FILES *.cpp stream/*.cpp)
file(GLOB HPP_FILES *.hpp)

add_library(${PROJECT_NAME} SHARED ${CPP_FILES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/..>
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Eigen3::Eigen
    PRIVATE
       ubloxcfg
       fpsdk_common
       nlohmann_json::nlohmann_json
       Boost::boost
       PkgConfig::OpenSSL
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        VERSION ${PROJECT_VERSION}
)


# INSTALL ==============================================================================================================

include(GNUInstallDirs) # Provides nice relative paths wrt CMAKE_INSTALL_PREFIX
set(PROJECT_RUNTIME_DIR ${CMAKE_INSTALL_FULL_BINDIR})
set(PROJECT_LIBRARY_DIR ${CMAKE_INSTALL_FULL_LIBDIR})
set(PROJECT_INCLUDE_DIR ${CMAKE_INSTALL_FULL_INCLUDEDIR}/${PROJECT_NAME})
set(PROJECT_DATA_DIR    ${CMAKE_INSTALL_FULL_DATAROOTDIR}/${PROJECT_NAME})
set(PROJECT_DOC_DIR     ${CMAKE_INSTALL_FULL_DOCDIR})

# Headers
install(FILES ${HPP_FILES}
    DESTINATION ${PROJECT_INCLUDE_DIR}
)

# Library
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-targets
    LIBRARY DESTINATION ${PROJECT_LIBRARY_DIR}
)

# CMake target config
install(EXPORT ${PROJECT_NAME}-targets
    NAMESPACE ${PROJECT_NAMESPACE_PREFIX}
    FILE ${PROJECT_NAME}-targets.cmake
    DESTINATION lib/cmake/${PROJECT_NAME}
)

# CMake config
include(CMakePackageConfigHelpers)
set(TARGET1 ${PROJECT_NAME})
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
    INSTALL_DESTINATION lib/cmake/${PROJECT_NAME}
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
    VERSION "${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}"
    COMPATIBILITY AnyNewerVersion
)
install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
    DESTINATION lib/cmake/${PROJECT_NAME}
)

# pkg-config config
include(CMakePackageConfigHelpers)
set(TARGET1 ${PROJECT_NAME})
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
    INSTALL_DESTINATION lib/cmake/${PROJECT_NAME}
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)
install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
    DESTINATION lib/pkgconfig
)

########################################################################################################################
