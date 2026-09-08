# The one and only target: a single translation unit with main(). There is no
# library here to link against, which is why the tests drive the CLI as a black
# box rather than unit-testing internals.

add_executable(hybrid-inverse-kinematics-solver
    hybridInverseKinematicsSolver.cpp
)

# OpenSim and RapidXML are found in the top-level CMakeLists.
target_link_libraries(hybrid-inverse-kinematics-solver PRIVATE
    ${OpenSim_LIBRARIES}
    RapidXML::RapidXML
)

# Solution Explorer grouping (see USE_FOLDERS at the top level).
set_target_properties(hybrid-inverse-kinematics-solver PROPERTIES FOLDER "Executables")

# <filesystem>. Extensions off so GCC gets -std=c++17 rather than -std=gnu++17.
target_compile_features(hybrid-inverse-kinematics-solver PRIVATE cxx_std_17)
set_target_properties(hybrid-inverse-kinematics-solver PROPERTIES CXX_EXTENSIONS OFF)

# printHeader() does std::string(PROJECT_MAJOR_VERSION), so these must arrive as
# string literals, not bare numbers. Writing the quotes inside the value is the
# portable way to say that: CMake re-escapes them per generator, giving a bare
# PROJECT_MAJOR_VERSION="0" entry in <PreprocessorDefinitions> for Visual Studio
# and an escaped -D form for Ninja and Makefiles. The values come from
# project(VERSION) at the top level.
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
#
# CMAKE_INSTALL_BINDIR comes from GNUInstallDirs, included at the top level.
# -----------------------------------------------------------------------------

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
