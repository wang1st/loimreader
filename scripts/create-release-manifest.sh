#!/usr/bin/env bash
set -euo pipefail

artifact_dir="${1:?artifact directory is required}"
output_file="${2:?output path is required}"
version="${RELEASE_VERSION:?RELEASE_VERSION is required}"
tag="${RELEASE_TAG:?RELEASE_TAG is required}"
repository="${RELEASE_REPOSITORY:?RELEASE_REPOSITORY is required}"
channel="stable"
if [[ "$version" == *-* ]]; then
  channel="beta"
fi
temporary_items="$(mktemp)"
trap 'rm -f "$temporary_items"' EXIT

sha256_file() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  else
    shasum -a 256 "$1" | awk '{print $1}'
  fi
}

for target in \
  linux:amd64:tar.gz \
  linux:arm64:tar.gz \
  windows:amd64:zip \
  windows:arm64:zip \
  macos:amd64:tar.gz \
  macos:arm64:tar.gz
do
  IFS=: read -r platform arch extension <<< "$target"
  filename="LoimReader_${version}_${platform}_${arch}.${extension}"
  path="${artifact_dir}/${filename}"
  if [[ ! -f "$path" ]]; then
    echo "Missing required release package: $filename" >&2
    exit 1
  fi
  sha256="$(sha256_file "$path")"
  size_bytes="$(wc -c < "$path" | tr -d ' ')"
  jq -n \
    --arg platform "$platform" \
    --arg arch "$arch" \
    --arg format "$extension" \
    --arg filename "$filename" \
    --arg url "https://github.com/${repository}/releases/download/${tag}/${filename}" \
    --arg sha256 "$sha256" \
    --argjson sizeBytes "$size_bytes" \
    '{platform: $platform, arch: $arch, format: $format, filename: $filename,
      url: $url, sha256: $sha256, sizeBytes: $sizeBytes}' >> "$temporary_items"
done

jq -s \
  --arg version "$version" \
  --arg tag "$tag" \
  --arg repository "$repository" \
  --arg channel "$channel" \
  --arg releaseUrl "https://github.com/${repository}/releases/tag/${tag}" \
  '{schemaVersion: 2, clientName: "LoimReader", version: $version,
    channel: $channel, releaseTag: $tag, repository: $repository,
    releaseUrl: $releaseUrl, forceUpdate: false, artifacts: .}' \
  "$temporary_items" > "$output_file"
