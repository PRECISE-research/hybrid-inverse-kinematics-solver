# Tests are driven by a small Python script rather than a C++ test framework.
# The tool's whole interface is a command line plus a .sto file, so testing it as
# a black box needs no framework and no restructuring of the single translation
# unit -- and it exercises exactly what a user runs.

set(TEST_DRIVER  "${CMAKE_CURRENT_SOURCE_DIR}/run_test.py")
set(EXAMPLES_DIR "${CMAKE_SOURCE_DIR}/examples")
set(SOLVER       $<TARGET_FILE:hybrid-inverse-kinematics-solver>)

# -----------------------------------------------------------------------------
# Golden-file regression
#
# The three examples cover the three distinct tracking paths: orientations with
# prescribed coordinates, markers combined with orientations, and markers alone.
# References were produced with OpenSim 4.4; see run_test.py on regenerating.
# -----------------------------------------------------------------------------
set(GOLDEN_EXAMPLES imu-orientations markerless-theia markers-cycling)

foreach(example IN LISTS GOLDEN_EXAMPLES)
    add_test(NAME golden.${example}
        COMMAND "${Python3_EXECUTABLE}" "${TEST_DRIVER}"
            --exe "${SOLVER}" --opensim-bin "${OpenSim_BIN_DIR}"
            golden
                --setup    "${EXAMPLES_DIR}/${example}/ikSetup.xml"
                --produced "${EXAMPLES_DIR}/${example}/ik_results.sto"
                --expected "${CMAKE_CURRENT_SOURCE_DIR}/expected/${example}.sto")
    # The solves are quick, but a loaded machine should not produce a red suite.
    set_tests_properties(golden.${example} PROPERTIES TIMEOUT 300)
endforeach()

# The shipped examples all use a zero <rotation>, so the degrees-to-radians
# conversion is a no-op in every one of them. This case gives it a non-zero
# angle so a regression there is actually caught.
add_test(NAME golden.rotation_90
    COMMAND "${Python3_EXECUTABLE}" "${TEST_DRIVER}"
        --exe "${SOLVER}" --opensim-bin "${OpenSim_BIN_DIR}"
        golden
            --setup    "${CMAKE_CURRENT_SOURCE_DIR}/cases/rotation_90.xml"
            --produced "${EXAMPLES_DIR}/imu-orientations/ik_rotation90.sto"
            --expected "${CMAKE_CURRENT_SOURCE_DIR}/expected/rotation_90.sto")
set_tests_properties(golden.rotation_90 PROPERTIES TIMEOUT 300)

# -----------------------------------------------------------------------------
# Error paths
#
# Every one of these was a crash, an uninitialised read, or silence before the
# guards went in. They need no solve, so the whole group runs in under a second.
# Fields: test name | case file | substring the output must contain.
# -----------------------------------------------------------------------------
set(ERROR_CASES
    "malformed_xml|bad_xml.xml|failed to parse cfg file"
    "no_root_element|no_root.xml|contains no XML root element"
    "missing_model|no_model.xml|no modelFilePath node available"
    "nothing_to_track|nothing_to_track.xml|nothing to track"
    "weight_missing_name|weight_no_name.xml|has no 'name' attribute"
    "weight_non_numeric|weight_non_numeric.xml|has a non-numeric value"
    "unrecognised_element|unknown_element.xml|unrecognised element 'applyRotation'"
    "missing_setup_file|no_such_file.xml|failed to open cfg file")

foreach(entry IN LISTS ERROR_CASES)
    string(REPLACE "|" ";" fields "${entry}")
    list(GET fields 0 case_name)
    list(GET fields 1 case_file)
    list(GET fields 2 case_text)
    add_test(NAME error.${case_name}
        COMMAND "${Python3_EXECUTABLE}" "${TEST_DRIVER}"
            --exe "${SOLVER}" --opensim-bin "${OpenSim_BIN_DIR}"
            error
                --setup "${CMAKE_CURRENT_SOURCE_DIR}/cases/${case_file}"
                --expect-code 1
                --expect-text "${case_text}")
endforeach()

# `cmake --build . --target regenerate-test-references` after a deliberate
# behaviour change. Read the failing diff before you reach for this.
add_custom_target(regenerate-test-references)
add_custom_command(TARGET regenerate-test-references POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${TEST_DRIVER}"
        --exe "${SOLVER}" --opensim-bin "${OpenSim_BIN_DIR}"
        golden --regenerate
            --setup    "${CMAKE_CURRENT_SOURCE_DIR}/cases/rotation_90.xml"
            --produced "${EXAMPLES_DIR}/imu-orientations/ik_rotation90.sto"
            --expected "${CMAKE_CURRENT_SOURCE_DIR}/expected/rotation_90.sto")
foreach(example IN LISTS GOLDEN_EXAMPLES)
    add_custom_command(TARGET regenerate-test-references POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${TEST_DRIVER}"
            --exe "${SOLVER}" --opensim-bin "${OpenSim_BIN_DIR}"
            golden --regenerate
                --setup    "${EXAMPLES_DIR}/${example}/ikSetup.xml"
                --produced "${EXAMPLES_DIR}/${example}/ik_results.sto"
                --expected "${CMAKE_CURRENT_SOURCE_DIR}/expected/${example}.sto")
endforeach()
add_dependencies(regenerate-test-references hybrid-inverse-kinematics-solver)
