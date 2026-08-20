#!/bin/sh
set -eu

postinst=$1
test_root=${TMPDIR:-/tmp}/loim-postinst-test.$$
mock_bin=$test_root/bin
log_file=$test_root/runuser.log
mkdir -p "$mock_bin"

cleanup()
{
    rm -rf "$test_root"
}
trap cleanup EXIT HUP INT TERM

printf '%s\n' '#!/bin/sh' \
    'case "$2" in' \
    '  1200) echo "alice:x:1200:1200::/home/alice:/bin/bash" ;;' \
    '  1201) echo "bob:x:1201:1201::/home/bob:/bin/bash" ;;' \
    '  1202) echo "carol:x:1202:1202::/home/carol:/bin/bash" ;;' \
    '  *) exit 2 ;;' \
    'esac' > "$mock_bin/getent"
printf '%s\n' '#!/bin/sh' \
    'printf "%s\n" "$*" >> "$LOIM_TEST_RUNUSER_LOG"' > "$mock_bin/runuser"
printf '%s\n' '#!/bin/sh' 'exit 0' > "$mock_bin/update-desktop-database"
printf '%s\n' '#!/bin/sh' \
    'printf "icon-cache %s\n" "$*" >> "$LOIM_TEST_RUNUSER_LOG"' \
    > "$mock_bin/gtk-update-icon-cache"
printf '%s\n' '#!/bin/sh' \
    'if [ "${LOIM_TEST_ACTIVE_SESSION:-}" != yes ]; then exit 0; fi' \
    'if [ "$1" = list-sessions ]; then echo "7 1202 carol seat0 tty2"; exit 0; fi' \
    'case "$4" in' \
    '  Active) echo yes ;;' \
    '  Remote) echo no ;;' \
    '  Type) echo wayland ;;' \
    '  User) echo 1202 ;;' \
    'esac' > "$mock_bin/loginctl"
chmod 755 "$mock_bin/getent" "$mock_bin/runuser" \
    "$mock_bin/update-desktop-database" "$mock_bin/gtk-update-icon-cache" \
    "$mock_bin/loginctl"

: > "$log_file"
PATH="$mock_bin:$PATH" LOIM_TEST_RUNUSER_LOG="$log_file" \
    PKEXEC_UID=1200 SUDO_UID=1201 sh "$postinst"
grep -q '^-u alice -- /usr/bin/LoimReader --ensure-desktop-shortcut$' "$log_file"
grep -q '^icon-cache -q -t -f /usr/share/icons/hicolor$' "$log_file"

: > "$log_file"
(
    unset PKEXEC_UID
    PATH="$mock_bin:$PATH" LOIM_TEST_RUNUSER_LOG="$log_file" \
        SUDO_UID=1201 sh "$postinst"
)
grep -q '^-u bob -- /usr/bin/LoimReader --ensure-desktop-shortcut$' "$log_file"

: > "$log_file"
(
    unset PKEXEC_UID SUDO_UID
    PATH="$mock_bin:$PATH" LOIM_TEST_RUNUSER_LOG="$log_file" \
        LOIM_TEST_ACTIVE_SESSION=yes sh "$postinst"
)
grep -q '^-u carol -- /usr/bin/LoimReader --ensure-desktop-shortcut$' "$log_file"

: > "$log_file"
(
    unset PKEXEC_UID SUDO_UID
    PATH="$mock_bin:$PATH" LOIM_TEST_RUNUSER_LOG="$log_file" sh "$postinst"
)
! grep -q '^-u ' "$log_file"
