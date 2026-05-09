# Cross-compiling REG-Player for aarch64 handhelds

Target distros: Batocera ARM, ROCKNIX, ArkOS, Knulli, JELOS — all share aarch64 + glibc/musl + SDL3-capable userland.

## Toolchain

Ubuntu/Debian host:

```sh
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu pkg-config
```

## Sysroot prep

REG-Player needs aarch64 versions of: **SDL3, SDL3_image, SDL3_ttf, libmpv, TagLib, libxmp, libgme, libasound**.

### Option A — Multiarch (Debian/Ubuntu host)

```sh
sudo dpkg --add-architecture arm64
echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports noble main universe" | \
    sudo tee /etc/apt/sources.list.d/arm64.list
sudo apt update
sudo apt install \
    libsdl3-dev:arm64 libtag1-dev:arm64 libmpv-dev:arm64 \
    libasound2-dev:arm64 libxmp-dev:arm64 libgme-dev:arm64
# SDL_image / SDL_ttf 3.2.x: build from source (see Option B)
```

### Option B — Build SDL_image + SDL_ttf into sysroot

```sh
SYSROOT=$HOME/aarch64-sysroot
git clone --depth 1 -b release-3.2.6 https://github.com/libsdl-org/SDL_image
cd SDL_image
cmake -B build-arm64 \
    -DCMAKE_TOOLCHAIN_FILE=$PWD/../REG-Player/cmake/aarch64-toolchain.cmake \
    -DCMAKE_INSTALL_PREFIX=$SYSROOT/usr \
    -DSDLIMAGE_VENDORED=OFF -DSDLIMAGE_AVIF=OFF -DSDLIMAGE_JXL=OFF \
    -DSDLIMAGE_TIF=OFF -DSDLIMAGE_WEBP=OFF -DSDLIMAGE_BACKEND_STB=ON
cmake --build build-arm64 -j$(nproc)
cmake --install build-arm64
# Repeat for SDL_ttf
```

### Option C — Sysroot from device

Pull `/usr/lib`, `/usr/include`, `/lib` from a running target device into a local sysroot dir. Fast on Batocera since SSH is enabled by default.

```sh
mkdir -p $HOME/aarch64-sysroot
rsync -av --include='lib/' --include='lib/aarch64-linux-gnu/' --include='*.so*' \
    root@batocera.local:/usr/lib/ $HOME/aarch64-sysroot/usr/lib/
rsync -av root@batocera.local:/usr/include/ $HOME/aarch64-sysroot/usr/include/
```

## Build

```sh
cmake -B build-arm64 \
    -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-toolchain.cmake \
    -DREG_SYSROOT=$HOME/aarch64-sysroot \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build-arm64 -j$(nproc)
file build-arm64/regplayer
# regplayer: ELF 64-bit LSB pie executable, ARM aarch64
```

## Deploy

### Batocera

```sh
scp build-arm64/regplayer root@batocera.local:/userdata/system/regplayer/
scp -r assets root@batocera.local:/userdata/system/regplayer/
ssh root@batocera.local "chmod +x /userdata/system/regplayer/regplayer"
```

Add to Batocera launcher menu via `/userdata/system/configs/emulationstation/es_systems_*.cfg` or a custom system.

### muOS-style (.muxapp)

```sh
mkdir -p REGPlayer/.regplayer/{bin,libs}
cp build-arm64/regplayer REGPlayer/.regplayer/bin/
cp -r assets REGPlayer/.regplayer/
cp packaging/regplayer-launch.sh REGPlayer/mux_launch.sh
zip -r REG-Player.muxapp REGPlayer/
```

## Known gotchas

- **Hwdec on handheld GPUs** — most ARM SoCs (RK3566, H700, A133) need `hwdec=v4l2m2m` or `hwdec=drm`. Currently hard-coded `auto-safe`. Adjust in player.cpp if video is too slow.
- **OpenGL ES 2 driver** — ensure target has Mesa with GLES2. SDL3 picks `opengles2` driver via hint in main.cpp.
- **ALSA mixer name** — varies per distro. Currently tries Master/PCM/Speaker. Add device-specific name if volume reads return null.
- **Sysfs paths** — battery sysfs varies. Currently globs `/sys/class/power_supply/BAT*`. Verify on target.
- **Font** — eurostile_bold.ttf bundled in assets/. No system font dependency.

## Status

Full cross-build verified 2026-05-09:
- aarch64-linux-gnu-gcc/g++ toolchain installed
- SDL3 3.2.28, SDL_image 3.2.6, SDL_ttf 3.2.2 built from source for arm64 → installed to `~/aarch64-install/usr/lib/`
- TagLib, ALSA, libxmp, libgme, zlib, libmpv, X11/Wayland deps installed via multiarch
- libpipewire downgraded to 1.0.5-1 (non-t64 variant) to satisfy libmpv2 dep
- **arm64 binary built: `build-arm64/regplayer` ~1.9MB ELF aarch64**

### Build invocation

```sh
PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig \
cmake -B build-arm64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-multiarch.cmake \
  -DCMAKE_PREFIX_PATH=/home/romain/aarch64-install/usr
cmake --build build-arm64 -j$(nproc)
```

### Toolchain files provided

- `cmake/aarch64-toolchain.cmake` — sysroot-based (point at extracted device sysroot)
- `cmake/aarch64-multiarch.cmake` — multiarch-based (use Debian/Ubuntu :arm64 packages on host)

### Untested

Actual deployment to a real handheld not yet performed. Expected work needed:
- Pull `~/aarch64-install/usr/lib/libSDL3*.so.0` to device alongside binary (or rely on device's SDL3)
- Verify `hwdec=auto-safe` works on target GPU (RK3566/A133/etc)
- Verify ALSA mixer name + sysfs paths on target distro
