#!/bin/sh
set -eu

app=${1:?application path is required}
xvfb_run=${2:?xvfb-run path is required}
xprop=${3:?xprop path is required}
work_dir=$(mktemp -d)

cleanup()
{
    rm -rf "$work_dir"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

"$xvfb_run" -a \
    -s "-screen 0 1280x1024x24 -extension BIG-REQUESTS" \
    sh -eu -c '
    app=$1
    xprop=$2
    work_dir=$3
    log_file="$work_dir/app.log"
    property_file="$work_dir/window.properties"
    pid=

    cleanup_child()
    {
        if [ "$pid" != "" ]; then
            kill "$pid" 2>/dev/null || true
            (
                sleep 2
                kill -KILL "$pid" 2>/dev/null || true
            ) &
            killer_pid=$!
            wait "$pid" 2>/dev/null || true
            kill "$killer_pid" 2>/dev/null || true
            wait "$killer_pid" 2>/dev/null || true
        fi
    }
    trap cleanup_child EXIT
    trap "exit 1" HUP INT TERM

    # Exercise the installed production entry point without internal test flags.
    # Keep user preferences and credentials outside the runner account profile.
    LOIMREADER_LANG=en XDG_CONFIG_HOME="$work_dir/config" \
        XDG_DATA_HOME="$work_dir/data" \
        SDL_VIDEO_DRIVER=x11 SDL_RENDER_DRIVER=software \
        "$app" >"$log_file" 2>&1 &
    pid=$!
    attempt=0
    while [ "$attempt" -lt 300 ]; do
        if ! kill -0 "$pid" 2>/dev/null; then
            break
        fi
        if "$xprop" -name LoimReader WM_CLASS >"$property_file" 2>/dev/null &&
           # Xlib exposes 32-bit CARDINALs in native unsigned-long slots.
           # Sixteen bytes includes width and height on 64-bit runners.
           "$xprop" -notype -f _NET_WM_ICON 32c -len 16 \
            -name LoimReader _NET_WM_ICON >>"$property_file" 2>/dev/null &&
           grep -Eq "WM_CLASS.*\"com\\.ctdy123\\.loimreader\"" \
            "$property_file" &&
           grep -Eq "^_NET_WM_ICON =([[:space:]]+)?64, 64(,|$)" \
            "$property_file" &&
           ! grep -Fq "not found" "$property_file"; then
            echo "window_icon=pass app_id=com.ctdy123.loimreader"
            exit 0
        fi
        attempt=$((attempt + 1))
        sleep 0.05
    done
    cat "$log_file" >&2
    test ! -s "$property_file" || cat "$property_file" >&2
    exit 1
' sh "$app" "$xprop" "$work_dir"
