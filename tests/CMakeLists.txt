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
    test_state_machine.cpp
)

target_link_libraries(charger_tests PRIVATE
    charger::core
    charger::warnings
    GTest::gmock_main # GoogleTest + GoogleMock + a ready-made main()
)

# Ask the test binary for its test list after it is built and register each
# TEST() with CTest by name. When cross-compiling this runs the binary through
# CMAKE_CROSSCOMPILING_EMULATOR (set in the toolchain file).
include(GoogleTest)
gtest_discover_tests(charger_tests)