# Fervor到新系统的平滑升级指南

## 🎯 升级策略概述

为了让使用Fervor框架的v2.5用户平滑升级到使用新轻量级更新系统的v2.7，我们需要：

1. **保持现有Fervor Appcast.xml兼容性**
2. **在XML中添加v2.7版本信息**
3. **引导用户下载新版本**
4. **新版本使用JSON配置系统**

## 📁 文件部署结构

### 在您的服务器上部署以下文件：

```
ctdy123.com/updates/
├── Appcast.xml              # 中文版Fervor XML（给v2.5用户）
└── legacy/
    └── Appcast-v2.5.xml     # 备份旧版本配置

limereader.com/updates/  
├── Appcast.xml              # 英文版Fervor XML（给v2.5用户）
└── legacy/
    └── Appcast-v2.5.xml     # 备份旧版本配置

releases.oss-cn-beijing.aliyuncs.com/
├── updates/
│   ├── update-config-zh.json    # 新系统JSON配置（给v2.7+用户）
│   └── update-config-en.json    # 新系统JSON配置（给v2.7+用户）
└── releases/
    ├── ctdy123-v2.7-macos.dmg   # 新版本文件
    ├── ctdy123-v2.7-windows.exe # 新版本文件
    ├── ctdy123-v2.6-macos.dmg   # 过渡版本
    └── ctdy123-v2.5-macos.dmg   # 当前版本
```

## 🔄 部署步骤

### 步骤1：更新现有的Fervor Appcast.xml

将我创建的 `Appcast-zh.xml` 和 `Appcast-en.xml` 分别部署到：
```bash
# 中文版
cp Appcast-zh.xml /path/to/ctdy123.com/updates/Appcast.xml

# 英文版  
cp Appcast-en.xml /path/to/limereader.com/updates/Appcast.xml
```

### 步骤2：上传新版本文件到OSS

```bash
# 上传v2.7版本文件到OSS
ossutil cp ctdy123-v2.7-macos.dmg oss://releases/releases/
ossutil cp ctdy123-v2.7-windows.exe oss://releases/releases/

# 上传新的JSON配置文件
ossutil cp update-config-zh.json oss://releases/updates/
ossutil cp update-config-en.json oss://releases/updates/
```

### 步骤3：设置正确的文件大小

更新XML中的 `length` 属性为实际文件大小：
```bash
# 获取文件大小（字节）
ls -l ctdy123-v2.7-macos.dmg
ls -l ctdy123-v2.7-windows.exe

# 更新XML中对应的 length 值
```

## 📋 XML配置详解

### 关键XML元素说明：

1. **版本标识**：
   ```xml
   <sparkle:version="2.7">    <!-- Sparkle兼容 -->
   <fervor:version="2.7">     <!-- Fervor专用 -->
   ```

2. **平台支持**：
   ```xml
   <fervor:platform="macos">   <!-- macOS版本 -->
   <fervor:platform="windows"> <!-- Windows版本 -->
   ```

3. **下载链接**：
   ```xml
   <enclosure url="https://releases.oss-cn-beijing.aliyuncs.com/releases/ctdy123-v2.7-macos.dmg"
              length="57671680"
              type="application/octet-stream" />
   ```

4. **发布说明**：
   ```xml
   <sparkle:releaseNotesLink>https://ctdy123.com/release-notes/v2.7.html</sparkle:releaseNotesLink>
   ```

## 🎯 用户升级体验

### 对于v2.5用户：
1. **自动检查**：Fervor框架检测到v2.7版本
2. **显示更新**：显示包含新特性说明的更新对话框
3. **下载安装**：用户点击更新，下载v2.7版本
4. **无缝切换**：v2.7版本自动使用新的JSON更新系统

### 对于v2.7+用户：
- 使用新的轻量级JSON更新系统
- 更快的检查速度和更好的用户体验

## 🚨 重要注意事项

### 1. 文件大小准确性
确保XML中的 `length` 属性与实际文件大小完全匹配：
```bash
# macOS获取文件大小
stat -f%z ctdy123-v2.7-macos.dmg

# Linux获取文件大小  
stat -c%s ctdy123-v2.7-macos.dmg
```

### 2. MIME类型设置
确保服务器正确设置MIME类型：
```apache
# Apache配置示例
AddType application/rss+xml .xml
AddType application/octet-stream .dmg
AddType application/octet-stream .exe
```

### 3. 缓存控制
为Appcast.xml设置合适的缓存策略：
```apache
# 建议缓存5分钟
<Files "Appcast.xml">
    Header set Cache-Control "max-age=300"
</Files>
```

### 4. 版本号格式
确保版本号格式一致：
- 使用语义化版本号：2.5, 2.6, 2.7
- Fervor会按字符串比较，确保正确排序

## 🔍 测试验证

### 验证XML有效性：
```bash
# 验证XML格式
xmllint --valid Appcast-zh.xml
xmllint --valid Appcast-en.xml

# 测试访问
curl -I https://ctdy123.com/updates/Appcast.xml
curl -I https://limereader.com/updates/Appcast.xml
```

### 模拟用户更新：
1. 运行v2.5版本应用
2. 触发手动更新检查
3. 验证显示v2.7更新信息
4. 确认下载链接有效

## 📊 升级监控

建议监控以下指标：
- Appcast.xml的访问频率
- v2.7版本的下载次数  
- 用户从v2.5到v2.7的转化率
- 新JSON配置的访问情况

这样，您的v2.5用户就能够平滑升级到使用新轻量级更新系统的v2.7版本，同时保持良好的用户体验！