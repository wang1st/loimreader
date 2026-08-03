#include "simpleupdater.h"
#include "app_version.h"
#include <QMessageBox>
#include <QCoreApplication>
#include <QJsonParseError>
#include <QSettings>
#include <QVersionNumber>
#include <QPixmap>
#include <QPainter>
#include <QDebug>
#include <QDesktopServices>

void SimpleUpdater::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    // 可以在这里添加下载进度显示逻辑
    Q_UNUSED(bytesReceived)
    Q_UNUSED(bytesTotal)
    // qDebug() << "Download progress:" << bytesReceived << "/" << bytesTotal;
}

SimpleUpdater::SimpleUpdater(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_reply(nullptr)
    , m_silentMode(false)
{
}

SimpleUpdater::~SimpleUpdater()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
    }
}

void SimpleUpdater::setUpdateUrl(const QString &url)
{
    m_updateUrl = url;
}

void SimpleUpdater::checkForUpdatesSilent()
{
    m_silentMode = true;
    checkForUpdatesNotSilent();
}

void SimpleUpdater::checkForUpdatesNotSilent()
{
    if (m_updateUrl.isEmpty()) {
        qWarning() << "Update URL is not set";
        return;
    }

    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
    }

    QNetworkRequest request(m_updateUrl);
    QString userAgent = QString("LoimReader/%1").arg(LoimReader::AppVersion::getAppVersion());
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent);
    request.setRawHeader("Accept", "application/json");

    m_reply = m_networkManager->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &SimpleUpdater::onUpdateCheckFinished);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &SimpleUpdater::onDownloadProgress);
}

void SimpleUpdater::onUpdateCheckFinished()
{
    if (!m_reply) return;

    QNetworkReply::NetworkError error = m_reply->error();
    QByteArray responseData = m_reply->readAll();
    
    m_reply->deleteLater();
    m_reply = nullptr;

    if (error != QNetworkReply::NoError) {
        if (!m_silentMode) {
            showErrorMessage("网络连接失败");
        }
        return;
    }

    if (!parseUpdateJson(responseData)) {
        if (!m_silentMode) {
            showErrorMessage("解析更新信息失败");
        }
        return;
    }

    // 检查版本
    QString currentVersion = LoimReader::AppVersion::getAppVersion();
    if (isNewerVersion(m_latestUpdate.version, currentVersion)) {
        QSettings settings;
        QString skippedVersion = settings.value("updater/skippedVersion").toString();
        if (skippedVersion == m_latestUpdate.version) {
            if (!m_silentMode) showNoUpdateMessage();
            return;
        }
        showUpdateDialog(m_latestUpdate);
    } else {
        if (!m_silentMode) showNoUpdateMessage();
    }
}

bool SimpleUpdater::parseUpdateJson(const QByteArray &jsonData)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        return false;
    }

    QJsonObject root = doc.object();
    QJsonArray releases = root.value("releases").toArray();
    
    if (releases.isEmpty()) return false;

    QString currentPlatform = getCurrentPlatform();
    
    for (const QJsonValue &releaseValue : releases) {
        QJsonObject release = releaseValue.toObject();
        QJsonArray platforms = release.value("platforms").toArray();
        
        for (const QJsonValue &platformValue : platforms) {
            QJsonObject platform = platformValue.toObject();
            if (platform.value("name").toString() == currentPlatform) {
                m_latestUpdate.version = release.value("version").toString();
                m_latestUpdate.title = release.value("title").toString();
                m_latestUpdate.description = release.value("description").toString();
                m_latestUpdate.downloadUrl = platform.value("downloadUrl").toString();
                m_latestUpdate.releaseDate = release.value("releaseDate").toString();
                m_latestUpdate.isForced = release.value("isForced").toBool();
                m_latestUpdate.platform = currentPlatform;
                m_latestUpdate.fileSize = platform.value("fileSize").toVariant().toLongLong();
                return true;
            }
        }
    }
    return false;
}

QString SimpleUpdater::getCurrentPlatform()
{
#ifdef Q_OS_MACOS
    return "macos";
#elif defined(Q_OS_WIN)
    return "windows";
#elif defined(Q_OS_LINUX)
    return "linux";
#else
    return "unknown";
#endif
}

bool SimpleUpdater::isNewerVersion(const QString &remoteVersion, const QString &currentVersion)
{
    QVersionNumber remote = QVersionNumber::fromString(remoteVersion);
    QVersionNumber current = QVersionNumber::fromString(currentVersion);
    return QVersionNumber::compare(remote, current) > 0;
}

void SimpleUpdater::showUpdateDialog(const UpdateInfo &updateInfo)
{
    UpdateDialog *dialog = new UpdateDialog(updateInfo);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void SimpleUpdater::showErrorMessage(const QString &message)
{
    QMessageBox::critical(nullptr, "更新检查错误", message);
}

void SimpleUpdater::showNoUpdateMessage()
{
    QMessageBox::information(nullptr, "检查更新", "您已使用最新版本");
}

// UpdateDialog 实现
UpdateDialog::UpdateDialog(const SimpleUpdater::UpdateInfo &updateInfo, QWidget *parent)
    : QDialog(parent), m_updateInfo(updateInfo)
{
    setupUI();
    applyUnifiedStyle();
    setWindowTitle("LoimReader 软件更新");
    setFixedSize(500, 400);
}

void UpdateDialog::setupUI()
{
    m_titleLabel = new QLabel(QString("发现新版本 %1").arg(m_updateInfo.version));
    m_titleLabel->setAlignment(Qt::AlignCenter);
    
    m_logoLabel = new QLabel;
    m_logoLabel->setAlignment(Qt::AlignCenter);
    m_logoLabel->setFixedSize(64, 64);
    
    QPixmap logoPixmap(64, 64);
    logoPixmap.fill(Qt::transparent);
    QPainter painter(&logoPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QBrush(QColor(74, 144, 226)));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(8, 8, 48, 48);
    painter.setPen(QPen(Qt::white, 3));
    painter.drawLine(32, 20, 32, 44);
    painter.drawLine(20, 32, 32, 20);
    painter.drawLine(44, 32, 32, 20);
    m_logoLabel->setPixmap(logoPixmap);
    
    m_contentBrowser = new QTextBrowser;
    m_contentBrowser->setHtml(createUpdateText());
    m_contentBrowser->setMaximumHeight(200);
    
    m_downloadBtn = new QPushButton("立即下载");
    m_remindLaterBtn = new QPushButton("稍后提醒");
    m_skipVersionBtn = new QPushButton("跳过此版本");
    
    m_downloadBtn->setFixedHeight(32);
    m_remindLaterBtn->setFixedHeight(32);
    m_skipVersionBtn->setFixedHeight(32);
    
    connect(m_downloadBtn, &QPushButton::clicked, this, &UpdateDialog::onDownloadClicked);
    connect(m_remindLaterBtn, &QPushButton::clicked, this, &UpdateDialog::onRemindLaterClicked);
    connect(m_skipVersionBtn, &QPushButton::clicked, this, &UpdateDialog::onSkipVersionClicked);
    
    auto buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(m_skipVersionBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_remindLaterBtn);
    buttonLayout->addWidget(m_downloadBtn);
    
    auto buttonWidget = new QWidget;
    buttonWidget->setLayout(buttonLayout);
    
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(40, 20, 40, 20);
    mainLayout->setSpacing(15);
    mainLayout->addWidget(m_titleLabel);
    mainLayout->addWidget(m_logoLabel);
    mainLayout->addWidget(m_contentBrowser, 1);
    mainLayout->addWidget(buttonWidget);
}

void UpdateDialog::applyUnifiedStyle()
{
    QFont f = font();
    f.setPointSize(14);
    setFont(f);
    
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(236, 236, 236));
    setPalette(pal);
    
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    
    QString buttonStyle = 
        "QPushButton { background-color: #4A90E2; color: white; border: none; border-radius: 4px; padding: 6px 20px; }"
        "QPushButton:hover { background-color: #357ABD; }"
        "QPushButton:pressed { background-color: #2968A3; }";
    m_downloadBtn->setStyleSheet(buttonStyle);
    
    QString secondaryStyle = 
        "QPushButton { background-color: #E0E0E0; color: #333; border: none; border-radius: 4px; padding: 6px 16px; }"
        "QPushButton:hover { background-color: #D0D0D0; }";
    m_remindLaterBtn->setStyleSheet(secondaryStyle);
    m_skipVersionBtn->setStyleSheet(secondaryStyle);
}

QString UpdateDialog::createUpdateText() const
{
    QString text = QString("<h3>%1</h3>").arg(m_updateInfo.title);
    if (!m_updateInfo.releaseDate.isEmpty()) {
        text += QString("<p><b>发布日期：</b>%1</p>").arg(m_updateInfo.releaseDate);
    }
    text += QString("<p>%1</p>").arg(m_updateInfo.description);
    return text;
}

void UpdateDialog::onDownloadClicked()
{
    QDesktopServices::openUrl(QUrl(m_updateInfo.downloadUrl));
    accept();
}

void UpdateDialog::onRemindLaterClicked()
{
    reject();
}

void UpdateDialog::onSkipVersionClicked()
{
    QSettings settings;
    settings.setValue("updater/skippedVersion", m_updateInfo.version);
    reject();
}