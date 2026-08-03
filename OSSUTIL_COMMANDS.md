# ossutil 2.x 常用命令参考

本文档列出 ossutil 2.2.0 的常用命令，帮助快速上手。

**官方文档：** https://help.aliyun.com/zh/oss/developer-reference/ossutil-overview/

## 版本信息

```bash
# 查看版本（注意：不是 --version）
ossutil version

# 输出：
# 2.2.0

# 查看帮助
ossutil --help

# 查看子命令帮助
ossutil cp --help
```

## 配置

### 交互式配置

```bash
ossutil config

# 按提示输入：
# - endpoint: oss-cn-hangzhou.aliyuncs.com
# - accessKeyID: 你的AccessKeyId
# - accessKeySecret: 你的AccessKeySecret  
# - region: cn-hangzhou (可选)

# 配置文件位置：~/.ossutilconfig
```

### 使用命令行参数（推荐用于脚本）

```bash
# 不需要配置文件，直接在命令中指定
ossutil cp file.txt oss://bucket/path/file.txt \
  --access-key-id YOUR_ACCESS_KEY_ID \
  --access-key-secret YOUR_ACCESS_KEY_SECRET \
  --endpoint https://oss-cn-hangzhou.aliyuncs.com
```

## 列出对象

```bash
# 列出所有bucket
ossutil ls

# 列出bucket中的对象
ossutil ls oss://bucket-name/

# 列出指定路径
ossutil ls oss://bucket-name/path/

# 递归列出（包含子目录）
ossutil ls oss://bucket-name/path/ --recursive

# 列出并显示详细信息
ossutil ls oss://bucket-name/ -d
```

## 上传文件

```bash
# 上传单个文件
ossutil cp local-file.txt oss://bucket-name/remote-file.txt

# 上传整个目录（递归）
ossutil cp local-dir/ oss://bucket-name/remote-dir/ --recursive

# 上传时指定存储类型
ossutil cp file.txt oss://bucket/file.txt --storage-class IA

# 上传大文件（自动分片上传）
ossutil cp large-file.dmg oss://bucket/large-file.dmg --bigfile-threshold 100

# 使用命令行参数指定密钥
ossutil cp file.txt oss://bucket/file.txt \
  --access-key-id YOUR_KEY \
  --access-key-secret YOUR_SECRET \
  --endpoint https://oss-cn-hangzhou.aliyuncs.com
```

## 下载文件

```bash
# 下载单个文件
ossutil cp oss://bucket-name/remote-file.txt local-file.txt

# 下载整个目录
ossutil cp oss://bucket-name/remote-dir/ local-dir/ --recursive

# 断点续传下载
ossutil cp oss://bucket/large-file.dmg local-file.dmg --checkpoint-dir .ossutil_checkpoint
```

## 同步

```bash
# 同步本地目录到OSS
ossutil sync local-dir/ oss://bucket/remote-dir/

# 同步OSS到本地
ossutil sync oss://bucket/remote-dir/ local-dir/

# 删除目标端多余的文件
ossutil sync local-dir/ oss://bucket/remote-dir/ --delete

# 只同步新文件
ossutil sync local-dir/ oss://bucket/remote-dir/ --update
```

## 删除对象

```bash
# 删除单个文件
ossutil rm oss://bucket-name/file.txt

# 删除目录（递归删除所有文件）
ossutil rm oss://bucket-name/dir/ --recursive

# 强制删除（不提示确认）
ossutil rm oss://bucket-name/dir/ --recursive --force

# 批量删除（使用通配符）
ossutil rm oss://bucket-name/prefix* --recursive
```

## 创建/删除Bucket

```bash
# 创建bucket
ossutil mb oss://new-bucket-name

# 删除bucket（必须为空）
ossutil rb oss://bucket-name

# 强制删除bucket（包含所有对象）
ossutil rb oss://bucket-name --force
```

## 对象信息

```bash
# 查看对象元信息
ossutil stat oss://bucket-name/file.txt

# 查看bucket信息
ossutil stat oss://bucket-name

# 计算目录大小
ossutil du oss://bucket-name/dir/

# 查看文件内容
ossutil cat oss://bucket-name/file.txt
```

## 生成预签名URL

```bash
# 生成可访问的URL（默认3600秒）
ossutil presign oss://bucket-name/file.txt

# 指定过期时间（单位：秒）
ossutil presign oss://bucket-name/file.txt --timeout 7200

# 生成用于上传的URL
ossutil presign oss://bucket-name/file.txt --method PUT
```

## 设置对象属性

```bash
# 设置对象ACL
ossutil set-props oss://bucket/file.txt --acl public-read

# 设置对象元数据
ossutil set-props oss://bucket/file.txt --meta x-oss-meta-key:value

# 设置Content-Type
ossutil set-props oss://bucket/file.txt --content-type application/pdf
```

## 高级功能

### 追加上传

```bash
# 追加内容到可追加对象
ossutil append local-file.txt oss://bucket/appendable-file.txt --position 0
```

### 哈希计算

```bash
# 计算本地文件哈希
ossutil hash local-file.txt

# 对比本地和OSS文件
ossutil hash local-file.txt oss://bucket/remote-file.txt
```

### 探测网络

```bash
# 测试上传
ossutil probe --upload --file test.txt oss://bucket/

# 测试下载
ossutil probe --download oss://bucket/test.txt

# 测试带宽
ossutil probe --upload --file test.txt oss://bucket/ --timeout 60
```

## 常用组合示例

### 项目部署示例

```bash
# 加载配置
source ../deploy/oss-config.sh

# 上传DMG文件
ossutil cp LoimReader_v2.7.2_macOS.dmg \
  oss://limereader-releases/updates/loimreader/LoimReader_v2.7.2_macOS.dmg \
  --access-key-id $OSS_ACCESS_KEY_ID \
  --access-key-secret $OSS_ACCESS_KEY_SECRET \
  --endpoint https://oss-cn-hangzhou.aliyuncs.com

# 上传version.json
ossutil cp version.json \
  oss://limereader-releases/updates/loimreader/version.json \
  --access-key-id $OSS_ACCESS_KEY_ID \
  --access-key-secret $OSS_ACCESS_KEY_SECRET \
  --endpoint https://oss-cn-hangzhou.aliyuncs.com

# 验证上传
ossutil ls oss://limereader-releases/updates/loimreader/ \
  --access-key-id $OSS_ACCESS_KEY_ID \
  --access-key-secret $OSS_ACCESS_KEY_SECRET \
  --endpoint https://oss-cn-hangzhou.aliyuncs.com
```

### 批量上传并设置权限

```bash
# 上传目录并设置为公共读
for file in dist/*; do
  ossutil cp "$file" "oss://bucket/$(basename "$file")" --acl public-read
done
```

### 同步网站静态文件

```bash
# 同步并设置缓存
ossutil sync website/ oss://bucket/ \
  --update \
  --delete \
  --meta Cache-Control:max-age=3600
```

## 过滤选项

ossutil 2.x 支持强大的过滤功能：

```bash
# 只上传特定扩展名的文件
ossutil cp dir/ oss://bucket/dir/ \
  --recursive \
  --include "*.jpg" \
  --include "*.png"

# 排除特定文件
ossutil cp dir/ oss://bucket/dir/ \
  --recursive \
  --exclude ".git/*" \
  --exclude "*.tmp"

# 组合使用
ossutil cp dir/ oss://bucket/dir/ \
  --recursive \
  --include "*.js" \
  --exclude "*.min.js"
```

## 性能优化

```bash
# 并发上传（默认3，可调整）
ossutil cp dir/ oss://bucket/dir/ \
  --recursive \
  --parallel 10

# 分片大小（大文件）
ossutil cp large-file.dmg oss://bucket/large-file.dmg \
  --part-size 10485760

# 设置超时
ossutil cp file.txt oss://bucket/file.txt \
  --timeout 300
```

## 常见错误处理

### Error: unknown flag: --version

```bash
# ❌ 错误
ossutil --version

# ✅ 正确
ossutil version
```

### Error: Please config endpoint

```bash
# 需要配置或在命令中指定 endpoint
ossutil config
# 或
ossutil ls oss://bucket/ --endpoint https://oss-cn-hangzhou.aliyuncs.com
```

### Error: AccessDenied

```bash
# 检查密钥是否正确
# 检查bucket权限
# 使用命令行参数明确指定密钥
```

## 与 ossutil 1.x 的主要区别

| 功能 | ossutil 1.x | ossutil 2.x |
|------|------------|------------|
| 命令名 | `ossutil64` | `ossutil` |
| 查看版本 | `ossutil64 --version` | `ossutil version` |
| 参数格式 | `-i -k -e` | `--access-key-id --access-key-secret --endpoint` |
| 帮助命令 | `ossutil64 help` | `ossutil --help` 或 `ossutil [cmd] --help` |

## 参考资源

- **官方完整命令列表：** https://help.aliyun.com/zh/oss/developer-reference/ossutil-overview/
- **命令详细说明：** 使用 `ossutil [command] --help` 查看
- **项目安装指南：** [OSSUTIL_INSTALL.md](./OSSUTIL_INSTALL.md)
- **OSS配置指南：** [OSS_SETUP.md](./OSS_SETUP.md)

---

**文档版本：** 1.0  
**ossutil 版本：** 2.2.0  
**更新日期：** 2025-10-20

