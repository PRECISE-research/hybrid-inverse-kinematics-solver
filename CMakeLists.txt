# =============================================================================
# hybrid-inverse-kinematics-solver
#
# A single command-line tool that runs an OpenSim InverseKinematicsSolver over
# any combination of marker, orientation and coordinate data described by an XML
# setup file.
#
# Neither dependency is discoverable without help:
#   * OpenSim 4.4   -- -DOpenSim_DIR=<opensim-root>/cmake
#   * RapidXML 1.13 -- -DRapidXML_ROOT=<directory containing rapidxml.hpp>
# CMakePresets.json holds the known-good values for this lab's machines.
#
# This file does project-wide setup only. The executable and its install rules
# live in src/CMakeLists.txt; the test suite lives in tests/CMakeLists.txt.
# =============================================================================

# 3.21 is the floor for install(TARGETS ... RUNTIME_DEPENDENCIES), which is how
# the OpenSim/SimTK DLLs get bundled next to the .exe on Windows. The range's
# upper bound opts in to current policy defaults without tripping deprecation
# warnings on newer CMake.
cmake_minimum_required(VERSION 3.21...3.31)

# This VERSION is the single source of truth: it is what the tool prints in its
# banner, via the PROJECT_*_VERSION macros set in src/CMakeLists.txt. Always give
# all three components -- a two-component VERSION leaves PROJECT_VERSION_PATCH
# empty and the banner silently reads "0.2." instead of "0.2.0".
project(hybrid-inverse-kinematics-solver
    VERSION 1.0.0
    DESCRIPTION "Hybrid marker/orientation/coordinate inverse kinematics solver for OpenSim"
    LANGUAGES CXX
)

# Group targets into folders in IDEs that support it, so Visual Studio's Solution
# Explorer shows Executables/ and Tests/ rather than a flat list mixed in with
# ALL_BUILD and ZERO_CHECK. On by default from CMake 3.26 (CMP0143), set
# explicitly so it survives someone lowering the minimum.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Let find_package() see our cmake/FindRapidXML.cmake.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Sensible default for single-config generators. OpenSim is unusably slow in a
# Debug build, and the Windows OpenSim 4.4 package ships release import
# libraries only (there is no OpenSimTargets-debug.cmake).
get_property(_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _is_multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

# -----------------------------------------------------------------------------
# Dependencies
#
# Found once here so both subdirectories can use them: src/ links them, tests/
# uses OpenSim_BIN_DIR to put the right DLLs on PATH.
# -----------------------------------------------------------------------------

# Defines the osim*/SimTK imported targets. They carry their own
# INTERFACE_INCLUDE_DIRECTORIES, so <OpenSim.h> resolves purely through
# target_link_libraries -- no include_directories() needed anywhere.
find_package(OpenSim REQUIRED)

# find_package(... REQUIRED), not include(cmake/FindRapidXML.cmake): only the
# former sets RapidXML_FIND_REQUIRED, which is what makes the module's
# "RapidXML was NOT found" FATAL_ERROR actually fire. Under a bare include() a
# missing RapidXML surfaced much later as a confusing link error.
find_package(RapidXML REQUIRED)

# Defines CMAKE_INSTALL_BINDIR, used by the install rules in src/. The original
# build referenced that variable without ever including this module: it expanded
# to nothing, CMake saw a valueless DESTINATION and fell back to its built-in
# "bin". Right answer, by accident. Included here, before add_subdirectory(src).
include(GNUInstallDirs)

# -----------------------------------------------------------------------------
# Subdirectories
#
# src/ before tests/: the test suite refers to the solver target, and
# add_dependencies() needs it to already exist.
# -----------------------------------------------------------------------------

add_subdirectory(src)

# CMAKE_FOLDER supplies the default FOLDER for every target created from here
# down, which covers both CTest's dashboard targets (Continuous, Experimental,
# Nightly...) and our own helpers in tests/, without naming each one.
set(CMAKE_FOLDER "Tests")

# Defines BUILD_TESTING (ON by default). Configure with -DBUILD_TESTING=OFF to
# skip the suite, e.g. on a machine without Python.
include(CTest)

# include(CTest) hardcodes FOLDER "CTestDashboardTargets" on the targets it
# creates, which beats CMAKE_FOLDER, so they would sit at the top of the
# solution. Re-home them under Tests/ by looking for that folder rather than
# listing target names, which change between CMake versions.
get_property(_ctest_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(_target IN LISTS _ctest_targets)
    get_target_property(_folder ${_target} FOLDER)
    if(_folder STREQUAL "CTestDashboardTargets")
        set_target_properties(${_target} PROPERTIES FOLDER "Tests/CTest")
    endif()
endforeach()

if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
        add_subdirectory(tests)
    else()
        message(STATUS "Python3 not found -- tests disabled")
    endif()
endif()

unset(CMAKE_FOLDER)
