# Root build description for the charge controller.
# Policy lives here (language standard, warnings, options); targets live in
# lib/, app/ and tests/.

# 3.25 is the first release that reads version 6 of CMakePresets.json.
cmake_minimum_required(VERSION 3.25)

project(charger
    VERSION 0.1.0
    DESCRIPTION "EV charge controller daemon"
    LANGUAGES CXX
)

# C++17, strictly: fail if the compiler lacks it, and no GNU extensions, so the
# same source builds with GCC and Clang.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Write compile_commands.json into the build directory. clangd and clang-tidy
# read it to learn the exact flags for every file.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Install under /opt/charger unless the caller chose a prefix.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/opt/charger" CACHE PATH "Install prefix" FORCE)
endif()

# Let include() find our own modules in cmake/.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GNUInstallDirs) # CMAKE_INSTALL_BINDIR and friends
include(Warnings) # defines the charger::warnings target
include(Coverage) # defines charger::coverage and the 'coverage' target
include(Tidy)     # defines charger_add_tidy_target()

option(CHARGER_BUILD_TESTS "Build the unit tests" ON)

add_subdirectory(lib)
add_subdirectory(app)

if(CHARGER_BUILD_TESTS)
    enable_testing()    # must be in the root file so ctest finds the tests
    add_subdirectory(tests)
endif()

# Static analysis covers what ships: the library and the daemon, not the tests.
charger_add_tidy_target(charger_core charger-daemon)