#include "aboutdialog.h"
#include "app_version.h"
#include <QtWidgets>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QClipboard>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent), m_lang("zh")
{
    setWindowTitle(tr("关于"));
    setModal(true);
    setFixedSize(480, 480);
    
    // Windows 下按 DPI 调整基础字号与控件尺寸
#ifdef Q_OS_WIN
    {
        QScreen* scr = QGuiApplication::primaryScreen();
        const double scale = scr ? scr->logicalDotsPerInchX() / 96.0 : 1.0;
        const double clampScale = qBound(1.0, scale, 1.6);

        QFont base = font();
        base.setPointSizeF(qBound(9.0, 10.0 * clampScale, 11.0));
        setFont(base);
    }
#endif

    // 应用与登录对话框统一的风格
    applyUnifiedStyle();
    
    // 创建界面元素
    createUI();
    
    // 连接信号槽
    connect(m_btnOk, &QPushButton::clicked, this, &AboutDialog::onOkClicked);
}

void AboutDialog::applyUnifiedStyle()
{
    QFont f = font();
    f.setPointSize(14);
    setFont(f);
    
    // 设置对话框背景为系统默认色，与工具栏保持一致
    setAutoFillBackground(true);
    QPalette pal = palette();
    // 在macOS上使用窗口背景色
    pal.setColor(QPalette::Window, QColor(236, 236, 236)); // macOS默认窗口背景色
    setPalette(pal);
    
    // 应用与登录对话框一致的全局样式
    setStyleSheet(R"(
        QDialog {
            background-color: #F5F5F5;
        }

        QLabel {
            color: #2C3E50;
        }

        QPushButton {
            font-size: 12pt;
            border-radius: 4px;
            padding: 6px 16px;
        }

        QPushButton:hover {
            opacity: 0.9;
        }

        QPushButton:pressed {
            opacity: 0.8;
        }
    )");
}

void AboutDialog::createUI()
{
    // 创建标题
    m_titleLabel = new QLabel(tr("LoimReader - 影谷长图阅读器"), this);
    QFont titleFont = m_titleLabel->font();
    
#ifdef Q_OS_WIN
    {
        QScreen* scr = QGuiApplication::primaryScreen();
        const double scale = scr ? scr->logicalDotsPerInchX() / 96.0 : 1.0;
        const double clampScale = qBound(1.0, scale, 1.6);
        titleFont.setPointSizeF(qBound(12.0, 13.0 * clampScale, 14.0));
    }
#else
    titleFont.setPointSize(20);
#endif
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setMinimumHeight(40); // 确保标题有足够高度
    
    // 创建版本号标签
    QString version = LoimReader::AppVersion::getAppVersion();
    m_versionLabel = new QLabel(tr("版本 %1").arg(version), this);
    QFont versionFont = m_versionLabel->font();
#ifdef Q_OS_WIN
    {
        QScreen* scr = QGuiApplication::primaryScreen();
        const double scale = scr ? scr->logicalDotsPerInchX() / 96.0 : 1.0;
        const double clampScale = qBound(1.0, scale, 1.6);
        versionFont.setPointSizeF(qBound(8.5, 9.5 * clampScale, 10.5));
    }
#else
    versionFont.setPointSize(12);
#endif
    m_versionLabel->setFont(versionFont);
    m_versionLabel->setAlignment(Qt::AlignCenter);
    m_versionLabel->setStyleSheet("QLabel { color: #666; }");
    
    // 创建Logo标签
    m_logoLabel = new QLabel(this);
    m_logoLabel->setAlignment(Qt::AlignCenter);
    m_logoLabel->setMinimumHeight(60);
    
    // 设置默认Logo
    setLanguage(m_lang);
    
    // 创建内容标签
    m_contentLabel = new QLabel(this);
    m_contentLabel->setTextFormat(Qt::RichText);
    m_contentLabel->setOpenExternalLinks(true); // 允许点击链接打开
    m_contentLabel->setWordWrap(true);
    m_contentLabel->setText(createAboutText());
    m_contentLabel->setAlignment(Qt::AlignLeft);
    
    // Windows 下调整内容字体大小
#ifdef Q_OS_WIN
    {
        QScreen* scr = QGuiApplication::primaryScreen();
        const double scale = scr ? scr->logicalDotsPerInchX() / 96.0 : 1.0;
        const double clampScale = qBound(1.0, scale, 1.6);
        QFont contentFont = m_contentLabel->font();
        contentFont.setPointSizeF(qBound(8.5, 9.5 * clampScale, 10.5));
        m_contentLabel->setFont(contentFont);
        // 调整行间距
        m_contentLabel->setStyleSheet(QString("QLabel { line-height: %1; }").arg(1.2 * clampScale));
    }
#endif
    
    // 创建按钮
    m_btnOk = new QPushButton(tr("确定"), this);
    {
#ifdef Q_OS_WIN
        QScreen* scr = QGuiApplication::primaryScreen();
        const double scale = scr ? scr->logicalDotsPerInchX() / 96.0 : 1.0;
        const double clampScale = qBound(1.0, scale, 1.6);
        const int btnH = qBound(32, int(qRound(34 * clampScale)), 40);
        m_btnOk->setFixedHeight(btnH);
#else
        m_btnOk->setFixedHeight(36);
#endif
    }
    m_btnOk->setFixedWidth(80);
    m_btnOk->setDefault(true);
    // 设置主要按钮样式
    m_btnOk->setStyleSheet(
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
    
    // 按钮布局 - 一个按钮居右
    auto buttonWidget = new QWidget(this);
    auto buttonLayout = new QHBoxLayout(buttonWidget);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(12);
    
    buttonLayout->addStretch(); // 添加弹性空间
    buttonLayout->addWidget(m_btnOk);
    
    // 主布局
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(40, 20, 40, 20); // 减少上下边距
    mainLayout->setSpacing(15); // 减少间距
    
    mainLayout->addWidget(m_titleLabel);
    mainLayout->addWidget(m_versionLabel);
    mainLayout->addWidget(m_logoLabel);
    mainLayout->addSpacing(10);
    mainLayout->addWidget(m_contentLabel);
    mainLayout->addStretch(1);
    mainLayout->addWidget(buttonWidget);
}

QString AboutDialog::createAboutText() const
{
    QString text;


    text += QString::fromUtf8("智能识别长图片最佳分页点，一键转多页 PDF；支持单/双栏布局、300DPI 高质量输出，全部在本地处理，隐私安全。<br/><br/>");
    text += QString::fromUtf8("<b>主要功能：</b><br/>");
    text += QString::fromUtf8("• 智能分页：避免在文字/对话中间断开<br/>");
    text += QString::fromUtf8("• 双栏布局：一页显示 4 个页面，节省纸张<br/>");
    text += QString::fromUtf8("• 多格式：PNG/JPG/JPEG/GIF/BMP → PDF<br/>");
    text += QString::fromUtf8("• 本地处理：0 服务器上传，保护隐私<br/><br/>");
    text += QString::fromUtf8("在线使用：<a href=\"https://ctdy123.com\">https://ctdy123.com</a><br/><br/>");
    text += QString::fromUtf8("wechat: shadow-valley<br/>");

    return text;
}

void AboutDialog::setLanguage(const QString &lang)
{
    m_lang = lang;
    if (lang == "en") {
        QPixmap pix(":/images/sitelogo.png");
        if (!pix.isNull()) {
            // 缩放Logo到合适大小
            pix = pix.scaled(120, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_logoLabel->setPixmap(pix);
        }
    } else {
        // 中文版可以设置不同的Logo或者不显示Logo
        m_logoLabel->clear();
        m_logoLabel->setMinimumHeight(20); // 减少高度
    }
}

void AboutDialog::onOkClicked()
{
    accept();
}