#!/bin/sh
set -eu

source_dir=${1:?source directory is required}
ci_workflow="$source_dir/.github/workflows/ci.yml"
release_workflow="$source_dir/.github/workflows/release.yml"

test -s "$ci_workflow"
test -s "$release_workflow"

# CI packages are diagnostic conveniences. A full account-level artifact quota
# must not turn successful builds into a false code failure.
grep -Fq 'continue-on-error: true' "$ci_workflow"
grep -Fq 'retention-days: 1' "$ci_workflow"

# The public edition uses its own artifact-based release workflow. Validate
# its publishing gates as well as the private edition's signed draft transport.
if ! grep -Fq 'prepare-release:' "$release_workflow"; then
    grep -Fq 'name: Release community edition' "$release_workflow"
    grep -Fq 'needs: build' "$release_workflow"
    grep -Fq 'actions/download-artifact@' "$release_workflow"
    grep -Fq 'scripts/create-sbom.sh' "$release_workflow"
    grep -Fq ' -eq 6' "$release_workflow"
    grep -Fq 'gh release create "$RELEASE_TAG"' "$release_workflow"
    exit 0
fi

# Production packages travel through a draft GitHub Release, not the
# quota-limited Actions artifact store. The release is published only after all
# six packages and signed metadata have been validated.
grep -Fq 'prepare-release:' "$release_workflow"
grep -Fq 'release_flags=(--draft)' "$release_workflow"
grep -Fq 'gh release create "$RELEASE_TAG" "${release_flags[@]}"' "$release_workflow"
grep -Fq 'gh release upload "$RELEASE_TAG"' "$release_workflow"
grep -Fq 'gh release download "$RELEASE_TAG"' "$release_workflow"
grep -Fq 'gh release edit "$RELEASE_TAG" --draft=false' "$release_workflow"
grep -Fq 'isDraft' "$release_workflow"

if grep -Fq 'actions/upload-artifact@' "$release_workflow" ||
   grep -Fq 'actions/download-artifact@' "$release_workflow"; then
    echo "Production release must not depend on Actions artifact quota" >&2
    exit 1
fi
