# LoimReader（影谷长图阅读器）

LoimReader 3.0 是面向 Windows、Linux、macOS 的 C17 桌面重构。默认发行路径不再链接 Qt 或 Poppler-Qt；Qt 2.x 源码仅作为行为和视觉对照，必须显式开启才会构建。

当前模板已经具备：

- 一次选择或拖入多张 PNG、JPEG、GIF、BMP 图片；单项失败不影响其余文件。
- 不生成巨型拼接位图的虚拟长文档，按需渲染每个源图片切片。
- 优先使用图片边界、空白区域与人工分割线的智能分页。
- 尽量复刻原程序的浅灰工具栏、双画布、可拖动中缝、分页线、页码、双列、边距和缩放交互。
- Windows、Linux、macOS 的 amd64/arm64 六目标 CI、打包、SBOM、签名 Release 与 ctdy123.com 版本登记工作流。

PDF 导出、系统打印与账号登录为专业版（Pro）功能；社区版中点击这些按钮会提示前往 ctdy123.com 获取专业版。两个版本共用同一份源码，见下文「社区版与专业版」。

## 本地构建

```bash
cmake -S . -B build \
  -DLOIM_BUILD_LEGACY_QT=OFF \
  -DLOIM_BUILD_DESKTOP=ON \
  -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

批量分页规划也可以独立运行：

```bash
./build/loimreader-plan image-1.png image-2.jpg
```

## 社区版与专业版

同一份源码构建两个发行版：

- **社区版（默认）**：长图阅读、智能分页、双画布预览、页码、边距、缩放与配置持久化全部可用；PDF 导出、打印、账号登录按钮会提示前往官网获取专业版。
- **专业版**：配置时加 `-DLOIM_EDITION_PRO=ON`，额外解锁 PDF 导出、打印与账号功能。

专业版代码以 `LOIM_EDITION_PRO` 预处理守卫隔离；公开的社区版源码树由剥离工具自动生成（移除守卫代码、`src/core/pdf.c`、`include/loim/pdf.h`、发布工作流与内部脚本），并经过完整构建与测试验证。

## 架构与发布

- [系统设计](docs/architecture/SYSTEM_DESIGN.md)
- [依赖与许可策略](docs/architecture/DEPENDENCY_POLICY.md)
- [架构决策记录](docs/adr/)
- [GitHub 自动发布与 ctdy123.com 登记](docs/architecture/GITHUB_RELEASE_SETUP.md)

发布工作流只有在六个平台产物全部通过测试后，才会创建签名清单并调用 ctdy123.com 的专用 API。该链路不持有 ECS root 凭据，不同步服务器目录，也不重启与 z-pulse.cn 共享的 Nginx 或其他服务。

## 旧版对照构建

只有迁移核对时才使用：

```bash
cmake -S . -B build-legacy -DLOIM_BUILD_LEGACY_QT=ON
```

旧版脚本、说明和 Qt 源码不进入 3.0 GitHub Release。任何旧 OSS 凭据都必须轮换，真实密钥只能保存在 GitHub Environment、网站进程环境或工作区外的受限密钥文件中。

## 许可

发行依赖及许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。当前 SDL3、SDL3_image 与 stb 均采用宽松许可；最终发行前仍应以实际二进制依赖扫描和法律审查为准。
