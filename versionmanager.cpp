#include "versionmanager.h"
#include <QtCore/QRegularExpression>
#include <QtCore/QDebug>
#include <QtCore/QSysInfo>

// Version结构体方法实现
VersionManager::Version VersionManager::Version::fromString(const QString& versionStr)
{
    Version version;
    QRegularExpression re(R"(^(\d+)\.(\d+)\.(\d+)$)");
    QRegularExpressionMatch match = re.match(versionStr);

    if (match.hasMatch()) {
        version.major = match.captured(1).toInt();
        version.minor = match.captured(2).toInt();
        version.patch = match.captured(3).toInt();
    }

    return version;
}

QString VersionManager::Version::toString() const
{
    return QString("%1.%2.%3").arg(major).arg(minor).arg(patch);
}

bool VersionManager::Version::needsForceUpdate(const Version& current) const
{
    return major > current.major;  // 主版本升级强制更新
}

bool VersionManager::Version::isNewerThan(const Version& current) const
{
    if (major > current.major) return true;
    if (major == current.major && minor > current.minor) return true;
    if (major == current.major && minor == current.minor && patch > current.patch) return true;
    return false;
}

bool VersionManager::Version::isSameAs(const Version& other) const
{
    return major == other.major && minor == other.minor && patch == other.patch;
}

// VersionManager静态方法实现
bool VersionManager::compareVersions(const QString& current, const QString& latest)
{
    Version currentVer = Version::fromString(current);
    Version latestVer = Version::fromString(latest);

    return latestVer.isNewerThan(currentVer);
}

int VersionManager::versionCompare(const Version& v1, const Version& v2)
{
    if (v1.major != v2.major) return v1.major - v2.major;
    if (v1.minor != v2.minor) return v1.minor - v2.minor;
    return v1.patch - v2.patch;
}

VersionManager::UpdateInfo VersionManager::parseUpdateInfo(const QJsonObject& json, const QString& currentVersion, const QString& platform)
{
    UpdateInfo info;

    // 基本信息
    info.currentVersion = Version::fromString(currentVersion);
    info.hasUpdate = json["hasUpdate"].toBool();
    info.forceUpdate = json["forceUpdate"].toBool();

    if (json.contains("latestVersion")) {
        info.latestVersion = Version::fromString(json["latestVersion"].toString());
    }

    // 包信息
    QJsonObject packageInfo = extractPackageInfo(json, platform);
    if (!packageInfo.isEmpty()) {
        info.updateUrl = packageInfo["downloadUrl"].toString();
        info.updateSize = packageInfo["size"].toString();
        info.checksumMD5 = packageInfo["hash"].toString();
        info.minSystemVersion = packageInfo["minSystemVersion"].toString();
    }

    // 其他信息
    info.updateLog = formatUpdateLog(json["updateLog"].toString());
    info.publishTime = formatDate(json["publishTime"].toString());
    info.platform = platform;

    // 验证更新信息
    if (info.hasUpdate && info.latestVersion.isValid()) {
        info.hasUpdate = info.latestVersion.isNewerThan(info.currentVersion);
        if (info.hasUpdate) {
            info.forceUpdate = info.latestVersion.needsForceUpdate(info.currentVersion);
        }
    }

    return info;
}

bool VersionManager::shouldSkipVersion(const UpdateInfo& updateInfo, const QString& skippedVersion)
{
    if (skippedVersion.isEmpty()) return false;

    Version skippedVer = Version::fromString(skippedVersion);
    return updateInfo.latestVersion.isSameAs(skippedVer);
}

bool VersionManager::isVersionSkippable(const Version& currentVersion, const Version& latestVersion)
{
    // 只有非强制更新可以跳过
    return !latestVersion.needsForceUpdate(currentVersion);
}

QString VersionManager::getCurrentPlatform()
{
#ifdef Q_OS_WIN
    return "windows";
#elif defined(Q_OS_MAC)
    return "macos";
#elif defined(Q_OS_LINUX)
    return "linux";
#else
    return "unknown";
#endif
}

QString VersionManager::getPlatformDisplayName()
{
    QString platform = getCurrentPlatform();
    if (platform == "windows") return "Windows";
    if (platform == "macos") return "macOS";
    if (platform == "linux") return "Linux";
    return "未知平台";
}

QString VersionManager::formatVersion(const Version& version)
{
    return QString("v%1.%2.%3").arg(version.major).arg(version.minor).arg(version.patch);
}

QString VersionManager::formatFileSize(const QString& sizeStr)
{
    // 如果已经包含单位，直接返回
    if (sizeStr.contains("MB") || sizeStr.contains("GB") || sizeStr.contains("KB")) {
        return sizeStr;
    }

    // 尝试解析为数字
    bool ok;
    double size = sizeStr.toDouble(&ok);
    if (!ok) return sizeStr;

    if (size >= 1024 * 1024 * 1024) {
        return QString("%1GB").arg(size / (1024 * 1024 * 1024), 0, 'f', 1);
    } else if (size >= 1024 * 1024) {
        return QString("%1MB").arg(size / (1024 * 1024), 0, 'f', 1);
    } else if (size >= 1024) {
        return QString("%1KB").arg(size / 1024, 0, 'f', 1);
    } else {
        return QString("%1B").arg(size);
    }
}

QString VersionManager::formatDate(const QString& dateStr)
{
    if (dateStr.isEmpty()) return "";

    // 尝试解析ISO格式日期
    QDateTime dateTime = QDateTime::fromString(dateStr, Qt::ISODate);
    if (dateTime.isValid()) {
        return dateTime.toString("yyyy-MM-dd hh:mm");
    }

    // 尝试其他常见格式
    QStringList formats = {
        "yyyy-MM-dd hh:mm:ss",
        "yyyy-MM-dd",
        "yyyy/MM/dd hh:mm:ss",
        "yyyy/MM/dd"
    };

    for (const QString& format : formats) {
        dateTime = QDateTime::fromString(dateStr, format);
        if (dateTime.isValid()) {
            return dateTime.toString("yyyy-MM-dd hh:mm");
        }
    }

    return dateStr; // 如果无法解析，返回原始字符串
}

QString VersionManager::buildUpdateUrl(const QString& baseUrl, const Version& version, const QString& platform)
{
    QString fileName = getUpdateFileName(version, platform);
    return baseUrl.endsWith('/') ? baseUrl + fileName : baseUrl + "/" + fileName;
}

QString VersionManager::getUpdateFileName(const Version& version, const QString& platform)
{
    QString suffix = "zip";  // Windows默认使用zip
    if (platform == "macos") {
        suffix = "dmg";
    }

    return QString("LoimReader_v%1_%2.%3")
           .arg(version.toString())
           .arg(platform == "windows" ? "Windows" : "macOS")
           .arg(suffix);
}

// 私有辅助方法
VersionManager::Version VersionManager::parseVersionString(const QString& versionStr)
{
    return Version::fromString(versionStr);
}

QJsonObject VersionManager::extractPackageInfo(const QJsonObject& json, const QString& platform)
{
    // 首先尝试从packages对象中获取
    if (json.contains("packages")) {
        QJsonObject packages = json["packages"].toObject();
        if (packages.contains(platform)) {
            return packages[platform].toObject();
        }
    }

    // 兼容旧格式，直接从根级别获取
    QJsonObject result;
    if (json.contains("updateUrl")) result["downloadUrl"] = json["updateUrl"];
    if (json.contains("updateSize")) result["size"] = json["updateSize"];
    if (json.contains("checksumMD5")) result["hash"] = json["checksumMD5"];
    if (json.contains("minSystemVersion")) result["minSystemVersion"] = json["minSystemVersion"];

    return result;
}

QString VersionManager::formatUpdateLog(const QString& rawLog)
{
    if (rawLog.isEmpty()) return "暂无更新说明";

    // 清理和格式化更新日志
    QString log = rawLog.trimmed();

    // 如果日志很长，进行截断
    if (log.length() > 500) {
        log = log.left(500) + "...";
    }

    return log;
}

