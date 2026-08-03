#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif
#include <QMainWindow>
#include <QtWidgets>
#include <QtNetwork>

#include "simpleupdater.h"
#include "updatedialog.h"

class QGraphicsScene;
class LeftView;
class MainView;
class QAction;
class QToolBox;
class QToolButton;
class QSplitter;
class QPainter;
class QPrinter;
class PopUp;


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    void SetLanguage(QString lang);
    void SetAutoPage(bool autopage);

protected:
    void resizeEvent(QResizeEvent* event);
    void moveEvent(QMoveEvent* event);
    void closeEvent(QCloseEvent* event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
private:
    uint32_t m_uuidHash;
    QGraphicsScene *m_leftScene,*m_leftScene2, *m_mainScene, *m_previewScene;
    MainView *m_mainView;
    LeftView* m_leftView;
    QPushButton *m_open, *m_resize, *m_print;
    QPixmap m_pixmap;
    QSplitter *m_splitter;
    bool m_autopage;

    QAction *m_helpAction;
    QAction *m_extandAction;
    QAction *m_openAction;
    QAction *m_printAction;
    QAction *m_saveAction;
    QAction *m_shrinkAction;
    QAction *m_zoominAction;
    QAction *m_zoomoutAction;
    QAction *m_exitAction;
    QAction *m_twoColsAction;
    QAction *m_pageNumAction;
    QAction *m_autotrimAction;
    QAction *m_openSiteAction;
    QAction *m_loginAction;
    QAction *m_aboutAction;
    QAction *m_checkUpdateAction;

    SimpleUpdater *m_updater;
    QLabel *m_updateLabel;
    QPushButton *m_updateButton;  // 新增：更新按钮
    
    // 更新信息存储
    QString m_latestVersion;
    QString m_downloadUrl;
    QString m_releaseNotes;
    QString m_checksumMD5;
    QString m_updateSize;
    bool m_hasUpdate;  // 新增：标记是否有更新
    
    // 下载管理
    QNetworkAccessManager* m_downloadManager;
    QNetworkReply* m_downloadReply;
    DownloadUpdateDialog* m_updateDialog;

    QToolBar *m_mainToolbar;
    QMenu *m_fileMenu;
    QMenu *m_editMenu;
    QMenu *m_aboutMenu;
    PopUp* m_popup;
    QString m_lang;

    QNetworkAccessManager m_netAccMan;
    QUrl m_url;
    QNetworkReply *m_netReply;
    QString m_uuid;


private:
    void fitViewPort();
    void createStatusBar();
    void createActions();
    void createMenus();
    void createToolbars();
    void print(QPainter *painter, QPrinter* printer);
    void loadFile(QString& fileName);
    void buildScenesFromPixmap();
    int getIconSize() const;  // Windows 下获取动态图标尺寸
    QRectF getIconDrawArea() const;  // 获取工具栏按钮的实际可用绘制区域
    double getIconScale() const;  // 获取图标内部元素的比例因子
    QRect ensureOddContentRect(const QPixmap& pix, int padding) const;  // 确保contentRect是奇数尺寸
    void drawCenteredIcon(QPixmap& targetPixmap, const QColor& iconColor, int padding, std::function<void(QPainter&, const QRect&)> drawFunction) const;  // 在内存中绘制完美居中的图标
    QIcon createSiteIcon() const;
    QIcon createLoginIcon() const;
    QIcon createAboutIcon() const;
    QIcon createStatusBarUpdateIcon() const;  // 新增：创建状态栏更新图标
    
    // 新增所有工具条按钮的图标创建函数
    QIcon createOpenIcon() const;
    QIcon createSaveIcon() const;
    QIcon createPrintIcon() const;
    QIcon createHelpIcon() const;
    QIcon createExtandIcon() const;
    QIcon createShrinkIcon() const;
    QIcon createZoomInIcon() const;
    QIcon createZoomOutIcon() const;
    QIcon createTwoColsIcon() const;
    QIcon createPageNumIcon() const;
    QIcon createAutoCutIcon() const;
    QIcon createExportIcon() const;
    QIcon createUpdateIcon() const;
    QIcon createMainAppIcon() const;
    void updateLoginAction();
    void loadLicense();
    void Get(QUrl url);
    
    // 窗口几何管理
    void initializeWindowGeometry();
    void saveWindowGeometry();
    bool restoreWindowGeometry();
    void saveSplitterState();
    void restoreSplitterState();
    bool isWindowGeometryValid(const QRect& geometry);
    QRect getDefaultWindowGeometry();
    void Threshold_pro(QImage *inputImage,QImage &outputImage,uint8_t bThres);
    void autoCut();
private slots:
    void onOpenClicked();
    void onResizeClicked();
    void onPrintClicked();
    void onHelpClicked();
    void onExtandClicked();
    void onSaveClicked();
    void onShrinkClicked();
    void onZoominClicked();
    void onZoomoutClicked();
    void onExitClicked();
    void onAutoCutClicked();
    void onAutoTrimClicked();
    void onTwoColsClicked();
    void onPageNumClicked();
    void onCheckUpdateClicked();
    void QueryFinished();
    void onVersionInfoReceived(const QString& latestVersion, bool hasUpdate,
                              const QString& updateUrl = QString(),
                              const QString& updateLog = QString(),
                              const QString& checksumMD5 = QString(),
                              const QString& updateSize = QString());
    
    // 更新对话框相关
    void showUpdateDialog();
    void startDownload();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void onDownloadError(QNetworkReply::NetworkError error);
};

#endif


