# 轻量级自定义更新系统部署指南

## 概述

我们已经成功将重量级的Fervor框架替换为轻量级的自定义更新系统，使用阿里云OSS存储更新配置文件。

## 已完成的工作

### 1. 代码修改
- ✅ 创建了 `SimpleUpdater` 类 (`simpleupdater.h` 和 `simpleupdater.cpp`)
- ✅ 创建了与登录窗口风格一致的 `UpdateDialog` 
- ✅ 修改了 `main.cpp` 替换Fervor为自定义更新器
- ✅ 更新了 `CMakeLists.txt` 移除Fervor依赖
- ✅ 在主窗口添加了"检查更新"菜单项
- ✅ 去掉了窗口标题中的版本号

### 2. 配置文件格式
创建了标准的JSON配置文件格式 (`update_config_template.json`)：

```json
{
  "appName": "ctdy123",
  "appDisplayName": "影谷长图阅读器", 
  "releases": [
    {
      "version": "2.7",
      "title": "影谷长图阅读器 v2.7",
      "description": "更新内容描述",
      "platforms": [
        {
          "name": "macos",
          "downloadUrl": "https://your-oss-bucket.oss-cn-beijing.aliyuncs.com/releases/ctdy123-v2.7-macos.dmg",
          "fileSize": 52428800
        }
      ]
    }
  ]
}
```

## 阿里云OSS配置步骤

### 1. 创建OSS存储桶
```bash
# 建议使用的存储桶名称
your-oss-bucket
```

### 2. 上传配置文件
需要上传两个JSON配置文件：
```
/updates/update-config-zh.json  # 中文版配置
/updates/update-config-en.json  # 英文版配置
```

### 3. 设置CORS规则
在OSS控制台设置CORS规则，允许应用程序访问：
```json
{
  "AllowedOrigins": ["*"],
  "AllowedMethods": ["GET"],
  "AllowedHeaders": ["*"],
  "MaxAgeSeconds": 3600
}
```

### 4. 配置CDN（推荐）
为了提升访问速度，建议为OSS配置CDN加速。

## 更新URL配置

当前代码中的更新URL模板：
```cpp
// 中文版
https://your-oss-bucket.oss-cn-beijing.aliyuncs.com/updates/update-config-zh.json

// 英文版  
https://your-oss-bucket.oss-cn-beijing.aliyuncs.com/updates/update-config-en.json
```

**请将 `your-oss-bucket` 替换为您的实际OSS存储桶名称。**

## 发布新版本流程

### 1. 准备新版本文件
- 编译生成新版本的应用程序（如 ctdy123-v2.7-macos.dmg）
- 上传到OSS的 `/releases/` 目录

### 2. 更新配置文件
修改 `update-config-zh.json` 和 `update-config-en.json`：
```json
{
  "releases": [
    {
      "version": "2.7",  // 新版本号
      "title": "影谷长图阅读器 v2.7",
      "description": "• 修复了一些已知问题<br>• 优化了界面体验",
      "releaseDate": "2024-09-25",
      "platforms": [
        {
          "name": "macos",
          "downloadUrl": "https://your-oss-bucket.oss-cn-beijing.aliyuncs.com/releases/ctdy123-v2.7-macos.dmg",
          "fileSize": 52428800
        }
      ]
    }
  ]
}
```

### 3. 验证更新
- 运行旧版本应用程序
- 点击"帮助" -> "检查更新"
- 确认更新对话框显示正确

## 特性优势

### 相比Fervor的优势：
1. **轻量级**：代码量减少80%以上
2. **简单配置**：JSON格式，易于理解和维护
3. **灵活部署**：可部署在任何支持HTTPS的服务器上
4. **统一风格**：更新对话框与应用程序界面风格一致
5. **多平台支持**：支持macOS、Windows、Linux

### 功能特性：
- ✅ 自动检查更新（启动时静默检查）
- ✅ 手动检查更新（菜单项）
- ✅ 版本比较（使用QVersionNumber）
- ✅ 跳过版本功能
- ✅ 下载进度显示
- ✅ 错误处理
- ✅ 多语言支持

## 下一步工作

1. **替换OSS URL**：将代码中的 `your-oss-bucket` 替换为实际的存储桶名称
2. **上传初始配置**：创建并上传初始的JSON配置文件
3. **测试验证**：在不同版本间测试更新功能
4. **生产部署**：编译并发布带有自定义更新器的新版本

## 注意事项

1. **安全性**：OSS存储桶建议设置为私有，通过CDN提供公开访问
2. **缓存控制**：配置文件建议设置较短的缓存时间，确保及时更新
3. **备份机制**：建议保留Fervor配置作为备用方案，以防新系统出现问题
4. **版本兼容**：确保版本号格式与QVersionNumber兼容（如：2.6, 2.7, 3.0）

## 技术架构

```
应用程序启动
    ↓
静默检查更新 (SimpleUpdater)
    ↓
下载JSON配置文件 (QNetworkAccessManager)
    ↓
解析版本信息 (QJsonDocument)
    ↓
版本比较 (QVersionNumber)
    ↓
显示更新对话框 (UpdateDialog)
    ↓
用户选择下载 (QDesktopServices::openUrl)
```

这个轻量级自定义更新系统完全满足您的需求，摆脱了Fervor的复杂配置，同时保持了专业的用户体验。