# 依赖与发行许可策略

1. 新核心代码只使用 C17 标准库；UI 和编解码依赖必须是 MIT、BSD、ISC、zlib、Apache-2.0 或经法律复核的等价宽松许可。
2. Qt、Poppler-Qt、GPL-only 组件和未知许可代码不得出现在 3.0 发行链接图中。
3. 所有第三方版本使用不可变标签或提交哈希，发布时生成 CycloneDX/SPDX SBOM、许可证文本和源码地址。
4. 每个平台对最终二进制执行依赖扫描：Windows 检查 PE imports，Linux 检查 ELF NEEDED，macOS 检查 Mach-O linked dylibs。
5. CI 发现 `Qt5*`、`Qt6*`、`poppler-qt*` 或未登记动态库时阻止发布。
6. 仓库内现有 `include/poppler-*.h` 只属于 legacy 迁移输入，在许可来源确认前不复制到新模块。
7. “宽松许可”不等于无义务；版权声明、许可证文本、商标和专利条款仍进入安装包。

本策略是工程控制，不构成法律意见。正式商业发行前应由有资质的法律顾问核对最终 SBOM 和分发方式。
