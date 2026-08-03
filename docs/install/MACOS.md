## macOS 安装说明（当前版本未签名）

> GitHub 页面底部自动显示的 **Source code (tar.gz)** 是源码压缩包，不是 macOS 安装包。请从 Assets 中下载文件名以 `.dmg` 结尾的文件。

### 应该下载哪个 DMG？

- Apple 芯片（M1、M2、M3、M4 等）：`macos_arm64.dmg`
- Intel 芯片：`macos_amd64.dmg`

### 安装与首次打开

1. 双击 DMG，将 `LoimReader.app` 拖入 `Applications`（应用程序）。
2. 在 Finder 的“应用程序”中，按住 Control 键点按 LoimReader，选择“打开”。
3. 若仍被阻止，前往“系统设置 → 隐私与安全”，在安全性区域选择“仍要打开”。

如果系统提示“应用已损坏”，请先确认文件来自 [ctdy123 官方下载页](https://ctdy123.com/download/loimreader)或本 GitHub Release，并核对 SHA-256。然后在终端执行：

```bash
xattr -dr com.apple.quarantine "/Applications/LoimReader.app"
```

该命令只移除 LoimReader 的下载隔离属性，不会全局关闭 Gatekeeper。完整说明也包含在 DMG 根目录的 `macOS 安装说明.txt` 中。
