# LoimReader 版本管理指南

## 版本号体系

### 当前版本号的位置

LoimReader 的版本号存储在以下文件中（按优先级排序）：

| 文件 | 位置 | 作用 | 优先级 |
|------|------|------|--------|
| `CMakeLists.txt` | 第5行 `VERSION 2.7.2` | 编译时版本（唯一真实来源） | 🔴 最高 |
| `app_version.h` | 第42行 `return "2.7.2"` | 代码中的回退版本 | 🟡 中 |
| `onekey_mac_deploy.sh` | 第17行 `PROJECT_VERSION="2.7.2"` | 部署脚本默认版本 | 🟢 低 |

**重要说明：**

- **CMakeLists.txt** 是版本号的**唯一真实来源**（Single Source of Truth）
- 编译时，CMake会将此版本号通过宏 `APP_VERSION` 传递给C++代码
- `app_version.h` 中的版本号仅作为回退值（当宏未定义时使用）
- `onekey_mac_deploy.sh` 中的版本号用于文件命名和 version.json 生成

## 快速发布新版本

### 方法一：使用 release.sh（最简单）

```bash
# 一键发布 2.7.1 版本
./release.sh 2.7.1

# 会自动完成所有步骤：
# ✅ 更新所有文件中的版本号
# ✅ Git提交和打标签
# ✅ 清理并构建
# ✅ 生成DMG和version.json
# ✅ 验证输出

# 发布并上传
./release.sh 2.7.1 -u

# 仅本地测试（不提交Git）
./release.sh 2.7.1 -s
```

### 方法二：手动步骤

#### 1. 修改版本号

**CMakeLists.txt：**
```cmake
project(LoimReader 
    VERSION 2.7.1    # 修改这里
    LANGUAGES CXX
)
```

**app_version.h：**
```cpp
#else
    return "2.7.1";  // 修改这里
#endif
```

**onekey_mac_deploy.sh：**
```bash
PROJECT_VERSION="2.7.1"  # 修改这里
```

**快速批量替换：**
```bash
# 替换 2.7.2 -> 2.7.1
sed -i '' 's/VERSION 2\.7\.2/VERSION 2.7.1/g' CMakeLists.txt
sed -i '' 's/return "2\.7\.2";/return "2.7.1";/g' app_version.h
sed -i '' 's/PROJECT_VERSION="2\.7\.2"/PROJECT_VERSION="2.7.1"/g' onekey_mac_deploy.sh
```

#### 2. 验证修改

```bash
# 检查所有文件
grep "2\.7\.1" CMakeLists.txt app_version.h onekey_mac_deploy.sh

# 应该看到三个匹配结果
```

#### 3. Git提交

```bash
git add CMakeLists.txt app_version.h onekey_mac_deploy.sh
git commit -m "chore: 版本更新到 2.7.1"
git tag -a v2.7.1 -m "Release version 2.7.1"
```

#### 4. 构建

```bash
# 清理并构建
./onekey_mac_deploy.sh -c

# 或指定版本号（会覆盖脚本中的值）
./onekey_mac_deploy.sh -c -v 2.7.1
```

#### 5. 验证输出

```bash
# 检查文件名
ls -lh ../deploy/uploads/loimreader/LoimReader_v2.7.1_macOS.dmg

# 检查version.json
cat ../deploy/uploads/loimreader/version.json | python3 -m json.tool | grep latestVersion
# 输出: "latestVersion": "2.7.1",
```

#### 6. 测试应用

```bash
# 挂载DMG
open ../deploy/uploads/loimreader/LoimReader_v2.7.1_macOS.dmg

# 在应用中验证版本号
# 菜单 -> 关于 -> 应该显示 "v2.7.1"
```

#### 7. 上传

```bash
cd ../deploy/uploads/loimreader
./upload_to_oss.sh
```

#### 8. 验证和推送

```bash
# 验证OSS
curl -s https://limereader-releases.oss-cn-hangzhou.aliyuncs.com/updates/loimreader/version.json | grep latestVersion

# 推送Git
git push origin main
git push origin v2.7.1
```

## 版本号规范

### 语义化版本

LoimReader 采用 [语义化版本](https://semver.org/lang/zh-CN/) 规范：`主版本号.次版本号.修订号`
