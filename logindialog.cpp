#include "logindialog.h"
#include "machineid.h"
#include "util.h"
#include "protection.h"
#include "app_version.h"
#include <QtWidgets>
#include <QDialog>
#include <QApplication>
#include <QCoreApplication>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTimer>
#include <QSettings>

bool LoginDialog::s_isTrialUser = true;

static QString iniPath() {
    QString appPath = QCoreApplication::applicationFilePath();
    int pos = appPath.lastIndexOf('/');
    QString appDir = appPath.left(pos);
    return appDir + "/user.ini";
}

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent), m_reply(nullptr)
{
    setWindowTitle(tr("登录 - LoimReader"));
    setModal(true);
    setFixedSize(480, 480);
#ifdef Q_OS_WIN
    // Windows 下按 DPI 自适应基础字号
    {
        QScreen* scr = QGuiApplication::primaryScreen();
        const double scale = scr ? scr->logicalDotsPerInchX() / 96.0 : 1.0;
        const double clampScale = qBound(1.0, scale, 1.6);
        QFont base = font();
        base.setPointSizeF(qBound(10.0, 11.0 * clampScale, 13.0));
        setFont(base);
    }
#else
    {
        QFont f = font();
        f.setPointSize(14);
        setFont(f);
    }
#endif
    
    // 设置对话框背景为系统默认色，与工具栏保持一致
    setAutoFillBackground(true);
    QPalette pal = palette();
    // 在macOS上使用窗口背景色
    pal.setColor(QPalette::Window, QColor(236, 236, 236)); // macOS默认窗口背景色
    setPalette(pal);
    
    // 创建界面元素 - 使用原始控件
    m_editEmail = new QLineEdit(this);
    m_editEmail->setPlaceholderText(tr("请输入邮箱地址"));
#ifdef Q_OS_WIN
    {
        QScreen* scr = QGuiApplication::primaryScreen();
        const double scale = scr ? scr->logicalDotsPerInchX() / 96.0 : 1.0;
        const double clampScale = qBound(1.0, scale, 1.6);
        m_editEmail->setFixedHeight(qBound(36, int(qRound(40 * clampScale)), 48));
    }
#else
    m_editEmail->setFixedHeight(44);
#endif
    
    m_editPassword = new QLineEdit(this);
    m_editPassword->setEchoMode(QLineEdit::Password);
    m_editPassword->setPlaceholderText(tr("请输入密码"));
#ifdef Q_OS_WIN
    {
        QScreen* scr = QGuiApplication::primaryScreen();
        const double scale = scr ? scr->logicalDotsPerInchX() / 96.0 : 1.0;
        const double clampScale = qBound(1.0, scale, 1.6);
        m_editPassword->setFixedHeight(qBound(36, int(qRound(40 * clampScale)), 48));
    }
#else
    m_editPassword->setFixedHeight(44);
#endif
    
    m_editDeviceName = new QLineEdit(this);
    m_editApiBase = new QLineEdit(this);
    m_editApiBase->setPlaceholderText("https://ctdy123.com/api/auth/client/login");
    m_editApiBase->setText("https://ctdy123.com/api/auth/client/login");

    m_chkRememberEmail = new QCheckBox(tr("记住邮箱"), this);
    m_chkRememberPassword = new QCheckBox(tr("记住密码"), this);

    m_btnLogin = new QPushButton(tr("登录"), this);
#ifdef Q_OS_WIN
    {
        QScreen* scr = QGuiApplication::primaryScreen();
        const double scale = scr ? scr->logicalDotsPerInchX() / 96.0 : 1.0;
        const double clampScale = qBound(1.0, scale, 1.6);
        m_btnLogin->setFixedHeight(qBound(32, int(qRound(34 * clampScale)), 40));
    }
#else
    m_btnLogin->setFixedHeight(36);
#endif
    m_btnLogin->setFixedWidth(80);
    m_btnLogin->setDefault(true);
    // 设置按钮样式，移除默认边框
    m_btnLogin->setStyleSheet(
        "QPushButton { "
        "    background-color: #4A90E2; "
        "    color: white; "
        "    border: none; "
        "    border-radius: 4px; "
        "    padding: 6px 20px; "
        "}"
        "QPushButton:hover { "
        "    background-color: #357ABD; "
        "}"
        "QPushButton:pressed { "
        "    background-color: #2968A3; "
        "}"
    );
    
    m_btnCancel = new QPushButton(tr("退出"), this);
#ifdef Q_OS_WIN
    {
        QScreen* scr = QGuiApplication::primaryScreen();
        const double scale = scr ? scr->logicalDotsPerInchX() / 96.0 : 1.0;
        const double clampScale = qBound(1.0, scale, 1.6);
        m_btnCancel->setFixedHeight(qBound(32, int(qRound(34 * clampScale)), 40));
    }
#else
    m_btnCancel->setFixedHeight(36);
#endif
    m_btnCancel->setFixedWidth(80);
    // 设置次要按钮样式
    m_btnCancel->setStyleSheet(
        "QPushButton { "
        "    background-color: #E0E0E0; "
        "    color: #333; "
        "    border: none; "
        "    border-radius: 4px; "
        "    padding: 6px 16px; "
        "}"
        "QPushButton:hover { "
        "    background-color: #D0D0D0; "
        "}"
    );
    
    m_status = new QLabel(this);
    m_status->setVisible(false);
    m_status->setWordWrap(true);
    m_status->setAlignment(Qt::AlignCenter);
    m_status->setFixedHeight(60); // 增加高度，提供更多显示空间
    
    // 创建标题
    auto titleLabel = new QLabel(tr("欢迎登录"), this);
    QFont titleFont = titleLabel->font();
#ifdef Q_OS_WIN
    {
        QScreen* scr = QGuiApplication::primaryScreen();
        const double scale = scr ? scr->logicalDotsPerInchX() / 96.0 : 1.0;
        const double clampScale = qBound(1.0, scale, 1.6);
        titleFont.setPointSizeF(qBound(13.0, 14.0 * clampScale, 16.0));
    }
#else
    titleFont.setPointSize(20);
#endif
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setMinimumHeight(40); // 确保标题有足够高度
    
    auto subtitleLabel = new QLabel(tr("请输入您的账户信息"), this);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setMinimumHeight(25); // 确保副标题有足够高度
    
    m_tip = new QLabel(this);
    m_tip->setTextFormat(Qt::RichText);
    m_tip->setOpenExternalLinks(true);
    m_tip->setText(tr("还没有账号？<a href=\"https://ctdy123.com\">前往官网注册</a>"));
    m_tip->setAlignment(Qt::AlignCenter);

    // 创建标签
    auto emailLabel = new QLabel(tr("邮箱:"), this);
    emailLabel->setFixedWidth(60); // 固定标签宽度
    auto passwordLabel = new QLabel(tr("密码:"), this);
    passwordLabel->setFixedWidth(60); // 固定标签宽度
    
    // 创建邮箱输入行 - 标签和输入框在同一行
    auto emailWidget = new QWidget(this);
    auto emailLayout = new QHBoxLayout(emailWidget);
    emailLayout->setContentsMargins(0, 0, 0, 0);
    emailLayout->setSpacing(10);
    emailLayout->addWidget(emailLabel);
    emailLayout->addWidget(m_editEmail);
    
    // 创建密码输入行 - 标签和输入框在同一行
    auto passwordWidget = new QWidget(this);
    auto passwordLayout = new QHBoxLayout(passwordWidget);
    passwordLayout->setContentsMargins(0, 0, 0, 0);
    passwordLayout->setSpacing(10);
    passwordLayout->addWidget(passwordLabel);
    passwordLayout->addWidget(m_editPassword);
    
    // 设备名、服务器URL默认不展示，但仍参与请求
    m_editDeviceName->setVisible(false);
    m_editApiBase->setVisible(false);

    // 记住选项布局
    auto checkboxWidget = new QWidget(this);
    auto checkboxLayout = new QHBoxLayout(checkboxWidget);
    checkboxLayout->setContentsMargins(0, 0, 0, 0);
    checkboxLayout->addWidget(m_chkRememberEmail);
    checkboxLayout->addWidget(m_chkRememberPassword);
    checkboxLayout->addStretch();

    // 按钮布局 - 以中心垂直线为基准，左右对称分布
    auto buttonWidget = new QWidget(this);
    auto buttonLayout = new QHBoxLayout(buttonWidget);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(0); // 移除按钮间距，使用中心布局
    
    // 左侧按钮区域
    auto leftButtonArea = new QWidget();
    auto leftButtonLayout = new QHBoxLayout(leftButtonArea);
    leftButtonLayout->setContentsMargins(0, 0, 0, 0);
    leftButtonLayout->addStretch(); // 右对齐
    leftButtonLayout->addWidget(m_btnCancel);
    
    // 中心分隔区域（模拟中心垂直线）
    auto centerSpacer = new QWidget();
    centerSpacer->setFixedWidth(15); // 中心间距
    
    // 右侧按钮区域
    auto rightButtonArea = new QWidget();
    auto rightButtonLayout = new QHBoxLayout(rightButtonArea);
    rightButtonLayout->setContentsMargins(0, 0, 0, 0);
    rightButtonLayout->addWidget(m_btnLogin);
    rightButtonLayout->addStretch(); // 左对齐
    
    buttonLayout->addWidget(leftButtonArea);
    buttonLayout->addWidget(centerSpacer);
    buttonLayout->addWidget(rightButtonArea);

    // 主布局
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(40, 20, 40, 20); // 减少上下边距
    mainLayout->setSpacing(15); // 减少间距
    
    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(subtitleLabel);
    mainLayout->addSpacing(15);
    
    // 添加输入区域 - 标签和输入框同一行
    mainLayout->addWidget(emailWidget);
    mainLayout->addSpacing(4);
    mainLayout->addWidget(passwordWidget);
    mainLayout->addSpacing(6);
    
    mainLayout->addWidget(checkboxWidget);
    mainLayout->addWidget(m_status);
    mainLayout->addSpacing(10); // 在状态消息和注册链接之间添加间距
    mainLayout->addWidget(m_tip);
    mainLayout->addStretch(1);
    mainLayout->addWidget(buttonWidget);

    connect(m_btnLogin, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(m_btnCancel, &QPushButton::clicked, this, &LoginDialog::reject);

    // 设置全局样式，包括状态标签的样式
    setStyleSheet(R"(
        QDialog {
            background-color: #F5F5F5;
        }

        QLineEdit {
            padding: 10px 16px;
            border: 1px solid #D5DBDB;
            border-radius: 6px;
            font-size: 12pt;
            background-color: white;
            color: #2C3E50;
            min-height: 20px;
        }

        QLineEdit:focus {
            border-color: #85929E;
            outline: none;
            background-color: #FAFAFA;
        }

        QLineEdit::placeholder {
            color: #AAB7B8;
        }

        QCheckBox {
            font-size: 12pt;
            color: #5D6D7E;
            spacing: 8px;
        }

        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 1px solid #BDC3C7;
            border-radius: 3px;
            background-color: white;
        }

        QCheckBox::indicator:checked {
            background-color: #85929E;
            border-color: #85929E;
        }

        QCheckBox::indicator:hover {
            border-color: #85929E;
        }

        #statusLabel {
            font-size: 12pt;
            padding: 10px;
            border-radius: 4px;
            margin: 8px 0;
        }

        #statusLabel[type="error"] {
            background-color: #FADBD8;
            color: #C0392B;
            border: 1px solid #F1948A;
        }

        #statusLabel[type="success"] {
            background-color: #E8F8F5;
            color: #148F77;
            border: 1px solid #7DCEA0;
        }

        #statusLabel[type="loading"] {
            background-color: #EBF5FB;
            color: #2E86AB;
            border: 1px solid #AED6F1;
        }

        #statusLabel[type="info"] {
            background-color: #EBF5FB;
            color: #2E86AB;
            border: 1px solid #AED6F1;
        }
    )");

    // 为状态标签设置对象名称，以便CSS选择器能够匹配
    m_status->setObjectName("statusLabel");

    loadFromIni();
}

void LoginDialog::loadFromIni()
{
    QSettings settings(iniPath(), QSettings::IniFormat);
    
    // 加载邮箱
    QString email = settings.value("email", "").toString();
    if (!email.isEmpty()) {
        m_editEmail->setText(email);
    }
    
    // 加载密码（如果记住密码被选中）
    bool rememberPassword = settings.value("rememberPassword", false).toBool();
    m_chkRememberPassword->setChecked(rememberPassword);
    if (rememberPassword) {
        QString password = settings.value("password", "").toString();
        if (!password.isEmpty()) {
            m_editPassword->setText(password);
        }
    }
    
    // 加载记住邮箱设置
    bool rememberEmail = settings.value("rememberEmail", false).toBool();
    m_chkRememberEmail->setChecked(rememberEmail);
    
    // 加载设备名称
    QString deviceName = settings.value("deviceName", "").toString();
    if (!deviceName.isEmpty()) {
        m_editDeviceName->setText(deviceName);
    }
    
    // 加载API基础地址
    QString apiBase = settings.value("apiBase", "https://ctdy123.com/api/auth/client/login").toString();
    m_editApiBase->setText(apiBase);
}

void LoginDialog::saveToIni()
{
    QSettings settings(iniPath(), QSettings::IniFormat);
    
    // 保存邮箱
    if (m_chkRememberEmail->isChecked()) {
        settings.setValue("email", m_editEmail->text());
        settings.setValue("rememberEmail", true);
    } else {
        settings.remove("email");
        settings.setValue("rememberEmail", false);
    }
    
    // 保存密码
    if (m_chkRememberPassword->isChecked()) {
        settings.setValue("password", m_editPassword->text());
        settings.setValue("rememberPassword", true);
    } else {
        settings.remove("password");
        settings.setValue("rememberPassword", false);
    }
    
    // 保存设备名称
    settings.setValue("deviceName", m_editDeviceName->text());
    
    // 保存API基础地址
    settings.setValue("apiBase", m_editApiBase->text());
    
    // 保存认证信息
    if (!m_token.isEmpty()) {
        settings.setValue("token", m_token);
        settings.setValue("email", m_editEmail->text());
        settings.setValue("apiBase", m_editApiBase->text());
    }
}

// 显示状态信息的辅助函数
void LoginDialog::showStatus(const QString& message, const QString& type)
{
    if (message.isEmpty()) {
        m_status->setVisible(false);
        return;
    }

    m_status->setText(message);
    m_status->setProperty("type", type);
    m_status->style()->polish(m_status);
    m_status->setVisible(true);

    // 5秒后自动隐藏成功消息
    if (type == "success") {
        QTimer::singleShot(5000, this, [this]() {
            m_status->setVisible(false);
        });
    }
}

QString LoginDialog::token() const { return m_token; }
QString LoginDialog::apiBase() const { return m_editApiBase->text(); }
QString LoginDialog::email() const { return m_editEmail->text(); }
QString LoginDialog::deviceName() const { return m_editDeviceName->text(); }



QByteArray LoginDialog::buildLoginPayload() const
{
    QString machineCode = machineIDHashKey();
    QString appVersion = LoimReader::AppVersion::getAppVersion();
    QString userAgent = QString("LoimReader/%1 (%2)").arg(appVersion, QSysInfo::prettyProductName());
    QString platform;
#ifdef Q_OS_WIN
    platform = "windows";
#elif defined(Q_OS_MAC)
    platform = "macos";
#elif defined(Q_OS_LINUX)
    platform = "linux";
#else
    platform = "unknown";
#endif
    
    // 构建设备信息对象（符合新API规范）
    QJsonObject deviceInfo{
        {"name", "LoimReader"},  // 固定设备名称
        {"type", "client"},      // 固定设备类型
        {"userAgent", userAgent},
        {"os", QSysInfo::prettyProductName()},
        {"appVersion", appVersion}
    };
    
    // 构建登录载荷（遵循新API规范）
    QJsonObject payload{
        {"email", m_editEmail->text()},
        {"password", m_editPassword->text()},
        {"machineCode", machineCode},
        {"deviceInfo", deviceInfo},
        {"clientName", "LoimReader"},     // 必填字段：固定值
        {"currentVersion", appVersion},   // 必填字段：客户端当前版本号
        {"platform", platform}            // 必填字段：客户端平台
    };
    
    return QJsonDocument(payload).toJson();
}

void LoginDialog::onLoginClicked()
{
    qDebug() << "[LoginDialog] ===== 开始登录流程 =====";
    qDebug() << "[LoginDialog] 用户邮箱:" << m_editEmail->text();
    qDebug() << "[LoginDialog] API地址:" << m_editApiBase->text();
    
    // 无论登录是否成功，都保存用户设置（如果用户选择了记住）
    saveToIni();
    
    // 反调试检查
    if (!ANTI_DEBUG_CHECK()) {
        qDebug() << "[LoginDialog] 反调试检查失败";
        showStatus(tr("检测到调试环境，登录被拒绝"), "error");
        return;
    }
    qDebug() << "[LoginDialog] 反调试检查通过";
    
    // 完整性检查
    if (!INTEGRITY_CHECK()) {
        qDebug() << "[LoginDialog] 完整性检查失败";
        showStatus(tr("文件完整性验证失败"), "error");
        return;
    }
    qDebug() << "[LoginDialog] 完整性检查通过";
    
    // 验证授权完整性
    if (!validateAuthIntegrity()) {
        qDebug() << "[LoginDialog] 授权验证失败";
        showStatus(tr("授权验证失败"), "error");
        return;
    }
    qDebug() << "[LoginDialog] 授权验证通过";
    
    if (m_reply) { m_reply->abort(); m_reply->deleteLater(); m_reply = nullptr; }
    showStatus(tr("正在登录..."), "loading");
    m_btnLogin->setEnabled(false);
    
    // 保护认证数据
    protectAuthData();
    
    QUrl url(m_editApiBase->text());
    qDebug() << "[LoginDialog] 请求URL:" << url.toString();
    qDebug() << "[LoginDialog] URL是否有效:" << url.isValid();
    
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // 构建登录载荷
    QByteArray payload = buildLoginPayload();
    qDebug() << "[LoginDialog] 登录载荷长度:" << payload.length();
    qDebug() << "[LoginDialog] 登录载荷内容:" << QString::fromUtf8(payload);
    
    // 加密登录载荷
    QByteArray encryptedPayload = Protection::encryptAuthData(payload);
    qDebug() << "[LoginDialog] 加密后载荷长度:" << encryptedPayload.length();
    
    m_reply = m_nam.post(req, encryptedPayload);
    connect(m_reply, &QNetworkReply::finished, this, &LoginDialog::onRequestFinished);
    qDebug() << "[LoginDialog] 网络请求已发送，等待响应...";
}

void LoginDialog::onRequestFinished()
{
    qDebug() << "[LoginDialog] ===== 开始处理登录响应 =====";
    
    // 再次进行反调试检查
    if (!ANTI_DEBUG_CHECK()) {
        qDebug() << "[LoginDialog] 反调试检查失败";
        showStatus(tr("检测到调试环境，登录被拒绝"), "error");
        m_btnLogin->setEnabled(true);
        return;
    }
    qDebug() << "[LoginDialog] 反调试检查通过";
    
    // 检查网络错误和HTTP状态
    QNetworkReply::NetworkError networkError = m_reply->error();
    int httpStatus = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString httpStatusText = m_reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    
    qDebug() << "[LoginDialog] 网络错误代码:" << networkError;
    qDebug() << "[LoginDialog] 网络错误字符串:" << m_reply->errorString();
    qDebug() << "[LoginDialog] HTTP状态码:" << httpStatus;
    qDebug() << "[LoginDialog] HTTP状态文本:" << httpStatusText;
    qDebug() << "[LoginDialog] 请求URL:" << m_reply->url().toString();
    
    // 读取响应数据 - 无论是否有网络错误都需要读取
    QByteArray body = m_reply->readAll();
    qDebug() << "[LoginDialog] 原始响应数据长度:" << body.length();
    qDebug() << "[LoginDialog] 原始响应数据内容:" << QString::fromUtf8(body);
    
    m_reply->deleteLater();
    m_reply = nullptr;
    
    // 检查HTTP状态码，即使有网络错误也要尝试解析响应
    if (httpStatus >= 400) {
        qDebug() << "[LoginDialog] HTTP错误状态码:" << httpStatus;
        
        // 尝试解析错误响应
        QJsonParseError parseError;
        QJsonDocument errorDoc = QJsonDocument::fromJson(body, &parseError);
        
        if (parseError.error == QJsonParseError::NoError && errorDoc.isObject()) {
            QJsonObject errorObj = errorDoc.object();
            QString serverError = errorObj.value("error").toString();
            QString serverMessage = errorObj.value("message").toString();
            
            qDebug() << "[LoginDialog] 服务器错误代码:" << serverError;
            qDebug() << "[LoginDialog] 服务器错误消息:" << serverMessage;
            qDebug() << "[LoginDialog] 完整错误对象:" << QJsonDocument(errorObj).toJson(QJsonDocument::Compact);
            
            QString displayMessage;
            if (!serverMessage.isEmpty()) {
                displayMessage = serverMessage;
                qDebug() << "[LoginDialog] 准备显示服务器消息:" << displayMessage;
            } else if (!serverError.isEmpty()) {
                displayMessage = tr("服务器错误: %1").arg(serverError);
                qDebug() << "[LoginDialog] 准备显示错误代码:" << displayMessage;
            } else {
                displayMessage = tr("服务器返回错误 (HTTP %1)").arg(httpStatus);
                qDebug() << "[LoginDialog] 准备显示HTTP错误:" << displayMessage;
            }
            
            qDebug() << "[LoginDialog] 调用showStatus，消息长度:" << displayMessage.length();
            qDebug() << "[LoginDialog] 调用showStatus，消息内容:" << displayMessage;
            qDebug() << "[LoginDialog] m_status指针:" << m_status;
            qDebug() << "[LoginDialog] m_status是否可见(调用前):" << (m_status ? m_status->isVisible() : false);
            
            showStatus(displayMessage, "error");
            
            qDebug() << "[LoginDialog] showStatus调用完成";
            qDebug() << "[LoginDialog] m_status是否可见(调用后):" << (m_status ? m_status->isVisible() : false);
            qDebug() << "[LoginDialog] m_status当前文本:" << (m_status ? m_status->text() : "null");
        } else {
            // 无法解析JSON，显示HTTP错误
            QString httpErrorMsg;
            switch (httpStatus) {
                case 400:
                    httpErrorMsg = tr("请求参数错误");
                    break;
                case 401:
                    httpErrorMsg = tr("未授权访问");
                    break;
                case 403:
                    httpErrorMsg = tr("访问被拒绝，请检查账号权限");
                    break;
                case 404:
                    httpErrorMsg = tr("请求的资源不存在");
                    break;
                case 500:
                    httpErrorMsg = tr("服务器内部错误");
                    break;
                default:
                    httpErrorMsg = tr("服务器错误 (HTTP %1)").arg(httpStatus);
                    break;
            }
            showStatus(httpErrorMsg, "error");
        }
        
        m_btnLogin->setEnabled(true);
        return;
    }
    
    // 处理其他网络错误（非HTTP错误）
    if (networkError != QNetworkReply::NoError && httpStatus < 400) {
        QString errorMsg;
        switch (networkError) {
            case QNetworkReply::ConnectionRefusedError:
                errorMsg = tr("连接被拒绝，请检查网络连接");
                break;
            case QNetworkReply::RemoteHostClosedError:
                errorMsg = tr("服务器关闭了连接");
                break;
            case QNetworkReply::HostNotFoundError:
                errorMsg = tr("无法找到服务器，请检查网络");
                break;
            case QNetworkReply::TimeoutError:
                errorMsg = tr("连接超时，请重试");
                break;
            case QNetworkReply::OperationCanceledError:
                errorMsg = tr("请求被取消");
                break;
            case QNetworkReply::SslHandshakeFailedError:
                errorMsg = tr("SSL握手失败");
                break;
            default:
                errorMsg = tr("网络错误: %1").arg(m_reply->errorString());
                break;
        }
        qDebug() << "[LoginDialog] 网络错误，显示消息:" << errorMsg;
        showStatus(errorMsg, "error");
        m_btnLogin->setEnabled(true);
        return;
    }

    // 解密响应数据
    QByteArray decryptedBody = Protection::decryptAuthData(body);
    qDebug() << "[LoginDialog] 解密后数据长度:" << decryptedBody.length();
    qDebug() << "[LoginDialog] 解密后数据内容:" << QString::fromUtf8(decryptedBody);
    
    QJsonParseError perr; 
    QJsonDocument doc = QJsonDocument::fromJson(decryptedBody, &perr);
    qDebug() << "[LoginDialog] 解密数据JSON解析错误:" << perr.error << "错误信息:" << perr.errorString();
    
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        qDebug() << "[LoginDialog] 解密数据解析失败，尝试解析原始数据";
        // 如果解密失败，尝试直接解析原始数据
        doc = QJsonDocument::fromJson(body, &perr);
        qDebug() << "[LoginDialog] 原始数据JSON解析错误:" << perr.error << "错误信息:" << perr.errorString();
        if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
            qDebug() << "[LoginDialog] 原始数据解析也失败，显示格式错误";
            showStatus(tr("服务器响应格式错误"), "error");
            m_btnLogin->setEnabled(true);
            return;
        }
        qDebug() << "[LoginDialog] 使用原始数据解析成功";
    } else {
        qDebug() << "[LoginDialog] 使用解密数据解析成功";
    }
    
    QJsonObject obj = doc.object();
    qDebug() << "[LoginDialog] JSON对象包含字段:" << obj.keys();
    qDebug() << "[LoginDialog] 完整JSON内容:" << QJsonDocument(obj).toJson(QJsonDocument::Compact);
    
    bool success = obj.value("success").toBool();
    qDebug() << "[LoginDialog] success字段值:" << success;
    
    if (success) {
        qDebug() << "[LoginDialog] 登录成功，处理响应";
        // 使用控制流混淆保护关键逻辑
        {
            m_token = obj.value("token").toString();
            qDebug() << "[LoginDialog] 获取到token，长度:" << m_token.length();
            
            // 读取类型字段，free 为试用
            QString utype;
            if (obj.contains("user") && obj.value("user").isObject()) {
                QJsonObject u = obj.value("user").toObject();
                utype = u.value("type").toString();
                qDebug() << "[LoginDialog] 用户类型:" << utype;
                if (utype.isEmpty()) {
                    // 兼容：部分后端可能用 subscription.type
                    if (u.contains("subscription") && u.value("subscription").isObject()) {
                        utype = u.value("subscription").toObject().value("type").toString();
                        qDebug() << "[LoginDialog] 从subscription获取类型:" << utype;
                    }
                }
            }
            
            // 使用混淆的字符串比较
            QString freeStr = OBFUSCATE_STR("free");
            s_isTrialUser = (utype.compare(freeStr, Qt::CaseInsensitive) == 0);
            qDebug() << "[LoginDialog] 是否为试用用户:" << s_isTrialUser;
            
            // 处理版本信息（服务器返回的是 updateInfo 字段）
            qDebug() << "[LoginDialog] 检查更新信息字段...";
            if (obj.contains("updateInfo") && obj.value("updateInfo").isObject()) {
                QJsonObject updateInfoObj = obj.value("updateInfo").toObject();
                QString latestVersion = updateInfoObj.value("latestVersion").toString();
                bool hasUpdate = updateInfoObj.value("hasUpdate").toBool();
                QString updateUrl = updateInfoObj.value("updateUrl").toString();
                QString updateLog = updateInfoObj.value("updateLog").toString();
                QString checksumMD5 = updateInfoObj.value("checksumMD5").toString();
                QString updateSize = updateInfoObj.value("updateSize").toString();
                
                qDebug() << "[LoginDialog] ✅ 找到更新信息";
                qDebug() << "[LoginDialog] 服务器最新版本:" << latestVersion;
                qDebug() << "[LoginDialog] 是否有更新:" << hasUpdate;
                qDebug() << "[LoginDialog] 更新URL:" << updateUrl;
                qDebug() << "[LoginDialog] 更新大小:" << updateSize;
                qDebug() << "[LoginDialog] MD5:" << checksumMD5;
                qDebug() << "[LoginDialog] 准备发射 versionInfoReceived 信号...";
                
                // 发送版本信息信号（包含完整更新信息）
                emit versionInfoReceived(latestVersion, hasUpdate, updateUrl, updateLog, checksumMD5, updateSize);
                
                qDebug() << "[LoginDialog] ✅ versionInfoReceived 信号已发射";
            } else {
                qDebug() << "[LoginDialog] ⚠️  未找到 updateInfo 字段";
            }
            
            saveToIni();
            showStatus(tr("登录成功！"), "success");
            // 短暂延迟后关闭对话框
            QTimer::singleShot(800, this, &LoginDialog::accept);
        }
        return;
    }
    
    // 登录失败，处理错误信息
    QString err = obj.value("error").toString();
    QString message = obj.value("message").toString();
    
    qDebug() << "[LoginDialog] 登录失败 - 错误代码:" << err;
    qDebug() << "[LoginDialog] 登录失败 - 错误消息:" << message;
    
    QString finalMessage;
    if (err == OBFUSCATE_STR("DEVICE_LIMIT_EXCEEDED")) {
        finalMessage = tr("设备数量超出限制，请登录网站管理设备");
        qDebug() << "[LoginDialog] 设备超限错误";
    } else if (err == OBFUSCATE_STR("INVALID_CREDENTIALS")) {
        finalMessage = tr("邮箱或密码错误");
        qDebug() << "[LoginDialog] 凭证错误";
    } else if (!message.isEmpty()) {
        // 显示服务器返回的具体错误信息
        finalMessage = message;
        qDebug() << "[LoginDialog] 使用服务器消息:" << message;
    } else if (!err.isEmpty()) {
        // 如果有错误代码但没有消息，显示错误代码
        finalMessage = tr("登录失败: %1").arg(err);
        qDebug() << "[LoginDialog] 使用错误代码:" << err;
    } else {
        // 默认错误提示
        finalMessage = tr("登录失败，请检查账号密码后重试");
        qDebug() << "[LoginDialog] 使用默认错误消息";
    }
    
    qDebug() << "[LoginDialog] 最终显示的错误消息:" << finalMessage;
    showStatus(finalMessage, "error");
    
    m_btnLogin->setEnabled(true);
    qDebug() << "[LoginDialog] ===== 登录响应处理完成 =====";
}

bool LoginDialog::validateAuthIntegrity()
{
    // 验证认证数据的完整性
    if (!Protection::validateLicense()) {
        return false;
    }
    
    // 检查关键字符串的完整性
    QString apiBase = m_editApiBase->text();
    if (apiBase.isEmpty() || !apiBase.contains("ctdy123.com")) {
        return false;
    }
    
    // 添加虚假验证逻辑
    volatile int dummy = 0;
    for (int i = 0; i < 100; ++i) {
        dummy += i * 2;
    }
    
    return dummy > 0;
}

void LoginDialog::protectAuthData()
{
    // 保护认证数据不被轻易获取
    Protection::protectWatermarkLogic();
    
    // 添加虚假操作
    volatile int dummy = 0;
    for (int i = 0; i < 200; ++i) {
        dummy += i * 3;
    }
    
    // 加密敏感数据
    QString email = m_editEmail->text();
    QString password = m_editPassword->text();
    
    if (!email.isEmpty()) {
        QByteArray encryptedEmail = Protection::encryptString(email);
        // 这里可以将加密后的数据存储到临时位置
    }
    
    if (!password.isEmpty()) {
        QByteArray encryptedPassword = Protection::encryptString(password);
        // 这里可以将加密后的数据存储到临时位置
    }
}


