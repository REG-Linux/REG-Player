# Cross-compile toolchain file for aarch64 Linux handhelds (Anbernic, Steam Deck arm,
# RK3326/RK3566 boards, Allwinner H700, etc.).
#
# Usage:
#   cmake -B build-arm64 -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-toolchain.cmake \
#         -DREG_SYSROOT=/path/to/aarch64-sysroot
#   cmake --build build-arm64 -j$(nproc)
#
# Sysroot must contain headers + libs for: SDL3, SDL3_image, SDL3_ttf, libmpv,
# TagLib, libxmp, libgme, libasound. Single-header deps (miniaudio, stb,
# nlohmann/json) are bundled in third_party/ and need no sysroot entries.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

if(NOT DEFINED REG_SYSROOT)
    set(REG_SYSROOT "$ENV{HOME}/aarch64-sysroot" CACHE PATH "aarch64 sysroot")
endif()

set(CMAKE_SYSROOT "${REG_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "${REG_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config inside sysroot
set(ENV{PKG_CONFIG_LIBDIR}
    "${REG_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${REG_SYSROOT}/usr/lib/pkgconfig:${REG_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${REG_SYSROOT}")
