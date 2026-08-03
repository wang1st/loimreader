#!/usr/bin/env bash
set -euo pipefail

version="${RELEASE_VERSION:?RELEASE_VERSION is required}"
repository="${RELEASE_REPOSITORY:?RELEASE_REPOSITORY is required}"
run_id="${GITHUB_RUN_ID:-local}"
output="${1:-loimreader.spdx.json}"

if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?$ ]]; then
  echo "Invalid semantic version: $version" >&2
  exit 1
fi

created="$(date -u +'%Y-%m-%dT%H:%M:%SZ')"
namespace="https://ctdy123.com/spdx/loimreader/${version}/${run_id}"

jq -n \
  --arg version "$version" \
  --arg repository "$repository" \
  --arg created "$created" \
  --arg namespace "$namespace" \
  '{
    spdxVersion: "SPDX-2.3",
    dataLicense: "CC0-1.0",
    SPDXID: "SPDXRef-DOCUMENT",
    name: ("LoimReader-" + $version),
    documentNamespace: $namespace,
    creationInfo: {
      created: $created,
      creators: ["Tool: LoimReader-GitHub-Actions"]
    },
    packages: [
      {
        SPDXID: "SPDXRef-Package-LoimReader",
        name: "LoimReader",
        versionInfo: $version,
        downloadLocation: ("https://github.com/" + $repository),
        filesAnalyzed: false,
        licenseConcluded: "NOASSERTION",
        licenseDeclared: "NOASSERTION",
        copyrightText: "NOASSERTION"
      },
      {
        SPDXID: "SPDXRef-Package-SDL3",
        name: "SDL",
        versionInfo: "3.4.10",
        downloadLocation: "git+https://github.com/libsdl-org/SDL.git@8e37db5e797b6167f3a00d697d816a684bd259c7",
        filesAnalyzed: false,
        licenseConcluded: "Zlib",
        licenseDeclared: "Zlib",
        copyrightText: "Copyright SDL contributors"
      },
      {
        SPDXID: "SPDXRef-Package-SDL3-image",
        name: "SDL_image",
        versionInfo: "3.4.4",
        downloadLocation: "git+https://github.com/libsdl-org/SDL_image.git@bec9134a26c7d0f31b36d6083c25296e04cabff5",
        filesAnalyzed: false,
        licenseConcluded: "Zlib",
        licenseDeclared: "Zlib",
        copyrightText: "Copyright SDL_image contributors"
      },
      {
        SPDXID: "SPDXRef-Package-stb-image",
        name: "stb_image",
        versionInfo: "2.30",
        downloadLocation: "https://github.com/nothings/stb",
        filesAnalyzed: false,
        licenseConcluded: "MIT",
        licenseDeclared: "MIT",
        copyrightText: "Copyright (c) 2017 Sean Barrett"
      }
    ],
    relationships: [
      {
        spdxElementId: "SPDXRef-DOCUMENT",
        relationshipType: "DESCRIBES",
        relatedSpdxElement: "SPDXRef-Package-LoimReader"
      },
      {
        spdxElementId: "SPDXRef-Package-LoimReader",
        relationshipType: "DEPENDS_ON",
        relatedSpdxElement: "SPDXRef-Package-SDL3"
      },
      {
        spdxElementId: "SPDXRef-Package-LoimReader",
        relationshipType: "DEPENDS_ON",
        relatedSpdxElement: "SPDXRef-Package-SDL3-image"
      },
      {
        spdxElementId: "SPDXRef-Package-SDL3-image",
        relationshipType: "DEPENDS_ON",
        relatedSpdxElement: "SPDXRef-Package-stb-image"
      }
    ]
  }' > "$output"
