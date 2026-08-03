#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif
#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QtNetwork>
#include "protection.h"

class QLineEdit;
class QPushButton;
class QLabel;
class QCheckBox;

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(QWidget *parent = nullptr);
    QString token() const;
    QString apiBase() const;
    QString email() const;
    QString deviceName() const;
    bool autoLoginAtStartup();

signals:
    void versionInfoReceived(const QString& latestVersion, bool hasUpdate, 
                            const QString& updateUrl = QString(), 
                            const QString& updateLog = QString(),
                            const QString& checksumMD5 = QString(),
                            const QString& updateSize = QString());

private slots:
    void onLoginClicked();
    void onRequestFinished();

private:
    void loadFromIni();
    void saveToIni();
    QByteArray buildLoginPayload() const;
    bool validateAuthIntegrity();
    void protectAuthData();
    void showStatus(const QString& message, const QString& type = "info");

private:
    QLineEdit *m_editEmail;
    QLineEdit *m_editPassword;
    QLineEdit *m_editDeviceName;
    QLineEdit *m_editApiBase;
    QCheckBox *m_chkRememberEmail;
    QCheckBox *m_chkRememberPassword;
    QPushButton *m_btnLogin;
    QPushButton *m_btnCancel;
    QLabel *m_status;
    QLabel *m_tip;

    QNetworkAccessManager m_nam;
    QNetworkReply *m_reply;

    QString m_token;
    QString m_savedPassword;
public:
    static bool s_isTrialUser;
};

#endif // LOGINDIALOG_H


