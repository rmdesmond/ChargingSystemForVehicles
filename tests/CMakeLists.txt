# Unit tests. GoogleTest is downloaded at configure time and built with our
# compiler and flags, so it always matches the target architecture.

include(FetchContent)

FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0 # pinned: the build must not change under us
    GIT_SHALLOW TRUE
)
# Keep GoogleTest out of 'cmake --install'; it is not part of the product.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(charger_tests
    test_line_protocol.cpp
    test_line_server.cpp
    test_service_notifier.cpp
    test_state_machine.cpp
)

target_link_libraries(charger_tests PRIVATE
    charger::core
    charger::warnings
    charger::coverage
    GTest::gmock_main # GoogleTest + GoogleMock + a ready-made main()
)

# Ask the test binary for its test list after it is built and register each
# TEST() with CTest by name. When cross-compiling this runs the binary through
# CMAKE_CROSSCOMPILING_EMULATOR (set in the toolchain file).
include(GoogleTest)
gtest_discover_tests(charger_tests)

# Run the daemon itself as tests. They prove the binary starts and exits
# cleanly, and they bring main.cpp into the coverage measurement.
#
# The demo runs on the foreign architecture too: because the command is a
# target, CTest adds the emulator from the toolchain file.
add_test(NAME daemon_demo COMMAND charger-daemon --demo)

# A wrong argument must print the usage line. With PASS_REGULAR_EXPRESSION the
# test passes when the output matches, whatever the exit status.
add_test(NAME daemon_rejects_unknown_argument COMMAND charger-daemon --bogus)
set_tests_properties(daemon_rejects_unknown_argument PROPERTIES
    PASS_REGULAR_EXPRESSION "usage: charger-daemon")

# REQ-009: in service mode the daemon runs until SIGTERM and then exits with
# status 0. 'timeout' sends the signal after 2 s and passes the status through.
# Native only: here the command is 'timeout', so CTest adds no emulator.
if(NOT CMAKE_CROSSCOMPILING)
    add_test(NAME daemon_service_stops_on_sigterm
        COMMAND timeout --preserve-status --signal=TERM 2 $<TARGET_FILE:charger-daemon>)
endif()