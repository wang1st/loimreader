#!/bin/sh
set -eu

source_dir=${1:?source directory is required}
eula="$source_dir/packaging/windows/LoimReader-EULA.txt"
ui="$source_dir/packaging/windows/LoimReaderUI.wxs"
patch="$source_dir/packaging/windows/LoimReaderInstallerPatch.xml"
template="$source_dir/packaging/windows/LoimReaderWix.template.in"
instructions="$source_dir/packaging/windows/安装说明.txt"

for file in "$eula" "$ui" "$patch" "$template" "$instructions"; do
    test -s "$file"
done

grep -Fq 'LoimReader 最终用户许可协议' "$eula"
grep -Fq 'LoimReader End User License Agreement' "$eula"
grep -Fq 'Copyright © 2024 Ctdy123.com' "$eula"
grep -Fq 'CREATE_DESKTOP_SHORTCUT' "$ui"
grep -Fq 'CM_SHORTCUT_DESKTOP_Runtime' "$patch"
grep -Fq 'Codepage="65001"' "$template"
grep -Fq 'Language="2052"' "$template"
grep -Fq 'CPACK_WIX_CULTURES "zh-CN"' "$source_dir/CMakeLists.txt"
grep -Fq 'WixUnelevatedShellExec' "$patch"
grep -Fq 'WIXUI_EXITDIALOGOPTIONALCHECKBOX' "$patch"
grep -Fq 'Value="立即运行 LoimReader"' "$patch"
grep -Fq 'Title="LoimReader 安装程序"' "$ui"
grep -Fq 'Text="安装选项"' "$ui"
if grep -Fq 'Installation options /' "$ui" ||
   grep -Fq 'Launch LoimReader now /' "$patch"; then
    echo 'Windows installer contains bilingual fallback UI instead of Chinese UI' >&2
    exit 1
fi
grep -Fq '完整解压' "$instructions"

if command -v xmllint >/dev/null 2>&1; then
    xmllint --noout "$ui" "$patch"
fi
