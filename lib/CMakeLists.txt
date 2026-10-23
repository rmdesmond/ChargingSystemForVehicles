# The charger core: all product logic, built as a static library so the daemon
# and the unit tests link exactly the same object code.

add_library(charger_core STATIC
    src/LineProtocol.cpp
    src/LineServer.cpp
    src/ServiceNotifier.cpp
    src/SocketVehicle.cpp
    src/StateMachine.cpp
)

add_library(charger::core ALIAS charger_core)

# PUBLIC: needed to build this library AND by anything that links to it.
target_include_directories(charger_core PUBLIC include)

# PRIVATE: applies to this library's own sources only.
target_link_libraries(charger_core PRIVATE charger::warnings charger::coverage)