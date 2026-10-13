# Project-wide warning policy, expressed as a target.
#
# An INTERFACE library compiles nothing. It only carries settings, and any
# target that links to it inherits them. Our targets link it PRIVATE, so the
# policy applies to our code and not to third-party code such as GoogleTest.

option(CHARGER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

add_library(charger_warnings INTERFACE)
add_library(charger::warnings ALIAS charger_warnings)

target_compile_options(charger_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow # a local name hides an outer one
    -Wconversion # implicit narrowing, e.g. int to uint8_t
    -Wsign-conversion # implicit signed/unsigned change
    -Wno-non-virtual-dtor # base class with virtual functions needs a virtual dtor
    -Wold-style-cast # C casts hide what kind of conversion happens
)

if(CHARGER_WARNINGS_AS_ERRORS)
    target_compile_options(charger_warnings INTERFACE -Werror)
endif()