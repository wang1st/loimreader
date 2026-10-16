#!/bin/sh
set -eu

source_dir=${1:?source directory is required}
desktop_entry="$source_dir/packaging/linux/com.ctdy123.loimreader.desktop"
main_source="$source_dir/src/desktop/main.c"
cmake_file="$source_dir/CMakeLists.txt"
runtime_test="$source_dir/tests/test_linux_window_icon_runtime.sh"

test -s "$desktop_entry"
test -s "$source_dir/icons/linux/64x64/loimreader.png"
test -s "$runtime_test"

# The Wayland app ID, X11 WM_CLASS, desktop-entry filename and themed icon
# must all resolve to the same reverse-domain identifier.
grep -Fxq 'Icon=com.ctdy123.loimreader' "$desktop_entry"
grep -Fxq 'StartupWMClass=com.ctdy123.loimreader' "$desktop_entry"
grep -Fq '"com.ctdy123.loimreader"' "$main_source"

# X11 taskbars do not all resolve the desktop entry from WM_CLASS reliably.
# Keep a programmatic window icon sourced from the same packaged PNG.
grep -Fq 'app_set_linux_window_icon' "$main_source"
grep -Fq 'SDL_SetWindowIcon' "$main_source"
grep -Fq 'SDL_HINT_APP_ID' "$main_source"
grep -Fq -- '--window-icon-smoke-test' "$main_source"
grep -Fq 'com.ctdy123.loimreader.png' "$main_source"
grep -Fq 'icons/hicolor/64x64/apps/com.ctdy123.loimreader.png' "$main_source"
grep -Fq 'icons/linux/64x64/loimreader.png' "$cmake_file"
grep -Fq '"${CMAKE_CURRENT_BINARY_DIR}/com.ctdy123.loimreader.png"' "$cmake_file"
grep -Fq 'RENAME "com.ctdy123.loimreader.png"' "$cmake_file"
grep -Fq 'set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")' "$cmake_file"
grep -Fq '_NET_WM_ICON' "$runtime_test"
grep -Fq -- '-extension BIG-REQUESTS' "$runtime_test"
grep -Fq '64, 64' "$runtime_test"
