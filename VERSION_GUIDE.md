# 版本号管理指南

## 🎯 版本号信源（Single Source of Truth）

LoimReader 使用**统一的版本号管理系统**，确保所有组件使用相同的版本号。

### 版本号传递链

```
┌─────────────────────────────────────────────────────────────┐
│  1. 部署脚本参数                                              │
│     Windows: .\onekey_win_deploy.ps1 -v 2.7.2               │
│     macOS:   ./onekey_mac_deploy.sh -v 2.7.2                │
└─────────────────────────┬───────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│  2. CMake 配置 (CMakeLists.txt)                             │
│     if(NOT DEFINED PROJECT_VERSION_OVERRIDE)                │
│         set(PROJECT_VERSION_OVERRIDE "2.7.2")               │
│     endif()                                                 │
│                                                             │
│     project(LoimReader VERSION ${PROJECT_VERSION_OVERRIDE})│
└─────────────────────────┬───────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│  3. 编译器宏定义                                              │
│     add_definitions(                                        │
│         -DAPP_VERSION="${PROJECT_VERSION}"                  │
│         -DFA_APP_VERSION="${PROJECT_VERSION}"               │
│     )                                                       │
└─────────────────────────┬───────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│  4. C++ 源代码 (app_version.h)                              │
│     #ifdef APP_VERSION                                      │
│         return QString(APP_VERSION);                        │
│     #else                                                   │
│         return "2.7.2";  // 回退值，保持一致                 │
│     #endif                                                  │
└─────────────────────────┬───────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│  5. 应用程序使用                                              │
│     - QApplication::setApplicationVersion(FA_APP_VERSION)   │
│     - AppVersion::getAppVersion()                           │
│     - 关于对话框、更新检查等                                  │
└─────────────────────────────────────────────────────────────┘
```

## 📝 版本号位置清单

### 必须保持一致的位置

| 文件 | 位置 | 当前值 | 说明 |
|------|------|--------|------|
| `CMakeLists.txt` | 第5行 | `2.7.2` | **主要信源**（默认值） |
| `app_version.h` | 第42行 | `2.7.2` | 回退值（应与CMake一致） |
| `onekey_win_deploy.ps1` | 第9行 | `2.7.2` | 脚本默认值 |
| `onekey_mac_deploy.sh` | 第17行 | `2.7.2` | 脚本默认值 |
| `installer.iss` | 第5行 | `2.7.2` | Inno Setup 默认值 |

### 动态版本号位置（不需要修改）

这些位置会自动从 CMake 获取版本号：

- ✅ `main.cpp` - 使用 `FA_APP_VERSION` 宏
- ✅ `app_version.h` - 使用 `APP_VERSION` 宏
- ✅ 所有使用 `AppVersion::getAppVersion()` 的代码

## 🔄 如何更新版本号

### 方法 1：使用参数（推荐用于临时构建）

```powershell
# Windows
.\onekey_win_deploy.ps1 -v 2.8.0

# macOS
./onekey_mac_deploy.sh -v 2.8.0
```

**优点**：
- 快速灵活
- 适合测试和临时构建
- 不修改源文件

**缺点**：
- 每次都要指定参数
- 容易遗忘

### 方法 2：更新默认版本号（推荐用于正式发布）

更新以下文件中的默认版本号（**保持一致**）：

1. **CMakeLists.txt** (第5行)
   ```cmake
   if(NOT DEFINED PROJECT_VERSION_OVERRIDE)
       set(PROJECT_VERSION_OVERRIDE "2.8.0")  # ← 更新这里
   endif()
   ```

2. **app_version.h** (第42行)
   ```cpp
   return "2.8.0";  // ← 更新这里
   ```

3. **onekey_win_deploy.ps1** (第9行)
   ```powershell
   [string]$Version = "2.8.0",  # ← 更新这里
   ```

4. **onekey_mac_deploy.sh** (第17行)
   ```bash
   PROJECT_VERSION="2.8.0"  # ← 更新这里
   ```

5. **installer.iss** (第5行)
   ```ini
   #define AppVersion "2.8.0"  // ← 更新这里
   ```

**优点**：
- 所有默认值保持一致
- 无需每次指定参数
- 适合正式发布

**缺点**：
- 需要修改多个文件
- 容易遗漏某个文件

## ✅ 版本号验证清单

发布新版本前，请检查以下内容：

- [ ] CMakeLists.txt 中的 `PROJECT_VERSION_OVERRIDE` 默认值已更新
- [ ] app_version.h 中的回退值已更新
- [ ] onekey_win_deploy.ps1 中的默认版本号已更新
- [ ] onekey_mac_deploy.sh 中的默认版本号已更新
- [ ] installer.iss 中的默认版本号已更新
- [ ] 使用 `-v` 参数测试构建，确认版本号正确传递
- [ ] 检查生成的可执行文件版本号（Windows 文件属性、macOS Get Info）
- [ ] 检查 version.json 中的版本号
- [ ] 检查安装包文件名中的版本号

## 🔍 如何验证版本号

### 1. 构建时检查

```powershell
# Windows
.\onekey_win_deploy.ps1 -v 2.8.0

# 输出应显示：
# -- 版本: 2.8.0                    ← 确认这里是正确的版本号
```

### 2. 运行时检查

在应用程序中：
- 打开"关于"对话框，检查版本号
- 检查 `QApplication::applicationVersion()`
- 使用 `AppVersion::getAppVersion()` 获取版本号

### 3. 文件属性检查

**Windows**:
- 右键点击 `LoimReader.exe` → 属性 → 详细信息
- 检查"产品版本"字段

**macOS**:
- 右键点击 `LoimReader.app` → 显示简介
- 检查"版本"字段

### 4. 安装包检查

- Windows: `LoimReader-Setup-v2.8.0.exe`
- ZIP: `LoimReader_v2.8.0_Windows.zip`
- macOS: `LoimReader-v2.8.0.dmg`

## ⚠️ 常见问题

### Q: 为什么有多个地方定义版本号？

**A**: 为了兼容性和灵活性：
- **CMakeLists.txt**: 构建系统的默认值
- **app_version.h**: 编译时未定义宏的回退值
- **部署脚本**: 用户可以通过参数覆盖

### Q: 我只改了脚本参数，为什么 CMake 还显示旧版本？

**A**: 您可能没有清理构建目录。使用 `-c` 参数：
```powershell
.\onekey_win_deploy.ps1 -v 2.8.0 -c
```

### Q: 如何确保所有地方的版本号一致？

**A**: 
1. 遵循"方法2"更新所有默认值
2. 使用本文档的验证清单
3. 构建前运行测试检查

### Q: 能否自动同步所有版本号？

**A**: 可以考虑使用脚本自动化，但当前的手动方法更透明、可控。

## 🚀 最佳实践

1. **发布前准备**：
   - 统一更新所有文件中的默认版本号
   - 提交版本号更改到版本控制
   - 打上 Git 标签：`git tag v2.8.0`

2. **版本号格式**：
   - 使用语义化版本：`主版本号.次版本号.修订号`
   - 主版本号：不兼容的 API 修改
   - 次版本号：向下兼容的功能性新增
   - 修订号：向下兼容的问题修正

3. **测试流程**：
   ```powershell
   # 1. 清理构建
   .\onekey_win_deploy.ps1 -c
   
   # 2. 使用新版本号构建
   .\onekey_win_deploy.ps1 -v 2.8.0
   
   # 3. 验证所有生成文件的版本号
   # 4. 测试应用程序功能
   # 5. 检查更新系统
   ```

4. **发布检查**：
   - 生成的文件名包含正确的版本号
   - version.json 包含正确的版本号
   - 应用程序显示正确的版本号
   - 更新检查功能正常工作

## 📚 相关文档

- `DEPLOY_PARAMETERS.md` - 部署脚本参数说明
- `VERSION_MANAGEMENT.md` - 版本管理策略
- `UPDATE_SYSTEM_GUIDE.md` - 更新系统指南
- `CMakeLists.txt` - 构建配置文件

---

**重要提示**：修改版本号后，务必进行完整的测试，确保所有组件都使用了正确的版本号。

