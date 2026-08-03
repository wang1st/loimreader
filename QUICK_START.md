# 🚀 影谷快速开始

## 一键运行

```bash
# 最简单的方式 - 构建并运行
./quick_run.sh

# 或者使用完整版本
./auto_build_and_run.sh
```

## 脚本说明

### 🏃‍♂️ 日常开发: `quick_run.sh`
- 🚀 **最快**: 增量构建，简洁输出
- 🔄 **智能**: 自动检测是否需要重建
- 📝 **日志**: 错误信息保存到 `build.log` 和 `signing.log`

### 🛠️ 完整版本: `auto_build_and_run.sh`
- 📊 **详细**: 完整的状态报告和错误检查
- ⚙️ **灵活**: 多种选项和运行模式
- 🔍 **调试**: 适合故障排除

## 常用命令

```bash
# 快速开发迭代
./quick_run.sh

# 强制重建
./quick_run.sh rebuild

# 完整重建（清理模式）
./auto_build_and_run.sh --rebuild

# 构建但不打开应用
./auto_build_and_run.sh --no-open

# 查看所有选项
./auto_build_and_run.sh --help
```

## 应用功能

✅ 无强制登录启动 - 程序启动时不弹出登录对话框  
✅ 工具栏登录按钮 - 用户可主动点击登录  
✅ 免费版模式 - 未登录时带水印使用  
✅ 统一界面设计 - 登录页面与主窗口风格一致  

---

🎉 **享受开发！**