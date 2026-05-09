# REG-Player

XMB-style media player for [REG-Linux](https://github.com/REG-Linux/REG-Linux) handhelds. C++17 / SDL3 / GLES2. Faithful port of [XMPlayer](https://github.com/atalaygrgn/XMPlayer) (Lua/Love2D) — same layout, same constants, same behaviour — extended with native chiptune subsystem and embedded video.

Default scan root: `/userdata/medias`. Falls back to `~/Music`, `~/Videos`, `~/Pictures`.

![status](https://img.shields.io/badge/version-v0.1.0-blue) ![license](https://img.shields.io/badge/license-MIT-green)

## Features

- XMB cross-bar navigation (Settings · Photo · Music · Video · Files)
- Music player: cover art, marquee titles, playlist, repeat-one, shuffle, auto-sleep
- Visualizers: PSP-style waves, audio waveform, frequency bars
- Embedded video via libmpv render API (no fork-out, no flicker)
- Image viewer with zoom + pan
- Theme system (3 themes × 8 accent colours), particle effects toggle, wallpaper picker
- Help overlay (X on root)
- Keyboard + gamepad input (SDL3 native, no gptokeyb)
- Battery / volume / brightness via generic Linux sysfs + ALSA

## Format support (48+ extensions)

| Class | Backend | Extensions |
|---|---|---|
| Standard audio | miniaudio | `mp3` `wav` `flac` `ogg` `m4a` |
| Opus | libopusfile | `opus` |
| Trackers (23) | libxmp | `mod` `xm` `it` `s3m` `mtm` `stm` `669` `far` `ult` `ams` `med` `okt` `dbm` `liq` `mdl` `mt2` `pt36` `ptm` `rtm` `gdm` `imf` `dsm` `psm` |
| Console chiptune (11) | libgme | `nsf` `nsfe` `gbs` `spc` `vgm` `vgz` `gym` `ay` `hes` `sap` `kss` |
| Atari ST | StSound (bundled, LGPL) | `ym` `sndh` |
| Commodore 64 | cSID-light (bundled, WTFPL) | `sid` |
| Video | libmpv | `mp4` `mkv` `avi` `mov` `wmv` `webm` + all libmpv codecs |
| Photos | SDL3_image | `jpg` `jpeg` `png` `gif` `bmp` |

Multi-track chiptune (NSFE, HES, KSS, SID, …) expand into per-tune browser entries (`file.ext#N`, capped 99). VGM/VGZ files parse GD3 metadata (track / game / system / author / dumper / date / notes).

## Build

System deps (Debian/Ubuntu):

```sh
sudo apt install libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev \
                 libmpv-dev libtaglib-dev \
                 libxmp-dev libgme-dev libopusfile-dev \
                 libasound2-dev zlib1g-dev \
                 cmake pkg-config
```

Build + install:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
```

Run from source tree (no install):

```sh
REG_ASSETS_DIR=$PWD/assets ./build/regplayer
```

## Cross-compile (aarch64)

See `docs/CROSSCOMPILE.md`. Toolchain at `cmake/aarch64-multiarch.cmake`. Produces 1.6 MB stripped binary; portable tarball includes all SDL3 family + libmpv + libgme + libxmp + TagLib for older glibc handhelds.

## Buildroot integration

REG-Linux ships REG-Player as a first-class package:

```
package/frontend/regplayer/        — REG-Player package
package/libraries/libgme/          — libgme package (new in tree)
```

Enable via `make menuconfig` → Frontend → REG-Player.

## Deps summary

| Class | Library | License | Source |
|---|---|---|---|
| Window/input/render | SDL3 + SDL3_image + SDL3_ttf | Zlib | system |
| Audio engine (header-only) | miniaudio | MIT-0 | bundled |
| Tags | TagLib | LGPL-2.1 / MPL-1.1 | system |
| Trackers | libxmp | MIT | system |
| Console chiptune | libgme | LGPL-2.1+ | system |
| Opus | libopusfile | BSD-3 | system |
| Atari ST | StSound | LGPL | bundled |
| C64 SID | cSID-light | WTFPL | bundled |
| Video | libmpv | LGPL-2.1+ | system |
| Mixer | libasound | LGPL | system |
| Image decode (header-only) | stb_image / stb_image_write | Public domain / MIT | bundled |
| JSON | nlohmann/json | MIT | bundled |
| zlib | zlib | Zlib | system |

Full attribution: [`NOTICES.md`](NOTICES.md).

## Layout

```
src/             — one .cpp + .h per Lua module (snake_case, names preserved)
third_party/     — bundled deps (single-header + StSound + cSID-light)
assets/          — fonts, icons, default wallpaper, sfx
cmake/           — toolchain files (aarch64-multiarch)
packaging/       — .desktop, manpage, launch.sh
docs/            — CROSSCOMPILE.md
ANTI_DRIFT.md    — port rules
NOTICES.md       — third-party attributions
```

## Config / cache paths

| Purpose | Path |
|---|---|
| Settings | `~/.config/regplayer/settings.json` |
| Media index | `~/.config/regplayer/index.json` |
| Listening history | `~/.config/regplayer/history.json` |
| Watched videos | `~/.config/regplayer/watched.json` |
| mpv resume points | `~/.config/regplayer/mpv/watch_later/` |
| Thumbnails (LRU 64) | `~/.cache/regplayer/thumbnails/` |

## Status

**v0.1.0** — feature-complete vs. upstream XMPlayer + native extras. All Lua modules ported 1:1 per `ANTI_DRIFT.md`. Both amd64 and aarch64 binaries build clean.

### Deviations from upstream Lua (intentional)

- libmpv embedded via render API instead of `os.execute("mpv ...")` — eliminates flicker and fork latency
- SDL3 gamepad direct, no `gptokeyb2` dependency
- JSON persistence instead of Lua-table `loadfile`
- Single `/userdata/medias` scan root (REG-Linux convention) instead of muOS `/mnt/mmc` + `/mnt/sdcard`
- No icon-gloss / wallpaper-blur shaders (GLES2 fixed pipeline only)

### TODO / known gaps

- Auto-sleep timer firing — code-verified, not yet live-verified on device
- Brightness OSD toast — coded, no easy external trigger on dev machine
- Real handheld deploy — arm64 portable tarball ready, no device tested yet
- Robustness edge cases untested: killed mid-scan, disk-full thumbnail write, non-ASCII filenames, broken ID3 tags, large libraries (1000+)
- Format extensions deferred: WebP, AVIF, HVL/AHX (Amiga)

## License

MIT — see [`LICENSE`](LICENSE). XMPlayer (Lua original) Copyright Atalay Görgün, MIT, retained in attribution.

Asset attributions and full third-party license text in [`NOTICES.md`](NOTICES.md).
