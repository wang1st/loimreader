#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif
#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

class QLabel;
class QPushButton;

class AboutDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AboutDialog(QWidget *parent = nullptr);
    void setLanguage(const QString &lang);

private slots:
    void onOkClicked();

private:
    void applyUnifiedStyle();
    void createUI();
    QString createAboutText() const;

private:
    QLabel *m_titleLabel;
    QLabel *m_versionLabel;
    QLabel *m_contentLabel;
    QLabel *m_logoLabel;
    QPushButton *m_btnOk;
    QString m_lang;
};

#endif // ABOUTDIALOG_H