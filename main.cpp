#include "mainwindow.h"
#include "simpleupdater.h"
#include "protection.h"
#include <QApplication>
#include <QTranslator>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QTextCodec>
#endif
#include "util.h"
#include "logindialog.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // 启动运行时保护
    Protection::startRuntimeProtection();
    
    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QTextCodec *codec = QTextCodec::codecForName("GB2312");
    QTextCodec::setCodecForLocale(codec);
    #endif
    QString iniFilePath = a.applicationDirPath() + "/user.ini";
    //qDebug() << iniFilePath;
    QTranslator translator;
    QString lang("zh");
    if (Util::readInit(iniFilePath, "language", lang)){
        if(lang == "en"){
            if (translator.load("ctdy123_en.qm",":/i18n")) {
                a.installTranslator(&translator);
            }
        }
        else{
            lang = "zh";
        }
    }

    QApplication::setApplicationName(FA_APP_NAME);
    QApplication::setApplicationVersion(FA_APP_VERSION);
    QApplication::setOrganizationName("ctdy123");
    QApplication::setOrganizationDomain("ctdy123.com");
    // 轻量级自定义更新器，使用阿里云OSS上的JSON配置
    SimpleUpdater *updater = new SimpleUpdater;
    QString updateURL("https://releases.oss-cn-beijing.aliyuncs.com/updates/update-config-en.json");
    if(lang=="zh")
        updateURL = "https://releases.oss-cn-beijing.aliyuncs.com/updates/update-config-zh.json";
    updater->setUpdateUrl(updateURL);
    updater->checkForUpdatesSilent();

    MainWindow w;
    w.SetLanguage(lang);
    QString autopage("true");
    if (Util::readInit(iniFilePath, "autopage", autopage)){
        autopage = autopage.toLower();
        if(autopage == "false" || autopage == "0"){
            w.SetAutoPage(false);
        }
        else{
            w.SetAutoPage(true);
        }
    }
    w.show();
    
    // 设置为免费版模式（默认带水印）
    LoginDialog::s_isTrialUser = true;
    
    int result = a.exec();
    Protection::stopRuntimeProtection();
    return result;
}


