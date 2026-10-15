# Static analysis: a 'tidy' target that runs clang-tidy over the product code.
#
# clang-tidy needs to know how each file is compiled (include paths, -std=...).
# It reads that from compile_commands.json in the build directory, which the
# root CMakeLists.txt asks CMake to write. The checks themselves are configured
# in .clang-tidy at the repository root.

function(charger_add_tidy_target)
    # clang-tidy analyses for the host. A cross build's flags would mislead it.
    if(CMAKE_CROSSCOMPILING)
        return()
    endif()

    find_program(CLANG_TIDY_EXECUTABLE clang-tidy)
    if(NOT CLANG_TIDY_EXECUTABLE)
        message(STATUS "clang-tidy not found: the 'tidy' target is not available")
        return()
    endif()

    # Collect the source files of the targets named in the arguments, so the
    # list of files to analyse can never drift from the list that is built.
    set(sources "")
    foreach(target IN LISTS ARGN)
        get_target_property(target_dir ${target} SOURCE_DIR)
        get_target_property(target_sources ${target} SOURCES)
        foreach(source IN LISTS target_sources)
            list(APPEND sources "${target_dir}/${source}")
        endforeach()
    endforeach()

    # .clang-tidy sets WarningsAsErrors, so any finding makes this target fail.
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXECUTABLE} -p ${CMAKE_BINARY_DIR} --quiet ${sources}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy on the product sources"
        VERBATIM
    )
endfunction()