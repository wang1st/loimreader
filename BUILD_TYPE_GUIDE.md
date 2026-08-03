# 构建类型配置指南（Release vs Debug）

## 📋 当前配置状态

LoimReader 项目**默认使用 Release 构建模式**，这是发布版本的标准配置。

### 两种构建类型对比

| 特性 | Release | Debug |
|------|---------|-------|
| 优化级别 | O2/O3（最高） | O0（无优化） |
| 调试信息 | 最少或无 | 完整 |
| 运行速度 | 快 | 慢 |
| 文件大小 | 较小 | 较大 |
| 适用场景 | 生产环境、发布 | 开发调试 |
| 断点调试 | 困难 | 容易 |
| 性能分析 | 准确 | 不准确 |

## ✅ Release 构建配置（默认）

### 三层配置保障

#### 1️⃣ CMakeLists.txt（主配置）

```cmake
# 设置构建类型（默认为Release，可通过命令行覆盖）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
```

**位置**: `CMakeLists.txt` 第18-21行

**说明**: 
- 如果未指定构建类型，默认使用 Release
- 使用 `CACHE` 和 `FORCE` 确保设置生效
- 可通过命令行参数 `-DCMAKE_BUILD_TYPE=Debug` 覆盖

#### 2️⃣ Windows 部署脚本

**文件**: `onekey_win_deploy.ps1`

```powershell
# CMake 配置（第186行）
& $cmakeExe -G "MinGW Makefiles" -S .. -B . `
    -DCMAKE_BUILD_TYPE=Release `              # ← 显式指定 Release
    -DCMAKE_PREFIX_PATH="..." `
    -DPROJECT_VERSION_OVERRIDE="..."

# 编译（第194行）
& $cmakeExe --build . --config Release -j 4   # ← 显式指定 Release
```

**特点**:
- CMake 配置阶段指定 `-DCMAKE_BUILD_TYPE=Release`
- 编译阶段指定 `--config Release`
- 双重保障确保生成 Release 版本

#### 3️⃣ macOS 部署脚本

**文件**: `onekey_mac_deploy.sh`

```bash
# CMake 配置（第149行）
cmake -DCMAKE_BUILD_TYPE=Release `
      -DPROJECT_VERSION_OVERRIDE="$PROJECT_VERSION" ..
```

**特点**:
- 显式指定 `-DCMAKE_BUILD_TYPE=Release`
- 使用单一配置生成器（Unix Makefiles）

## 🔍 如何验证构建类型

### 方法 1：查看 CMake 配置输出

运行构建脚本时，查看输出：

```
-- === 构建配置信息 ===
-- 项目名称: LoimReader
-- 版本: 2.7.2
-- 构建类型: Release                    ← 确认这里显示 Release
-- Qt6路径: ...
-- ==================
```

### 方法 2：检查编译器标志

**Windows (MinGW)**:
- Release: `-O3 -DNDEBUG`
- Debug: `-g -O0`

**macOS (Clang)**:
- Release: `-O3 -DNDEBUG`
- Debug: `-g -O0`

### 方法 3：检查可执行文件大小

```powershell
# Windows
Get-Item .\build_release\bin\LoimReader.exe | Select-Object Length

# Release 版本通常比 Debug 版本小 30-50%
```

### 方法 4：检查文件属性（Windows）

右键点击 `LoimReader.exe` → 属性 → 详细信息

- Release 版本：通常没有调试信息
- Debug 版本：包含完整调试信息，文件更大

### 方法 5：运行性能测试

Release 版本的运行速度应明显快于 Debug 版本（通常快 2-10 倍）

## 🛠️ 如何切换构建类型

### 临时切换到 Debug（用于开发调试）

#### Windows

修改 `onekey_win_deploy.ps1` 或手动构建：

```powershell
# 方法1：修改脚本中的构建类型
# 将第186行和第194行的 Release 改为 Debug

# 方法2：手动构建 Debug 版本
cd build_release
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --config Debug
```

#### macOS

```bash
# 方法1：修改脚本
# 将第149行的 Release 改为 Debug

# 方法2：手动构建 Debug 版本
mkdir build_debug
cd build_debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(sysctl -n hw.ncpu)
```

### 永久切换配置

**不推荐**：修改 `CMakeLists.txt` 的默认值

```cmake
# 不推荐：将默认值改为 Debug
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
```

**推荐**：保持默认为 Release，需要 Debug 时使用命令行参数

## 🎯 最佳实践

### 发布版本（生产环境）

```powershell
# Windows - 使用默认 Release 配置
.\onekey_win_deploy.ps1 -v 2.7.2 -c

# macOS - 使用默认 Release 配置
./onekey_mac_deploy.sh -v 2.7.2 -c
```

**特点**:
- ✅ 最优性能
- ✅ 最小文件体积
- ✅ 无调试符号（更安全）
- ✅ 用户体验最佳

### 开发调试版本

```powershell
# Windows - Debug 模式
cd build_debug
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DPROJECT_VERSION_OVERRIDE="2.7.2-debug" ..
cmake --build . -j 4

# macOS - Debug 模式
mkdir build_debug && cd build_debug
cmake -DCMAKE_BUILD_TYPE=Debug -DPROJECT_VERSION_OVERRIDE="2.7.2-debug" ..
make -j$(sysctl -n hw.ncpu)
```

**特点**:
- ✅ 包含完整调试信息
- ✅ 可以设置断点
- ✅ 便于问题排查
- ⚠️ 运行速度慢
- ⚠️ 文件体积大

### 混合配置（RelWithDebInfo）

如果需要在保持性能的同时有部分调试信息：

```cmake
-DCMAKE_BUILD_TYPE=RelWithDebInfo
```

**特点**:
- 优化级别：O2
- 包含调试信息
- 性能接近 Release
- 可进行性能分析

## 📊 构建类型详细说明

### 四种标准构建类型

| 类型 | 优化 | 调试信息 | 断言 | 说明 |
|------|------|---------|------|------|
| **Release** | O3 | 无 | 禁用 | 生产版本 |
| **Debug** | O0 | 完整 | 启用 | 开发调试 |
| **RelWithDebInfo** | O2 | 有 | 禁用 | 性能分析 |
| **MinSizeRel** | Os | 无 | 禁用 | 体积优先 |

### LoimReader 推荐配置

- **发布版本**: Release（默认）
- **开发调试**: Debug
- **性能分析**: RelWithDebInfo
- **不推荐**: MinSizeRel（性能损失较大）

## ⚠️ 常见问题

### Q: 为什么我的程序运行很慢？

**A**: 检查是否使用了 Debug 构建：
```powershell
# 查看构建类型
cmake -LA build_release | findstr CMAKE_BUILD_TYPE
```

如果显示 Debug，请清理并重新构建：
```powershell
.\onekey_win_deploy.ps1 -c
```

### Q: 如何确认当前是 Release 版本？

**A**: 三种方法：
1. 查看 CMake 配置输出
2. 检查文件大小（Release 更小）
3. 测试运行速度（Release 更快）

### Q: 部署脚本已经指定 Release，为什么还是 Debug？

**A**: 可能原因：
1. 构建目录缓存了旧配置 → 使用 `-c` 参数清理
2. 环境变量覆盖了配置 → 检查 `CMAKE_BUILD_TYPE` 环境变量
3. CMake 缓存问题 → 删除 `CMakeCache.txt` 重新配置

### Q: 能否同时构建 Release 和 Debug？

**A**: 可以，使用不同的构建目录：
```powershell
# Release
mkdir build_release
cd build_release
cmake -DCMAKE_BUILD_TYPE=Release ..

# Debug
mkdir build_debug
cd build_debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

## 🔧 故障排除

### 问题：生成的是 Debug 版本

**解决方案**:

```powershell
# 1. 清理构建目录
.\onekey_win_deploy.ps1 -c

# 2. 检查脚本配置
Select-String -Path onekey_win_deploy.ps1 -Pattern "CMAKE_BUILD_TYPE"
# 应该看到: -DCMAKE_BUILD_TYPE=Release

# 3. 检查 CMakeLists.txt
Select-String -Path CMakeLists.txt -Pattern "CMAKE_BUILD_TYPE"
# 应该看到默认为 Release

# 4. 重新构建
.\onekey_win_deploy.ps1 -v 2.7.2
```

### 问题：Release 版本无法调试

**这是正常的**！Release 版本专门用于发布，不包含调试信息。

如果需要调试，请构建 Debug 版本：
```powershell
cd build_debug
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

### 问题：不同机器构建类型不同

**原因**: CMake 缓存或环境变量

**解决方案**:
```powershell
# 清理所有构建产物
Remove-Item -Recurse -Force build_*

# 使用干净环境重新构建
.\onekey_win_deploy.ps1 -c -v 2.7.2
```

## 📚 相关文档

- `DEPLOY_PARAMETERS.md` - 部署脚本参数
- `VERSION_GUIDE.md` - 版本号管理
- `CMakeLists.txt` - 构建配置文件
- CMake 官方文档: https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html

## 🎓 总结

### ✅ 当前配置（推荐保持）

- **默认构建类型**: Release
- **部署脚本**: 显式指定 Release
- **验证方法**: 查看 CMake 输出

### 📌 关键配置位置

1. **CMakeLists.txt** (第18-21行) - 默认配置
2. **onekey_win_deploy.ps1** (第186、194行) - Windows 显式配置
3. **onekey_mac_deploy.sh** (第149行) - macOS 显式配置

### 🚀 快速检查清单

- [ ] CMake 输出显示 "构建类型: Release"
- [ ] 可执行文件大小合理（非异常大）
- [ ] 运行速度快（非明显慢）
- [ ] 无调试符号（使用 file/objdump 检查）

---

**重要**: 发布到用户的版本**必须使用 Release 构建**，以确保最佳性能和用户体验！

