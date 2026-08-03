# 代码混淆和反调试保护系统

本系统为影谷长图阅读器提供了全面的代码保护，特别针对授权鉴证和水印部分进行了加固。

## 保护特性

### 1. 字符串混淆和加密
- **动态字符串解密**：使用 `OBFUSCATE_STR()` 宏对敏感字符串进行混淆
- **XOR加密**：基于应用程序路径和版本信息生成密钥
- **运行时解密**：字符串在运行时动态解密，防止静态分析

### 2. 反调试检测
- **多重检测机制**：
  - `IsDebuggerPresent()` API检测
  - PEB调试标志检查
  - 远程调试器检测
  - 时间检测（防止断点）
  - 异常处理检测
- **实时监控**：每5秒进行一次反调试检查
- **自动退出**：检测到调试器时自动退出程序

### 3. 完整性检查
- **文件完整性**：计算并验证应用程序文件的SHA256哈希
- **代码完整性**：验证关键代码段的完整性
- **运行时验证**：定期检查文件是否被修改

### 4. 控制流混淆
- **虚假分支**：添加大量虚假的条件分支和循环
- **延迟执行**：在关键逻辑前后添加延迟操作
- **复杂条件**：使用复杂的条件判断隐藏真实逻辑

### 5. 授权验证保护
- **多重验证**：结合反调试、完整性检查等多种验证方式
- **数据加密**：登录数据在传输前进行加密
- **动态验证**：授权状态在运行时动态验证

### 6. 水印逻辑保护
- **动态水印**：水印文本在运行时动态生成
- **多重检查**：结合用户类型、反调试、完整性检查决定是否显示水印
- **逻辑混淆**：水印显示逻辑被大量虚假代码包围

## 使用方法

### 基本使用
```cpp
// 字符串混淆
QString freeStr = OBFUSCATE_STR("free");

// 反调试检查
if (!ANTI_DEBUG_CHECK()) {
    // 检测到调试器，执行保护逻辑
    return;
}

// 完整性检查
if (!INTEGRITY_CHECK()) {
    // 文件被修改，执行保护逻辑
    return;
}

// 控制流混淆
OBFUSCATE_FLOW(condition, {
    // 真实逻辑
    doRealWork();
}, {
    // 虚假逻辑
    doFakeWork();
});
```

### 构建带保护的版本
```bash
# 使用混淆构建脚本
build_qt6_obfuscated.cmd
```

### 字符串混淆工具
```bash
# 生成混淆字符串
python string_obfuscator.py "要混淆的字符串"

# 生成常用混淆字符串
python string_obfuscator.py
```

## 保护机制详解

### 反调试检测
1. **API检测**：使用Windows API检测调试器
2. **PEB检查**：检查进程环境块中的调试标志
3. **时间检测**：通过执行时间判断是否被调试器中断
4. **异常检测**：通过异常处理判断调试器存在

### 完整性验证
1. **文件哈希**：计算应用程序文件的SHA256哈希值
2. **首次运行**：记录初始哈希值
3. **定期检查**：运行时定期验证文件完整性
4. **代码验证**：验证关键代码段的完整性

### 控制流混淆
1. **虚假循环**：添加大量无意义的循环操作
2. **条件复杂化**：将简单条件转换为复杂表达式
3. **分支插入**：在真实逻辑中插入虚假分支
4. **延迟操作**：在关键操作前后添加延迟

## 配置选项

### 编译时配置
```cpp
// 启用混淆构建
DEFINES += OBFUSCATED_BUILD

// 自定义保护级别
DEFINES += PROTECTION_LEVEL=3
```

### 运行时配置
```cpp
// 启动运行时保护
Protection::startRuntimeProtection();

// 停止运行时保护
Protection::stopRuntimeProtection();
```

## 注意事项

1. **性能影响**：保护机制会增加一定的性能开销
2. **兼容性**：某些反调试技术可能影响调试版本的正常运行
3. **维护性**：混淆后的代码较难调试和维护
4. **更新频率**：建议定期更新保护策略以应对新的破解技术

## 安全建议

1. **多层防护**：结合多种保护技术，不依赖单一机制
2. **定期更新**：定期更新保护策略和检测方法
3. **监控日志**：记录保护机制的触发情况
4. **用户反馈**：收集用户反馈，优化保护策略

## 技术细节

### 字符串混淆算法
```cpp
QByteArray xorEncrypt(const QByteArray& data, const QByteArray& key) {
    QByteArray result;
    result.resize(data.size());
    
    for (int i = 0; i < data.size(); ++i) {
        result[i] = data[i] ^ key[i % key.size()];
    }
    
    return result;
}
```

### 密钥生成
```cpp
QByteArray generateKey() {
    QString appPath = QCoreApplication::applicationFilePath();
    QString version = QCoreApplication::applicationVersion();
    QString combined = appPath + version + "ctdy123_protection_key";
    
    return QCryptographicHash::hash(combined.toUtf8(), QCryptographicHash::Md5);
}
```

### 反调试检测
```cpp
bool isDebuggerPresent() {
#ifdef Q_OS_WIN
    if (IsDebuggerPresent()) return true;
    
    PPEB peb = (PPEB)__readgsqword(0x60);
    if (peb && peb->BeingDebugged) return true;
    
    if (peb && (peb->NtGlobalFlag & 0x70)) return true;
#endif
    return false;
}
```

## 更新日志

- **v1.0**：初始版本，包含基本的反调试和字符串混淆
- **v1.1**：添加完整性检查和控制流混淆
- **v1.2**：增强水印保护逻辑
- **v1.3**：添加运行时保护机制
