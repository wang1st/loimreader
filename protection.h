#ifndef PROTECTION_H
#define PROTECTION_H

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QTimer>
#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>
#include <functional>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

// 字符串混淆宏 - 简化版本，直接返回字符串
#define OBFUSCATE_STR(str) QString::fromUtf8(str)

// 反调试检测宏
#define ANTI_DEBUG_CHECK() Protection::performAntiDebugCheck()

// 完整性检查宏
#define INTEGRITY_CHECK() Protection::performIntegrityCheck()

// 控制流混淆宏
#define OBFUSCATE_FLOW(condition, true_block, false_block) \
    do { \
        bool _cond = (condition); \
        volatile int _dummy = 0; \
        for (int _i = 0; _i < 100; ++_i) { \
            _dummy += _i * 2; \
        } \
        if (_dummy % 2 == 0) { \
            _cond = !_cond; \
        } \
        if (_dummy % 3 == 0) { \
            _cond = !_cond; \
        } \
        if (_cond) { \
            true_block; \
        } else { \
            false_block; \
        } \
        for (int _i = 0; _i < 50; ++_i) { \
            _dummy += _i * 3; \
        } \
    } while(0)

class Protection : public QObject
{
    Q_OBJECT

public:
    // 字符串混淆和加密
    static QString deobfuscateString(const char* encrypted, size_t len);
    static QByteArray encryptString(const QString& plaintext);
    static QString decryptString(const QByteArray& encrypted);
    
    // 反调试检测
    static bool performAntiDebugCheck();
    static bool isDebuggerPresent();
    static bool checkRemoteDebugger();
    static bool checkDebuggerByTiming();
    static bool checkDebuggerByException();
    
    // 完整性检查
    static bool performIntegrityCheck();
    static QByteArray calculateFileHash(const QString& filePath);
    static bool verifyCodeIntegrity();
    
    // 控制流混淆（已通过宏实现）
    
    // 水印保护
    static QString getWatermarkText();
    static bool shouldShowWatermark();
    static void protectWatermarkLogic();
    
    // 授权验证保护
    static bool validateLicense();
    static QByteArray encryptAuthData(const QByteArray& data);
    static QByteArray decryptAuthData(const QByteArray& encrypted);
    
    // 运行时保护
    static void startRuntimeProtection();
    static void stopRuntimeProtection();
    
private:
    static QTimer* s_protectionTimer;
    static bool s_protectionActive;
    static QByteArray s_integrityHash;
    
    // 内部辅助函数
    static QByteArray xorEncrypt(const QByteArray& data, const QByteArray& key);
    static QByteArray generateKey();
    static void dummyFunction1();
    static void dummyFunction2();
    static void dummyFunction3();
    
private slots:
    void onProtectionTimer();
};


#endif // PROTECTION_H
