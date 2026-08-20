#!/bin/sh
set -eu

source_dir=${1:?source directory is required}
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM

mkdir -p "$work_dir/dist"
for package in \
    LoimReader_3.0.3_linux_amd64.deb \
    LoimReader_3.0.3_linux_arm64.deb \
    LoimReader_3.0.3_windows_amd64.zip \
    LoimReader_3.0.3_windows_arm64.zip \
    LoimReader_3.0.3_macos_amd64.dmg \
    LoimReader_3.0.3_macos_arm64.dmg
do
    printf 'fixture:%s\n' "$package" > "$work_dir/dist/$package"
done

RELEASE_VERSION=3.0.3 \
RELEASE_TAG=v3.0.3 \
RELEASE_REPOSITORY=wang1st/loimreader \
    "$source_dir/scripts/create-release-manifest.sh" \
    "$work_dir/dist" "$work_dir/release-manifest.json"

jq -e '
    .version == "3.0.3" and
    (.artifacts | length) == 6 and
    ([.artifacts[] | select(.platform == "windows")] | length) == 2 and
    ([.artifacts[] | select(
        .platform == "windows" and
        .format == "zip" and
        (.filename | endswith(".zip")) and
        (.url | endswith(".zip"))
    )] | length) == 2 and
    ([.artifacts[] | select(.filename | endswith(".msi"))] | length) == 0
' "$work_dir/release-manifest.json" >/dev/null

if RELEASE_VERSION=3.0.3 \
    RELEASE_TAG=v3.0.3 \
    RELEASE_REPOSITORY=wang1st/loimreader \
    "$source_dir/scripts/create-release-manifest.sh" \
    "$work_dir/dist" "$work_dir/second-manifest.json" 2>/dev/null
then
    :
else
    echo 'ZIP-only release manifest generation must be repeatable' >&2
    exit 1
fi
