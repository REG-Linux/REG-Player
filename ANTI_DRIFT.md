# REG-Player anti-drift rules

C++17 / SDL3 / GLES2 port of XMPlayer (Lua/Love2D). Target: REG Linux handhelds. Scan root: `/userdata/medias`.

Source of truth: `/home/romain/test/XMPlayer/.xmplayer/`. Read it before writing any module. Match it.

## Hard rules (do not break)

1. **Module boundaries match Lua 1:1.** One C++ TU per Lua module. Same name. State module + draw module stay split (xmb / xmb_draw, music_player / music_view, image_viewer / image_view, settings / settings_view).
2. **No new features.** If Lua original does not do X, port does not do X. Roadmap items in README do not get pre-implemented.
3. **No refactors of original architecture.** Same data flow, same call sites. If `main.lua` calls `xmb.update(dt)` then `xmb_draw.draw()`, C++ main calls `xmb_update(dt)` then `xmb_draw()`. Do not invent managers, services, or DI.
4. **Constants are sacred.** Copy verbatim. Includes:
   - `REPEAT_DELAY = 0.4`, `REPEAT_INTERVAL = 0.08`
   - `BATTERY_UPDATE_INTERVAL = 10`, `UI_CHECK_INTERVAL = 0.1`
   - `LAUNCH_STATUS_DURATION = 3`
   - item_h = 75, icon_size = 64, icon_spacing = 64
   - thumb_size = 120
   - history cap = 50
   - particle count = 50
   - visualizer bars = 42, gap = 4, samples = 120
   - smooth speeds: 10, 12, 14
   - fade speeds (multipliers): 4, 6, 10, 12
   - palette RGB triples in `theme.lua`
   - REPEAT_INTERVAL, sleep choices `{0,5,10,15,30,60,180}`
   - all `* 0.x` alpha multipliers in `xmb_draw.lua`, `ui.lua`, `music_view.lua`, `settings_view.lua`
5. **Visual layout matches.** Coordinates and offsets match Lua source. Read Lua code, copy numbers.
6. **Input bindings match `xmplayer.gptk` + Love key names** in xmb.lua/keypressed. Map gamepad → same logical keys: `a, b, x, y, up, down, left, right, return, backspace, escape, space, pageup, pagedown, l, e`. Use logical key strings throughout, not SDL enums in game logic.
7. **Persistence file names.** `index.json`, `settings.json`, `history.json`, `watched.json`. (Lua used `.cfg` for Lua-table; we use JSON same names but `.json`.) Path: `$XDG_CONFIG_HOME/regplayer/`, default `~/.config/regplayer/`. Thumbnails: `~/.cache/regplayer/thumbnails/`.
8. **No shaders.** GLES2 fixed pipeline only. Drop gloss shader (icons render plain). Drop blur shader (wallpaper renders plain). Glow effect already shader-free in original (multi-pass offset draws); port that. PSP waves use `SDL_RenderGeometry` with per-vertex color (no shader).
9. **No new deps beyond approved list.** SDL3, SDL3_image, SDL3_ttf, libmpv, miniaudio, TagLib, nlohmann/json, stb_image_write, libasound. Nothing else.
10. **No XDG/locale/i18n.** English only, matches original.
11. **No logging framework.** `fprintf(stderr, ...)` matches Lua's `print()`. No spdlog, no fmt::print.
12. **No error wrapping.** Original Lua uses `pcall` + bool returns. Port uses bool returns + nullable pointers. No exceptions, no `std::expected`, no Result<T, E>.
13. **No async/coroutines.** Use `std::thread` + `std::atomic` + `std::mutex` for indexing. Coroutine yield points in Lua become checkpoint reads of an atomic progress string.
14. **Same coords system.** Origin top-left, Y down, no flip. SDL3 default.
15. **Same redraw model.** `SDL_RenderClear` every frame. No dirty-rect tracking.

## Drift checks (run mentally before each commit)

- Did I add a feature not in Lua original? → revert.
- Did I rename a function from snake_case Lua name? → revert. (Member functions can be `Module::do_thing`; free functions stay `module_do_thing`.)
- Did I add a config option not in `settings.lua`? → revert. (Exception: removing muOS-specifics — `vol_bright_control` stays only if ALSA path works; otherwise hide.)
- Did I add a logging call where Lua had none? → revert.
- Did I introduce a new file that has no Lua counterpart? → must be platform glue (SDL boot, gamepad map, libmpv wrapper, ALSA wrapper, JSON helpers). Document in `docs/PLATFORM.md`. Otherwise revert.
- Did I tweak a constant "to look better"? → revert. Match Lua.
- Did I "improve" a hand-rolled algorithm (UTF-8 sanitize, ID3 parser when not using TagLib, smooth lerp)? → revert behavior; only allow language idiom changes.
- Does my output look different from a Lua screenshot at same input? → match Lua.

## Allowed deviations (and only these)

- **Persistence format**: Lua-tables → JSON (same key names; same hierarchy where possible).
- **Filesystem walks**: shell `find`/`ls` → `nftw`/`opendir`. Same recursion behavior, same hidden-skip rule.
- **Battery/volume/brightness**: muOS sysfs paths → generic Linux paths (`/sys/class/power_supply/BAT*`, ALSA, `/sys/class/backlight/*`).
- **Gamepad input**: gptokeyb2 process → SDL3 `SDL_OpenGamepad`. Translate buttons to same logical key strings the Lua code expects.
- **Video player**: shell-out `mpv` → embedded `libmpv` render API. Resume/watched semantics preserved.
- **Shaders**: gloss + blur removed (constraint: no shaders). All other rendering ports verbatim.
- **Storage tabs**: dual `/mnt/mmc` + `/mnt/sdcard` removed. Single `/userdata/medias`-rooted "Files" tab.
- **Default media dirs**: empty in Lua → default to `/userdata/medias/{music,videos,photos}` in port (user can still override).

Anything beyond this list = drift. Stop, re-read original, retry.

## Module port checklist (per-module, before marking done)

- [ ] Read full Lua source for module.
- [ ] List every function. Port each with same name (snake_case).
- [ ] List every constant. Copy verbatim.
- [ ] List every magic number / offset / alpha. Copy verbatim.
- [ ] List every external state mutated. Mirror in C++.
- [ ] No function added that wasn't in Lua (except platform glue, documented).
- [ ] No function silently dropped (if dropped, note in `docs/SKIPPED.md` with reason).
- [ ] Compiles.
- [ ] Visual diff vs original (when renderer ready).

## Reference paths

- Original Lua: `/home/romain/test/XMPlayer/.xmplayer/`
- Port: `/home/romain/REG-Player/src/`
- Assets (copy): `/home/romain/REG-Player/assets/`
