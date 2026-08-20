#!/bin/sh
set -eu

source_dir=${1:?source directory is required}
store="$source_dir/src/desktop/credential_store.c"
cmake_file="$source_dir/CMakeLists.txt"

test -s "$store"
grep -Fq 'CredWriteW' "$store"
grep -Fq 'CRED_PERSIST_LOCAL_MACHINE' "$store"
grep -Fq 'SecItemUpdate' "$store"
grep -Fq 'kSecClassGenericPassword' "$store"
grep -Fq 'secret-tool store' "$store"
grep -Fq 'libsecret-tools' "$cmake_file"

if grep -Eq 'SDL_SaveFile|fopen\(|password=' "$store"; then
    echo 'credential store must not fall back to a plaintext file' >&2
    exit 1
fi
