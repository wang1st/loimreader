#!/bin/sh
set -eu

source_dir=${1:?source directory is required}
cmake_file="$source_dir/CMakeLists.txt"
package_test="$source_dir/tests/test_linux_deb_compatibility.sh"
local_builder="$source_dir/scripts/build-linux-deb-in-container.sh"
ci_workflow="$source_dir/.github/workflows/ci.yml"

test -s "$package_test"
test -s "$local_builder"
grep -Fq 'set(CPACK_DEBIAN_PACKAGE_RECOMMENDS' "$cmake_file"
grep -Fq 'tests/test_linux_deb_compatibility.sh' "$local_builder"
grep -Fq 'cmake==3.28.4' "$local_builder"
grep -Eq '(^|[[:space:]\\])jq([[:space:]\\]|$)' "$local_builder"
if grep -Eq 'ldd[^|]*\|[[:space:]]*head' "$local_builder"; then
    echo "Local DEB builder must not truncate ldd under pipefail" >&2
    exit 1
fi
grep -Fq 'debian/eol:buster@sha256:' "$ci_workflow"
grep -Fq 'scripts/build-linux-deb-in-container.sh' "$ci_workflow"

set -- "$source_dir/.github/workflows/release.yml"
if [ -f "$source_dir/tools/community-release/release.yml" ]; then
    set -- "$@" "$source_dir/tools/community-release/release.yml"
fi
for workflow do
    test -s "$workflow"
    grep -Fq 'debian/eol:buster@sha256:' "$workflow"
    grep -Fq 'cmake==3.28.4' "$workflow"
    grep -Fq 'glibc 2.28' "$workflow"
    grep -Fq 'tests/test_linux_deb_compatibility.sh' "$workflow"
    grep -Fq 'cmake --build build --config Release --parallel 2' "$workflow"
    if grep -Fq 'ubuntu:20.04' "$workflow" ||
       grep -Fq 'glibc 2.31' "$workflow"; then
        echo "Legacy Linux release baseline remains in $workflow" >&2
        exit 1
    fi
done
