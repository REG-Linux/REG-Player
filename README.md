# REG-Player

C++17 / SDL3 / GLES2 port of [XMPlayer](https://github.com/atalaygrgn/XMPlayer) (Lua/Love2D). Target: REG Linux handhelds. Scan root: `/userdata/medias`.

Source of truth: `/home/romain/test/XMPlayer/.xmplayer/`. Read `ANTI_DRIFT.md` before touching code.

## Status

Skeleton compiles + launches. Most modules ported as stubs; full visual + behavior parity is the next pass per `ANTI_DRIFT.md` checklist.

| Module | State |
|---|---|
| theme | ported |
| categories | ported (defaults to `/userdata/medias/...`) |
| utils | ported |
| settings (model + JSON) | ported (Lua bug fixed) |
| system (battery, ALSA vol, backlight) | ported (generic Linux) |
| history | ported |
| video_manager | ported |
| browser | ported (opendir, no shell) |
| metadata | ported (TagLib) |
| persist | ported |
| gamepad | ported (SDL3, no gptokeyb) |
| assets | ported |
| ui (glow text, marquee, toasts, progress, indexing popup) | ported |
| render helpers | ported |
| main loop | ported |
| background (gradient, particles, PSP waves, wave visualizer) | ported (no blur, no shader) |
| indexing | partial — async scan works; album/artist aggregation + thumbnail gen TODO |
| xmb (nav state) | partial — basic nav + scroll lerp; submenu transitions, view_type variants TODO |
| xmb_draw | partial — basic draw; marquee scissor + thumbs + watched eye TODO |
| music_player | partial — load/play/seek/visualizer pcm; cover art texture decode TODO |
| music_view | stub — display sleep + options popup TODO |
| image_viewer / image_view | ported |
| settings_view | stub — choice popup + folder picker TODO |
| player (video) | stub — shells out to `mpv` binary; libmpv embed TODO |

## Build

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
```

Run dev:
```sh
REG_ASSETS_DIR=$PWD/assets ./build/regplayer
```

## Deps

Required at build:
- SDL3 (≥3.2.28)
- SDL3_image (3.2.x)
- SDL3_ttf (3.2.x)
- TagLib (1.13)
- libmpv (≥0.37)
- libasound (ALSA)
- C++17 toolchain, CMake 3.20+

Bundled (single-header, in `third_party/`):
- miniaudio
- stb_image_write
- nlohmann/json

## Layout

```
src/                   — one .cpp + .h per Lua module (same names)
third_party/           — single-header deps
assets/                — copied from XMPlayer (icons, font, sfx, default wallpaper)
ANTI_DRIFT.md          — port rules
```

## Config paths

- Settings: `~/.config/regplayer/settings.json`
- Index: `~/.config/regplayer/index.json`
- History: `~/.config/regplayer/history.json`
- Watched: `~/.config/regplayer/watched.json`
- mpv watch_later: `~/.config/regplayer/mpv/watch_later/`
- Thumbnails: `~/.cache/regplayer/thumbnails/`
