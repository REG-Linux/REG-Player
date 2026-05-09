# Changelog

## v0.2.0 — 2026-05-10

### Added
- Real FFT spectrum visualizer (kissfft, log-binned 50 Hz–12 kHz, peak hold +
  decay smoothing) — replaces synthetic time-window RMS bars

## v0.1.1 — 2026-05-10

### Added
- Opus playback via libopusfile (`.opus` files at any sample rate, decoded
  to 48 kHz stereo internally)
- Opus cover art extraction via TagLib XiphComment METADATA_BLOCK_PICTURE

## v0.1 — 2026-05-09

Initial port from XMPlayer (Lua/Love2D) to C++17 / SDL3 / GLES2.

### Features
- XMB-style media browser (categories: Settings, Photo, Music, Video, Files)
- Music player with cover art, marquee, playlist, repeat one, auto sleep
- Visualizers: PSP-style waves, audio waveform, frequency bars
- Video playback via embedded libmpv (SW render, no shell-out)
- Image viewer with zoom + pan (Y + D-Pad)
- Settings: theme + accent color, particle effects, wallpaper, media dirs
- Persistence: settings.json, history.json, watched.json, mpv watch_later
- Photo + album thumbnail cache (stb_image resize, PNG output)
- Battery / volume / brightness reads via generic Linux sysfs + ALSA
- SDL3 gamepad (no gptokeyb dep)
- Keyboard help overlay (X on XMB root)

### Audio formats
- mp3, wav, flac, ogg, m4a (miniaudio)
- Tracker modules via libxmp: mod, xm, it, s3m, mtm, stm, 669, far, ult,
  ams, med, okt, dbm, liq, mdl, mt2, pt36, ptm, rtm, gdm, imf, dsm, psm
- Console chiptune via libgme: nsf, nsfe, gbs, spc, vgm, vgz, gym, ay, hes,
  sap, kss
- Atari ST chiptune via StSound (bundled, LGPL): ym, sndh
- Commodore 64 SID via cSID-light (bundled, WTFPL): sid
- Multi-track chiptune sub-tune expansion (file.ext#N entries, capped 99)
- VGM/VGZ GD3 metadata parsing (track, game, system, author, dumper, date)

### Video formats
- All libmpv-supported (mp4, mkv, avi, mov, wmv, webm, etc.)

### Image formats
- jpg, jpeg, png, gif, bmp (SDL3_image)

### Config paths
- `$XDG_CONFIG_HOME/regplayer` (default `~/.config/regplayer`)
- `$XDG_CACHE_HOME/regplayer/thumbnails` (default `~/.cache/regplayer/thumbnails`)
- Default scan root: `/userdata/medias`, fallback `~/Music`, `~/Videos`, `~/Pictures`

### Differences vs XMPlayer (Lua original)
- No icon gloss shader (GLES2 fixed pipeline only)
- No wallpaper blur shader (same reason)
- mpv embedded via libmpv render API, not forked binary
- gamepad via SDL3 directly, not gptokeyb2
- JSON persistence (was Lua-table `loadfile`)
- Single `/userdata/medias` root (was muOS `/mnt/mmc` + `/mnt/sdcard`)
