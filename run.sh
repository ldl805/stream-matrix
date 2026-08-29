#!/usr/bin/env bash
# StreamMatrix Smart Launcher for Raspberry Pi

# Fallback XDG_RUNTIME_DIR if missing
if [ -z "$XDG_RUNTIME_DIR" ]; then
    export XDG_RUNTIME_DIR="/run/user/$(id -u)"
fi

# Auto-detect Wayland and X11 display socket if running from SSH or terminal without exported GUI vars
if [ -z "$WAYLAND_DISPLAY" ] && [ -S "$XDG_RUNTIME_DIR/wayland-0" ]; then
    export WAYLAND_DISPLAY=wayland-0
fi

if [ -z "$DISPLAY" ]; then
    if [ -S "/tmp/.X11-unix/X0" ]; then
        export DISPLAY=:0
    elif [ -n "$WAYLAND_DISPLAY" ]; then
        export DISPLAY=:0
    fi
fi

# Allow Qt to use Wayland or fallback to X11 (xcb) seamlessly
if [ -z "$QT_QPA_PLATFORM" ]; then
    if [ -n "$WAYLAND_DISPLAY" ]; then
        export QT_QPA_PLATFORM="wayland;xcb"
    fi
fi

SCRIPT_SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SCRIPT_SOURCE" ]; do
    SCRIPT_DIR="$(cd "$(dirname "$SCRIPT_SOURCE")" && pwd)"
    SCRIPT_SOURCE="$(readlink "$SCRIPT_SOURCE")"
    [[ $SCRIPT_SOURCE != /* ]] && SCRIPT_SOURCE="$SCRIPT_DIR/$SCRIPT_SOURCE"
done
SCRIPT_DIR="$(cd "$(dirname "$SCRIPT_SOURCE")" && pwd)"

BINARY="$SCRIPT_DIR/build/stream-matrix"
if [ ! -f "$BINARY" ] && [ -f "/usr/bin/stream-matrix" ]; then
    BINARY="/usr/bin/stream-matrix"
fi

if [ ! -f "$BINARY" ]; then
    echo "StreamMatrix binary not found at $BINARY. Please build first." >&2
    exit 1
fi

exec "$BINARY" "$@"
