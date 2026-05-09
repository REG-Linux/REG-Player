#!/bin/sh
# Launch wrapper for handheld distros (batocera-style).
# Works whether regplayer is installed system-wide or extracted from a portable tarball.

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"

# If running from extracted tarball with bundled assets + libs:
if [ -d "$SCRIPT_DIR/assets" ]; then
    export REG_ASSETS_DIR="$SCRIPT_DIR/assets"
fi
if [ -d "$SCRIPT_DIR/lib" ]; then
    export LD_LIBRARY_PATH="$SCRIPT_DIR/lib:$LD_LIBRARY_PATH"
fi

# Fallback to system install
if [ -z "$REG_ASSETS_DIR" ] && [ -d /usr/local/share/regplayer/assets ]; then
    export REG_ASSETS_DIR=/usr/local/share/regplayer/assets
fi

# Refresh ld cache if installing system-wide for first time
ldconfig -p | grep -q "libSDL3_image" 2>/dev/null || ldconfig 2>/dev/null

if [ -x "$SCRIPT_DIR/regplayer" ]; then
    exec "$SCRIPT_DIR/regplayer" "$@"
else
    exec regplayer "$@"
fi
