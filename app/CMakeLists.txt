# The daemon: a thin main() around the core library.

add_executable(charger-daemon
    src/main.cpp
)

target_link_libraries(charger-daemon PRIVATE
    charger::core
    charger::warnings
    charger::coverage
)

# 'cmake --install' copies the binary to <prefix>/bin.
install(TARGETS charger-daemon
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)