# Third-Party Notices

REG-Player is licensed under the MIT License (see [LICENSE](LICENSE)).

This project is a derivative work of [XMPlayer](https://github.com/atalaygrgn/XMPlayer)
by Atalay Görgün, also MIT licensed.

The following third-party components are used. Each retains its own license.

## Runtime libraries (linked dynamically or statically)

| Component | Version | License | Source |
|---|---|---|---|
| SDL3 | 3.2.28 | Zlib | https://www.libsdl.org |
| SDL3_image | 3.2.6 | Zlib | https://github.com/libsdl-org/SDL_image |
| SDL3_ttf | 3.2.2 | Zlib | https://github.com/libsdl-org/SDL_ttf |
| libmpv | 0.37 | LGPL-2.1+ | https://mpv.io |
| TagLib | 1.13.1 | LGPL-2.1 / MPL-1.1 | https://taglib.org |
| libxmp | 4.6.0 | MIT | https://xmp.sourceforge.net |
| libgme (Game Music Emu) | 0.6.5 | LGPL-2.1+ | https://github.com/libgme/game-music-emu |
| libopusfile | 0.12 | BSD-3-Clause | https://opus-codec.org |
| libopus | 1.4 | BSD-3-Clause | https://opus-codec.org |
| libogg | 1.3 | BSD-3-Clause | https://xiph.org/ogg |
| libasound (ALSA) | system | LGPL-2.1 | https://www.alsa-project.org |
| zlib | system | Zlib | https://zlib.net |

## Bundled single-header / source-included libraries

Located under `third_party/`. Each preserves its original license.

| Component | License | Source |
|---|---|---|
| miniaudio | MIT-0 / public domain | https://github.com/mackron/miniaudio |
| stb_image | MIT / public domain | https://github.com/nothings/stb |
| stb_image_write | MIT / public domain | https://github.com/nothings/stb |
| nlohmann/json | MIT | https://github.com/nlohmann/json |
| kissfft | BSD-3-Clause | https://github.com/mborgerding/kissfft |
| StSound (Atari ST YM) | LGPL-2.1 | https://github.com/arnaud-carre/StSound |
| cSID-light (C64 SID) | WTFPL | by Hermit (Mihaly Horvath), 2017 |

## Assets

### Fonts
- `assets/font/Orbitron-Bold.ttf` — Orbitron font by The Orbitron Project Authors,
  licensed under SIL Open Font License 1.1. See `assets/font/OFL.txt`.

### Icons
- `assets/icons/*.png` — derived from Remix Icon library
  ([https://remixicon.com](https://remixicon.com)), licensed Apache 2.0.
  See `assets/icons/REMIX-LICENSE.txt`.

### Sound effects
- `assets/sfx/keytone.wav` — synthesized sine tone, generated via FFmpeg
  during this project. No copyright claim, public domain.

### Wallpaper
- `assets/background/bg.jpg` — derivative asset from XMPlayer original
  (Atalay Görgün, MIT license).

## Build-only tooling (not redistributed)

- CMake — BSD-3
- gcc/g++ / aarch64-linux-gnu toolchain — GPL (used as tool, not linked)
- pkg-config — GPL (used as tool)

## License compatibility

The aggregate work distributes binaries that link against LGPL libraries
(libmpv, TagLib, libgme, libasound). Per LGPL-2.1 §6, dynamic linking is
permitted under the terms of LGPL. Source modifications to LGPL components
must remain LGPL; this project does not modify any LGPL component source.

Static-linked LGPL component (StSound, in `third_party/stsound/`) is
retained as-is; users of REG-Player binaries are entitled to the StSound
source under LGPL terms — provided in this repository.

The WTFPL-licensed cSID-light is bundled with one trivial wrapper export
appended (see end of `third_party/tinysid/csid-light.c`); the WTFPL
explicitly permits any modifications.
