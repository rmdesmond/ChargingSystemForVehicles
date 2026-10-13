# Toolchain file: build for x86-64 Linux from another architecture.
#
# Used in the development VM, which is aarch64, to prove the code is
# architecture-clean in the "other" direction. CMake reads this file before
# project() and once more for every compiler check, so it must only describe
# the target system. It must never contain project settings.
#
# Needs (Ubuntu): gcc-x86-64-linux-gnu g++-x86-64-linux-gnu qemu-user-static

# The operating system of the machine the binaries will RUN on. Setting this is
# what switches CMake into cross-compiling mode (CMAKE_CROSSCOMPILING = TRUE).
set(CMAKE_SYSTEM_NAME Linux)

# The CPU of that machine. Project code can test it; CMake itself does not use
# it to pick a compiler.
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# The GNU target triple. Ubuntu prefixes every cross tool with it and installs
# the target's libraries and headers under /usr/<triple>.
set(CROSS_TRIPLE x86_64-linux-gnu)

# The cross compilers. C is listed although our code is C++ only, because
# GoogleTest's own project() enables C.
set(CMAKE_C_COMPILER ${CROSS_TRIPLE}-gcc)
set(CMAKE_CXX_COMPILER ${CROSS_TRIPLE}-g++)

# Where the target's libraries and headers live on this machine. find_library(),
# find_path() and find_package() look here first.
set(CMAKE_FIND_ROOT_PATH /usr/${CROSS_TRIPLE})

# Programs (code generators, ccache, git) run on the build machine, so search
# for them on the host only, never in the target tree.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Libraries, headers and packages get linked into the target binary, so search
# for them in the target tree only. This stops an aarch64 host library from
# being picked up by accident and failing at link time.
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# How to run a target binary on the build machine. CTest and
# gtest_discover_tests() prefix every test command with this, so the x86-64
# unit tests run under QEMU user-mode emulation. -L tells QEMU where to find the
# target's dynamic loader and shared libraries.
set(CMAKE_CROSSCOMPILING_EMULATOR qemu-x86_64-static -L /usr/${CROSS_TRIPLE})