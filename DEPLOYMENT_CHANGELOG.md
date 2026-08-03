# 部署系统变更日志

## 2025-10-20 - 统一部署脚本重构

### 🎯 主要变更

将分散的构建脚本统一为两个一键部署脚本，完全符合阿里云OSS发布规范。

### ✅ 新增文件

#### 核心脚本
- **`onekey_mac_deploy.sh`** - macOS统一部署脚本
  - 完整构建流程
  - 自动生成DMG（符合OSS命名规范）
  - 自动生成version.json
  - 自动生成OSS上传脚本
  - 支持多种命令行选项

- **`onekey_win_deploy.bat`** - Windows统一部署脚本
  - 完整构建流程
  - 自动打包ZIP（符合OSS命名规范）
  - 自动生成version.json
  - 自动生成OSS上传脚本

#### 文档
- **`一键部署指南.md`** - 详细的部署使用指南
- **`DEPLOYMENT_CHANGELOG.md`** - 本变更日志

### ❌ 删除文件

以下旧的构建脚本已删除，功能已整合到统一脚本中：

- `build_dmg.sh`
- `build.sh`
- `快速构建DMG指南.md`
- `BUILD_EXAMPLES.md`

### 📝 更新文件

- **`README.md`**
  - 更新项目名称为 LoimReader
  - 添加版本号显示
  - 更新快速开始部分
  - 更新脚本说明

- **`构建打包指南.md`**
  - 添加重要提示，指向新的统一脚本
  - 更新所有命令和路径
  - 添加OSS上传说明
  - 更新输出文件路径

### 🔧 技术改进

#### 符合OSS规范

**文件命名：**
- macOS: `LoimReader_v2.7.2_macOS.dmg`
- Windows: `LoimReader_v2.7.2_Windows.zip`
- 版本文件: `version.json`

**输出目录：**
```
../deploy/uploads/loimreader/
├── LoimReader_v2.7.2_macOS.dmg
├── LoimReader_v2.7.2_Windows.zip
├── version.json
├── upload_to_oss.sh
└── upload_to_oss.bat
```

**version.json格式：**
```json
{
  "latestVersion": "2.7.2",
  "releaseDate": "2025-10-20T10:30:00Z",
  "releaseNotes": "...",
  "forceUpdate": false,
  "packages": {
    "macos": {
      "url": "LoimReader_v2.7.2_macOS.dmg",
      "size": "48.5M",
      "hash": "...",
      "downloadUrl": "https://limereader-releases.oss-cn-hangzhou.aliyuncs.com/updates/loimreader/...",
      "minSystemVersion": "macOS 14.0 或更高版本"
    }
  }
}
```

#### 自动化功能

1. **自动生成version.json**
   - 包含文件大小、MD5哈希
   - 完整的下载URL
   - 发布时间（UTC格式）

2. **自动生成上传脚本**
   - macOS: `upload_to_oss.sh`
   - Windows: `upload_to_oss.bat`
   - 包含验证命令

3. **智能环境检查**
   - 检查Qt、CMake、Python等工具
   - 自动查找macdeployqt路径
   - 友好的错误提示

4. **灵活的命令行选项**
   - `-v` 指定版本号
   - `-c` 清理后构建
   - `-s` 跳过构建
   - `-u` 自动上传

### 📦 输出对比

#### 之前
```
dist/
└── 影谷长图阅读器_v2.7.dmg

build_cmake/
└── bin/
    └── ctdy123.app
```

#### 现在
```
../deploy/uploads/loimreader/
├── LoimReader_v2.7.2_macOS.dmg      # 符合OSS命名规范
├── version.json                     # 自动生成，格式规范
└── upload_to_oss.sh                 # 自动生成上传脚本

build_cmake/
└── bin/
    └── LoimReader.app               # 统一应用名称
```

### 🚀 使用方式对比

#### 之前（多个脚本）
```bash
# 构建
./build.sh build

# 打包DMG
./build_dmg.sh -b -c

# 手动创建version.json
vim version.json

# 手动上传
ossutil64 cp ...
```

#### 现在（一键完成）
```bash
# 一条命令完成所有
./onekey_mac_deploy.sh -c

# 或直接上传
./onekey_mac_deploy.sh -c -u
```

### 🎯 OSS集成

#### OSS路径规范
```
Bucket: limereader-releases
Region: oss-cn-hangzhou
Path: updates/loimreader/

完整URL:
https://limereader-releases.oss-cn-hangzhou.aliyuncs.com/updates/loimreader/version.json
https://limereader-releases.oss-cn-hangzhou.aliyuncs.com/updates/loimreader/LoimReader_v2.7.2_macOS.dmg
```

#### 上传流程
```bash
# 1. 构建
./onekey_mac_deploy.sh -c

# 2. 上传（自动生成的脚本）
cd ../deploy/uploads/loimreader
./upload_to_oss.sh

# 3. 验证
curl https://limereader-releases.oss-cn-hangzhou.aliyuncs.com/updates/loimreader/version.json
```

### 📚 文档结构

```
项目文档体系
├── README.md                    # 项目总览，快速开始
├── 一键部署指南.md              # 详细部署文档（新增）
├── 构建打包指南.md              # 技术细节和故障排查（更新）
└── DEPLOYMENT_CHANGELOG.md      # 变更日志（本文件）

相关文档
└── ../deploy/OSS_RELEASE_GUIDE.md  # OSS发布规范
```

### ✨ 优势总结

1. **统一化：** 两个脚本覆盖所有平台
2. **规范化：** 完全符合OSS发布规范
3. **自动化：** 一键完成构建到上传准备
4. **简单化：** 无需记忆多个命令
5. **可靠性：** 自动验证和错误检查
6. **可维护：** 代码集中，易于更新

### 🔄 迁移指南

#### 从旧脚本迁移

**之前使用 `build_dmg.sh`：**
```bash
# 旧命令
./build_dmg.sh -b -c

# 新命令
./onekey_mac_deploy.sh -c
```

**之前使用 `build.sh`：**
```bash
# 旧命令
./build.sh all

# 新命令
./onekey_mac_deploy.sh -c
```

### 📋 检查清单

升级后请确认：

- [ ] 删除了旧的构建脚本
- [ ] 两个新脚本有可执行权限
- [ ] 父目录下有 `deploy` 文件夹
- [ ] 已配置ossutil（如需上传）
- [ ] 阅读了 `一键部署指南.md`
- [ ] 测试了构建流程
- [ ] 验证了生成的文件格式

### 🐛 已知问题

无

### 📞 支持

如有问题，请参考：
1. `一键部署指南.md` - 详细使用说明
2. `构建打包指南.md` - 技术细节
3. `../deploy/OSS_RELEASE_GUIDE.md` - OSS规范

---

**变更作者：** AI Assistant  
**变更日期：** 2025-10-20  
**影响范围：** 构建和部署流程

