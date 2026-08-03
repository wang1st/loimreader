#ifndef DOWNLOAD_UPDATE_DIALOG_H
#define DOWNLOAD_UPDATE_DIALOG_H

#include <QDialog>
#include <QProgressBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QDateTime>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

class DownloadUpdateDialog : public QDialog
{
    Q_OBJECT

public:
    // 更新信息结构
    struct UpdateInfo {
        QString latestVersion;
        QString currentVersion;
        QString updateUrl;
        QString updateLog;
        QString updateSize;
        QString checksumMD5;
        bool forceUpdate;
        bool hasUpdate;
        
        UpdateInfo() : forceUpdate(false), hasUpdate(false) {}
    };
    
    explicit DownloadUpdateDialog(const UpdateInfo& updateInfo, QWidget* parent = nullptr);
    ~DownloadUpdateDialog();

    // 对话框结果
    enum Result {
        Update,
        Later,
        Skip
    };

    Result getResult() const { return m_result; }

signals:
    void requestStartDownload();
    void requestPauseDownload();

public slots:
    void onUpdateClicked();
    void onLaterClicked();
    void onSkipClicked();
    void onCloseClicked();
    void onDownloadProgress(int progress, qint64 received, qint64 total);
    void onDownloadFinished(const QString& filePath);
    void onDownloadError(const QString& error);
    void updateProgressDisplay();
    void openDownloadFolder();
    void setDownloadFileSize(qint64 bytes);

private:
    void setupUI();
    void setupStyles();
    void updateDownloadMode();
    void showProgressMode();
    void showCompleteMode();
    void showErrorMode(const QString& error);
    QString formatFileSize(qint64 bytes) const;
    QString formatSpeed(qint64 bytesPerSecond) const;
    bool verifyFileHash(const QString& filePath, const QString& expectedHash) const;

    // UI组件
    QLabel* m_titleLabel;
    QLabel* m_versionInfoLabel;
    QLabel* m_changelogLabel;
    QLabel* m_sizeLabel;
    QLabel* m_md5Label;
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QLabel* m_speedLabel;
    QLabel* m_filePathLabel;

    QPushButton* m_updateButton;
    QPushButton* m_laterButton;
    QPushButton* m_skipButton;
    QPushButton* m_openFolderButton;
    QPushButton* m_retryButton;
    QPushButton* m_closeButton;

    // 布局
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_buttonLayout;
    QHBoxLayout* m_progressLayout;
    QHBoxLayout* m_statusLayout;

    // 数据和状态
    UpdateInfo m_updateInfo;
    Result m_result;
    bool m_isDownloading;
    bool m_downloadComplete;
    bool m_hashVerified;
    QString m_downloadFilePath;
    qint64 m_downloadSpeed;
    QString m_lastError;

    // 定时器
    QTimer* m_progressTimer;
    QTimer* m_speedTimer;
    qint64 m_lastSpeedCheck;
    qint64 m_lastDownloadedBytes;

    // 样式常量
    static const int DIALOG_WIDTH = 480;
    static const int DIALOG_HEIGHT = 580;  // 增加高度以容纳更多内容
    static const int BUTTON_HEIGHT = 36;
    static const int BUTTON_WIDTH = 100;
    static const int PROGRESS_HEIGHT = 8;
};

#endif // DOWNLOAD_UPDATE_DIALOG_H

