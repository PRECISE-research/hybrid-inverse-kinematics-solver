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
# =============================================================================

# 3.21 is the floor for install(TARGETS ... RUNTIME_DEPENDENCIES), which is how
# the OpenSim/SimTK DLLs get bundled next to the .exe on Windows. The range's
# upper bound opts in to current policy defaults without tripping deprecation
# warnings on newer CMake.
cmake_minimum_required(VERSION 3.21...3.31)

# This VERSION is the single source of truth: it is what the tool prints in its
# banner, via the PROJECT_*_VERSION macros defined further down. Always give all
# three components -- a two-component VERSION leaves PROJECT_VERSION_PATCH empty
# and the banner silently reads "0.2." instead of "0.2.0".
project(hybrid-inverse-kinematics-solver
    VERSION 0.2.0
    DESCRIPTION "Hybrid marker/orientation/coordinate inverse kinematics solver for OpenSim"
    LANGUAGES CXX
)

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
# -----------------------------------------------------------------------------

# Defines the osim*/SimTK imported targets. They carry their own
# INTERFACE_INCLUDE_DIRECTORIES, so <OpenSim.h> resolves purely through
# target_link_libraries below -- no include_directories() needed.
find_package(OpenSim REQUIRED)

# find_package(... REQUIRED), not include(cmake/FindRapidXML.cmake): only the
# former sets RapidXML_FIND_REQUIRED, which is what makes the module's
# "RapidXML was NOT found" FATAL_ERROR actually fire. Under a bare include() a
# missing RapidXML surfaced much later as a confusing link error.
find_package(RapidXML REQUIRED)

# -----------------------------------------------------------------------------
# The one and only target
# -----------------------------------------------------------------------------

add_executable(hybrid-inverse-kinematics-solver
    src/hybridInverseKinematicsSolver.cpp
)

target_link_libraries(hybrid-inverse-kinematics-solver PRIVATE
    ${OpenSim_LIBRARIES}
    RapidXML::RapidXML
)

# <filesystem>. Extensions off so GCC gets -std=c++17 rather than -std=gnu++17.
target_compile_features(hybrid-inverse-kinematics-solver PRIVATE cxx_std_17)
set_target_properties(hybrid-inverse-kinematics-solver PROPERTIES CXX_EXTENSIONS OFF)

# printHeader() does std::string(PROJECT_MAJOR_VERSION), so these must arrive as
# *string literals*, not bare numbers. Writing the quotes inside the value is the
# portable way to say that: CMake re-escapes them per generator (a bare
# PROJECT_MAJOR_VERSION="0" entry in <PreprocessorDefinitions> for Visual Studio,
# -DPROJECT_MAJOR_VERSION=\"0\" for Ninja/Makefiles).
target_compile_definitions(hybrid-inverse-kinematics-solver PRIVATE
    PROJECT_MAJOR_VERSION="${PROJECT_VERSION_MAJOR}"
    PROJECT_MINOR_VERSION="${PROJECT_VERSION_MINOR}"
    PROJECT_PATCH_VERSION="${PROJECT_VERSION_PATCH}"
)

# Silence the deprecation noise OpenSim's headers generate, per compiler family.
# C4996 is an MSVC-only spelling. Target-scoped, so it never leaks elsewhere.
target_compile_options(hybrid-inverse-kinematics-solver PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/wd4996>
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-deprecated-declarations>
)

# GCC < 9 keeps std::filesystem in a separate library.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(hybrid-inverse-kinematics-solver PRIVATE stdc++fs)
endif()

# Quality of life: F5 in Visual Studio finds the OpenSim DLLs without putting
# them on the system PATH. Silently ignored by every other generator.
set_property(TARGET hybrid-inverse-kinematics-solver PROPERTY
    VS_DEBUGGER_ENVIRONMENT "PATH=${OpenSim_BIN_DIR};%PATH%")

# -----------------------------------------------------------------------------
# Install
# -----------------------------------------------------------------------------

# Defines CMAKE_INSTALL_BINDIR. The original build referenced this without ever
# including this module: it expanded to nothing, CMake saw a valueless
# DESTINATION and fell back to its built-in "bin". Right answer, by accident.
include(GNUInstallDirs)

if(WIN32)
    # Copy the OpenSim/SimTK/BLAS DLLs the .exe actually imports into the same
    # bin/, so the install tree runs on a machine with no OpenSim installation.
    # OpenSim_BIN_DIR comes from OpenSimConfig.cmake -- more robust than deriving
    # it by chopping "/cmake" off OpenSim_DIR, which assumed one install layout.
    #
    # That directory also ships msvcp140/vcruntime140/concrt140, and it is
    # searched before the system path, so the MSVC runtime is bundled from there
    # rather than from System32. That is why excluding System32 below does not
    # leave the install tree needing the VC++ redistributable.
    install(TARGETS hybrid-inverse-kinematics-solver
        RUNTIME_DEPENDENCIES
            DIRECTORIES "${OpenSim_BIN_DIR}"
            PRE_EXCLUDE_REGEXES
                "api-ms-"                 # Windows API sets
                "ext-ms-"
            POST_EXCLUDE_REGEXES
                "[Ss]ystem32"             # OS DLLs; case-insensitive on purpose
                "[Ss]ys[Ww][Oo][Ww]64"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Applications
    )
else()
    # No bundling off Windows: GET_RUNTIME_DEPENDENCIES would happily copy libc
    # and libstdc++, and the exclude regexes above are Windows path names. Rely
    # on the loader (RPATH / LD_LIBRARY_PATH) instead.
    install(TARGETS hybrid-inverse-kinematics-solver
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Applications
    )
endif()
