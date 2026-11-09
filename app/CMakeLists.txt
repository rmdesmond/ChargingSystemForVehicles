# The daemon: a thin main() around the core library.

# The version string lives in a generated source, rewritten on every build
# only if 'git describe' changed (cmake/GenerateVersion.cmake). A custom
# target with no inputs runs every time; BYPRODUCTS tells Ninja that the file
# comes from it, so the daemon is relinked only when the file changes.
set(version_cpp ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)
add_custom_target(charger_version
    COMMAND ${CMAKE_COMMAND}
        -DOUT=${version_cpp} -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/GenerateVersion.cmake
    BYPRODUCTS ${version_cpp}
    COMMENT "Checking the version string"
    VERBATIM
)
add_executable(charger-daemon
    src/main.cpp
    ${version_cpp}
)
add_dependencies(charger-daemon charger_version)
# version.cpp includes "Version.h" from src/, as main.cpp does.
target_include_directories(charger-daemon PRIVATE src)

target_link_libraries(charger-daemon PRIVATE
    charger::core
    charger::warnings
    charger::coverage
)

# 'cmake --install' copies the binary to <prefix>/bin.
install(TARGETS charger-daemon
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)