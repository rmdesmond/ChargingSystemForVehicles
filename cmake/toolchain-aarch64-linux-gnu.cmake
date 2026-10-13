# Toolchain file: build for aarch64 Linux (Raspberry Pi 5) from x86-64.
#
# Used by CI, whose runners are x86-64. The development VM is already aarch64
# and builds for the Pi with the 'native' preset, so in the VM this file is for
# reference only. See toolchain-x86_64-linux-gnu.cmake for the reasoning behind
# each setting; the two files differ only in the three values marked below.
#
# Needs (Ubuntu): gcc-aarch64-linux-gnu g++-aarch64-linux-gnu qemu-user-static

# Target operating system; switches CMake into cross-compiling mode.
set(CMAKE_SYSTEM_NAME Linux)

# Target CPU. (differs)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# GNU target triple. (differs)
set(CROSS_TRIPLE aarch64-linux-gnu)

# Cross compilers; C is needed by GoogleTest.
set(CMAKE_C_COMPILER ${CROSS_TRIPLE}-gcc)
set(CMAKE_CXX_COMPILER ${CROSS_TRIPLE}-g++)

# Target libraries and headers.
set(CMAKE_FIND_ROOT_PATH /usr/${CROSS_TRIPLE})

# Programs from the host; libraries, headers and packages from the target.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Run target binaries, including the unit tests, under QEMU. (differs)
set(CMAKE_CROSSCOMPILING_EMULATOR qemu-aarch64-static -L /usr/${CROSS_TRIPLE})