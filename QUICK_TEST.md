# 快速测试指南

## 验证新部署脚本

### 步骤 1: 检查文件

```bash
# 检查脚本是否存在
ls -l onekey_mac_deploy.sh onekey_win_deploy.bat

# 应该看到：
# -rwxr-xr-x  onekey_mac_deploy.sh  (macOS有执行权限)
# -rw-r--r--  onekey_win_deploy.bat (Windows批处理文件)
```

### 步骤 2: 测试 macOS 脚本

```bash
# 查看帮助信息
./onekey_mac_deploy.sh -h

# 应该看到：
# ========================================
#  LoimReader macOS 一键部署脚本
# ========================================
# 用法说明...
```

### 步骤 3: 试运行（不实际构建）

```bash
# 检查环境（不会修改任何文件）
./onekey_mac_deploy.sh -h

# 预期输出：帮助信息和选项说明
```

### 步骤 4: 完整测试（可选）

```bash
# 完整构建测试
./onekey_mac_deploy.sh -c

# 预期结果：
# 1. 清理旧文件
# 2. 生成图标和背景图
# 3. 配置CMake
# 4. 编译应用
# 5. 部署Qt依赖
# 6. 修复签名
# 7. 创建DMG
# 8. 生成version.json
# 9. 生成上传脚本
#
# 输出位置：../deploy/uploads/loimreader/
```

### 步骤 5: 验证输出文件

```bash
# 检查生成的文件
ls -lh ../deploy/uploads/loimreader/

# 应该看到：
# LoimReader_v2.7.2_macOS.dmg     (DMG安装包)
# version.json                    (版本信息)
# upload_to_oss.sh                (上传脚本)
```

### 步骤 6: 验证 version.json 格式

```bash
# 检查JSON格式
cat ../deploy/uploads/loimreader/version.json | python3 -m json.tool

# 应该看到格式化的JSON，无错误
```

### 步骤 7: 测试DMG（可选）

```bash
# 挂载DMG
open ../deploy/uploads/loimreader/LoimReader_v2.7.2_macOS.dmg

# 在Finder中：
# 1. 看到LoimReader.app
# 2. 看到Applications链接
# 3. 看到使用说明.txt
# 4. 拖拽到Applications测试安装
```

## Windows 脚本测试

### 在 Windows 上

```cmd
# 双击 onekey_win_deploy.bat
# 或在命令行执行：
onekey_win_deploy.bat

# 检查输出
dir ..\deploy\uploads\loimreader\

# 应该看到：
# LoimReader_v2.7.2_Windows.zip
# version.json
# upload_to_oss.bat
```

## 常见测试问题

### macOS

**问题：** Permission denied
```bash
chmod +x onekey_mac_deploy.sh
```

**问题：** Qt not found
```bash
export PATH="/opt/homebrew/opt/qt@6/bin:$PATH"
```

**问题：** CMake not found
```bash
brew install cmake
```

### Windows

**问题：** QT_DIR 未设置
```cmd
set QT_DIR=D:\Qt\6.8.1\mingw_64
```

**问题：** MINGW_DIR 未设置
```cmd
set MINGW_DIR=D:\Qt\Tools\mingw1120_64
```

## 测试通过标准

✅ **成功标准：**

1. 脚本运行无错误
2. 生成了DMG/ZIP文件
3. version.json格式正确
4. 生成了upload脚本
5. DMG/ZIP可以正常打开
6. 应用可以运行

❌ **失败标准：**

1. 脚本报错退出
2. 文件生成不完整
3. JSON格式错误
4. DMG/ZIP损坏
5. 应用无法运行

## 快速验证命令

```bash
# 一键验证（macOS）
./onekey_mac_deploy.sh -h && \
ls -l onekey_mac_deploy.sh && \
echo "✅ 脚本就绪"

# 检查依赖
which cmake && \
which qmake && \
which python3 && \
echo "✅ 依赖完整"
```

## 回滚方案

如果新脚本有问题，可以暂时使用手动构建：

```bash
# 手动构建步骤
mkdir -p build_cmake
cd build_cmake
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.ncpu)
cd bin
macdeployqt LoimReader.app
cd ../../..
```

## 需要帮助？

1. 查看详细文档：`一键部署指南.md`
2. 查看变更日志：`DEPLOYMENT_CHANGELOG.md`
3. 查看OSS规范：`../deploy/OSS_RELEASE_GUIDE.md`
4. 检查脚本内的注释和错误信息

---

测试日期：2025-10-20  
测试版本：v2.7.2

