#include "protection.h"
#include <QtCore/QCryptographicHash>
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>
#include <QtCore/QProcess>
#include <QtCore/QDebug>
#include <QtCore/QRandomGenerator>
#include <QtCore/QDateTime>
#include <QtCore/QThread>
#include <QtWidgets/QApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <intrin.h>
#pragma comment(lib, "psapi.lib")
#endif

QTimer* Protection::s_protectionTimer = nullptr;
bool Protection::s_protectionActive = false;
QByteArray Protection::s_integrityHash;

// 字符串混淆和加密
QString Protection::deobfuscateString(const char* encrypted, size_t len)
{
    QByteArray data(encrypted, len);
    QByteArray key = generateKey();
    QByteArray decrypted = xorEncrypt(data, key);
    return QString::fromUtf8(decrypted);
}

QByteArray Protection::encryptString(const QString& plaintext)
{
    QByteArray data = plaintext.toUtf8();
    QByteArray key = generateKey();
    return xorEncrypt(data, key);
}

QString Protection::decryptString(const QByteArray& encrypted)
{
    QByteArray key = generateKey();
    QByteArray decrypted = xorEncrypt(encrypted, key);
    return QString::fromUtf8(decrypted);
}

QByteArray Protection::xorEncrypt(const QByteArray& data, const QByteArray& key)
{
    QByteArray result;
    result.resize(data.size());
    
    for (int i = 0; i < data.size(); ++i) {
        result[i] = data[i] ^ key[i % key.size()];
    }
    
    return result;
}

QByteArray Protection::generateKey()
{
    // 使用应用程序路径和版本信息生成密钥
    QString appPath = QCoreApplication::applicationFilePath();
    QString version = QCoreApplication::applicationVersion();
    QString combined = appPath + version + "ctdy123_protection_key";
    
    return QCryptographicHash::hash(combined.toUtf8(), QCryptographicHash::Md5);
}

// 反调试检测
bool Protection::performAntiDebugCheck()
{
    // 多重反调试检测
    if (isDebuggerPresent()) {
        return false;
    }
    
    if (checkRemoteDebugger()) {
        return false;
    }
    
    if (checkDebuggerByTiming()) {
        return false;
    }
    
    if (checkDebuggerByException()) {
        return false;
    }
    
    return true;
}

bool Protection::isDebuggerPresent()
{
#ifdef Q_OS_WIN
    // 检查调试器标志
    if (IsDebuggerPresent()) {
        return true;
    }
#endif
    
    return false;
}

bool Protection::checkRemoteDebugger()
{
#ifdef Q_OS_WIN
    // 检查远程调试器
    BOOL isRemoteDebuggerPresent = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &isRemoteDebuggerPresent);
    
    if (isRemoteDebuggerPresent) {
        return true;
    }
#endif
    
    return false;
}

bool Protection::checkDebuggerByTiming()
{
    // 使用时间检测调试器
    QElapsedTimer timer;
    timer.start();
    
    // 执行一些操作
    volatile int dummy = 0;
    for (int i = 0; i < 1000; ++i) {
        dummy += i;
    }
    
    qint64 elapsed = timer.elapsed();
    
    // 如果执行时间过长，可能被调试器中断
    return elapsed > 100; // 100ms阈值
}

bool Protection::checkDebuggerByException()
{
    // 简化的异常检测
    return false;
}

// 完整性检查
bool Protection::performIntegrityCheck()
{
    // 检查文件完整性
    QString appPath = QCoreApplication::applicationFilePath();
    QByteArray currentHash = calculateFileHash(appPath);
    
    if (s_integrityHash.isEmpty()) {
        s_integrityHash = currentHash;
        return true;
    }
    
    return currentHash == s_integrityHash;
}

QByteArray Protection::calculateFileHash(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    
    return hash.result();
}

bool Protection::verifyCodeIntegrity()
{
    // 验证关键代码段的完整性
    QByteArray keyCode = "ctdy123_protection_integrity";
    QByteArray expectedHash = QCryptographicHash::hash(keyCode, QCryptographicHash::Sha256);
    
    // 这里可以添加更多完整性检查逻辑
    return true;
}

// 水印保护
QString Protection::getWatermarkText()
{
    // 动态生成水印文本
    QString text = "影谷长图阅读器 ctdy123.com";
    return text;
}

bool Protection::shouldShowWatermark()
{
    // 多重验证是否应该显示水印
    if (!performAntiDebugCheck()) {
        return true; // 检测到调试器，强制显示水印
    }
    
    if (!performIntegrityCheck()) {
        return true; // 文件被修改，强制显示水印
    }
    
    // 这里可以添加更多验证逻辑
    return false;
}

void Protection::protectWatermarkLogic()
{
    // 保护水印逻辑不被轻易修改
    volatile int dummy = 0;
    for (int i = 0; i < 1000; ++i) {
        dummy += i * 2;
    }
    
    // 添加虚假的水印检查
    if (dummy % 2 == 0) {
        dummyFunction1();
    } else {
        dummyFunction2();
    }
    
    dummyFunction3();
}

// 授权验证保护
bool Protection::validateLicense()
{
    // 多重验证授权
    if (!performAntiDebugCheck()) {
        return false;
    }
    
    if (!performIntegrityCheck()) {
        return false;
    }
    
    if (!verifyCodeIntegrity()) {
        return false;
    }
    
    return true;
}

QByteArray Protection::encryptAuthData(const QByteArray& data)
{
    // 简化版本：直接返回原始数据
    return data;
}

QByteArray Protection::decryptAuthData(const QByteArray& encrypted)
{
    // 简化版本：直接返回原始数据
    return encrypted;
}

// 运行时保护
void Protection::startRuntimeProtection()
{
    if (s_protectionActive) {
        return;
    }
    
    s_protectionActive = true;
    s_protectionTimer = new QTimer();
    s_protectionTimer->setInterval(5000); // 每5秒检查一次
    
    Protection* instance = new Protection();
    QObject::connect(s_protectionTimer, &QTimer::timeout, instance, &Protection::onProtectionTimer);
    
    s_protectionTimer->start();
}

void Protection::stopRuntimeProtection()
{
    if (s_protectionTimer) {
        s_protectionTimer->stop();
        s_protectionTimer->deleteLater();
        s_protectionTimer = nullptr;
    }
    s_protectionActive = false;
}

void Protection::onProtectionTimer()
{
    // 定期执行保护检查
    if (!performAntiDebugCheck()) {
        QCoreApplication::quit();
        return;
    }
    
    if (!performIntegrityCheck()) {
        QCoreApplication::quit();
        return;
    }
    
    // 添加随机延迟防止定时攻击
    int delay = QRandomGenerator::global()->bounded(100, 500);
    QThread::msleep(delay);
}

// 辅助函数
void Protection::dummyFunction1()
{
    volatile int dummy = 0;
    for (int i = 0; i < 100; ++i) {
        dummy += i;
    }
}

void Protection::dummyFunction2()
{
    volatile int dummy = 0;
    for (int i = 0; i < 200; ++i) {
        dummy += i * 2;
    }
}

void Protection::dummyFunction3()
{
    volatile int dummy = 0;
    for (int i = 0; i < 150; ++i) {
        dummy += i * 3;
    }
}
