#ifndef APP_VERSION_H
#define APP_VERSION_H

#include <QtCore/QString>
#include <QtCore/QCoreApplication>
#include <QtCore/QRegularExpression>

/**
 * @file app_version.h
 * @brief LoimReader 统一版本管理系统
 *
 * 这是应用程序版本号的唯一真实来源 (Single Source of Truth)
 * 所有版本相关的代码都应该使用这个系统
 *
 * 版本号在编译时通过 CMake 从 CMakeLists.txt 传递
 * 确保了版本号的一致性和统一管理
 */

namespace LoimReader {

/**
 * @brief 应用程序版本管理类
 *
 * 提供统一的版本号获取接口，替代分散在各个文件中的硬编码版本
 */
class AppVersion {
public:
    /**
     * @brief 获取应用程序版本号
     * @return 版本号字符串，格式：major.minor.patch (例如: "2.7.2")
     *
     * 版本号来自编译时定义的宏 APP_VERSION
     * 该宏由 CMake 从 CMakeLists.txt 中的 VERSION 字段自动生成
     *
     * @note 这是应用程序版本号的唯一真实来源
     */
    static QString getAppVersion() {
#ifdef APP_VERSION
        return QString(APP_VERSION);
#else
        // 编译时未定义版本号的回退值（应与CMakeLists.txt中的默认版本一致）
        return "2.7.2";
#endif
    }

    /**
     * @brief 获取带有前缀的版本号
     * @return 带有 "v" 前缀的版本号，例如: "v2.7.2"
     *
     * 用于UI显示场景，如状态栏、关于对话框等
     */
    static QString getVersionWithPrefix() {
        return QString("v%1").arg(getAppVersion());
    }

    /**
     * @brief 获取完整的应用程序名称和版本
     * @return 完整的版本字符串，例如: "LoimReader v2.7.2"
     *
     * 用于用户代理字符串、日志记录等场景
     */
    static QString getFullAppVersion() {
        return QString("LoimReader/%1").arg(getAppVersion());
    }

    /**
     * @brief 获取User-Agent字符串
     * @return 用于网络请求的User-Agent字符串
     *
     * 在网络请求中使用，用于版本识别和统计
     */
    static QString getUserAgent() {
        return QString("LoimReader/%1 (macOS)").arg(getAppVersion());
    }

    /**
     * @brief 验证版本号格式
     * @param version 要验证的版本号字符串
     * @return 如果格式正确返回true，否则返回false
     *
     * 版本号格式应为：major.minor.patch
     * 例如：2.7.2, 1.0.0, 10.15.3
     */
    static bool isValidVersionFormat(const QString& version) {
        QRegularExpression versionRegex(R"(^\d+\.\d+\.\d+$)");
        return versionRegex.match(version).hasMatch();
    }

    /**
     * @brief 获取版本号的各个部分
     * @param version 版本号字符串
     * @return 包含major, minor, patch的结构体
     */
    struct VersionParts {
        int major = 0;
        int minor = 0;
        int patch = 0;
    };

    static VersionParts parseVersion(const QString& version) {
        VersionParts parts;
        QRegularExpression regex(R"(^(\d+)\.(\d+)\.(\d+)$)");
        QRegularExpressionMatch match = regex.match(version);

        if (match.hasMatch()) {
            parts.major = match.captured(1).toInt();
            parts.minor = match.captured(2).toInt();
            parts.patch = match.captured(3).toInt();
        }

        return parts;
    }

    /**
     * @brief 比较两个版本号
     * @param version1 第一个版本号
     * @param version2 第二个版本号
     * @return 如果version1 > version2返回1，相等返回0，小于返回-1
     */
    static int compareVersions(const QString& version1, const QString& version2) {
        VersionParts v1 = parseVersion(version1);
        VersionParts v2 = parseVersion(version2);

        if (v1.major != v2.major) {
            return v1.major > v2.major ? 1 : -1;
        }
        if (v1.minor != v2.minor) {
            return v1.minor > v2.minor ? 1 : -1;
        }
        if (v1.patch != v2.patch) {
            return v1.patch > v2.patch ? 1 : -1;
        }

        return 0;
    }

    /**
     * @brief 检查是否需要强制更新
     * @param currentVersion 当前版本
     * @param serverVersion 服务器版本
     * @return 如果需要强制更新返回true
     *
     * 强制更新规则：主版本号不同或次版本号差距大于1
     */
    static bool needsForceUpdate(const QString& currentVersion, const QString& serverVersion) {
        VersionParts current = parseVersion(currentVersion);
        VersionParts server = parseVersion(serverVersion);

        // 主版本号不同需要强制更新
        if (current.major != server.major) {
            return true;
        }

        // 次版本号差距大于1需要强制更新
        if (abs(current.minor - server.minor) > 1) {
            return true;
        }

        return false;
    }

    /**
     * @brief 获取构建信息
     * @return 构建信息字符串，包含编译时间和Qt版本
     */
    static QString getBuildInfo() {
        QString buildDate = QString(__DATE__).simplified();
        QString buildTime = QString(__TIME__).simplified();
        QString qtVersion = qVersion();

        return QString("Built on %1 %2 with Qt %3")
               .arg(buildDate, buildTime, qtVersion);
    }

    /**
     * @brief 获取应用程序的完整信息
     * @return 包含版本、构建信息等的完整字符串
     */
    static QString getFullAppInfo() {
        return QString("%1 %2\n%3")
               .arg(getFullAppVersion(), getBuildInfo());
    }
};

/**
 * @brief 兼容性别名，保持向后兼容
 *
 * 为了保持与现有代码的兼容性，提供这些别名
 * 新代码应该直接使用 AppVersion 类
 */
namespace VersionCompatibility {
    inline QString getCurrentVersion() {
        return AppVersion::getAppVersion();
    }

    inline QString getVersionString() {
        return AppVersion::getVersionWithPrefix();
    }
}

} // namespace LoimReader

/**
 * @brief 全局便利函数
 *
 * 提供全局函数接口，方便在不想使用命名空间的情况下调用
 */
inline QString getAppVersion() {
    return LoimReader::AppVersion::getAppVersion();
}

inline QString getVersionWithPrefix() {
    return LoimReader::AppVersion::getVersionWithPrefix();
}

#endif // APP_VERSION_H

