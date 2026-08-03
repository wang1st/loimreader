#include "updatedialog.h"
#include "app_version.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QScrollArea>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>
#include <QSettings>
#include <QStyle>
#include <QFileInfo>
#include <QDateTime>
#include <QScreen>
#include <QGuiApplication>
#include <QFrame>
#include <QRect>


DownloadUpdateDialog::DownloadUpdateDialog(const UpdateInfo& updateInfo, QWidget* parent)
    : QDialog(parent)
    , m_updateInfo(updateInfo)
    , m_result(Later)
    , m_isDownloading(false)
    , m_downloadComplete(false)
    , m_hashVerified(false)
    , m_downloadSpeed(0)
    , m_lastSpeedCheck(0)
    , m_lastDownloadedBytes(0)
    , m_progressTimer(new QTimer(this))
    , m_speedTimer(new QTimer(this))
{
    qDebug() << "[DownloadUpdateDialog] 创建更新对话框，版本:" << updateInfo.latestVersion;

    // Windows 下按 DPI 调整基础字号与控件尺寸
#ifdef Q_OS_WIN
    {
        QScreen* scr = QGuiApplication::primaryScreen();
        const double scale = scr ? scr->logicalDotsPerInchX() / 96.0 : 1.0;
        const double clampScale = qBound(1.0, scale, 1.6);

        QFont base = font();
        base.setPointSizeF(qBound(9.0, 10.0 * clampScale, 11.0));
        setFont(base);
        
        // 设置Windows下的对话框属性，确保正确显示
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAttribute(Qt::WA_NoSystemBackground, false);
    }
#endif

    setupUI();
    setupStyles();

    // 连接定时器
    connect(m_progressTimer, &QTimer::timeout, this, &DownloadUpdateDialog::updateProgressDisplay);
    connect(m_speedTimer, &QTimer::timeout, this, [this]() {
        if (m_isDownloading) {
            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
            if (m_lastSpeedCheck > 0) {
                qint64 timeDiff = currentTime - m_lastSpeedCheck;
                if (timeDiff > 1000) {
                    m_downloadSpeed = m_lastDownloadedBytes * 1000 / timeDiff;
                    m_speedLabel->setText(tr("下载速度：%1").arg(formatSpeed(m_downloadSpeed)));
                    m_lastSpeedCheck = currentTime;
                    m_lastDownloadedBytes = 0;
                }
            }
        }
    });

    setModal(true);
    
    // Windows下使用自适应尺寸，避免固定尺寸导致的问题
#ifdef Q_OS_WIN
    {
        QScreen* scr = QGuiApplication::primaryScreen();
        const double scale = scr ? scr->logicalDotsPerInchX() / 96.0 : 1.0;
        const double clampScale = qBound(1.0, scale, 1.6);
        
        int scaledWidth = static_cast<int>(DIALOG_WIDTH * clampScale);
        int scaledHeight = static_cast<int>(DIALOG_HEIGHT * clampScale);
        setFixedSize(scaledWidth, scaledHeight);
        
        // 确保对话框居中显示
        if (scr) {
            QRect screenGeometry = scr->availableGeometry();
            move((screenGeometry.width() - scaledWidth) / 2,
                 (screenGeometry.height() - scaledHeight) / 2);
        }
    }
#else
    setFixedSize(DIALOG_WIDTH, DIALOG_HEIGHT);
#endif
}

DownloadUpdateDialog::~DownloadUpdateDialog()
{
    qDebug() << "[UpdateDialog] 销毁更新对话框";
}

void DownloadUpdateDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(12);
    
    // Windows 下调整边距，根据DPI缩放
#ifdef Q_OS_WIN
    {
        QScreen* scr = QGuiApplication::primaryScreen();
        const double scale = scr ? scr->logicalDotsPerInchX() / 96.0 : 1.0;
        const double clampScale = qBound(1.0, scale, 1.6);
        
        int margin = static_cast<int>(30 * clampScale);
        m_mainLayout->setContentsMargins(margin, margin, margin, margin);
    }
#else
    m_mainLayout->setContentsMargins(40, 30, 40, 30);
#endif

    // 标题
    m_titleLabel = new QLabel(this);
    if (m_updateInfo.forceUpdate) {
        m_titleLabel->setText(tr("重要更新通知"));
        m_titleLabel->setStyleSheet("font-size: 18pt; font-weight: 600; color: #1d1d1f; margin-bottom: 8px;");
    } else {
        m_titleLabel->setText(tr("发现新版本"));
        m_titleLabel->setStyleSheet("font-size: 18pt; font-weight: 600; color: #1d1d1f; margin-bottom: 8px;");
    }

    // 版本信息
    m_versionInfoLabel = new QLabel(this);
    m_versionInfoLabel->setText(tr("LoimReader %1 现已可用！")
                              .arg(m_updateInfo.latestVersion));
    m_versionInfoLabel->setStyleSheet("font-size: 15pt; font-weight: 500; color: #1d1d1f; margin-bottom: 16px;");

    // 当前版本信息
    QLabel* currentVersionLabel = new QLabel(tr("当前版本：%1").arg(LoimReader::AppVersion::getAppVersion()));
    currentVersionLabel->setStyleSheet("font-size: 13pt; color: #48484a; margin-bottom: 4px;");

    // 文件大小
    QString sizeText = m_updateInfo.updateSize;
    if (sizeText.isEmpty()) {
        sizeText = tr("计算中...");
    }
    m_sizeLabel = new QLabel(tr("文件大小：%1").arg(sizeText));
    m_sizeLabel->setStyleSheet("font-size: 13pt; color: #48484a; margin-bottom: 4px;");

    // MD5校验值
    QString md5Text = m_updateInfo.checksumMD5;
    if (md5Text.isEmpty()) {
        md5Text = tr("无校验信息");
    } else if (md5Text.length() > 16) {
        // 显示前8位和后8位，中间用...代替
        md5Text = md5Text.left(8) + "..." + md5Text.right(8);
    }
    m_md5Label = new QLabel(tr("MD5校验：%1").arg(md5Text));
    m_md5Label->setStyleSheet("font-size: 13pt; color: #48484a; margin-bottom: 16px; font-family: monospace;");

    // 更新日志
    QLabel* changelogTitleLabel = new QLabel(tr("更新内容："));
    changelogTitleLabel->setStyleSheet("font-size: 13pt; font-weight: 500; color: #1d1d1f; margin-bottom: 8px;");

    // 创建滚动区域用于更新内容
    QScrollArea* changelogScrollArea = new QScrollArea(this);
    changelogScrollArea->setWidgetResizable(true);
    changelogScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    changelogScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    changelogScrollArea->setFrameShape(QFrame::NoFrame);
    changelogScrollArea->setMinimumHeight(100);
    changelogScrollArea->setMaximumHeight(120);
    
    // Windows下优化滚动条样式，避免显示异常
#ifdef Q_OS_WIN
    changelogScrollArea->setStyleSheet(
        "QScrollArea {"
        "    background-color: #f5f5f5;"
        "    border-radius: 8px;"
        "    border: 1px solid #e5e5e7;"
        "}"
        "QScrollBar:vertical {"
        "    background-color: transparent;"
        "    width: 12px;"
        "    margin: 0px;"
        "    border: none;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background-color: #c0c0c0;"
        "    border-radius: 6px;"
        "    min-height: 20px;"
        "    margin: 2px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background-color: #a0a0a0;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    height: 0px;"
        "    subcontrol-position: top;"
        "    subcontrol-origin: margin;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "    background: none;"
        "}"
    );
#else
    changelogScrollArea->setStyleSheet(
        "QScrollArea {"
        "    background-color: #f5f5f5;"
        "    border-radius: 8px;"
        "    border: 1px solid #e5e5e7;"
        "}"
        "QScrollBar:vertical {"
        "    background-color: #f0f0f0;"
        "    width: 8px;"
        "    border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background-color: #c0c0c0;"
        "    border-radius: 4px;"
        "    min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background-color: #a0a0a0;"
        "}"
    );
#endif

    m_changelogLabel = new QLabel(this);
    m_changelogLabel->setWordWrap(true);
    m_changelogLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_changelogLabel->setStyleSheet(
        "font-size: 13pt; color: #48484a; background-color: transparent; "
        "padding: 12px;"
    );
    
    changelogScrollArea->setWidget(m_changelogLabel);
    
    // Windows下确保滚动区域正确显示
#ifdef Q_OS_WIN
    changelogScrollArea->setAttribute(Qt::WA_OpaquePaintEvent, false);
    changelogScrollArea->viewport()->setAttribute(Qt::WA_OpaquePaintEvent, false);
    changelogScrollArea->setAttribute(Qt::WA_NoSystemBackground, true);
    changelogScrollArea->viewport()->setAttribute(Qt::WA_NoSystemBackground, true);
#endif

    // 设置更新日志
    QString changelog = m_updateInfo.updateLog;
    if (changelog.isEmpty()) {
        changelog = tr("• 性能优化\n• 稳定性提升\n• 用户体验改进");
    } else {
        // 处理长文本
        if (changelog.length() > 300) {
            changelog = changelog.left(300) + "...";
        }
        // 确保每行不超过一定长度
        QStringList lines = changelog.split('\n');
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].length() > 50) {
                lines[i] = lines[i].left(47) + "...";
            }
        }
        changelog = lines.join('\n');
    }
    m_changelogLabel->setText(changelog);

    // 进度条区域（初始隐藏）
    QWidget* progressWidget = new QWidget(this);
    m_progressLayout = new QHBoxLayout(progressWidget);
    m_progressLayout->setContentsMargins(0, 16, 0, 8);
    
    // 为进度条区域添加背景和边框，使其更明显
    progressWidget->setStyleSheet(
        "QWidget {"
        "    background-color: #f8f9fa;"
        "    border-radius: 8px;"
        "    border: 1px solid #e9ecef;"
        "    padding: 12px;"
        "}"
    );

    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(PROGRESS_HEIGHT);
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "  border: none;"
        "  border-radius: 4px;"
        "  background-color: #f5f5f5;"
        "}"
        "QProgressBar::chunk {"
        "  border-radius: 4px;"
        "  background-color: #007aff;"
        "}"
    );

    QLabel* progressPercentLabel = new QLabel("0%");
    progressPercentLabel->setStyleSheet("font-size: 13pt; color: #48484a;");

    m_progressLayout->addWidget(m_progressBar, 1);
    m_progressLayout->addWidget(progressPercentLabel);

    // 状态区域
    m_statusLayout = new QHBoxLayout();
    m_statusLayout->setContentsMargins(0, 0, 0, 16);

    m_statusLabel = new QLabel(tr("准备下载..."));
    m_statusLabel->setStyleSheet("font-size: 13pt; color: #48484a;");

    m_speedLabel = new QLabel();
    m_speedLabel->setStyleSheet("font-size: 13pt; color: #8e8e93;");
    m_speedLabel->hide();

    m_statusLayout->addWidget(m_statusLabel);
    m_statusLayout->addStretch();
    m_statusLayout->addWidget(m_speedLabel);

    // 文件路径（下载完成时显示）
    m_filePathLabel = new QLabel();
    m_filePathLabel->setWordWrap(true);
    m_filePathLabel->hide();
    m_filePathLabel->setStyleSheet("font-size: 12pt; color: #8e8e93;");

    // 按钮区域 - 使用固定布局避免动态修改问题
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setSpacing(12);
    m_buttonLayout->setContentsMargins(0, 8, 0, 0);

    // 创建所有按钮
    m_updateButton = new QPushButton(tr("立即更新"));
    m_updateButton->setFixedSize(BUTTON_WIDTH, BUTTON_HEIGHT);
    m_updateButton->setObjectName("primaryButton");

    m_laterButton = new QPushButton(tr("稍后提醒"));
    m_laterButton->setFixedSize(BUTTON_WIDTH, BUTTON_HEIGHT);

    m_skipButton = new QPushButton(tr("跳过此版本"));
    m_skipButton->setFixedSize(BUTTON_WIDTH, BUTTON_HEIGHT);

    m_openFolderButton = new QPushButton(tr("打开文件夹"));
    m_openFolderButton->setFixedSize(BUTTON_WIDTH, BUTTON_HEIGHT);

    m_closeButton = new QPushButton(tr("关闭"));
    m_closeButton->setFixedSize(BUTTON_WIDTH, BUTTON_HEIGHT);

    m_retryButton = new QPushButton(tr("重试"));
    m_retryButton->setFixedSize(BUTTON_WIDTH, BUTTON_HEIGHT);

    // 居中布局：左右各一个伸缩空间，按钮组居中，默认按钮在最右
    m_buttonLayout->addStretch();                   // 左侧伸缩空间
    m_buttonLayout->addWidget(m_skipButton);        // 跳过版本按钮
    m_buttonLayout->addWidget(m_laterButton);       // 稍后提醒按钮
    m_buttonLayout->addWidget(m_updateButton);      // 重新下载按钮
    m_buttonLayout->addWidget(m_openFolderButton);  // 打开文件夹按钮
    m_buttonLayout->addWidget(m_closeButton);       // 关闭/安装更新按钮（默认按钮）
    m_buttonLayout->addStretch();                   // 右侧伸缩空间

    // 初始状态：根据是否强制更新显示不同按钮
    if (m_updateInfo.forceUpdate) {
        // 强制更新：只显示更新按钮
        m_updateButton->show();
        m_updateButton->setDefault(true);  // 设为默认按钮
        m_skipButton->hide();
        m_laterButton->hide();
        m_openFolderButton->hide();
        m_closeButton->hide();
    } else {
        // 正常更新：显示跳过、稍后、立即更新
        m_skipButton->show();
        m_laterButton->show();
        m_updateButton->show();
        m_updateButton->setDefault(true);  // 设为默认按钮
        m_openFolderButton->hide();
        m_closeButton->hide();
    }

    // 添加到主布局
    m_mainLayout->addWidget(m_titleLabel);
    m_mainLayout->addWidget(m_versionInfoLabel);
    m_mainLayout->addWidget(currentVersionLabel);
    m_mainLayout->addWidget(m_sizeLabel);
    m_mainLayout->addWidget(m_md5Label);
    m_mainLayout->addWidget(changelogTitleLabel);
    m_mainLayout->addWidget(changelogScrollArea);  // 使用滚动区域替代直接添加标签
    m_mainLayout->addWidget(progressWidget);
    m_mainLayout->addLayout(m_statusLayout);
    m_mainLayout->addWidget(m_filePathLabel);
    m_mainLayout->addLayout(m_buttonLayout);

    // 初始状态
    progressWidget->hide();
    m_filePathLabel->hide();

    // 连接信号
    connect(m_updateButton, &QPushButton::clicked, this, &DownloadUpdateDialog::onUpdateClicked);
    connect(m_laterButton, &QPushButton::clicked, this, &DownloadUpdateDialog::onLaterClicked);
    connect(m_skipButton, &QPushButton::clicked, this, &DownloadUpdateDialog::onSkipClicked);
    connect(m_openFolderButton, &QPushButton::clicked, this, &DownloadUpdateDialog::openDownloadFolder);
    connect(m_retryButton, &QPushButton::clicked, this, &DownloadUpdateDialog::onUpdateClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &DownloadUpdateDialog::onCloseClicked);
}

void DownloadUpdateDialog::setupStyles()
{
    // Windows下优化整体样式
#ifdef Q_OS_WIN
    setStyleSheet(R"(
        QDialog {
            background-color: #F5F5F5;
            border: 1px solid #d0d0d0;
        }

        QPushButton {
            background-color: #f0f0f0;
            color: #333;
            border: 1px solid #ccc;
            border-radius: 4px;
            padding: 8px 16px;
            font-size: 11pt;
            text-align: center;
            min-width: 80px;
        }

        QPushButton:hover {
            background-color: #e0e0e0;
            border-color: #999;
        }

        QPushButton:pressed {
            background-color: #d0d0d0;
        }

        QPushButton:disabled {
            background-color: #F7F9FA;
            color: #BDC3C7;
            border-color: #E5E8E8;
        }

        #primaryButton {
            background-color: #007aff;
            color: white;
            border: 1px solid #007aff;
            font-weight: 600;
        }

        #primaryButton:hover {
            background-color: #0056cc;
            border-color: #0056cc;
        }

        #primaryButton:pressed {
            background-color: #004999;
            border-color: #004999;
        }
    )");
#else
    // 使用与登录对话框一致的统一按钮样式
    setStyleSheet(R"(
        QDialog {
            background-color: #F5F5F5;
        }

        QPushButton {
            background-color: #f0f0f0;
            color: #333;
            border: 1px solid #ccc;
            border-radius: 4px;
            padding: 6px 12px;
            gap: 11pt;
            text-align: center;
        }

        QPushButton:hover {
            background-color: #e0e0e0;
            border-color: #999;
        }

        QPushButton:pressed {
            background-color: #d0d0d0;
        }

        QPushButton:disabled {
            background-color: #F7F9FA;
            color: #BDC3C7;
            border-color: #E5E8E8;
        }

        #primaryButton {
            background-color: #007aff;
            color: white;
            border: 1px solid #007aff;
        }

        #primaryButton:hover {
            background-color: #0056cc;
            border-color: #0056cc;
        }

        #primaryButton:pressed {
            background-color: #004999;
            border-color: #004999;
        }
    )");
#endif
}

void DownloadUpdateDialog::onUpdateClicked()
{
    qDebug() << "[UpdateDialog] ===== onUpdateClicked() 开始 =====";
    qDebug() << "[UpdateDialog] 下载状态 - isDownloading:" << m_isDownloading << "downloadComplete:" << m_downloadComplete << "hashVerified:" << m_hashVerified;

    if (m_isDownloading) {
        // 暂停下载
        qDebug() << "[UpdateDialog] 暂停下载";
        emit requestPauseDownload();
    } else if (m_downloadComplete && !m_hashVerified) {
        // 下载完成但校验失败 - 重新下载
        qDebug() << "[UpdateDialog] 校验失败，重新下载";
        // 重置状态
        m_downloadComplete = false;
        m_hashVerified = false;
        m_downloadFilePath.clear();
        // 开始重新下载
        m_result = Update;
        updateDownloadMode();
        qDebug() << "[UpdateDialog] 发射 requestStartDownload 信号";
        emit requestStartDownload();
    } else if (m_downloadComplete && m_hashVerified) {
        // 下载完成且校验通过 - 重新安装
        qDebug() << "[UpdateDialog] 重新安装，文件路径:" << m_downloadFilePath;
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadFilePath));
    } else {
        // 开始下载
        qDebug() << "[UpdateDialog] 开始下载更新";
        qDebug() << "[UpdateDialog] 更新信息 - 版本:" << m_updateInfo.latestVersion;
        qDebug() << "[UpdateDialog] 更新信息 - URL:" << m_updateInfo.updateUrl;

        m_result = Update;
        updateDownloadMode();

        qDebug() << "[UpdateDialog] 发射 requestStartDownload 信号";
        emit requestStartDownload();
        qDebug() << "[UpdateDialog] ===== onUpdateClicked() 完成 =====";
    }
}

void DownloadUpdateDialog::onLaterClicked()
{
    qDebug() << "[UpdateDialog] 用户选择稍后提醒";
    m_result = Later;
    reject();
}

void DownloadUpdateDialog::onSkipClicked()
{
    qDebug() << "[UpdateDialog] 用户选择跳过此版本:" << m_updateInfo.latestVersion;
    m_result = Skip;
    // 记录跳过的版本到配置文件
    QSettings settings("user.ini", QSettings::IniFormat);
    settings.setValue("update/skippedVersion", m_updateInfo.latestVersion);
    reject();
}

void DownloadUpdateDialog::onCloseClicked()
{
    // 根据按钮文本判断是关闭还是安装更新
    if (m_closeButton->text() == tr("安装更新")) {
        // 执行安装更新操作
        qDebug() << "[UpdateDialog] 用户点击安装更新";
        if (!m_downloadFilePath.isEmpty()) {
            // 打开下载的文件进行安装
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadFilePath));
        }
        m_result = Update;
        accept();  // 成功完成
    } else {
        // 普通关闭
        qDebug() << "[UpdateDialog] 用户关闭对话框";
        reject();
    }
}

void DownloadUpdateDialog::onDownloadProgress(int progress, qint64 received, qint64 total)
{
    if (!m_isDownloading) {
        m_isDownloading = true;
        updateDownloadMode();
        m_progressTimer->start(100); // 每100ms更新一次显示
        m_speedTimer->start(1000); // 每秒更新一次速度
        m_lastSpeedCheck = QDateTime::currentMSecsSinceEpoch();
        m_lastDownloadedBytes = 0;

        // 设置文件大小（第一次获取到total时）
        if (total > 0) {
            setDownloadFileSize(total);
        }
    }

    m_lastDownloadedBytes = received;

    // 更新进度条
    m_progressBar->setValue(progress);

    // 更新百分比标签
    QWidget* percentWidget = m_progressLayout->itemAt(1)->widget();
    if (QLabel* percentLabel = qobject_cast<QLabel*>(percentWidget)) {
        percentLabel->setText(QString("%1%").arg(progress));
    }

    // 更新状态
    m_statusLabel->setText(tr("正在下载... %1 / %2")
                          .arg(formatFileSize(received))
                          .arg(formatFileSize(total)));
}

void DownloadUpdateDialog::onDownloadFinished(const QString& filePath)
{
    qDebug() << "[UpdateDialog] 下载完成，文件路径:" << filePath;
    m_downloadComplete = true;
    m_isDownloading = false;
    m_downloadFilePath = filePath;

    // 执行哈希校验
    qDebug() << "[UpdateDialog] 开始哈希校验";
    m_hashVerified = verifyFileHash(filePath, m_updateInfo.checksumMD5);

    showCompleteMode();

    // 停止定时器
    m_progressTimer->stop();
    m_speedTimer->stop();
}

void DownloadUpdateDialog::onDownloadError(const QString& error)
{
    qDebug() << "[UpdateDialog] 下载错误:" << error;
    m_isDownloading = false;
    m_lastError = error;
    showErrorMode(error);

    // 停止定时器
    m_progressTimer->stop();
    m_speedTimer->stop();
}

void DownloadUpdateDialog::updateProgressDisplay()
{
    if (m_isDownloading) {
        // 更新速度显示
        if (m_lastSpeedCheck > 0) {
            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
            qint64 timeDiff = currentTime - m_lastSpeedCheck;
            if (timeDiff > 100) {
                qint64 speed = m_lastDownloadedBytes * 1000 / timeDiff;
                m_speedLabel->setText(tr("下载速度：%1").arg(formatSpeed(speed)));
                m_lastSpeedCheck = currentTime;
                m_lastDownloadedBytes = 0;
            }
        }
    }
}

void DownloadUpdateDialog::updateDownloadMode()
{
    // 显示进度区域
    QWidget* progressWidget = m_progressLayout->parentWidget();
    if (progressWidget) {
        progressWidget->show();
    }
    m_speedLabel->show();

    // 更新按钮文本和显示状态
    m_updateButton->setText(tr("暂停"));
    m_updateButton->show();

    // 设置下载中的按钮显示状态
    if (m_updateInfo.forceUpdate) {
        // 强制更新：只显示暂停按钮
        m_skipButton->hide();
        m_laterButton->hide();
        m_openFolderButton->hide();
        m_closeButton->hide();
    } else {
        // 正常更新：显示暂停和关闭按钮
        m_skipButton->hide();
        m_laterButton->hide();
        m_openFolderButton->hide();
        m_closeButton->show();
    }
}

void DownloadUpdateDialog::showProgressMode()
{
    QWidget* progressWidget = m_progressLayout->parentWidget();
    if (progressWidget) {
        progressWidget->show();
    }
    m_statusLabel->setText(tr("正在下载..."));
}

void DownloadUpdateDialog::showCompleteMode()
{
    // 隐藏进度相关
    m_speedLabel->hide();

    // 根据哈希校验结果设置状态
    if (m_hashVerified) {
        m_statusLabel->setText(tr("下载完成，校验通过！"));
        // 更新MD5标签显示校验通过
        m_md5Label->setText(tr("MD5校验：通过 ✓"));
        m_md5Label->setStyleSheet("font-size: 13pt; color: #28a745; margin-bottom: 16px; font-family: monospace;");
    } else {
        m_statusLabel->setText(tr("下载完成，但校验失败！"));
        // 更新MD5标签显示校验失败
        m_md5Label->setText(tr("MD5校验：失败 ✗"));
        m_md5Label->setStyleSheet("font-size: 13pt; color: #dc3545; margin-bottom: 16px; font-family: monospace;");
    }

    // 显示文件路径
    m_filePathLabel->setText(tr("文件位置：%1").arg(m_downloadFilePath));
    m_filePathLabel->show();

    // 根据哈希校验结果设置按钮显示状态
    if (m_hashVerified) {
        // 校验通过：显示[打开文件夹] [安装更新]
        m_updateButton->hide();
        m_skipButton->hide();
        m_laterButton->hide();
        m_openFolderButton->show();
        m_closeButton->show();
        m_closeButton->setText(tr("安装更新"));
        m_closeButton->setObjectName("primaryButton");
        m_closeButton->setDefault(true);
    } else {
        // 校验失败：显示[重新下载] [打开文件夹] [关闭]
        m_updateButton->show();
        m_updateButton->setText(tr("重新下载"));
        m_skipButton->hide();
        m_laterButton->hide();
        m_openFolderButton->show();
        m_closeButton->show();
        m_closeButton->setText(tr("关闭"));
        m_closeButton->setObjectName("");
    }
    
    // 刷新样式
    m_closeButton->style()->unpolish(m_closeButton);
    m_closeButton->style()->polish(m_closeButton);
}

void DownloadUpdateDialog::showErrorMode(const QString& error)
{
    m_statusLabel->setText(tr("下载失败：%1").arg(error));

    // 设置错误状态的按钮显示：[重试] [关闭]
    m_updateButton->setText(tr("重试"));
    m_updateButton->show();
    m_skipButton->hide();
    m_laterButton->hide();
    m_openFolderButton->hide();
    m_closeButton->show();
}

void DownloadUpdateDialog::openDownloadFolder()
{
    if (!m_downloadFilePath.isEmpty()) {
        QFileInfo fileInfo(m_downloadFilePath);
        QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
    }
}

QString DownloadUpdateDialog::formatFileSize(qint64 bytes) const
{
    if (bytes < 1024) {
        return QString("%1 B").arg(bytes);
    } else if (bytes < 1024 * 1024) {
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    } else if (bytes < 1024 * 1024 * 1024) {
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    } else {
        return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
    }
}

QString DownloadUpdateDialog::formatSpeed(qint64 bytesPerSecond) const
{
    return formatFileSize(bytesPerSecond) + "/s";
}

void DownloadUpdateDialog::setDownloadFileSize(qint64 bytes)
{
    m_sizeLabel->setText(tr("文件大小：%1").arg(formatFileSize(bytes)));
}

bool DownloadUpdateDialog::verifyFileHash(const QString& filePath, const QString& expectedHash) const
{
    if (expectedHash.isEmpty()) {
        qDebug() << "[UpdateDialog] 没有期望的哈希值，跳过校验";
        return true; // 没有哈希值时跳过校验
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "[UpdateDialog] 无法打开文件进行哈希校验:" << filePath;
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    while (!file.atEnd()) {
        QByteArray data = file.read(8192);
        hash.addData(data);
    }

    QString actualHash = hash.result().toHex();
    qDebug() << "[UpdateDialog] 期望哈希:" << expectedHash;
    qDebug() << "[UpdateDialog] 实际哈希:" << actualHash;

    bool isValid = (actualHash.toLower() == expectedHash.toLower());
    qDebug() << "[UpdateDialog] 哈希校验结果:" << (isValid ? "通过" : "失败");

    return isValid;
}

