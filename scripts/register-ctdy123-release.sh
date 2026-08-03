#!/usr/bin/env bash
set -euo pipefail

manifest_file="${MANIFEST_FILE:-release-manifest.json}"
signature_file="${SIGNATURE_FILE:-release-manifest.sig.base64}"
base_url="${CTDY123_BASE_URL:-https://ctdy123.com}"
release_version="${RELEASE_VERSION:?RELEASE_VERSION is required}"
release_token="${CTDY123_RELEASE_TOKEN:?CTDY123_RELEASE_TOKEN is required}"
artifact_dir="${ARTIFACT_DIR:-dist}"
verify_attempts="${CTDY123_VERIFY_ATTEMPTS:-18}"
verify_delay_seconds="${CTDY123_VERIFY_DELAY_SECONDS:-10}"

if [[ ! "$verify_attempts" =~ ^[1-9][0-9]*$ ]] ||
   [[ ! "$verify_delay_seconds" =~ ^[0-9]+$ ]]; then
  echo "Invalid ctdy123 verification retry configuration" >&2
  exit 1
fi
if [[ ! -s "$manifest_file" || ! -s "$signature_file" ]]; then
  echo "Signed release metadata is missing" >&2
  exit 1
fi

manifest_version="$(jq -er '.version' "$manifest_file")"
manifest_targets="$(jq -er '.artifacts | length' "$manifest_file")"
if [[ "$manifest_version" != "$release_version" || "$manifest_targets" != 6 ]]; then
  echo "Release metadata does not match the requested six-target version" >&2
  exit 1
fi

signature="$(tr -d '\r\n' < "$signature_file")"
if [[ ! "$signature" =~ ^[A-Za-z0-9+/]{86}==$ ]]; then
  echo "Release signature is not canonical Ed25519 base64" >&2
  exit 1
fi

response_file="$(mktemp)"
prepare_file="$(mktemp)"
trap 'rm -f "$response_file" "$prepare_file"' EXIT

curl --fail-with-body --silent --show-error \
  --retry 4 --retry-all-errors \
  --output "$prepare_file" \
  --request POST \
  --header "Authorization: Bearer $release_token" \
  --header "X-Release-Signature: $signature" \
  --header "Content-Type: application/json" \
  --header "Idempotency-Key: github-${GITHUB_RUN_ID:-manual}-${GITHUB_RUN_ATTEMPT:-1}-prepare" \
  --data-binary "@$manifest_file" \
  "$base_url/api/admin/versions/uploads/prepare"

if ! jq -e --arg version "$release_version" \
  '.success == true and .version == $version and (.uploads | length) == 6' \
  "$prepare_file" >/dev/null; then
  echo "ctdy123 rejected the artifact upload preparation request" >&2
  jq '{success, error, message, version}' "$prepare_file" >&2
  exit 1
fi

while IFS=$'\t' read -r filename upload_url content_type; do
  artifact_path="${artifact_dir}/${filename}"
  if [[ ! -s "$artifact_path" ]]; then
    echo "Missing release artifact for ctdy123 upload: $filename" >&2
    exit 1
  fi
  echo "Uploading verified release artifact to ctdy123 storage: $filename"
  curl --fail-with-body --silent --show-error \
    --retry 4 --retry-all-errors \
    --request PUT \
    --header "Content-Type: $content_type" \
    --upload-file "$artifact_path" \
    "$upload_url" >/dev/null
done < <(jq -r '.uploads[] | [.filename, .uploadUrl, .contentType] | @tsv' "$prepare_file")

curl --fail-with-body --silent --show-error \
  --retry 4 --retry-all-errors \
  --output "$response_file" \
  --request POST \
  --header "Authorization: Bearer $release_token" \
  --header "X-Release-Signature: $signature" \
  --header "Content-Type: application/json" \
  --header "Idempotency-Key: github-${GITHUB_RUN_ID:-manual}-${GITHUB_RUN_ATTEMPT:-1}" \
  --data-binary "@$manifest_file" \
  "$base_url/api/admin/versions/publish"

if ! jq -e --arg version "$release_version" \
  '.success == true and .version == $version' "$response_file" >/dev/null; then
  echo "ctdy123 rejected the signed release metadata" >&2
  jq '{success, error, message, version}' "$response_file" >&2
  exit 1
fi

for ((attempt = 1; attempt <= verify_attempts; attempt += 1)); do
  verify_url="${base_url}/api/client/version/info/loimreader?release=${release_version}-${GITHUB_RUN_ID:-manual}-${attempt}"
  if response="$(curl --fail --silent --show-error \
      --retry 2 --retry-all-errors \
      --header 'Cache-Control: no-cache' \
      "$verify_url")"; then
    observed_version="$(jq -r '.version // empty' <<< "$response")"
    observed_targets="$(jq -r '(.artifacts // []) | length' <<< "$response")"
    ctdy_urls="$(jq -r \
      '[.artifacts[]? | select(.url | startswith("https://ctdy123.com/download/loimreader/"))] | length' \
      <<< "$response")"
    if [[ "$observed_version" == "$release_version" && "$observed_targets" == 6 &&
          "$ctdy_urls" == 6 ]]; then
      all_downloads_ready=true
      while IFS= read -r download_url; do
        if ! curl --fail --silent --show-error --location \
            --retry 2 --retry-all-errors --output /dev/null "$download_url"; then
          all_downloads_ready=false
          break
        fi
      done < <(jq -r '.artifacts[].url' <<< "$response")
      if [[ "$all_downloads_ready" == true ]]; then
        echo "ctdy123 release verified: $release_version (six native installers)"
        exit 0
      fi
    fi
  fi
  if ((attempt < verify_attempts)); then
    sleep "$verify_delay_seconds"
  fi
done

echo "ctdy123 public endpoint did not expose $release_version with six targets" >&2
exit 1
