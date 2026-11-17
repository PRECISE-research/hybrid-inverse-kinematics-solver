# Copies the DLLs an executable imports from OpenSim's bin/ into the executable's
# own directory. Run as a post-build step (cmake -P), so the build-tree .exe finds
# the OpenSim it was linked against instead of whichever one comes first on PATH.
#
# Inputs (-D): EXE, the built executable; SEARCH_DIR, OpenSim's bin/.
#
# Uses the same dependency scan and exclusions as the install rule in
# src/CMakeLists.txt, so the build tree and the install tree carry the same DLLs.

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${EXE}"
    RESOLVED_DEPENDENCIES_VAR _resolved
    UNRESOLVED_DEPENDENCIES_VAR _unresolved
    DIRECTORIES "${SEARCH_DIR}"
    PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
    POST_EXCLUDE_REGEXES "[Ss]ystem32" "[Ss]ys[Ww][Oo][Ww]64"
)

get_filename_component(_dest "${EXE}" DIRECTORY)
foreach(_dll IN LISTS _resolved)
    get_filename_component(_name "${_dll}" NAME)
    # Copies only when the file differs, so rebuilds stay quick.
    file(COPY_FILE "${_dll}" "${_dest}/${_name}" ONLY_IF_DIFFERENT)
endforeach()

if(_unresolved)
    message(WARNING "Could not locate these DLLs for ${EXE}: ${_unresolved}")
endif()
