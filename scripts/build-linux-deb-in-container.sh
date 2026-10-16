#!/usr/bin/env bash
set -euo pipefail

version=${1:?usage: build-linux-deb-in-container.sh VERSION amd64|arm64}
architecture=${2:?usage: build-linux-deb-in-container.sh VERSION amd64|arm64}
jobs=${LOIM_BUILD_JOBS:-2}

case "$architecture" in
    amd64|arm64) ;;
    *) echo "unsupported architecture: $architecture" >&2; exit 2 ;;
esac

host_architecture=$(dpkg --print-architecture)
if [[ "$host_architecture" != "$architecture" ]]; then
    echo "container architecture is $host_architecture, requested $architecture" >&2
    exit 2
fi
host_glibc=$(ldd --version 2>&1 |
    sed -n '1s/.* \([0-9][0-9.]*\)$/\1/p')
if [[ -z "$host_glibc" ]]; then
    echo "cannot determine container glibc version" >&2
    exit 2
fi
if dpkg --compare-versions "$host_glibc" gt 2.28; then
    echo "container glibc $host_glibc is too new; use debian/eol:buster" >&2
    exit 2
fi

export DEBIAN_FRONTEND=noninteractive
if [[ -n "${LOIM_APT_MIRROR:-}" ]]; then
    sed -i \
        -e "s|http://archive.ubuntu.com/ubuntu|${LOIM_APT_MIRROR}|g" \
        -e "s|http://security.ubuntu.com/ubuntu|${LOIM_APT_MIRROR}|g" \
        /etc/apt/sources.list
fi
apt-get update -qq
apt-get install -y -qq --no-install-recommends \
    build-essential ca-certificates curl desktop-file-utils \
    dpkg-dev file fonts-dejavu-core git jq python3 python3-pip \
    libcurl4-openssl-dev libdbus-1-dev libwayland-dev libx11-dev libxcursor-dev \
    libxext-dev libxfixes-dev libxi-dev libxkbcommon-dev libxrandr-dev \
    libxss-dev libxtst-dev xauth x11-utils xvfb
pip_arguments=(--no-cache-dir -q)
if [[ -n "${LOIM_PIP_INDEX_URL:-}" ]]; then
    pip_arguments+=(-i "$LOIM_PIP_INDEX_URL")
fi
python3 -m pip install "${pip_arguments[@]}" --upgrade pip==24.0
python3 -m pip install "${pip_arguments[@]}" cmake==3.28.4
export PATH="/usr/local/bin:$PATH"

build_directory="build-linux-${architecture}-local"
distribution_directory="dist-local-${architecture}"
cmake_arguments=(
    -DLOIM_BUILD_LEGACY_QT=OFF \
    -DLOIM_BUILD_DESKTOP=ON \
    -DBUILD_TESTING=ON \
    -DLOIM_ENABLE_STRICT_WARNINGS=ON \
    -DLOIM_VERSION_OVERRIDE="$version" \
    -DLOIM_RELEASE_PLATFORM=linux \
    -DLOIM_RELEASE_ARCH="$architecture" \
    -DCMAKE_BUILD_TYPE=Release
)
if [[ -d vendor/sdl3 && -d vendor/sdl3_image && -d vendor/sdl3_ttf ]]; then
    cmake_arguments+=(
        -DFETCHCONTENT_SOURCE_DIR_SDL3=/workspace/vendor/sdl3
        -DFETCHCONTENT_SOURCE_DIR_SDL3_IMAGE=/workspace/vendor/sdl3_image
        -DFETCHCONTENT_SOURCE_DIR_SDL3_TTF=/workspace/vendor/sdl3_ttf
    )
fi
cmake -S . -B "$build_directory" "${cmake_arguments[@]}"
cmake --build "$build_directory" --parallel "$jobs"
ctest --test-dir "$build_directory" --output-on-failure

desktop-file-validate packaging/linux/com.ctdy123.loimreader.desktop
mkdir -p "$distribution_directory"
cpack --config "$build_directory/CPackConfig.cmake" \
    -C Release -B "$distribution_directory"
package="$distribution_directory/LoimReader_${version}_linux_${architecture}.deb"
test -s "$package"

control_directory=$(mktemp -d)
extract_directory=$(mktemp -d)
dpkg-deb --control "$package" "$control_directory"
test -x "$control_directory/postinst"
grep -Fq -- '--ensure-desktop-shortcut' "$control_directory/postinst"
dpkg-deb --extract "$package" "$extract_directory"
sh tests/test_linux_deb_compatibility.sh \
    "$package" "$extract_directory/usr/bin/LoimReader" "$architecture"
if find "$extract_directory" -type f \
    \( -iname '*qt*' -o -iname '*poppler*' \) | grep -q .; then
    echo 'Legacy Qt/Poppler file found in Linux package' >&2
    exit 1
fi
cmake \
    -DLOIM_BINARY="$extract_directory/usr/bin/LoimReader" \
    -DLOIM_STAGE_ROOT="$extract_directory/usr" \
    -P scripts/check-runtime-dependencies.cmake
desktop-file-validate \
    "$extract_directory/usr/share/applications/com.ctdy123.loimreader.desktop"
for size in 16 24 32 48 64 128 256 512; do
    test -f \
        "$extract_directory/usr/share/icons/hicolor/${size}x${size}/apps/com.ctdy123.loimreader.png"
done
test -f \
    "$extract_directory/usr/share/icons/hicolor/scalable/apps/com.ctdy123.loimreader.svg"

# A container has no active graphical user. The general DEB must still install
# successfully and its best-effort postinst must return zero in this case.
dpkg -i "$package"

echo "Validated $package"
