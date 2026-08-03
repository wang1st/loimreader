# 部署脚本参数说明

## Windows 部署脚本 (onekey_win_deploy.ps1)

### 快速开始

```powershell
# 显示帮助
.\onekey_win_deploy.ps1 -h

# 完整构建和打包
.\onekey_win_deploy.ps1

# 清理后重新构建
.\onekey_win_deploy.ps1 -c

# 指定版本号
.\onekey_win_deploy.ps1 -v 2.7.3

# 构建并自动上传到OSS
.\onekey_win_deploy.ps1 -u

# 跳过构建，仅重新打包
.\onekey_win_deploy.ps1 -s

# 组合使用：指定版本、清理构建、自动上传
.\onekey_win_deploy.ps1 -v 2.8.0 -c -u
```

### 参数说明

| 参数 | 简写 | 说明 | 默认值 |
|------|------|------|--------|
| `-Version` | `-v` | 指定版本号（会传递给CMake） | 2.7.2 |
| `-Clean` | `-c` | 构建前清理旧文件 | false |
| `-SkipBuild` | `-s` | 跳过构建，仅打包 | false |
| `-Upload` | `-u` | 构建完成后自动上传到OSS | false |
| `-Help` | `-h` | 显示帮助信息 | - |

### 工作流程

#### 标准流程（默认）
1. 检查环境（Qt、MinGW、CMake）
2. 构建应用程序
3. 部署Qt依赖
4. 生成安装程序（Inno Setup）
5. 打包ZIP文件
6. 生成version.json
7. 创建OSS上传脚本

#### 清理模式 (-Clean)
- 删除 `build_release/`
- 删除 `dist/`
- 删除上传目录
- 然后执行标准流程

#### 跳过构建模式 (-SkipBuild)
- 检查是否存在已构建的程序
- 跳过CMake配置和编译步骤
- 直接进行打包和部署

#### 自动上传模式 (-Upload)
- 执行完标准流程后
- 自动调用 `upload_to_oss.ps1`
- 上传所有文件到阿里云OSS

### 常见用例

#### 开发测试
```powershell
# 快速重新打包（不重新编译）
.\onekey_win_deploy.ps1 -s
```

#### 发布新版本
```powershell
# 完整清理构建，指定新版本号，自动上传
.\onekey_win_deploy.ps1 -v 2.8.0 -c -u
```

#### 修复打包问题
```powershell
# 保留构建结果，仅重新打包
.\onekey_win_deploy.ps1 -s
```

#### CI/CD 自动化
```powershell
# 环境变量方式指定版本
$env:QT_DIR = "D:\Qt\6.8.1\mingw_64"
.\onekey_win_deploy.ps1 -v $env:BUILD_VERSION -c -u
```

---

## macOS 部署脚本 (onekey_mac_deploy.sh)

### 快速开始

```bash
# 显示帮助
./onekey_mac_deploy.sh -h

# 完整构建和打包
./onekey_mac_deploy.sh

# 清理后重新构建
./onekey_mac_deploy.sh -c

# 指定版本号
./onekey_mac_deploy.sh -v 2.7.3

# 构建并自动上传到OSS
./onekey_mac_deploy.sh -u

# 跳过构建，仅重新打包
./onekey_mac_deploy.sh -s

# 组合使用
./onekey_mac_deploy.sh -v 2.8.0 -c -u
```

### 参数说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-v, --version` | 指定版本号（会传递给CMake） | 2.7.2 |
| `-c, --clean` | 构建前清理旧文件 | false |
| `-s, --skip-build` | 跳过构建，仅打包 | false |
| `-u, --upload` | 构建完成后自动上传到OSS | false |
| `-h, --help` | 显示帮助信息 | - |

---

## 生成的文件

### Windows
```
deploy/uploads/loimreader/
├── LoimReader_v2.7.2_Windows.zip          # 免安装版本
├── LoimReader-Setup-v2.7.2.exe            # 安装程序
├── version.json                            # 版本信息
└── upload_to_oss.ps1                       # OSS上传脚本
```

### macOS
```
deploy/uploads/loimreader/
├── LoimReader-v2.7.1.dmg                   # macOS磁盘镜像
├── version.json                            # 版本信息
└── upload_to_oss.sh                        # OSS上传脚本
```

## 版本号机制

### 版本号传递流程

当您使用 `-v` 参数指定版本号时，脚本会自动将版本号传递到所有相关组件：

1. **CMake 构建系统**: 通过 `-DPROJECT_VERSION_OVERRIDE` 参数
2. **Inno Setup 安装程序**: 通过 `/DAppVersion` 参数
3. **安装包文件名**: 自动生成版本化的文件名
4. **version.json**: 包含完整的版本信息

### 版本号示例

```powershell
# Windows
.\onekey_win_deploy.ps1 -v 2.8.0
# 结果：
# - CMake 构建版本: 2.8.0
# - 安装程序: LoimReader-Setup-v2.8.0.exe
# - ZIP包: LoimReader_v2.8.0_Windows.zip
# - version.json: latestVersion = "2.8.0"
```

```bash
# macOS
./onekey_mac_deploy.sh -v 2.8.0
# 结果：
# - CMake 构建版本: 2.8.0
# - DMG文件: LoimReader-v2.8.0.dmg
# - version.json: latestVersion = "2.8.0"
```

## 注意事项

1. **首次使用**需要配置环境变量：
   - Windows: `QT_DIR`, `MINGW_DIR`（可自动检测）
   - macOS: 需要安装 Qt6、CMake、Python3

2. **Inno Setup**（Windows）:
   - 下载地址: https://jrsoftware.org/isdl.php
   - 如未安装，将跳过安装程序生成，仅创建ZIP包

3. **版本号格式**: 建议使用语义化版本，如 `2.7.2`
   - 主版本号.次版本号.修订号
   - 版本号会自动传递给所有构建组件

4. **OSS上传**: 需要先配置 `ossutil64` 工具或环境变量

## 故障排除

### Windows

**问题**: CMake 未找到
```powershell
# 解决方法：安装CMake或使用Qt自带的
$env:Path += ";C:\Qt\Tools\CMake_64\bin"
```

**问题**: 找不到已构建的程序（使用 -s 时）
```powershell
# 解决方法：先运行完整构建
.\onekey_win_deploy.ps1
```

### macOS

**问题**: Qt未找到
```bash
# 解决方法
brew install qt@6
export PATH="/opt/homebrew/opt/qt@6/bin:$PATH"
```

**问题**: macdeployqt不在PATH中
```bash
# 解决方法
export PATH="/opt/homebrew/opt/qt@6/bin:$PATH"
```

