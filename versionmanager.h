#ifndef VERSIONMANAGER_H
#define VERSIONMANAGER_H

#include <QtCore/QString>
#include <QtCore/QJsonObject>
#include <QtCore/QDateTime>

class VersionManager
{
public:
    struct Version {
        int major = 0;
        int minor = 0;
        int patch = 0;

        Version() = default;
        Version(int maj, int min, int pat) : major(maj), minor(min), patch(pat) {}

        static Version fromString(const QString& versionStr);
        QString toString() const;

        bool needsForceUpdate(const Version& current) const;
        bool isNewerThan(const Version& current) const;
        bool isSameAs(const Version& other) const;

        bool isValid() const { return major > 0 || minor > 0 || patch > 0; }
    };

    struct UpdateInfo {
        bool hasUpdate = false;
        bool forceUpdate = false;
        Version currentVersion;
        Version latestVersion;
        QString updateUrl;
        QString updateSize;
        QString updateLog;
        QString publishTime;
        QString checksumMD5;
        QString platform;
        QString minSystemVersion;

        bool isValid() const { return latestVersion.isValid(); }
    };

public:
    // 版本比较
    static bool compareVersions(const QString& current, const QString& latest);
    static int versionCompare(const Version& v1, const Version& v2);

    // 解析更新信息
    static UpdateInfo parseUpdateInfo(const QJsonObject& json, const QString& currentVersion, const QString& platform);

    // 版本检查
    static bool shouldSkipVersion(const UpdateInfo& updateInfo, const QString& skippedVersion);
    static bool isVersionSkippable(const Version& currentVersion, const Version& latestVersion);

    // 平台信息
    static QString getCurrentPlatform();
    static QString getPlatformDisplayName();

    // 版本格式化
    static QString formatVersion(const Version& version);
    static QString formatFileSize(const QString& sizeStr);
    static QString formatDate(const QString& dateStr);

    // 更新URL构建
    static QString buildUpdateUrl(const QString& baseUrl, const Version& version, const QString& platform);
    static QString getUpdateFileName(const Version& version, const QString& platform);

private:
    // 辅助方法
    static Version parseVersionString(const QString& versionStr);
    static QJsonObject extractPackageInfo(const QJsonObject& json, const QString& platform);
    static QString formatUpdateLog(const QString& rawLog);
};

#endif // VERSIONMANAGER_H

