# Code coverage, expressed as a target like the warning policy.
#
# charger::coverage always exists. It is empty unless ENABLE_COVERAGE is ON, so
# the other CMakeLists files link it unconditionally and stay free of if()s.
# Only our own targets link it, so GoogleTest is not instrumented.

option(ENABLE_COVERAGE "Instrument the build and add the 'coverage' target (GCC only)" OFF)

add_library(charger_coverage INTERFACE)
add_library(charger::coverage ALIAS charger_coverage)

if(NOT ENABLE_COVERAGE)
    return()
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "ENABLE_COVERAGE needs GCC (gcov); found ${CMAKE_CXX_COMPILER_ID}")
endif()
if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Coverage is measured on the native build only")
endif()

# --coverage makes GCC write a .gcno file per object at compile time and makes
# the program write a .gcda file with execution counts when it exits.
target_compile_options(charger_coverage INTERFACE --coverage)
target_link_options(charger_coverage INTERFACE --coverage)

find_program(LCOV_EXECUTABLE lcov REQUIRED)
find_program(GENHTML_EXECUTABLE genhtml REQUIRED)

set(COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage")
# Record branch data as well as lines. '--filter branch' drops branch records on
# lines that contain no condition: GCC emits hidden branches for exception
# handling there, which no test can reach and which would bury the real ones.
set(LCOV_OPTIONS --rc branch_coverage=1 --filter branch --quiet)
# Leave these files out while reading the data: the tests themselves,
# GoogleTest, and the compiler's own headers. Product code is what remains.
set(LCOV_EXCLUDES
    --exclude "${CMAKE_SOURCE_DIR}/tests/*"
    --exclude "${CMAKE_BINARY_DIR}/_deps/*"
    --exclude "/usr/*"
)

add_custom_target(coverage
    # 1. Start from zero so counts from an earlier run cannot leak in.
    COMMAND ${LCOV_EXECUTABLE} ${LCOV_OPTIONS} --zerocounters --directory ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_DIR}
    # 2. Run every test. A failing test fails the target.
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} --output-on-failure
    # 3. Collect the counts from the .gcda files, product code only.
    COMMAND ${LCOV_EXECUTABLE} ${LCOV_OPTIONS} --capture --directory ${CMAKE_BINARY_DIR}
        ${LCOV_EXCLUDES} --output-file ${COVERAGE_DIR}/charger.info
    # 4. Browsable report. genhtml prints the totals; keep a copy for CI.
    COMMAND ${GENHTML_EXECUTABLE} ${LCOV_OPTIONS} ${COVERAGE_DIR}/charger.info
        --output-directory ${COVERAGE_DIR}/html
    COMMAND ${LCOV_EXECUTABLE} --rc branch_coverage=1 --summary ${COVERAGE_DIR}/charger.info
        > ${COVERAGE_DIR}/summary.txt
    DEPENDS charger_tests charger-daemon
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running tests and measuring coverage"
    VERBATIM
)