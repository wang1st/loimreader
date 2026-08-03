#ifndef SIMPLEUPDATER_H
#define SIMPLEUPDATER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QProgressBar>
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>
#include <QTimer>

/**
 * 轻量级自定义更新器
 * 用于替代重量级的Fervor框架
 * 使用JSON格式的更新配置文件，部署在阿里云OSS
 */
class SimpleUpdater : public QObject
{
    Q_OBJECT

public:
    struct UpdateInfo {
        QString version;
        QString title;
        QString description;
        QString downloadUrl;
        QString releaseDate;
        bool isForced = false;
        QString platform;
        qint64 fileSize = 0;
    };

    explicit SimpleUpdater(QObject *parent = nullptr);
    ~SimpleUpdater();

    // 设置更新源URL（阿里云OSS上的JSON文件）
    void setUpdateUrl(const QString &url);
    
    // 检查更新（静默）
    void checkForUpdatesSilent();
    
    // 检查更新（显示结果）
    void checkForUpdatesNotSilent();

private slots:
    void onUpdateCheckFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_reply;
    QString m_updateUrl;
    bool m_silentMode;
    UpdateInfo m_latestUpdate;

    // 版本比较
    bool isNewerVersion(const QString &remoteVersion, const QString &currentVersion);
    
    // 解析更新JSON
    bool parseUpdateJson(const QByteArray &jsonData);
    
    // 获取当前平台标识
    QString getCurrentPlatform();
    
    // 显示更新对话框
    void showUpdateDialog(const UpdateInfo &updateInfo);
    
    // 显示错误信息
    void showErrorMessage(const QString &message);
    
    // 显示无更新信息
    void showNoUpdateMessage();
};

/**
 * 更新对话框 - 与登录窗口风格一致
 */
class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateDialog(const SimpleUpdater::UpdateInfo &updateInfo, QWidget *parent = nullptr);

private slots:
    void onDownloadClicked();
    void onRemindLaterClicked();
    void onSkipVersionClicked();

private:
    void setupUI();
    void applyUnifiedStyle();
    QString createUpdateText() const;

    SimpleUpdater::UpdateInfo m_updateInfo;
    QLabel *m_titleLabel;
    QLabel *m_logoLabel;
    QTextBrowser *m_contentBrowser;
    QPushButton *m_downloadBtn;
    QPushButton *m_remindLaterBtn;
    QPushButton *m_skipVersionBtn;
};

#endif // SIMPLEUPDATER_H