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
    done_file="$work_dir/done"
    log_file="$work_dir/app.log"
    property_file="$work_dir/window.properties"
    pid=
    watchdog_pid=
    app_status=0

    cleanup_child()
    {
        if [ "$pid" != "" ]; then
            kill "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
        fi
        if [ "$watchdog_pid" != "" ]; then
            kill "$watchdog_pid" 2>/dev/null || true
            wait "$watchdog_pid" 2>/dev/null || true
        fi
    }
    trap cleanup_child EXIT
    trap "exit 1" HUP INT TERM

    SDL_VIDEO_DRIVER=x11 SDL_RENDER_DRIVER=software \
        "$app" --window-icon-smoke-test "$done_file" >"$log_file" 2>&1 &
    pid=$!
    attempt=0
    found=false
    while [ "$attempt" -lt 300 ]; do
        if ! kill -0 "$pid" 2>/dev/null; then
            break
        fi
        if "$xprop" -name LoimReaderWindowIconSmoke \
            WM_CLASS >"$property_file" 2>/dev/null &&
           # Xlib exposes 32-bit CARDINALs in native unsigned-long slots.
           # Sixteen bytes includes width and height on 64-bit runners.
           "$xprop" -notype -f _NET_WM_ICON 32c -len 16 \
            -name LoimReaderWindowIconSmoke _NET_WM_ICON \
            >>"$property_file" 2>/dev/null &&
           grep -Eq "WM_CLASS.*\"com\\.ctdy123\\.loimreader\"" \
            "$property_file" &&
           grep -Eq "^_NET_WM_ICON =([[:space:]]+)?64, 64(,|$)" \
            "$property_file" &&
           ! grep -Fq "not found" "$property_file"; then
            found=true
            break
        fi
        attempt=$((attempt + 1))
        sleep 0.05
    done
    if [ "$found" != true ]; then
        cat "$log_file" >&2
        test ! -s "$property_file" || cat "$property_file" >&2
        exit 1
    fi
    : >"$done_file"
    (
        sleep 5
        kill -TERM "$pid" 2>/dev/null || true
        sleep 1
        kill -KILL "$pid" 2>/dev/null || true
    ) &
    watchdog_pid=$!
    wait "$pid" || app_status=$?
    pid=
    kill "$watchdog_pid" 2>/dev/null || true
    wait "$watchdog_pid" 2>/dev/null || true
    watchdog_pid=
    if [ "$app_status" -ne 0 ]; then
        echo "LoimReader did not finish the window icon smoke test cleanly" >&2
        cat "$log_file" >&2
        exit 1
    fi
    grep -Fq "window_icon=ready app_id=com.ctdy123.loimreader" "$log_file"
    grep -Fq "window_icon=pass app_id=com.ctdy123.loimreader" "$log_file"
' sh "$app" "$xprop" "$work_dir"
