# GalaxyRP addition: [TaystJK multi-arch] Linux ARM64 (aarch64) cross-compile toolchain file.
# Neither upstream OpenJK nor TaystJK ships one of these (TaystJK's own "ARM support" is
# macOS-native-runner-only, built natively on Apple Silicon, not cross-compiled) -- this is new,
# modeled directly on the existing linux-i686.cmake / *-w64-mingw32.cmake toolchain files in this
# same directory, for anyone who wants to cross-build a Linux aarch64 (e.g. Raspberry Pi, AWS
# Graviton, generic ARM64 Linux) dedicated server from an x86_64 Linux host.
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchains/linux-aarch64.cmake ..
#
# Requires an aarch64-linux-gnu cross-compiler on the host, e.g. on Debian/Ubuntu:
#   apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
