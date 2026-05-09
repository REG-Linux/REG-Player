cmake_minimum_required(VERSION 3.20)
project(regplayer CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-function)

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(TAGLIB REQUIRED IMPORTED_TARGET taglib)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)
pkg_check_modules(XMP REQUIRED IMPORTED_TARGET libxmp)
pkg_check_modules(GME REQUIRED IMPORTED_TARGET libgme)
pkg_check_modules(OPUSFILE REQUIRED IMPORTED_TARGET opusfile)
find_package(ZLIB REQUIRED)

set(STSOUND_SRC
    third_party/stsound/digidrum.cpp
    third_party/stsound/Ym2149Ex.cpp
    third_party/stsound/Ymload.cpp
    third_party/stsound/YmMusic.cpp
    third_party/stsound/YmUserInterface.cpp
    third_party/stsound/LZH/LzhLib.cpp
)
add_library(stsound STATIC ${STSOUND_SRC})
target_include_directories(stsound PUBLIC ${CMAKE_SOURCE_DIR}/third_party/stsound)
target_compile_options(stsound PRIVATE -w)

add_library(csid STATIC third_party/tinysid/csid-light.c)
target_include_directories(csid PUBLIC ${CMAKE_SOURCE_DIR}/third_party/tinysid)
target_compile_options(csid PRIVATE -w -Wno-implicit-function-declaration)

set(REG_SRC
    src/main.cpp
    src/render.cpp
    src/theme.cpp
    src/categories.cpp
    src/utils.cpp
    src/assets.cpp
    src/ui.cpp
    src/background.cpp
    src/browser.cpp
    src/indexing.cpp
    src/metadata.cpp
    src/history.cpp
    src/video_manager.cpp
    src/music_player.cpp
    src/music_view.cpp
    src/image_viewer.cpp
    src/image_view.cpp
    src/settings.cpp
    src/settings_view.cpp
    src/system.cpp
    src/player.cpp
    src/xmb.cpp
    src/xmb_draw.cpp
    src/persist.cpp
    src/gamepad.cpp
    src/chiptune.cpp
    src/opus_audio.cpp
    src/miniaudio_impl.c
    src/stb_image_write_impl.c
    src/stb_image_impl.c
)

add_executable(regplayer ${REG_SRC})

target_include_directories(regplayer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(regplayer PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    PkgConfig::TAGLIB
    PkgConfig::MPV
    PkgConfig::ALSA
    PkgConfig::XMP
    PkgConfig::GME
    PkgConfig::OPUSFILE
    ZLIB::ZLIB
    stsound
    csid
    pthread
    m
)

target_compile_definitions(regplayer PRIVATE
    REG_ASSETS_DIR="${CMAKE_INSTALL_PREFIX}/share/regplayer/assets"
)

install(TARGETS regplayer RUNTIME DESTINATION bin)
install(DIRECTORY assets/ DESTINATION share/regplayer/assets)
install(FILES packaging/regplayer.desktop DESTINATION share/applications)
install(FILES packaging/regplayer.6       DESTINATION share/man/man6)
install(FILES assets/icons/music.png      DESTINATION share/icons/hicolor/64x64/apps RENAME regplayer.png)
install(FILES LICENSE CHANGELOG.md README.md NOTICES.md DESTINATION share/doc/regplayer)
