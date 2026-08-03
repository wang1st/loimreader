# ossutil 更新说明

## 更新内容

根据[阿里云官方文档](https://help.aliyun.com/zh/oss/developer-reference/ossutil-overview/)，已将所有安装指引更新到最新版本。

### 版本变化

- **旧版本：** ossutil 1.7.18
- **新版本：** ossutil 2.2.0

### 主要变化

1. **命令名称变化**
   - 旧版：`ossutil64`
   - 新版：`ossutil`

2. **参数格式变化**
   ```bash
   # 旧版本（1.x）
   ossutil64 cp file.dmg oss://bucket/path -i ACCESS_KEY -k SECRET_KEY -e ENDPOINT
   
   # 新版本（2.x）
   ossutil cp file.dmg oss://bucket/path --access-key-id ACCESS_KEY --access-key-secret SECRET_KEY --endpoint ENDPOINT
   ```

3. **下载链接变化**
   ```bash
   # 旧版本（macOS ARM64）
   https://gosspublic.alicdn.com/ossutil/1.7.18/ossutil-v1.7.18-darwin-arm64.zip
   
   # 新版本（macOS ARM64）
   https://gosspublic.alicdn.com/ossutil/v2/2.2.0/ossutil-2.2.0-mac-arm64.zip
   ```

## 已更新的文件

### 安装脚本
- ✅ `install_ossutil.sh` - 更新到2.2.0，修正下载链接和安装步骤

### 文档
- ✅ `OSSUTIL_INSTALL.md` - 完整的安装指南
- ✅ `OSS_SETUP.md` - OSS配置指南
- ✅ `一键部署指南.md` - 部署流程文档
- ✅ `README.md` - 项目README

### 部署脚本
- ✅ `onekey_mac_deploy.sh` - 生成的上传脚本已更新命令格式
- ⚠️  `onekey_win_deploy.bat` - Windows脚本（命令保持兼容）

## 安装方法

### 快速安装（推荐）

```bash
# 运行自动安装脚本
./install_ossutil.sh

# 脚本会：
# 1. 自动检测系统架构（M系列/Intel）
# 2. 下载对应版本的 ossutil 2.2.0
# 3. 安装到 /usr/local/bin/
# 4. 验证安装
```

### 手动安装

#### macOS Apple Silicon

```bash
curl -o ossutil.zip https://gosspublic.alicdn.com/ossutil/v2/2.2.0/ossutil-2.2.0-mac-arm64.zip
unzip ossutil.zip
cd ossutil-2.2.0-mac-arm64
chmod 755 ossutil
sudo mv ossutil /usr/local/bin/
ossutil --version
```

#### macOS Intel

```bash
curl -o ossutil.zip https://gosspublic.alicdn.com/ossutil/v2/2.2.0/ossutil-2.2.0-mac-amd64.zip
unzip ossutil.zip
cd ossutil-2.2.0-mac-amd64
chmod 755 ossutil
sudo mv ossutil /usr/local/bin/
ossutil --version
```

## 迁移指南

### 如果已安装旧版本

```bash
# 1. 删除旧版本
sudo rm /usr/local/bin/ossutil64
sudo rm /usr/bin/ossutil64

# 2. 安装新版本
./install_ossutil.sh

# 3. 验证
ossutil version
# 输出：2.2.0
```

### 命令迁移

| 旧命令（1.x） | 新命令（2.x） |
|-------------|-------------|
| `ossutil64 --version` | `ossutil version` |
| `ossutil64 config` | `ossutil config` |
| `ossutil64 cp file oss://bucket/path -i KEY -k SECRET` | `ossutil cp file oss://bucket/path --access-key-id KEY --access-key-secret SECRET` |
| `ossutil64 ls oss://bucket/` | `ossutil ls oss://bucket/` |

## 使用说明

### 使用项目配置文件（推荐）

```bash
# 加载配置
source ../deploy/oss-config.sh

# 测试连接
ossutil ls oss://limereader-releases/updates/loimreader/

# 使用一键部署
./onekey_mac_deploy.sh -u
```

### 手动配置

```bash
ossutil config

# 按提示输入：
# endpoint: oss-cn-hangzhou.aliyuncs.com
# accessKeyID: ${OSS_ACCESS_KEY_ID}
# accessKeySecret: ${OSS_ACCESS_KEY_SECRET}
# region: cn-hangzhou
```

## 常见问题

### Q: brew install ossutil 不工作？

**A:** Homebrew 中没有 ossutil，需要手动安装或使用项目提供的脚本。

### Q: 旧版本的配置文件能用吗？

**A:** 可以，但建议重新运行 `ossutil config` 以使用新版本的配置格式。

### Q: 命令参数格式变了怎么办？

**A:** 项目的所有脚本已自动更新，直接使用即可：
```bash
# 这个会自动使用正确的命令格式
./onekey_mac_deploy.sh -u
```

### Q: 如何验证是否是新版本？

```bash
ossutil version
# 应该显示：2.2.0

# 如果命令不存在或显示 1.x.x，说明还是旧版本
```

## 测试

### 测试安装

```bash
# 检查命令是否可用
which ossutil

# 检查版本
ossutil version

# 测试连接
source ../deploy/oss-config.sh
ossutil ls oss://limereader-releases/updates/loimreader/
```

### 测试上传

```bash
# 创建测试文件
echo "test" > test.txt

# 上传测试
source ../deploy/oss-config.sh
ossutil cp test.txt oss://limereader-releases/test.txt \
  --access-key-id $OSS_ACCESS_KEY_ID \
  --access-key-secret $OSS_ACCESS_KEY_SECRET \
  --endpoint https://oss-cn-hangzhou.aliyuncs.com

# 验证
curl https://limereader-releases.oss-cn-hangzhou.aliyuncs.com/test.txt

# 清理
ossutil rm oss://limereader-releases/test.txt \
  --access-key-id $OSS_ACCESS_KEY_ID \
  --access-key-secret $OSS_ACCESS_KEY_SECRET \
  --endpoint https://oss-cn-hangzhou.aliyuncs.com
rm test.txt
```

## 参考链接

- **官方文档：** https://help.aliyun.com/zh/oss/developer-reference/ossutil-overview/
- **下载页面：** https://gosspublic.alicdn.com/ossutil/
- **详细安装指南：** [OSSUTIL_INSTALL.md](./OSSUTIL_INSTALL.md)
- **OSS配置指南：** [OSS_SETUP.md](./OSS_SETUP.md)

---

**更新日期：** 2025-10-20  
**更新人员：** AI Assistant  
**影响范围：** 所有OSS上传相关功能

