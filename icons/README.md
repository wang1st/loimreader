# LoimReader 图标资源

`loimreader.svg` 是 1024×1024 的唯一母版，采用暖红 `#E64A3B` 与暖白
`#FFF8F0`。图形表示一条纵向长纸带经过两道断口，形成三张轻微错位的
A4 页面；不包含文字、渐变或依赖字体的细节。

在 macOS 上运行 `scripts/generate-icons.sh` 可重复生成：

- `macos.icns`：16–1024px 的 Retina iconset，保留 macOS 安全边距与圆角底板。
- `windows.ico`：16、24、32、48、64、128、256px 多尺寸 ICO。
- `linux/*/loimreader.png`：16、24、32、48、64、128、256、512px PNG。
- `loimreader.png`：兼容现有构建引用的 256px PNG。

Linux 同时直接安装 SVG 母版，以便支持高分辨率启动器。
