# 影谷构建脚本使用指南

本项目提供了多个自动化脚本来简化构建、签名和运行流程。

## 📚 脚本概览

### 🔧 基础构建脚本

| 脚本名称 | 用途 | 特点 |
|---------|------|------|
| `build_cmake.sh` | CMake构建 | 完整的构建过程，详细输出 |
| `fix_cmake_signing.sh` | 代码签名修复 | 解决macOS Sequoia签名问题 |

### 🚀 自动化脚本

| 脚本名称 | 用途 | 特点 |
|---------|------|------|
| `auto_build_and_run.sh` | 完整自动化 | 构建+签名+运行，带详细状态 |
| `quick_run.sh` | 快速开发 | 简洁版本，适合日常开发 |

## 🎯 使用方法

### 1. 完整自动化脚本 (`auto_build_and_run.sh`)

**功能**: 一键完成构建、签名修复、应用启动的完整流程

```bash
# 标准构建和运行
./auto_build_and_run.sh

# 重新构建（清理后构建）
./auto_build_and_run.sh --rebuild

# 构建但不自动打开应用
./auto_build_and_run.sh --no-open

# 跳过代码签名（仅调试时使用）
./auto_build_and_run.sh --skip-signing

# 组合使用
./auto_build_and_run.sh -r -n  # 重建但不打开
```

**优势**:
- ✅ 完整的错误检查和状态报告
- ✅ 详细的构建信息显示
- ✅ 智能的应用进程管理
- ✅ 多种运行模式选择

### 2. 快速开发脚本 (`quick_run.sh`)

**功能**: 适合日常开发的简洁版本

```bash
# 快速构建和运行
./quick_run.sh

# 强制重建
./quick_run.sh rebuild
# 或
./quick_run.sh -r
```

**优势**:
- 🚀 执行速度快
- 📝 输出简洁
- 🔄 智能增量构建
- 📋 错误日志记录

### 3. 基础构建脚本

```bash
# 仅构建应用
./build_cmake.sh

# 清理后重新构建
./build_cmake.sh rebuild

# 仅修复代码签名
./fix_cmake_signing.sh
```

## 🛠️ 项目结构

```
longimgprint/
├── CMakeLists.txt              # CMake构建配置
├── Info.plist.in              # macOS应用信息模板
├── build_cmake.sh             # 基础CMake构建脚本
├── fix_cmake_signing.sh       # 代码签名修复脚本
├── auto_build_and_run.sh      # 完整自动化脚本
├── quick_run.sh               # 快速开发脚本
├── build_cmake/               # CMake构建目录
├── dist_cmake/                # 应用输出目录
│   └── ctdy123.app           # 最终应用程序
├── build.log                  # 构建日志（由quick_run.sh生成）
└── signing.log               # 签名日志（由quick_run.sh生成）
```

## 📋 依赖要求

- **CMake** >= 3.25
- **Qt6** (安装在 `/opt/homebrew/lib`)
- **Ninja** 或 **Make**
- **Xcode命令行工具**

## 🔍 故障排除

### 构建失败
```bash
# 查看详细构建日志
cat build.log

# 检查Qt6安装
ls /opt/homebrew/lib/Qt*.framework

# 强制重新构建
./auto_build_and_run.sh --rebuild
```

### 代码签名问题
```bash
# 查看签名日志
cat signing.log

# 手动修复签名
./fix_cmake_signing.sh

# 验证签名
codesign --verify --deep dist_cmake/ctdy123.app
```

### 应用启动失败
```bash
# 检查应用是否存在
ls -la dist_cmake/ctdy123.app

# 检查可执行权限
ls -la dist_cmake/ctdy123.app/Contents/MacOS/ctdy123

# 手动启动查看错误
open dist_cmake/ctdy123.app
```

## 🎨 开发工作流建议

### 日常开发
```bash
# 修改代码后快速测试
./quick_run.sh
```

### 完整验证
```bash
# 重新构建并完整测试
./auto_build_and_run.sh --rebuild
```

### 发布准备
```bash
# 完整构建，不自动打开（用于发布前验证）
./auto_build_and_run.sh --rebuild --no-open
```

## 🔧 自定义配置

### 修改Qt路径
编辑 `CMakeLists.txt`:
```cmake
set(CMAKE_PREFIX_PATH "/your/qt/path")
```

### 修改应用信息
编辑 `Info.plist.in`:
```xml
<key>CFBundleDisplayName</key>
<string>你的应用名</string>
```

### 添加编译选项
编辑 `CMakeLists.txt`:
```cmake
add_definitions(-DYOUR_CUSTOM_DEFINE)
```

## 📞 支持

如果遇到问题，请检查：
1. 所有依赖是否正确安装
2. 脚本是否有执行权限
3. 日志文件中的错误信息
4. macOS系统版本兼容性

---

**Happy Coding! 🎉**