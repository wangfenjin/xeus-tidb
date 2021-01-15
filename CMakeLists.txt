############################################################################
# Copyright (c) 2020, QuantStack and xeus-tidb contributors                #
#                                                                          #
#                                                                          #
# Distributed under the terms of the BSD 3-Clause License.                 #
#                                                                          #
# The full license is in the file LICENSE, distributed with this software. #
############################################################################

cmake_minimum_required(VERSION 3.4.3)
project(xeus-tidb)

set(XEUS_TIDB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(XEUS_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)

set(CMAKE_FIND_ROOT_PATH ${CMAKE_PREFIX_PATH})
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_FRAMEWORK NEVER)
set(CMAKE_FIND_APPBUNDLE NEVER)
set(CMAKE_PROGRAM_PATH ${CMAKE_PREFIX_PATH})
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Versioning
# ==========

file(STRINGS "${XEUS_TIDB_INCLUDE_DIR}/xeus-tidb/xeus_tidb_config.hpp" xtidb_version_defines
     REGEX "#define XTIDB_VERSION_(MAJOR|MINOR|PATCH)")
foreach(ver ${xtidb_version_defines})
    if(ver MATCHES "#define XTIDB_VERSION_(MAJOR|MINOR|PATCH) +([^ ]+) *$")
        set(XTIDB_VERSION_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE INTERNAL "")
    endif()
endforeach()
set(${PROJECT_NAME}_VERSION
${XTIDB_VERSION_MAJOR}.${XTIDB_VERSION_MINOR}.${XTIDB_VERSION_PATCH})
message(STATUS "Building xeus-tidb v${${PROJECT_NAME}_VERSION}")

# Configuration
# =============

include(GNUInstallDirs)

# We generate the kernel.json file, given the installation prefix and the executable name
configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels/xtidb/kernel.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels/xtidb/kernel.json"
)

# Build options
# =============

OPTION(XTIDB_DISABLE_ARCH_NATIVE "disable -march=native flag" OFF)
OPTION(XTIDB_DISABLE_TUNE_GENERIC "disable -mtune=generic flag" OFF)

OPTION(XTIDB_BUILD_STATIC "Build xeus-tidb static library" ON)
OPTION(XTIDB_BUILD_SHARED "Split xtidb build into executable and library" ON)
OPTION(XTIDB_BUILD_XTIDB_EXECUTABLE "Build the xtidb executable" ON)

OPTION(XTIDB_USE_SHARED_XEUS "Link xtidb with the xeus shared library (instead of the static library)" ON)
OPTION(XTIDB_USE_SHARED_XEUS_TIDB "Link xtidb with the xeus-tidb shared library (instead of the static library)" ON)

OPTION(XTIDB_DOWNLOAD_GTEST "build gtest from downloaded sources" OFF)
OPTION(XTIDB_BUILD_TESTS "xeus-tidb test suite" OFF)

OPTION(CMAKE_USE_WIN32_THREADS_INIT "using WIN32 threads" ON)
OPTION(gtest_disable_pthreads "Disable uses of pthreads in gtest." ON)


# Dependencies
# ============

# Be sure to use recent versions
set(xeus_REQUIRED_VERSION 0.23.10)

find_package(xeus ${xeus_REQUIRED_VERSION} REQUIRED)
find_package(xvega)

find_package(Threads REQUIRED)

find_package(Soci REQUIRED MODULE)
find_package(mysql)

# Target and link
# ===============

string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)

# xeus-tidb source directory
set(XEUS_TIDB_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# xeus-tidb source files
set(XEUS_TIDB_SRC
    ${XEUS_TIDB_SRC_DIR}/xeus_tidb_interpreter.cpp
)

set(XEUS_TIDB_HEADERS
    include/xeus-tidb/soci_handler.hpp
    include/xeus-tidb/xeus_tidb_config.hpp
    include/xeus-tidb/xeus_tidb_interpreter.hpp
)

set(XTIDB_SRC ${XEUS_TIDB_SRC_DIR}/main.cpp)

# Targets and link - Macros
# =========================

include(CheckCXXCompilerFlag)

set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib:${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

macro(xtidb_set_common_options target_name)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
        CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR
        CMAKE_CXX_COMPILER_ID MATCHES "Intel")

        target_compile_options(${target_name} PUBLIC -Wunused-parameter -Wextra -Wreorder)
    
        # Mtune generic/native
        if (XTIDB_DISABLE_ARCH_NATIVE AND NOT XTIDB_DISABLE_TUNE_GENERIC)
            target_compile_options(${target_name} PUBLIC -mtune=generic)
        elseif (XTIDB_DISABLE_TUNE_GENERIC)
        else ()
            target_compile_options(${target_name} PUBLIC -march=native)
        endif ()

        # C++14 flag, setting this flag is failing on Clang
        if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            CHECK_CXX_COMPILER_FLAG("-std=c++14" HAS_CPP14_FLAG)
            if (HAS_CPP14_FLAG)
            target_compile_features(${target_name} PRIVATE cxx_std_14)
            else ()
                message(FATAL_ERROR "Unsupported compiler -- xeus-tidb requires C++14 support!")
            endif ()
        else ()
            target_compile_features(${target_name} PRIVATE cxx_std_14)
        endif ()

        # Enable link time optimization and set the default symbol
        # visibility to hidden (very important to obtain small binaries)
        if (NOT ${U_CMAKE_BUILD_TYPE} MATCHES DEBUG)
            # Check for Link Time Optimization support
            # (GCC/Clang)
            CHECK_CXX_COMPILER_FLAG("-flto" HAS_LTO_FLAG)
            if (HAS_LTO_FLAG)
                target_compile_options(${target_name} PUBLIC -flto)
            endif ()

            # Avoids removing symbols from the static library
            CHECK_CXX_COMPILER_FLAG("-ffat-lto-objects" HAS_FATLTO_FLAG)
            if (${linkage_upper} MATCHES "STATIC" AND  HAS_FATLTO_FLAG)
                message(STATUS "ENABLE FAT LTO OBJECTS")
                target_compile_options(${target_name} PUBLIC -ffat-lto-objects)
            endif ()

            # Intel equivalent to LTO is called IPO
            if (CMAKE_CXX_COMPILER_ID MATCHES "Intel")
                CHECK_CXX_COMPILER_FLAG("-ipo" HAS_IPO_FLAG)
                if (HAS_IPO_FLAG)
                    target_compile_options(${target_name} PUBLIC -ipo)
                endif ()
            endif ()
        endif ()
    endif ()

    if (APPLE)
        set_target_properties(${target_name} PROPERTIES
            MACOSX_RPATH ON
        )
    else ()
        set_target_properties(${target_name} PROPERTIES
            BUILD_WITH_INSTALL_RPATH 1
            SKIP_BUILD_RPATH FALSE
        )
    endif ()

    set_target_properties(${target_name} PROPERTIES
        INSTALL_RPATH_USE_LINK_PATH TRUE
    )
endmacro()


# Common macro for shared and static library xeus-tidb
macro(xtidb_create_target target_name linkage output_name)
    string(TOUPPER "${linkage}" linkage_upper)

    if (NOT ${linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library linkage: ${linkage}")
    endif ()

    add_library(${target_name} ${linkage_upper} ${XEUS_TIDB_SRC} ${XEUS_TIDB_HEADERS})
    xtidb_set_common_options(${target_name})

    set_target_properties(${target_name} PROPERTIES
                          PUBLIC_HEADER "${XEUS_TIDB_HEADERS}"
                          # COMPILE_DEFINITIONS "XEUS_TIDB_EXPORTS"
                          PREFIX ""
                          VERSION ${${PROJECT_NAME}_VERSION}
                          SOVERSION ${XTIDB_VERSION_MAJOR}
                          OUTPUT_NAME "lib${output_name}")

    target_compile_definitions(${target_name} PUBLIC "XEUS_TIDB_EXPORTS")

    target_include_directories(${target_name}
                               PUBLIC
                               ${XTIDB_INCLUDE_DIRS}
                               $<BUILD_INTERFACE:${XEUS_TIDB_INCLUDE_DIR}>
                               $<INSTALL_INTERFACE:include>)

    if (XTIDB_USE_SHARED_XEUS)
        set(XTIDB_XEUS_TARGET xeus)
    else ()
        set(XTIDB_XEUS_TARGET xeus-static)
    endif ()

    target_link_libraries(${target_name} PUBLIC 
      ${XTIDB_XEUS_TARGET}
      xvega
      ${SOCI_LIBRARY}
    )

    # find_package(Threads) # TODO: add Threads as a dependence of xeus-static?
    target_link_libraries(${target_name} PRIVATE ${CMAKE_THREAD_LIBS_INIT})
endmacro()

# xeus-tidb
# ===========

set(XEUS_TIDB_TARGETS "")

if (XTIDB_BUILD_SHARED)
    # Build libraries
    xtidb_create_target(xeus-tidb SHARED xeus-tidb)
    list(APPEND XEUS_TIDB_TARGETS xeus-tidb)
endif ()

if (XTIDB_BUILD_STATIC)
    # On Windows, a static library should use a different output name
    # to avoid the conflict with the import library of a shared one.
    if (CMAKE_HOST_WIN32)
        xtidb_create_target(xeus-tidb-static STATIC xeus-tidb-static)
    else ()
        xtidb_create_target(xeus-tidb-static STATIC xeus-tidb)
    endif ()
    list(APPEND XEUS_TIDB_TARGETS xeus-tidb-static)
endif ()

# xtidb
# =======

if (XTIDB_BUILD_XTIDB_EXECUTABLE)
    add_executable(xtidb ${XTIDB_SRC})
    set_target_properties(xtidb PROPERTIES ENABLE_EXPORTS 1)

    xtidb_set_common_options(xtidb)

    if (XTIDB_USE_SHARED_XEUS_TIDB)
        target_link_libraries(xtidb PRIVATE xeus-tidb)

        if(CMAKE_DL_LIBS)
            target_link_libraries(xtidb PRIVATE ${CMAKE_DL_LIBS} util)
        endif()
    else ()
        target_link_libraries(xtidb PRIVATE xeus-tidb-static)
    endif()
endif()


# Tests
# =====

if(XTIDB_DOWNLOAD_GTEST OR GTEST_SRC_DIR)
    set(XTIDB_BUILD_TESTS ON)
endif()


if(XTIDB_BUILD_TESTS)
    add_subdirectory(test)
endif()

# Installation
# ============

include(CMakePackageConfigHelpers)

set(XEUS_TIDB_CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE STRING "install path for xeus-tidbConfig.cmake")

# Install xeus-tidb and xtidb
if (XTIDB_BUILD_SHARED OR XTIDB_BUILD_STATIC)
    install(TARGETS ${XEUS_TIDB_TARGETS}
            EXPORT ${PROJECT_NAME}-targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xeus-tidb)

    # Makes the project importable from the build directory
    export(EXPORT ${PROJECT_NAME}-targets
           FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake")
endif ()

# Install xtidb
if (XTIDB_BUILD_XTIDB_EXECUTABLE)
    install(TARGETS xtidb
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # Configuration and data directories for jupyter and xeus_tidb
    set(XJUPYTER_DATA_DIR "share/jupyter" CACHE STRING "Jupyter data directory")

    # Install xtidb Jupyter kernelspecs
    set(XTIDB_KERNELSPEC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels)
    install(DIRECTORY ${XTIDB_KERNELSPEC_DIR}
            DESTINATION ${XJUPYTER_DATA_DIR}
            PATTERN "*.in" EXCLUDE)


    # Extra path for installing Jupyter kernelspec
    if (XEXTRA_JUPYTER_DATA_DIR)
        install(DIRECTORY ${XTIDB_KERNELSPEC_DIR}
                DESTINATION ${XEXTRA_JUPYTER_DATA_DIR}
                PATTERN "*.in" EXCLUDE)
    endif(XEXTRA_JUPYTER_DATA_DIR)
endif()

# Configure 'xeus-tidbConfig.cmake' for a build tree
set(XEUS_TIDB_CONFIG_CODE "####### Expanded from \@XEUS_TIDB_CONFIG_CODE\@ #######\n")
set(XEUS_TIDB_CONFIG_CODE "${XEUS_TIDB_CONFIG_CODE}set(CMAKE_MODULE_PATH \"${CMAKE_CURRENT_SOURCE_DIR}/cmake;\${CMAKE_MODULE_PATH}\")\n")
set(XEUS_TIDB_CONFIG_CODE "${XEUS_TIDB_CONFIG_CODE}##################################################")
configure_package_config_file(${PROJECT_NAME}Config.cmake.in
                              "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                              INSTALL_DESTINATION ${PROJECT_BINARY_DIR})

# Configure 'xeus-tidbConfig.cmake' for an install tree
set(XEUS_TIDB_CONFIG_CODE "")
configure_package_config_file(${PROJECT_NAME}Config.cmake.in
                              "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}Config.cmake"
                              INSTALL_DESTINATION ${XEUS_TIDB_CMAKECONFIG_INSTALL_DIR})

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
                                 VERSION ${${PROJECT_NAME}_VERSION}
                                 COMPATIBILITY AnyNewerVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}Config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
              DESTINATION ${XEUS_TIDB_CMAKECONFIG_INSTALL_DIR})

if (XTIDB_BUILD_SHARED)
    install(EXPORT ${PROJECT_NAME}-targets
            FILE ${PROJECT_NAME}Targets.cmake
            DESTINATION ${XEUS_TIDB_CMAKECONFIG_INSTALL_DIR})
endif ()
