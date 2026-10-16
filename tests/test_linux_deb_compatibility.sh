#!/bin/sh
set -eu

package=${1:?Debian package path is required}
binary=${2:?installed or staged ELF path is required}
expected_architecture=${3:?expected Debian architecture is required}
max_glibc=2.28

for tool in dpkg dpkg-deb objdump; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "Missing required compatibility test tool: $tool" >&2
        exit 1
    }
done

test -s "$package"
test -x "$binary"

actual_architecture=$(dpkg-deb --field "$package" Architecture)
if [ "$actual_architecture" != "$expected_architecture" ]; then
    echo "DEB architecture is $actual_architecture, expected $expected_architecture" >&2
    exit 1
fi

host_architecture=$(dpkg --print-architecture)
if [ "$host_architecture" != "$expected_architecture" ]; then
    echo "Build container architecture is $host_architecture, expected $expected_architecture" >&2
    exit 1
fi

depends=$(dpkg-deb --field "$package" Depends)
recommends=$(dpkg-deb --field "$package" Recommends 2>/dev/null || true)
libc_requirement=$(printf '%s\n' "$depends" |
    sed -n 's/.*libc6 (>= \([^)]*\)).*/\1/p')
if [ "$libc_requirement" = '' ]; then
    echo "DEB does not declare its generated libc6 requirement: $depends" >&2
    exit 1
fi
if dpkg --compare-versions "$libc_requirement" gt "$max_glibc"; then
    echo "DEB requires libc6 >= $libc_requirement, above UOS V20 glibc $max_glibc" >&2
    exit 1
fi

for optional_package in fonts-noto-cjk xdg-utils libsecret-tools zenity xdg-desktop-portal; do
    case ",$depends," in
        *",$optional_package,"*|*", $optional_package,"*)
            echo "$optional_package must be a recommendation, not a hard dependency" >&2
            exit 1
            ;;
    esac
    case ",$recommends," in
        *",$optional_package,"*|*", $optional_package,"*) ;;
        *)
            echo "Missing recommended package $optional_package: $recommends" >&2
            exit 1
            ;;
    esac
done

max_required_glibc=$(objdump -T "$binary" |
    grep -oE 'GLIBC_[0-9]+(\.[0-9]+)*' |
    sed 's/^GLIBC_//' |
    sort -V |
    tail -1)
if [ "$max_required_glibc" = '' ]; then
    echo "Could not determine the binary GLIBC requirement" >&2
    exit 1
fi
if dpkg --compare-versions "$max_required_glibc" gt "$max_glibc"; then
    echo "Binary requires GLIBC_$max_required_glibc, above UOS V20 $max_glibc" >&2
    exit 1
fi

printf 'deb_compatibility=pass arch=%s libc=%s glibc=%s\n' \
    "$actual_architecture" "$libc_requirement" "$max_required_glibc"
