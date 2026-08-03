#include <QtGui>
#include <QPrinter>
#include <QPrintDialog>
#include <QDesktopServices>
#include <QMessageBox>
#include <QClipboard>
#include <QGuiApplication>
#include <QSettings>
#include <QScreen>
#include <QTimer>
#include <QLabel>
#include <QStandardPaths>
#include <QFileInfo>


#ifndef NO_POPPLER
#include "poppler-qt5.h"
#endif
#include "mainwindow.h"
#include "mainview.h"
#include "leftview.h"
#include "pageitem.h"
#include "previewitem.h"
#include "popup.h"
#include "machineid.h"
#include "aboutdialog.h"
#include "logindialog.h"
#include "util.h"
#include "app_version.h"
#include "updatedialog.h"
#include "customsplitter.h"

MainWindow::MainWindow()
{
    createActions();
    createMenus();

    m_mainView = new MainView();
    m_leftView = new LeftView();
    m_mainView->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    m_leftView->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    this->setAcceptDrops(true);
    m_mainView->setAcceptDrops(true);
    m_leftView->setAcceptDrops(true);
    m_mainScene = new QGraphicsScene();
    m_mainScene->setBackgroundBrush(QColor(150, 150, 150));
    m_mainView->setScene(m_mainScene);

    m_leftScene = new QGraphicsScene();
    m_leftScene->setBackgroundBrush(QColor(150, 150, 150));
   // m_leftView->setScene(m_leftScene);

    m_leftScene2 = new QGraphicsScene();
    m_leftScene2->setBackgroundBrush(QColor(150, 150, 150));
//    m_leftView->setScene(m_leftScene2);

    m_previewScene = m_leftScene;
    m_leftView->setScene(m_previewScene);
    setWindowTitle(tr("影谷长图阅读器"));
    // 去除版本号显示

    // 使用Jony Ive风格的自定义分割条
    m_splitter = new CustomSplitter();
    m_splitter->addWidget(m_leftView);
    m_splitter->addWidget(m_mainView);
    // 默认左右各占一半（50:50）
    m_splitter->setSizes(QList<int>({50000, 50000}));

    setCentralWidget(m_splitter);

    createToolbars();
    updateLoginAction(); // 设置登录按钮的初始状态
    createStatusBar();
    
    // 初始化窗口大小和位置
    initializeWindowGeometry();

    m_mainScene->setSceneRect(m_mainView->viewport()->rect());
    m_leftScene->setSceneRect(m_leftView->viewport()->rect());
    m_leftScene2->setSceneRect(m_leftView->viewport()->rect());
    setUnifiedTitleAndToolBarOnMac(true);
    m_popup = new PopUp(this);

#ifdef Q_OS_WIN
    // Windows 下根据 DPI 放大工具栏图标
    {
        QScreen* scr = QGuiApplication::primaryScreen();
        const double scale = scr ? scr->logicalDotsPerInchX() / 96.0 : 1.0;
        const double clampScale = qBound(1.0, scale, 1.6);
        int iconPx = 32;  // 基础尺寸从28提升到32
        if (clampScale >= 1.4) iconPx = 40;  // 高DPI从36提升到40
        else if (clampScale >= 1.15) iconPx = 36;  // 中DPI从32提升到36
        m_mainToolbar->setIconSize(QSize(iconPx, iconPx));
        // 将 padding 降到最小，最大化图标可绘制空间；仍保持足够的点击面积
        m_mainToolbar->setStyleSheet("QToolButton { padding: 0px; min-width: 44px; min-height: 44px; } ");
    }
#endif

    m_uuid = "";
    m_netReply = Q_NULLPTR;

    m_autopage = true;
    
    // 初始化下载管理器
    m_downloadManager = new QNetworkAccessManager(this);
    m_downloadReply = nullptr;
    m_updateDialog = nullptr;
    
    // 初始化更新器
    m_updater = new SimpleUpdater(this);
    m_updater->setUpdateUrl("https://ctdy123.com/api/update/check");
    
    // 启动时显示当前版本（版本信息将在登录时从服务器获取）
    QTimer::singleShot(1000, this, [this]() {
        QString currentVersion = LoimReader::AppVersion::getAppVersion();
        qDebug() << "=== [MainWindow] 初始化版本显示 ===";
        qDebug() << "[MainWindow] 当前版本:" << currentVersion;
        m_updateLabel->setText(tr("v%1").arg(currentVersion));
        m_updateLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; }");
        m_updateLabel->setVisible(true);
        qDebug() << "[MainWindow] 版本标签已显示";
    });

}

void MainWindow::SetAutoPage(bool autopage)
{
    m_autopage = autopage;
}

void MainWindow::SetLanguage(QString lang)
{
    m_lang = lang;
    qDebug() << "MachineID: " << machineID() <<
                "MachineIDHash: " << machineIDHash() <<
                "MachineIDHashKey: " << machineIDHashKey();
    loadLicense();
}

void MainWindow::loadFile(QString &fileName)
{
    QString id = m_uuid;
    QDataStream in(QCryptographicHash::hash(id.toLatin1(), QCryptographicHash::Md5));
    in.setByteOrder(QDataStream::LittleEndian);
    uint32_t result = 0;
    for(int i = 0; i < 4; i++)
    {
        uint32_t x;
        in >> x;
        result ^= x;
    }
    qDebug() <<"uuid hash"<< result; //has("") == 997549406


    if(fileName.toLower().endsWith(".pdf")){
#ifdef NO_POPPLER
        // Qt6 默认禁用 Poppler 路径：提示不支持PDF，继续按图片失败处理
        m_pixmap = QPixmap();
#else
        Poppler::Document *document = Poppler::Document::load(fileName); //将pdf文件加载进Document
        //document->setRenderBackend(Poppler::Document::RenderBackend::ArthurBackend);
        QImage image;
        if(document != 0){
            Poppler::Page *pdfPage = document->page(0);
            if(pdfPage != 0){
                if(pdfPage->textList().count() > 0){
                    QList<float> sizeList({720, 600, 300, 216, 150, 96, 72});
                    QListIterator<float> itor(sizeList);
                    while(itor.hasNext()){
                        float res = itor.next();
                        image = pdfPage->renderToImage(res, res);
                        //qDebug() << "res=" << res;
                        m_pixmap = QPixmap::fromImage(image);
                        //qDebug() << "m_pixmap, w=" << m_pixmap.width() << ",h=" << m_pixmap.height();
                        if(m_pixmap.height() > 10 || m_pixmap.width() > 10)
                            break;
                     }
                }
                else{
                    image = pdfPage->renderToImage(72, 72);
                    m_pixmap = QPixmap::fromImage(image);
                }
            }
            delete pdfPage;
        }
        delete document;
#endif
    }
    else{
        QImageReader reader(fileName);
        reader.setAutoTransform(true);
        // 为超长/超高图片放宽内存限制（Qt6 默认约 128MB）
        QSize sz = reader.size();
        if (sz.isValid()) {
            qint64 bytes = qint64(sz.width()) * qint64(sz.height()) * 4; // 估算32位像素
            int needMB = int((bytes + (1<<20) - 1) >> 20);
            int limitMB = qMax(256, needMB + 32); // 预留余量
            QImageReader::setAllocationLimit(limitMB);
        } else {
            QImageReader::setAllocationLimit(1024); // 兜底1GB
        }
        QImage img = reader.read();
        if(!img.isNull())
            m_pixmap = QPixmap::fromImage(img, Qt::NoFormatConversion);
        else
        m_pixmap.load(fileName);
    }

    QRectF rect = m_pixmap.rect();
    //qDebug() << "img size:" << rect;
    if (rect.width() > 10.0 && rect.height() > 10.0){
//        QImage inImg = m_pixmap.toImage();
//        QImage outImg = m_pixmap.toImage();
//        Threshold_pro(&inImg, outImg, 128);
//        m_pixmap = QPixmap::fromImage(outImg);

        buildScenesFromPixmap();
    }
    else{
        //qDebug() << "不支持的图片格式或文件已损坏";
        m_popup->setPopupText(tr("不支持的图片格式或文件已损坏"));
        m_popup->show();
    }
}

void MainWindow::buildScenesFromPixmap()
{
    QRectF rect = m_pixmap.rect();
    if (rect.width() > 10.0 && rect.height() > 10.0){
        // recompute uuid hash used for watermarking/preview items
        uint32_t result = 0;
        {
            QString id = m_uuid;
            QByteArray md5 = QCryptographicHash::hash(id.toLatin1(), QCryptographicHash::Md5);
            QDataStream in(md5);
            in.setByteOrder(QDataStream::LittleEndian);
            for(int i = 0; i < 4; i++){
                uint32_t x; in >> x; result ^= x;
            }
        }
        PageItem::initRatio();
        PreviewItem::initMaxHeight();
        m_mainScene->clear();
//        m_mainScene->setSceneRect(rect);
//        m_mainScene->addPixmap(m_pixmap);
//        m_mainView->fitInView(rect, Qt::KeepAspectRatio);

//        m_leftScene->clear();

        fitViewPort();

        //A4纸张的尺寸 210mm×297mm
        qreal width = rect.width();
        qreal height = width / 0.7; //210 * 297;
        quint32 nums = quint32(rect.height()) / quint32(height) + 1;
        //qDebug() << "width:" << width << ",height:" << height <<",pagenums:" << nums;
        m_mainScene->clear();
        m_leftScene->clear();
        m_leftScene2->clear();
        //qDebug() << "rect:" << rect;
        qreal left_height = 0.0;
        qreal left_height2 = 0.0;
        qreal left_width = 0.0;
        qreal main_height = 0.0;
        PageItem *prev = NULL;
        PreviewItem *prevw = NULL;
        PreviewItem *prevw2 = NULL;

        QString appPath = QCoreApplication::applicationFilePath();
        int pos = appPath.lastIndexOf('/');
        QString appDir = appPath.left(pos);
        QString iniFilePath =  appDir + "/user.ini";
            //qDebug() << "appPath:" << appPath <<", ini:"  << iniFilePath;
        QString marginsc = "10";
        Util::readInit(iniFilePath, "marginsc", marginsc);
        if(marginsc == "") marginsc = "10";
        int marginsc_n = marginsc.toInt();

        for(quint32 i = 0; i < nums; i++){
            QRect new_rect(0, i * height, width, height);
            PageItem* item = new PageItem(m_pixmap, new_rect);
            item->setPagenum(i);
            if(prev){
                prev->setNextPage(item);
            }
            prev = item;
            m_mainScene->addItem(item);
            PreviewItem* thumb = new PreviewItem(item);
            thumb->setMarginScale(marginsc_n);
            thumb->SetLanguage(m_lang);
            m_uuidHash = result;
            thumb->setUuidHash(m_uuidHash);
            thumb->setPageNum(i);
            if(prevw){
                prevw->setNext(thumb);
            }
            prevw = thumb;
            m_leftScene->addItem(thumb);

            if(i % 4 == 0){
                PreviewItem* thumb2 = new PreviewItem(item, 4);
                thumb2->setMarginScale(marginsc_n);
                thumb2->SetLanguage(m_lang);
                thumb2->setUuidHash(m_uuidHash);
                thumb2->setPageNum(i / 4);
                m_leftScene2->addItem(thumb2);
                left_height2 = left_height2 + thumb2->boundingRect().height() + thumb2->pageSpace();
                if(prevw2)
                    prevw2->setNext(thumb2);
                prevw2 = thumb2;
            }
            else{
                prevw2->appendItem(item);
            }


            main_height = main_height + item->boundingRect().height();
            left_height = left_height + thumb->boundingRect().height() + thumb->pageSpace();
            left_width = thumb->boundingRect().width();
        }
        rect.setHeight(main_height);
        //qDebug() << "rect:" << rect;
        m_mainScene->setSceneRect(rect);
        rect.setHeight(left_height);
        rect.setWidth(left_width);
        //qDebug() << "rect:" << rect;
        m_leftScene->setSceneRect(rect);
        rect.setHeight(left_height2);
        m_leftScene2->setSceneRect(rect);

        if(m_autopage)
            autoCut();

        fitViewPort();
        if(nums > 0){
            m_saveAction->setEnabled(true);
            m_printAction->setEnabled(true);
            m_extandAction->setEnabled(true);
            m_shrinkAction->setEnabled(true);
            m_zoominAction->setEnabled(true);
            m_zoomoutAction->setEnabled(true);
            m_twoColsAction->setEnabled(true);
            m_pageNumAction->setEnabled(true);
            if(m_autotrimAction) m_autotrimAction->setEnabled(true);
        }
    }
    
    // 启用滚动条，因为现在有内容需要滚动
    if (m_mainView) {
        m_mainView->enableScrollBars();
    }
    if (m_leftView) {
        m_leftView->enableScrollBars();
    }
}

void MainWindow::onOpenClicked()
{
    //qDebug("onOpen");
    QString appPath = QCoreApplication::applicationFilePath();
    int pos = appPath.lastIndexOf('/');
    QString appDir = appPath.left(pos);
    QString iniFilePath =  appDir + "/user.ini";
        //qDebug() << "appPath:" << appPath <<", ini:"  << iniFilePath;
    QString dir = appDir;
    Util::readInit(iniFilePath, "imagedir", dir);
    if(dir == "")
        dir = appDir;
    QString fileName = QFileDialog::getOpenFileName(this, tr("打开图像或PDF文件"),
            dir, tr("文件(*.pdf *.jpg *.png *.jpeg)"));
    if(fileName == "") return;
    pos = fileName.lastIndexOf('/');
    if (pos > 0){
        dir = fileName.left(pos);
        Util::writeInit(iniFilePath, "imagedir", dir);
    }
    loadFile(fileName);
}


void MainWindow::onResizeClicked()
{
    qDebug("onResize");

}

void MainWindow::onPrintClicked()
{
    qDebug("onPrint");
    QPrinter printer;

    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("打印"));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    //qDebug() <<"fromPage:" << printer.fromPage() << ",toPage:"
     //       << printer.toPage() << ",numCopies:" << printer.numCopies();
    QPainter painter(&printer);
    print(&painter, &printer);
    painter.end();
}

void MainWindow::onSaveClicked()
{

    QString appPath = QCoreApplication::applicationFilePath();
    int pos = appPath.lastIndexOf('/');
    QString appDir = appPath.left(pos);
    QString iniFilePath =  appDir + "/user.ini";
    QString dir = appDir;
    Util::readInit(iniFilePath, "savepath", dir);
    if(dir == "")
        dir = appDir;

    QString fileName = QFileDialog::getSaveFileName(this, tr("导出到PDF"),
    dir, tr("PDF文件(*.pdf)"));
    //qDebug() << "filename:" << fileName;
    if(fileName == "") return;
    pos = fileName.lastIndexOf('/');
    if (pos > 0){
        dir = fileName.left(pos);
        Util::writeInit(iniFilePath, "savepath", dir);
    }

    QPrinter printer(QPrinter::HighResolution);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(QPageLayout::Portrait);
    printer.setOutputFormat(QPrinter::PdfFormat);
#else
    printer.setPageSize(QPrinter::A4);
    printer.setOrientation(QPrinter::Portrait);
    printer.setOutputFormat(QPrinter::PdfFormat);
#endif
    printer.setOutputFileName(fileName);
    //qDebug() <<"fromPage:" << printer.fromPage() << ",toPage:"
     //       << printer.toPage() << ",numCopies:" << printer.numCopies();
    QPainter painter(&printer);
    print(&painter, &printer);
    painter.end();
    QString message = tr("处理结果已输出至文件");
    message += fileName;
    m_popup->setPopupText(message);
    m_popup->show();
}


void MainWindow::print(QPainter *painter, QPrinter* printer)
{
    QRect rect = painter->viewport();
    PreviewItem* item = dynamic_cast<PreviewItem*> (m_previewScene->itemAt(1.0, 1.0, QTransform()));
    while(item)
    {
        qreal mul_w = qreal(rect.width()) / qreal(item->boundingRect().width());
        qreal mul_h = qreal(rect.height()) / qreal(item->boundingRect().height());
        qreal mul = mul_w < mul_h ? mul_w : mul_h;
        painter->scale(mul, mul);
        int pageNum = item->pageNum() + 1;
        if(pageNum >= printer->fromPage() && pageNum <= printer->toPage() ||
                printer->fromPage() + printer->toPage() == 0){
            item->print(painter);
            if(item->next()){
                printer->newPage();
            }
        }
        painter->scale(1/mul, 1/mul);
        item = item->next();
    }

}

void MainWindow::onHelpClicked()
{
    // Help功能已移除
}


void MainWindow::onExtandClicked()
{
    QList<QGraphicsItem*> items = m_previewScene->items();
    QGraphicsItem* item;
    int marginsc;
    foreach(item, items){
        PreviewItem *it = dynamic_cast<PreviewItem*>(item);
        marginsc = it->decreaseMargins();
    }
    QString appPath = QCoreApplication::applicationFilePath();
    int pos = appPath.lastIndexOf('/');
    QString appDir = appPath.left(pos);
    QString iniFilePath =  appDir + "/user.ini";
    Util::writeInit(iniFilePath, "marginsc", QString("%1").arg(marginsc));

}

void MainWindow::onShrinkClicked()
{
    QList<QGraphicsItem*> items = m_previewScene->items();
    QGraphicsItem* item;
    int marginsc;
    foreach(item, items){
        PreviewItem *it = dynamic_cast<PreviewItem*>(item);
        marginsc = it->increaseMargins();
    }
    QString appPath = QCoreApplication::applicationFilePath();
    int pos = appPath.lastIndexOf('/');
    QString appDir = appPath.left(pos);
    QString iniFilePath =  appDir + "/user.ini";
    Util::writeInit(iniFilePath, "marginsc", QString("%1").arg(marginsc));
}

void MainWindow::onZoominClicked()
{
    qreal scale = m_mainView->scale();
    if(scale < 0.90){
        scale += 0.05;
        m_mainView->setScale(scale);
        m_mainView->fitViewPort(m_mainView->viewport()->rect().width());
    }
}

void MainWindow::onZoomoutClicked()
{
    qreal scale = m_mainView->scale();
    if(scale > 0.45){
        scale -= 0.05;
        m_mainView->setScale(scale);
        m_mainView->fitViewPort(m_mainView->viewport()->rect().width());
    }
}

void MainWindow::onExitClicked()
{
    this->close();
}


void MainWindow::onTwoColsClicked()
{
     qDebug("TwoCols");
    if(m_previewScene == m_leftScene)
        m_previewScene = m_leftScene2;
    else
        m_previewScene = m_leftScene;
    m_leftView->setScene(m_previewScene);
    m_leftView->update();

}


void MainWindow::onPageNumClicked()
{
    QList<QGraphicsItem*> items = m_previewScene->items();
    if(items.count()){
        PreviewItem *it = dynamic_cast<PreviewItem*>(items[0]);
        it->switchPageNum();
        QGraphicsItem* item;
        foreach(item, items){
            it = dynamic_cast<PreviewItem*>(item);
            it->update();
        }
    }
}

void MainWindow::autoCut()
{
    PageItem* item = dynamic_cast<PageItem*> (m_mainScene->itemAt(10.0, 10.0, QTransform()));
    while(item)
    {
        item->autoCut();
        item = item->nextPage();
    }

}

void MainWindow::onAutoCutClicked()
{
    PageItem* item = dynamic_cast<PageItem*> (m_mainScene->itemAt(10.0, 10.0, QTransform()));
    while(item)
    {
        item->autoCut();
        item = item->nextPage();
    }

}



void MainWindow::resizeEvent(QResizeEvent *event)
{
    qDebug("resizeEvent");
    fitViewPort();
    
    // 保存窗口几何信息
    saveWindowGeometry();
    
    QWidget::resizeEvent(event);
}


void MainWindow::fitViewPort()
{
    QRectF rect2 = m_mainView->viewport()->rect();
    m_mainView->fitViewPort(rect2.width());

    rect2 = m_leftView->viewport()->rect();
    m_leftView->fitViewPort(rect2.width());


    //m_view->setAlignment(Qt::AlignTop | Qt::AlignLeft);
}


void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("就绪"));
    
    // 创建版本信息容器
    QWidget *versionWidget = new QWidget(this);
    QHBoxLayout *versionLayout = new QHBoxLayout(versionWidget);
    versionLayout->setContentsMargins(8, 4, 8, 4);
    versionLayout->setSpacing(6);
    
    // 创建版本信息标签
    m_updateLabel = new QLabel(this);
    m_updateLabel->setText(tr("检查更新中..."));
    m_updateLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_updateLabel->setStyleSheet(
        "QLabel { "
        "    color: #8E8E93; "  // 平时灰色
        "    font-size: 12px; "
        "    font-weight: 500; "
        "    padding: 2px 6px; "
        "    background-color: rgba(142, 142, 147, 0.1); "
        "    border-radius: 4px; "
        "}"
    );
    m_updateLabel->setVisible(false);
    
    // 创建更新图标按钮（初始隐藏）
    m_updateButton = new QPushButton(this);
    m_updateButton->setFixedSize(20, 20);
    m_updateButton->setVisible(false);  // 初始隐藏
    m_updateButton->setIcon(createStatusBarUpdateIcon());
    m_updateButton->setIconSize(QSize(16, 16));
    m_updateButton->setStyleSheet(
        "QPushButton { "
        "    border: none; "
        "    background: transparent; "
        "    border-radius: 2px; "
        "    padding: 2px; "
        "} "
        "QPushButton:hover { "
        "    background-color: rgba(0, 122, 255, 0.1); "
        "} "
        "QPushButton:pressed { "
        "    background-color: rgba(0, 122, 255, 0.2); "
        "}"
    );
    m_updateButton->setToolTip(tr("点击下载最新版本"));
    
    // 连接更新按钮点击信号
    connect(m_updateButton, &QPushButton::clicked, this, &MainWindow::showUpdateDialog);
    
    versionLayout->addWidget(m_updateLabel);
    versionLayout->addWidget(m_updateButton);
    
    // 将版本信息容器添加到状态栏右侧
    statusBar()->addPermanentWidget(versionWidget);
    
    // 初始化更新标志
    m_hasUpdate = false;
}

void MainWindow::createActions()
{
    m_helpAction = new QAction(createHelpIcon(), tr("指南"), this);
    m_helpAction->setShortcut(tr("Ctrl+H"));
    m_helpAction->setStatusTip(tr("打开软件使用指南"));
    connect(m_helpAction, &QAction::triggered, this, &MainWindow::onHelpClicked);

    m_extandAction = new QAction(createExtandIcon(), tr("减少边距"), this);
    m_extandAction->setStatusTip(tr("放大图片减少打印边距"));
    m_extandAction->setEnabled(false);
    connect(m_extandAction, &QAction::triggered, this, &MainWindow::onExtandClicked);
    qDebug() << "extandAction 图标已设置，尺寸:" << m_extandAction->icon().pixmap(32, 32).size();

    m_shrinkAction = new QAction(createShrinkIcon(), tr("增加边距"), this);
    m_shrinkAction->setStatusTip(tr("缩小图片增加打印边距"));
    m_shrinkAction->setEnabled(false);
    connect(m_shrinkAction, &QAction::triggered, this, &MainWindow::onShrinkClicked);
    qDebug() << "shrinkAction 图标已设置，尺寸:" << m_shrinkAction->icon().pixmap(32, 32).size();


    m_zoominAction = new QAction(createZoomInIcon(), tr("放大"), this);
    m_zoominAction->setStatusTip(tr("放大画布显示更多细节"));
    m_zoominAction->setEnabled(false);
    connect(m_zoominAction, &QAction::triggered, this, &MainWindow::onZoominClicked);
    m_zoomoutAction = new QAction(createZoomOutIcon(), tr("缩小"), this);
    m_zoomoutAction->setStatusTip(tr("缩小画布显示更多图片"));
    m_zoomoutAction->setEnabled(false);
    connect(m_zoomoutAction, &QAction::triggered, this, &MainWindow::onZoomoutClicked);

    m_openAction = new QAction(createOpenIcon(), tr("打开文件"), this);
    m_openAction->setShortcut(tr("Ctrl+O"));
    m_openAction->setStatusTip(tr("打开需要处理的长图片文件"));
    connect(m_openAction, &QAction::triggered, this, &MainWindow::onOpenClicked);
    m_saveAction = new QAction(createExportIcon(), tr("导出为PDF"), this);
    m_saveAction->setShortcut(tr("Ctrl+S"));
    m_saveAction->setStatusTip(tr("导出到PDF文件"));
    m_saveAction->setEnabled(false);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSaveClicked);

    m_printAction = new QAction(createPrintIcon(), tr("打印"), this);
    m_printAction->setShortcut(tr("Ctrl+P"));
    m_printAction->setStatusTip(tr("输出到打印机"));
    m_printAction->setEnabled(false);
    connect(m_printAction, &QAction::triggered, this, &MainWindow::onPrintClicked);
    m_exitAction = new QAction(tr("退出"), this);
    m_exitAction->setShortcut(tr("Ctrl+X"));
    m_exitAction->setStatusTip(tr("关闭窗口退出应用程序"));
    connect(m_exitAction, &QAction::triggered, this, &MainWindow::onExitClicked);



    m_twoColsAction = new QAction(createTwoColsIcon(), tr("双排"), this);
    m_twoColsAction->setStatusTip(tr("页面单/双排输出切换"));
    connect(m_twoColsAction, &QAction::triggered, this, &MainWindow::onTwoColsClicked);
    m_twoColsAction->setEnabled(false);
    m_pageNumAction = new QAction(createPageNumIcon(), tr("页码"), this);
    m_autotrimAction = new QAction(createAutoCutIcon(), tr("自动切边"), this);
    m_autotrimAction->setStatusTip(tr("去除原图四周空白并重新处理"));
    m_autotrimAction->setEnabled(false);
    connect(m_autotrimAction, &QAction::triggered, this, &MainWindow::onAutoTrimClicked);
    m_loginAction = new QAction(tr("登录"), this);
    // 创建登录图标
    m_loginAction->setIcon(createLoginIcon());
    m_loginAction->setStatusTip(tr("登录账户移除水印"));
    connect(m_loginAction, &QAction::triggered, this, [this](){
        LoginDialog dlg(this);
        
        // 连接版本信息信号
        connect(&dlg, &LoginDialog::versionInfoReceived, this, &MainWindow::onVersionInfoReceived);
        
        int wd= this->x() + (this->width() - dlg.width()) / 2;
        int ht= this->y() + (this->height() - dlg.height()) / 2;
        dlg.move(wd, ht);
        if (dlg.exec() == QDialog::Accepted) {
            // 更新登录状态
            // 初始化轻量级自定义更新器
    m_updater = new SimpleUpdater(this);
    QString updateURL("https://releases.oss-cn-beijing.aliyuncs.com/updates/update-config-en.json");
    if(m_lang=="zh")
        updateURL = "https://releases.oss-cn-beijing.aliyuncs.com/updates/update-config-zh.json";
    m_updater->setUpdateUrl(updateURL);
            m_popup->setPopupText(tr("登录成功"));
            m_popup->show();
        }
    });
    m_pageNumAction->setStatusTip(tr("页码显示输出切换"));
    connect(m_pageNumAction, &QAction::triggered, this, &MainWindow::onPageNumClicked);
    m_pageNumAction->setEnabled(false);

    
    // 移除"检查更新"功能
    // m_checkUpdateAction = new QAction(createUpdateIcon(), tr("检查更新"), this);
    // m_checkUpdateAction->setStatusTip(tr("检查软件更新"));
    // connect(m_checkUpdateAction, &QAction::triggered, this, &MainWindow::onCheckUpdateClicked);

}

// Windows 下获取动态图标尺寸的辅助函数
int MainWindow::getIconSize() const
{
#ifdef Q_OS_WIN
    QScreen* scr = QGuiApplication::primaryScreen();
    const double scale = scr ? scr->logicalDotsPerInchX() / 96.0 : 1.0;
    const double clampScale = qBound(1.0, scale, 1.6);
    // 使用奇数尺寸确保中心对称性
    if (clampScale >= 1.4) return 41;  // 高DPI：奇数尺寸
    else if (clampScale >= 1.15) return 37;  // 中DPI：奇数尺寸
    else return 33;  // 基础DPI：奇数尺寸
#else
    return 33;  // macOS也使用奇数尺寸
#endif
}

// 获取工具栏按钮的实际可用绘制区域
QRectF MainWindow::getIconDrawArea() const
{
    const int S = getIconSize();
    // 让图标尽可能填满整个按钮区域，减少与系统边框的间隙
    // 只留很小的边距，让图标边框接近系统hover边框
    int margin = 0;  // 0px 边距，贴近系统边框
    return QRectF(margin, margin, S - margin * 2, S - margin * 2);
}

// 获取图标内部元素的比例因子
double MainWindow::getIconScale() const
{
    return getIconSize() / 32.0;  // 相对于原始32px的比例
}

// 确保contentRect是奇数尺寸，保证中心对称
QRect MainWindow::ensureOddContentRect(const QPixmap& pix, int padding) const
{
    QRect contentRect = pix.rect().adjusted(padding, padding, -padding, -padding);
    
    // 确保contentRect是奇数尺寸，保证中心对称
    if (contentRect.width() % 2 == 0) contentRect.setWidth(contentRect.width() - 1);
    if (contentRect.height() % 2 == 0) contentRect.setHeight(contentRect.height() - 1);
    
    return contentRect;
}

// 直接在目标pixmap上绘制完美居中的图标（不使用中间临时pixmap，避免模糊）
void MainWindow::drawCenteredIcon(QPixmap& targetPixmap, const QColor& iconColor, int padding, std::function<void(QPainter&, const QRect&)> drawFunction) const
{
    QPainter painter(&targetPixmap);
    // 使用高质量渲染提示
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    
    // 直接在目标pixmap上绘制
    const QRect contentRect = ensureOddContentRect(targetPixmap, padding);
    drawFunction(painter, contentRect);
    painter.end();
}



QIcon MainWindow::createLoginIcon() const
{
    const int S = getIconSize();
    QPixmap pix(S, S);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    // 高质量渲染
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // 最大化利用可绘制区域 - 减少padding到最小
    const QColor iconColor("#6c6c6c");
    const int padding = 2; // 从4减少到2，最大化可用空间
    const QRect contentRect = ensureOddContentRect(pix, padding);
    
    QPen pen(iconColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    
    // 优化的人形图标 - 充满整个可绘制区域
    // 头部 (圆形) - 使用更大的半径
    int headRadius = qRound(contentRect.width() * 0.3); // 从1/4增加到0.3
    QPoint headCenter(contentRect.center().x(), contentRect.top() + headRadius + 1);
    p.drawEllipse(headCenter, headRadius, headRadius);
    
    // 身体 (U形或弧形) - 使用整个可用高度
    int bodyTop = headCenter.y() + headRadius;
    QRectF bodyArcRect(contentRect.left() + 1, bodyTop, 
                       contentRect.width() - 2, 
                       contentRect.bottom() - bodyTop - 1);
    p.drawArc(bodyArcRect, 0 * 16, 180 * 16);
    
    p.end();
    return QIcon(pix);
}

// 新增所有工具条按钮的图标创建函数
QIcon MainWindow::createOpenIcon() const
{
    const int S = getIconSize();
    QPixmap pix(S, S);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    // 高质量渲染
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // 完全按照limereader的方式
    const QColor iconColor("#6c6c6c");
    const int padding = 4;
    const QRect contentRect = ensureOddContentRect(pix, padding);
    
    QPen pen(iconColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    
    // 山+太阳的图片图标设计
    int w = contentRect.width();
    int h = contentRect.height();
    int left = contentRect.left();
    int top = contentRect.top();
    int right = contentRect.right();
    int bottom = contentRect.bottom();
    int cx = contentRect.center().x();
    int cy = contentRect.center().y();
    
    // 统一的外边框（与其他图标一致）
    p.drawRect(contentRect);
    
    // 山形（左下角）
    int mountainLeft = left + w * 0.2;
    int mountainRight = left + w * 0.6;
    int mountainTop = bottom - h * 0.3;
    int mountainBottom = bottom - h * 0.1;
    
    // 山峰
    p.drawLine(mountainLeft, mountainBottom, cx - w * 0.1, mountainTop);
    p.drawLine(cx - w * 0.1, mountainTop, mountainRight, mountainBottom);
    
    // 太阳（右上角）
    int sunRadius = w * 0.15;
    int sunX = right - w * 0.25;
    int sunY = top + h * 0.25;
    p.drawEllipse(sunX - sunRadius, sunY - sunRadius, sunRadius * 2, sunRadius * 2);
    
    // 太阳内部小圆
    int innerRadius = sunRadius * 0.4;
    p.setBrush(iconColor);
    p.drawEllipse(sunX - innerRadius, sunY - innerRadius, innerRadius * 2, innerRadius * 2);
    
    p.end();
    return QIcon(pix);
}

QIcon MainWindow::createSaveIcon() const
{
    const int S = getIconSize();
    QPixmap pix(S, S);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    // 高质量渲染
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // 完全按照limereader的方式
    const QColor iconColor("#6c6c6c");
    const int padding = 4;
    const QRect contentRect = ensureOddContentRect(pix, padding);
    
    QPen pen(iconColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    
    // 统一外边框（与其他图标一致）
    p.drawRect(contentRect);
    
    const int w = contentRect.width();
    const int h = contentRect.height();
    const int left = contentRect.left();
    const int top = contentRect.top();
    const int right = contentRect.right();
    const int bottom = contentRect.bottom();
    
    // 开放的正方形（右侧开口）
    const int openWidth = w * 0.15; // 开口宽度
    p.drawLine(left, top, right - openWidth, top); // 上边
    p.drawLine(left, top, left, bottom); // 左边
    p.drawLine(left, bottom, right, bottom); // 下边
    
    // 内部箭头：水平指向右侧
    const int centerY = (top + bottom) / 2;
    const int arrowStartX = left + w * 0.25;
    const int arrowEndX = right - openWidth * 0.5;
    const int arrowY = centerY;
    
    // 箭头主体
    p.drawLine(arrowStartX, arrowY, arrowEndX, arrowY);
    
    // 箭头头部
    const int arrowHeadSize = w * 0.08;
    p.drawLine(arrowEndX, arrowY, arrowEndX - arrowHeadSize, arrowY - arrowHeadSize);
    p.drawLine(arrowEndX, arrowY, arrowEndX - arrowHeadSize, arrowY + arrowHeadSize);
    
    // 左侧文档元素（弯曲的垂直线）
    const int docX = left + w * 0.15;
    const int docTop = top + h * 0.2;
    const int docBottom = bottom - h * 0.2;
    const int docCurve = w * 0.03; // 弯曲程度
    
    // 主垂直线
    p.drawLine(docX, docTop, docX, docBottom);
    // 弯曲部分
    p.drawLine(docX, docTop, docX + docCurve, docTop + h * 0.1);
    p.drawLine(docX, docBottom, docX + docCurve, docBottom - h * 0.1);

    p.end();
    return QIcon(pix);
}

QIcon MainWindow::createPrintIcon() const
{
    const int S = getIconSize();
    QPixmap pix(S, S);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    // 高质量渲染
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    // 完全按照limereader的方式
    const QColor iconColor("#6c6c6c");
    const int padding = 4;
    const QRect contentRect = ensureOddContentRect(pix, padding);
    
    QPen pen(iconColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    
    // 标准的打印机图标（侧视图）
    int w = contentRect.width();
    int h = contentRect.height();
    int left = contentRect.left();
    int top = contentRect.top();
    int right = contentRect.right();
    int bottom = contentRect.bottom();
    
    // 打印机主体（中间部分）
    int printerTop = top + h * 0.3;
    int printerBottom = top + h * 0.65;
    p.drawRect(left, printerTop, w, printerBottom - printerTop);
    
    // 纸张上部（从打印机顶部伸出）
    int paperWidth = w * 0.6;
    int paperLeft = left + (w - paperWidth) / 2;
    p.drawLine(paperLeft, top, paperLeft, printerTop);
    p.drawLine(paperLeft + paperWidth, top, paperLeft + paperWidth, printerTop);
    p.drawLine(paperLeft, top, paperLeft + paperWidth, top);
    
    // 纸张下部（从打印机底部伸出）
    p.drawLine(paperLeft, printerBottom, paperLeft, bottom);
    p.drawLine(paperLeft + paperWidth, printerBottom, paperLeft + paperWidth, bottom);
    p.drawLine(paperLeft, bottom, paperLeft + paperWidth, bottom);
    
    // 打印机指示灯（小圆点）
    p.drawEllipse(right - 8, printerTop + 4, 3, 3);
    
    p.end();
    return QIcon(pix);
}

QIcon MainWindow::createHelpIcon() const
{
    qDebug() << "createHelpIcon() called - 创建灯泡图标 (Jony Ive风格)";
    const int S = getIconSize();
    QPixmap pix(S, S);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    // 高质量渲染
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // 最大化利用可绘制区域
    const QColor iconColor("#6c6c6c");
    const int padding = 2; // 最小padding，最大化可用空间
    const QRect contentRect = ensureOddContentRect(pix, padding);
    
    QPen pen(iconColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    
    // 灯泡设计 - 充满整个可绘制区域
    const QPoint center = contentRect.center();
    const int bulbRadius = qRound(qMin(contentRect.width(), contentRect.height()) * 0.4);
    
    // 灯泡主体 - 圆形
    p.drawEllipse(center, bulbRadius, bulbRadius);
    
    // 灯泡底部 - 螺纹部分
    const int threadHeight = qRound(bulbRadius * 0.3);
    const int threadY = center.y() + bulbRadius;
    QRect threadRect(center.x() - qRound(bulbRadius * 0.6), threadY, 
                     qRound(bulbRadius * 1.2), threadHeight);
    p.drawRect(threadRect);
    
    // 灯泡内部 - 发光效果（细线）
    QPen thinPen(iconColor, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(thinPen);
    
    // 内部发光线条 - 从中心向外辐射
    const int innerRadius = qRound(bulbRadius * 0.6);
    p.drawLine(center.x(), center.y() - innerRadius, center.x(), center.y() + innerRadius);
    p.drawLine(center.x() - innerRadius, center.y(), center.x() + innerRadius, center.y());
    p.drawLine(center.x() - qRound(innerRadius * 0.7), center.y() - qRound(innerRadius * 0.7), 
               center.x() + qRound(innerRadius * 0.7), center.y() + qRound(innerRadius * 0.7));
    p.drawLine(center.x() + qRound(innerRadius * 0.7), center.y() - qRound(innerRadius * 0.7), 
               center.x() - qRound(innerRadius * 0.7), center.y() + qRound(innerRadius * 0.7));
    
    p.end();
    qDebug() << "createHelpIcon() 完成，图标尺寸:" << S;
    return QIcon(pix);
}

QIcon MainWindow::createMainAppIcon() const
{
    qDebug() << "createMainAppIcon() called - 创建主程序图标 (Jony Ive风格)";
    const int S = 512; // 主程序图标使用高分辨率
    QPixmap pix(S, S);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    // 高质量渲染
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // macOS风格的设计 - 使用渐变和阴影
    const QRect fullRect(0, 0, S, S);
    
    // 背景渐变 - 从浅灰到深灰，增加立体感
    QLinearGradient bgGradient(0, 0, 0, S);
    bgGradient.setColorAt(0, QColor("#f5f5f5"));
    bgGradient.setColorAt(1, QColor("#e0e0e0"));
    p.setBrush(bgGradient);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(fullRect, S * 0.2, S * 0.2);
    
    // 主体设计 - 双栏结构，象征核心功能
    const int margin = S / 8;
    const int contentWidth = S - 2 * margin;
    const int contentHeight = S - 2 * margin;
    const QRect contentRect(margin, margin, contentWidth, contentHeight);
    
    // 双栏布局 - 使用黄金比例
    const int columnWidth = qRound(contentWidth * 0.45);
    const int gap = qRound(contentWidth * 0.1);
    
    // 左栏
    QRect leftColumn(contentRect.left(), contentRect.top(), columnWidth, contentHeight);
    QLinearGradient leftGradient(leftColumn.left(), leftColumn.top(), leftColumn.right(), leftColumn.bottom());
    leftGradient.setColorAt(0, QColor("#4a90e2"));
    leftGradient.setColorAt(1, QColor("#357abd"));
    p.setBrush(leftGradient);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(leftColumn, S * 0.05, S * 0.05);
    
    // 右栏
    QRect rightColumn(leftColumn.right() + gap, contentRect.top(), columnWidth, contentHeight);
    QLinearGradient rightGradient(rightColumn.left(), rightColumn.top(), rightColumn.right(), rightColumn.bottom());
    rightGradient.setColorAt(0, QColor("#7b68ee"));
    rightGradient.setColorAt(1, QColor("#6a5acd"));
    p.setBrush(rightGradient);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rightColumn, S * 0.05, S * 0.05);
    
    // 内容线条 - 象征文档内容
    p.setPen(QPen(QColor("#ffffff"), S * 0.008, Qt::SolidLine, Qt::RoundCap));
    const int lineSpacing = S * 0.03;
    const int lineMargin = S * 0.02;
    
    // 左栏内容线条
    for (int i = 0; i < 8; i++) {
        int y = leftColumn.top() + lineMargin + i * lineSpacing;
        if (y < leftColumn.bottom() - lineMargin) {
            p.drawLine(leftColumn.left() + lineMargin, y, 
                      leftColumn.right() - lineMargin, y);
        }
    }
    
    // 右栏内容线条
    for (int i = 0; i < 8; i++) {
        int y = rightColumn.top() + lineMargin + i * lineSpacing;
        if (y < rightColumn.bottom() - lineMargin) {
            p.drawLine(rightColumn.left() + lineMargin, y, 
                      rightColumn.right() - lineMargin, y);
        }
    }
    
    // 连接线 - 象征双栏的关联性
    p.setPen(QPen(QColor("#ffffff"), S * 0.012, Qt::SolidLine, Qt::RoundCap));
    const int centerY = contentRect.center().y();
    p.drawLine(leftColumn.right(), centerY, rightColumn.left(), centerY);
    
    // 高光效果 - 增加立体感
    QLinearGradient highlightGradient(0, 0, 0, S);
    highlightGradient.setColorAt(0, QColor("#ffffff"));
    highlightGradient.setColorAt(0.3, QColor("#ffffff"));
    highlightGradient.setColorAt(0.3, QColor(255, 255, 255, 0));
    p.setBrush(highlightGradient);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(fullRect, S * 0.2, S * 0.2);
    
    p.end();
    qDebug() << "createMainAppIcon() 完成，图标尺寸:" << S;
    return QIcon(pix);
}

QIcon MainWindow::createExtandIcon() const
{
    qDebug() << "createShrinkIcon() called - 创建减少边距图标 (Jony Ive风格)";
    const int S = getIconSize();
    QPixmap pix(S, S);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    // 高质量渲染
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // 外部矩形边框 - 标准颜色
    const QColor borderColor("#6c6c6c");
    const int padding = 4;
    const QRect contentRect = ensureOddContentRect(pix, padding);
    
    QPen borderPen(borderColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(borderPen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(contentRect);
    
    // 内部正方形 - 大尺寸（减少边距 = 更少空白）
    // 正方形大小为外框的70%，确保一眼就能识别
    const int innerSize = qRound(contentRect.width() * 0.7);
    const QPoint center = contentRect.center();
    const int halfSize = qRound(innerSize / 2.0);
    const QRect innerRect(center.x() - halfSize, center.y() - halfSize, 
                          innerSize, innerSize);
    
    // 内部正方形 - 颜色深度低两个度（更浅）
    const QColor innerColor("#9c9c9c"); // 比#6c6c6c浅两个度
    QPen innerPen(innerColor, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(innerPen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(innerRect);
    
    p.end();
    qDebug() << "createShrinkIcon() 完成，图标尺寸:" << S;
    return QIcon(pix); 
}


QIcon MainWindow::createShrinkIcon() const
{
    qDebug() << "createExtandIcon() called - 创建增加边距图标 (Jony Ive风格)";
    const int S = getIconSize();
    QPixmap pix(S, S);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    // 高质量渲染
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // 外部矩形边框 - 标准颜色
    const QColor borderColor("#6c6c6c");
    const int padding = 4;
    const QRect contentRect = ensureOddContentRect(pix, padding);
    
    QPen borderPen(borderColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(borderPen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(contentRect);
    
    // 内部正方形 - 小尺寸（增加边距 = 更多空白）
    // 正方形大小为外框的40%，确保一眼就能识别
    const int innerSize = qRound(contentRect.width() * 0.4);
    const QPoint center = contentRect.center();
    const int halfSize = qRound(innerSize / 2.0);
    const QRect innerRect(center.x() - halfSize, center.y() - halfSize, 
                          innerSize, innerSize);
    
    // 内部正方形 - 颜色深度低两个度（更浅）
    const QColor innerColor("#9c9c9c"); // 比#6c6c6c浅两个度
    QPen innerPen(innerColor, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(innerPen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(innerRect);
    
    p.end();
    qDebug() << "createExtandIcon() 完成，图标尺寸:" << S;
    return QIcon(pix);
}


QIcon MainWindow::createZoomInIcon() const
{
    const int S = getIconSize();
    QPixmap pix(S, S);
    pix.fill(Qt::transparent);
    
    const QColor iconColor("#6c6c6c");
    const int padding = 4;
    
    // 使用新的居中绘制方法
    drawCenteredIcon(pix, iconColor, padding, [&](QPainter& p, const QRect& contentRect) {
        QPen borderPen(iconColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(borderPen);
        p.setBrush(Qt::NoBrush);
        
        // 统一的外边框
        p.drawRect(contentRect);

        // 内部线条使用更细的笔触
        QPen thinPen(iconColor, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(thinPen);

        // 简洁的"+"号
        const QPoint center = contentRect.center();
        const int len = qRound(qMin(contentRect.width(), contentRect.height()) * 0.28);
        p.drawLine(QPoint(center.x() - len, center.y()), QPoint(center.x() + len, center.y()));
        p.drawLine(QPoint(center.x(), center.y() - len), QPoint(center.x(), center.y() + len));
    });
    
    return QIcon(pix);
}

QIcon MainWindow::createZoomOutIcon() const
{
    const int S = getIconSize();
    QPixmap pix(S, S);
    pix.fill(Qt::transparent);
    
    const QColor iconColor("#6c6c6c");
    const int padding = 4;
    
    // 使用新的居中绘制方法
    drawCenteredIcon(pix, iconColor, padding, [&](QPainter& p, const QRect& contentRect) {
        QPen borderPen(iconColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(borderPen);
        p.setBrush(Qt::NoBrush);

        // 统一的外边框
        p.drawRect(contentRect);

        // 内部线条使用更细的笔触
        QPen thinPen(iconColor, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(thinPen);

        // 简洁的"-"号
        const QPoint center = contentRect.center();
        const int len = qRound(qMin(contentRect.width(), contentRect.height()) * 0.28);
        p.drawLine(QPoint(center.x() - len, center.y()), QPoint(center.x() + len, center.y()));
    });
    
    return QIcon(pix);
}

QIcon MainWindow::createTwoColsIcon() const
{
    const int S = getIconSize();
    QPixmap pix(S, S);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    // 高质量渲染
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    // 完全按照limereader的方式
    const QColor iconColor("#6c6c6c");
    const int padding = 4;
    const QRect contentRect = ensureOddContentRect(pix, padding);
    
    QPen pen(iconColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    
    // 复制limereader的drawTwoColumns逻辑
    // 绘制A4纸外框轮廓
    p.setPen(QPen(iconColor, 2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(contentRect, 2, 2);

    // 绘制内部2x2布局（列优先排列）
    QRect innerContentRect = contentRect.adjusted(4, 3, -4, -4);
    int colSpacing = 3;
    int rowSpacing = 1;
    int colWidth = (innerContentRect.width() - colSpacing) / 2;
    int rowHeight = (innerContentRect.height() + rowSpacing) / 2;

    p.setBrush(iconColor);
    p.setPen(Qt::NoPen);

    // 列优先布局：左列（位置1,2），右列（位置3,4）
    p.drawRect(innerContentRect.x(), innerContentRect.y(), colWidth, rowHeight);                     // 位置1
    p.drawRect(innerContentRect.x(), innerContentRect.y() + rowHeight + rowSpacing, colWidth, rowHeight); // 位置2
    p.drawRect(innerContentRect.x() + colWidth + colSpacing, innerContentRect.y(), colWidth, rowHeight);     // 位置3
    p.drawRect(innerContentRect.x() + colWidth + colSpacing, innerContentRect.y() + rowHeight + rowSpacing, colWidth, rowHeight); // 位置4
    
    p.end();
    return QIcon(pix);
}

QIcon MainWindow::createPageNumIcon() const
{
    const int S = getIconSize();
    QPixmap pix(S, S);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    // 高质量渲染
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    // 完全按照limereader的方式
    const QColor iconColor("#6c6c6c");
    const int padding = 4;
    const QRect contentRect = ensureOddContentRect(pix, padding);
    
    QPen pen(iconColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    
    // 绘制页面矩形
    p.drawRect(contentRect);
    
    // 页码数字
    QFont font;
    font.setPixelSize(contentRect.height() * 0.6);
    font.setBold(true);
    p.setFont(font);
    p.drawText(contentRect, Qt::AlignCenter, "1");
    
    p.end();
    return QIcon(pix);
}

QIcon MainWindow::createAutoCutIcon() const
{
    const int S = getIconSize();
    QPixmap pix(S, S);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    // 高质量渲染
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    // 完全按照limereader的方式
    const QColor iconColor("#6c6c6c");
    const int padding = 4;
    const QRect contentRect = ensureOddContentRect(pix, padding);
    
    QPen pen(iconColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    
    // 外图片框 - 各边12%
    // 简化的自动切边图标
    p.drawRect(contentRect);
    
    // 内部内容区域
    QRect innerRect = contentRect.adjusted(4, 4, -4, -4);
    p.fillRect(innerRect, QColor(iconColor.red(), iconColor.green(), iconColor.blue(), 80));
    
    // 四角标记线
    int markLen = 6;
    // 左上角
    p.drawLine(contentRect.left(), contentRect.top() + markLen, contentRect.left() + markLen, contentRect.top() + markLen);
    p.drawLine(contentRect.left() + markLen, contentRect.top(), contentRect.left() + markLen, contentRect.top() + markLen);
    
    // 右上角
    p.drawLine(contentRect.right() - markLen, contentRect.top() + markLen, contentRect.right(), contentRect.top() + markLen);
    p.drawLine(contentRect.right() - markLen, contentRect.top(), contentRect.right() - markLen, contentRect.top() + markLen);
    
    // 左下角
    p.drawLine(contentRect.left(), contentRect.bottom() - markLen, contentRect.left() + markLen, contentRect.bottom() - markLen);
    p.drawLine(contentRect.left() + markLen, contentRect.bottom() - markLen, contentRect.left() + markLen, contentRect.bottom());
    
    // 右下角
    p.drawLine(contentRect.right() - markLen, contentRect.bottom() - markLen, contentRect.right(), contentRect.bottom() - markLen);
    p.drawLine(contentRect.right() - markLen, contentRect.bottom() - markLen, contentRect.right() - markLen, contentRect.bottom());
    
    p.end();
    return QIcon(pix);
}

QIcon MainWindow::createExportIcon() const
{
    const int S = getIconSize();
    QPixmap pix(S, S);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    // 高质量渲染
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // 完全按照limereader的方式
    const QColor iconColor("#6c6c6c");
    const int padding = 4;
    const QRect contentRect = ensureOddContentRect(pix, padding);
    
    QPen pen(iconColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    
    // 绘制带缺口的矩形（去掉右上角）
    int w = contentRect.width();
    int h = contentRect.height();
    int left = contentRect.left();
    int top = contentRect.top();
    int right = contentRect.right();
    int bottom = contentRect.bottom();
    
    // 计算缺口位置：顶部边框的右半部分 + 右边边框的上半部分
    int topGapStart = left + w / 2;        // 顶部边框从中间开始断开
    int rightGapEnd = top + h / 2;         // 右边边框到中间结束
    
    // 绘制矩形边框，右上角有缺口
    p.drawLine(left, top, topGapStart, top);           // 上边（左半部分）
    p.drawLine(right, rightGapEnd, right, bottom);   // 右边（下半部分）
    p.drawLine(right, bottom, left, bottom);          // 下边
    p.drawLine(left, bottom, left, top);              // 左边
    
    // 绘制斜45度箭头指向缺口（从矩形中心开始）
    int centerX = contentRect.center().x();
    int centerY = contentRect.center().y();
    int arrowEndX = right;  
    int arrowEndY = top;    
    
    // 绘制箭头主线（从中心到右上角缺口）
    p.drawLine(centerX, centerY, arrowEndX, arrowEndY);
    
    // 绘制箭头头部（两个边的长度都是矩形边长的1/4）
    int arrowHeadSize = qMin(w, h) / 4;  // 矩形边长的1/4
    p.drawLine(arrowEndX, arrowEndY, 
        arrowEndX - arrowHeadSize, arrowEndY);
    p.drawLine(arrowEndX, arrowEndY, 
        arrowEndX, arrowEndY + arrowHeadSize);
    
    p.end();
    return QIcon(pix);
}

QIcon MainWindow::createUpdateIcon() const
{
    const int S = getIconSize();
    QPixmap pix(S, S);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    // 高质量渲染
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    // 完全按照limereader的方式
    const QColor iconColor("#6c6c6c");
    const int padding = 4;
    const QRect contentRect = ensureOddContentRect(pix, padding);
    
    QPen pen(iconColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    
    // 简化的更新图标 - 圆圈
    p.drawEllipse(contentRect);
    
    // 箭头指向右上
    int centerX = contentRect.center().x();
    int centerY = contentRect.center().y();
    int radius = contentRect.width() / 2 - 4;
    
    // 箭头
    p.drawLine(centerX + radius - 4, centerY - radius + 4, centerX + radius + 4, centerY - radius - 4);
    p.drawLine(centerX + radius + 4, centerY - radius - 4, centerX + radius, centerY - radius);
    p.drawLine(centerX + radius + 4, centerY - radius - 4, centerX + radius + 2, centerY - radius + 2);
    
    p.end();
    return QIcon(pix);
}


void MainWindow::updateLoginAction()
{
    if (LoginDialog::s_isTrialUser) {
        m_loginAction->setText(tr("登录"));
        m_loginAction->setStatusTip(tr("登录账户移除水印"));
    } else {
        m_loginAction->setText(tr("已登录"));
        m_loginAction->setStatusTip(tr("当前已登录，点击重新登录"));
    }
}

void MainWindow::createMenus()
{
    m_fileMenu = menuBar()->addMenu(tr("文件"));
    m_fileMenu->addAction(m_openAction);
    m_fileMenu->addAction(m_saveAction);
    m_fileMenu->addAction(m_printAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_loginAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_exitAction);

    m_editMenu = menuBar()->addMenu(tr("编辑"));
    m_editMenu->addAction(m_twoColsAction);
    m_editMenu->addAction(m_pageNumAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_extandAction);
    m_editMenu->addAction(m_shrinkAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_zoominAction);
    m_editMenu->addAction(m_zoomoutAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_autotrimAction);

    m_aboutMenu = menuBar()->addMenu(tr("帮助"));
    // 移除"检查更新"菜单项
    // m_aboutMenu->addAction(m_checkUpdateAction);
    // m_aboutMenu->addSeparator();
    QAction *aboutAct = new QAction(createHelpIcon(), tr("关于"), this);
    connect(aboutAct, &QAction::triggered, this, [this]() {
        AboutDialog dialog(this);
        dialog.setLanguage(m_lang);
        
        // 居中显示
        int wd = this->x() + (this->width() - dialog.width()) / 2;
        int ht = this->y() + (this->height() - dialog.height()) / 2;
        dialog.move(wd, ht);
        
        dialog.exec();
    });
    m_aboutMenu->addAction(aboutAct);

}

void MainWindow::createToolbars()
{
    m_mainToolbar = addToolBar(tr("main"));
    m_mainToolbar->setObjectName("mainToolbar"); // 设置objectName以避免saveState()警告
    // 登录按钮放在最左边
    m_mainToolbar->addAction(m_loginAction);
    m_mainToolbar->addSeparator();
    m_mainToolbar->addAction(m_openAction);
    m_mainToolbar->addAction(m_saveAction);
    m_mainToolbar->addAction(m_printAction);
    m_mainToolbar->addSeparator();
    m_mainToolbar->addAction(m_twoColsAction);
    m_mainToolbar->addAction(m_pageNumAction);
    m_mainToolbar->addSeparator();
    m_mainToolbar->addAction(m_autotrimAction);
    m_mainToolbar->addSeparator();
    m_mainToolbar->addAction(m_extandAction);
    m_mainToolbar->addAction(m_shrinkAction);
    m_mainToolbar->addSeparator();
    m_mainToolbar->addAction(m_zoominAction);
    m_mainToolbar->addAction(m_zoomoutAction);
    
    // 强制重新设置图标，确保使用代码绘制的图标
    qDebug() << "重新设置extand和shrink图标";
    m_extandAction->setIcon(createExtandIcon());
    m_shrinkAction->setIcon(createShrinkIcon());
    qDebug() << "图标重新设置完成";
}
void MainWindow::onAutoTrimClicked()
{
    if (m_pixmap.isNull()) return;
    QImage img = m_pixmap.toImage().convertToFormat(QImage::Format_Grayscale8);

    // 1) 计算 Otsu 全局阈值（自适应光照）
    int hist[256] = {0};
    const int w = img.width();
    const int h = img.height();
    for (int y = 0; y < h; ++y) {
        const uchar* row = img.constScanLine(y);
        for (int x = 0; x < w; ++x) hist[row[x]]++;
    }
    long long total = (long long)w * h;
    long long sumAll = 0;
    for (int i = 0; i < 256; ++i) sumAll += (long long)i * hist[i];
    long long sumB = 0; long long wB = 0; double maxVar = -1.0; int otsuT = 200;
    for (int t = 0; t < 256; ++t) {
        wB += hist[t]; if (wB == 0) continue;
        long long wF = total - wB; if (wF == 0) break;
        sumB += (long long)t * hist[t];
        double mB = (double)sumB / wB;
        double mF = (double)(sumAll - sumB) / wF;
        double varBetween = (double)wB * (double)wF * (mB - mF) * (mB - mF);
        if (varBetween > maxVar) { maxVar = varBetween; otsuT = t; }
    }

    // 2) 以 Otsu 阈值为"内容像素"，行/列若内容像素比例高于极小阈值则判为非空白
    auto hasContentRow = [&](int y){
        const uchar *p = img.constScanLine(y);
        int cnt = 0;
        for(int x=0;x<w;x++) if (p[x] <= otsuT) { cnt++; if (cnt > w/1000 + 1) return true; }
        return false;
    };
    auto hasContentCol = [&](int x){
        int cnt = 0;
        for(int y=0;y<h;y++) { const uchar* p = img.constScanLine(y); if (p[x] <= otsuT) { cnt++; if (cnt > h/1000 + 1) return true; } }
        return false;
    };

    int left=0,right=w-1,top=0,bottom=h-1;
    while(top<=bottom && !hasContentRow(top)) top++;
    while(bottom>=top && !hasContentRow(bottom)) bottom--;
    while(left<=right && !hasContentCol(left)) left++;
    while(right>=left && !hasContentCol(right)) right--;
    if(left<right && top<bottom){
        QRect r(left, top, right-left+1, bottom-top+1);
        QImage cropped = m_pixmap.toImage().copy(r);
        m_pixmap = QPixmap::fromImage(cropped);
        buildScenesFromPixmap();
    }else{
        m_popup->setPopupText(tr("未检测到可裁剪的空白边"));
        m_popup->show();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
    qDebug() << "main window dragEnterEvent";
}


void MainWindow::dropEvent(QDropEvent *event)
{
    QString name = event->mimeData()->urls().first().toString();
    qDebug() << name;
    #ifdef Q_OS_WIN
        QString indecate = "file:///";
    #endif

    #ifdef Q_OS_LINUX
        QString indecate = "file://";
    #endif

    #ifdef Q_OS_MAC
        QString indecate = "file://";
    #endif

    if(name.startsWith(indecate)){
        QString fileName = name.mid(indecate.length());
        qDebug() << fileName;
        loadFile(fileName);
    }


}


void MainWindow::loadLicense()
{
//    QUrl url(QString("https://ssdlugnu.lc-cn-n1-shared.com/1.1/classes/users?where={\"hostid\":\"%1\"}").arg(machineIDHash()));
    QUrl url(QString("https://api.limereader.com/users/?hostid=%1").arg(machineIDHash()));
    if(m_lang == "zh")
        url = QString("https://api.ctdy123.com/users/?hostid=%1").arg(machineIDHash());
    Get(url);
}

void MainWindow::Get(QUrl url)
{
    m_url = url;
    QNetworkRequest request(m_url);
    request.setRawHeader("X-LC-Id", "ssdLugNURpzlMY79mprVvjYk-gzGzoHsz");
    request.setRawHeader("X-LC-Key", "gpHDAGf4thHD1e5KFdWh502n");
    request.setRawHeader("Content-Type","application/json");
//    m_netReply = m_netAccMan.get(request);
    //qDebug() << m_url.toString();
    if(m_netReply != Q_NULLPTR)
    {
        m_netReply->deleteLater();
    }
    m_netReply = m_netAccMan.get(request);

    connect(m_netReply, &QNetworkReply::finished, this, &MainWindow::QueryFinished);

}

void MainWindow::QueryFinished()
{
    QByteArray bytes = m_netReply->readAll();
    const QVariant redirectionTarget = m_netReply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    m_netReply->deleteLater();
    m_netReply = Q_NULLPTR;
    if (!redirectionTarget.isNull()) {//如果网址跳转重新请求
        const QUrl redirectedUrl = m_url.resolved(redirectionTarget.toUrl());
        qDebug()<<"redirectedUrl:"<<redirectedUrl.url();
        Get(redirectedUrl);
        return;
    }
    //qDebug()<<"license query finished";
    QString html_text = bytes;
    //qDebug()<<"get returned:"<<html_text;

    QJsonParseError jsonError;
    QJsonDocument document = QJsonDocument::fromJson(bytes, &jsonError); //转化为JSON文档
    if( !document.isNull() && (jsonError.error == QJsonParseError::NoError)) //解析未发生错误
    {
        QJsonObject root_obj = document.object();
        QVariantMap root_map = root_obj.toVariantMap();
//        QVariantList results_list = root_map["results"].toList();
        QVariantList results_list = root_map["resources"].toList();
        if(results_list.size() > 0){
            QVariantMap results_map = results_list[0].toMap();
            qDebug()<< results_map["uuid"].toString();
            qDebug()<< results_map["objectId"].toString();
            qDebug()<< results_map["hostid"].toString();
            m_uuid = results_map["uuid"].toString();
        }
        if(m_uuid.length() == 36){
            QString wt = windowTitle();
            if(m_lang == "zh"){
                wt = wt + "~~[~-~~~已~~~~注~~~~册~~~-~]~~";
            }
            else{
                wt = wt + "~~[~-~R~~e~~g~~i~~s~~t~~e~~r~~e~~d~-~]~~";
            }
            wt = wt.replace("~", "");
            setWindowTitle(wt);
        }
        else{
            QString wt = windowTitle();
            if(m_lang == "zh"){
                wt = wt + "~~[~~-~~未~~~~注~~~~册~~-~~]~~";
            }
            else{
                wt = wt + "~~[~-~U~~n~~r~~e~~g~~i~~s~~t~~e~~r~~e~~d~-~]~~";
            }
            wt = wt.replace("~", "");
            setWindowTitle(wt);
        }
    }
}

//图像二值化
void MainWindow::Threshold_pro(QImage *inputImage,QImage &outputImage,uint8_t bThres)
{
    uint8_t bt;
    QColor oldColor;

    for(int i = 0; i<inputImage->height(); i++)
    {
        for(int j = 0; j<inputImage->width(); j++)
        {
            oldColor = QColor(inputImage->pixel(j,i));
            bt = oldColor.red();

            if(bt<bThres)
            {
                bt = 0;
            }else {
                bt = 255;
            }
            outputImage.setPixel(j,i,qRgb(bt, bt, bt));

        }
    }
}

void MainWindow::onCheckUpdateClicked()
{
    if (m_updater) {
        m_updater->checkForUpdatesNotSilent();
    } else {
        QMessageBox::warning(this, tr("检查更新"), tr("更新器未初始化"));
    }
}

// 窗口几何管理实现
void MainWindow::initializeWindowGeometry()
{
    // 尝试恢复保存的窗口几何信息
    if (!restoreWindowGeometry()) {
        // 如果没有保存的信息或恢复失败，使用默认设置
        QRect defaultGeometry = getDefaultWindowGeometry();
        setGeometry(defaultGeometry);
    }
    
    // 恢复分割条状态
    restoreSplitterState();
}

void MainWindow::saveWindowGeometry()
{
    QSettings settings("LoimReader", "Settings");
    settings.setValue("windowGeometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

bool MainWindow::restoreWindowGeometry()
{
    QSettings settings("LoimReader", "Settings");
    
    QByteArray geometryData = settings.value("windowGeometry").toByteArray();
    QByteArray state = settings.value("windowState").toByteArray();
    
    if (!geometryData.isEmpty()) {
        restoreGeometry(geometryData);
        restoreState(state);
        
        // 验证窗口几何是否有效
        QRect currentGeometry = this->geometry();
        if (!isWindowGeometryValid(currentGeometry)) {
            // 如果几何无效，使用默认设置
            QRect defaultGeometry = getDefaultWindowGeometry();
            setGeometry(defaultGeometry);
        }
        return true;
    }
    
    return false;
}

void MainWindow::saveSplitterState()
{
    QSettings settings("LoimReader", "Settings");
    settings.setValue("splitterState", m_splitter->saveState());
}

void MainWindow::restoreSplitterState()
{
    QSettings settings("LoimReader", "Settings");
    QByteArray splitterState = settings.value("splitterState").toByteArray();
    
    if (!splitterState.isEmpty()) {
        m_splitter->restoreState(splitterState);
    } else {
        // 首次启动，设置默认分割比例（左右各占一半 50:50）
        QList<int> defaultSizes;
        int totalWidth = width();
        defaultSizes << totalWidth / 2 << totalWidth / 2;
        m_splitter->setSizes(defaultSizes);
    }
}

bool MainWindow::isWindowGeometryValid(const QRect& geometry)
{
    // 检查窗口是否太小
    if (geometry.width() < 400 || geometry.height() < 300) {
        return false;
    }
    
    // 检查窗口是否在屏幕范围内
    QScreen* screen = QGuiApplication::screenAt(geometry.center());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        // 检查窗口是否完全在屏幕外
        if (!screenGeometry.intersects(geometry)) {
            return false;
        }
    }
    
    return true;
}

QRect MainWindow::getDefaultWindowGeometry()
{
    QScreen* screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    
    // 计算屏幕的2/3大小
    int width = screenGeometry.width() * 2 / 3;
    int height = screenGeometry.height() * 2 / 3;
    
    // 计算居中位置
    int x = screenGeometry.x() + (screenGeometry.width() - width) / 2;
    int y = screenGeometry.y() + (screenGeometry.height() - height) / 2;
    
    return QRect(x, y, width, height);
}

void MainWindow::moveEvent(QMoveEvent* event)
{
    QMainWindow::moveEvent(event);
    // 窗口移动时保存几何信息
    saveWindowGeometry();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // 关闭窗口时保存几何信息
    saveWindowGeometry();
    saveSplitterState();
    QMainWindow::closeEvent(event);
}

// 处理版本信息
void MainWindow::onVersionInfoReceived(const QString& latestVersion, bool hasUpdate,
                                      const QString& updateUrl,
                                      const QString& updateLog,
                                      const QString& checksumMD5,
                                      const QString& updateSize)
{
    qDebug() << "";
    qDebug() << "=================================================================";
    qDebug() << "=== [MainWindow] onVersionInfoReceived 被调用 ===";
    qDebug() << "=================================================================";
    qDebug() << "[MainWindow] 参数 - 最新版本:" << latestVersion;
    qDebug() << "[MainWindow] 参数 - 有更新:" << hasUpdate;
    qDebug() << "[MainWindow] 参数 - 更新URL:" << updateUrl;
    qDebug() << "[MainWindow] 参数 - 更新日志长度:" << updateLog.length();
    qDebug() << "[MainWindow] 参数 - MD5:" << checksumMD5;
    qDebug() << "[MainWindow] 参数 - 文件大小:" << updateSize;
    
    QString currentVersion = LoimReader::AppVersion::getAppVersion();
    qDebug() << "[MainWindow] 当前版本:" << currentVersion;
    
    if (hasUpdate) {
        qDebug() << "[MainWindow] ✅ 检测到有更新，开始更新UI...";
        
        // 有更新，显示版本升级信息（参考limereader风格）
        QString versionText = QString("v%1→v%2").arg(currentVersion, latestVersion);
        qDebug() << "[MainWindow] 版本文本:" << versionText;
        
        m_updateLabel->setText(versionText);
        m_updateLabel->setStyleSheet(
            "QLabel { "
            "    color: #FF9F0A; "  // 淡黄色 - 表示有更新
            "    font-size: 12px; "
            "    font-weight: 500; "
            "    padding: 2px 6px; "
            "    background-color: rgba(255, 159, 10, 0.1); "
            "    border-radius: 4px; "
            "}"
        );
        m_updateLabel->setToolTip(tr("当前版本: %1\n服务器版本: %2\n点击右侧图标下载更新").arg(currentVersion, latestVersion));
        m_updateLabel->setVisible(true);
        qDebug() << "[MainWindow] 更新标签已设置，可见性:" << m_updateLabel->isVisible();
        qDebug() << "[MainWindow] 更新标签文本:" << m_updateLabel->text();
        
        // 显示更新按钮
        m_updateButton->setVisible(true);
        qDebug() << "[MainWindow] 更新按钮已显示，可见性:" << m_updateButton->isVisible();
        
        // 在状态栏显示更新信息
        statusBar()->showMessage(tr("发现新版本 v%1 可用，点击右侧按钮下载").arg(latestVersion), 5000);
        
        // 存储完整的版本信息
        m_latestVersion = latestVersion;
        m_downloadUrl = updateUrl.isEmpty() ? 
            QString("https://limereader-releases.oss-cn-hangzhou.aliyuncs.com/updates/loimreader/LoimReader_v%1_macOS.dmg").arg(latestVersion) : 
            updateUrl;
        m_releaseNotes = updateLog;
        m_checksumMD5 = checksumMD5;
        m_updateSize = updateSize;
        m_hasUpdate = true;
        
        qDebug() << "[MainWindow] 📥 下载地址:" << m_downloadUrl;
        qDebug() << "[MainWindow] 📝 更新日志:" << (m_releaseNotes.isEmpty() ? "无" : QString::number(m_releaseNotes.length()) + " 字符");
        qDebug() << "[MainWindow] 🔐 MD5:" << (m_checksumMD5.isEmpty() ? "无" : m_checksumMD5.left(16) + "...");
        qDebug() << "[MainWindow] 📦 文件大小:" << (m_updateSize.isEmpty() ? "未知" : m_updateSize);
        qDebug() << "[MainWindow] 状态栏消息已显示";
        qDebug() << "=================================================================";
        qDebug() << "=== [MainWindow] 更新UI完成 ===";
        qDebug() << "=================================================================";
    } else {
        qDebug() << "[MainWindow] ℹ️  没有更新，显示当前版本";
        
        // 没有更新，显示当前版本（灰色）
        m_updateLabel->setText(QString("v%1").arg(currentVersion));
        m_updateLabel->setStyleSheet(
            "QLabel { "
            "    color: #8E8E93; "  // 平时灰色
            "    font-size: 12px; "
            "    font-weight: 500; "
            "    padding: 2px 6px; "
            "    background-color: rgba(142, 142, 147, 0.1); "
            "    border-radius: 4px; "
            "}"
        );
        m_updateLabel->setToolTip(tr("当前版本: %1（已是最新）").arg(currentVersion));
        m_updateLabel->setVisible(true);
        qDebug() << "[MainWindow] 版本标签已设置（无更新），可见性:" << m_updateLabel->isVisible();
        qDebug() << "[MainWindow] 版本标签文本:" << m_updateLabel->text();
        
        // 隐藏更新按钮
        m_updateButton->setVisible(false);
        qDebug() << "[MainWindow] 更新按钮已隐藏";
        
        // 在状态栏显示信息
        statusBar()->showMessage(tr("当前已是最新版本"), 3000);
        
        m_hasUpdate = false;
        qDebug() << "=================================================================";
        qDebug() << "=== [MainWindow] onVersionInfoReceived 完成（无更新）===";
        qDebug() << "=================================================================";
    }
}

// 创建状态栏更新图标（小尺寸）
QIcon MainWindow::createStatusBarUpdateIcon() const
{
    const int S = 20;  // 状态栏图标尺寸较小
    QPixmap pix(S, S);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    // 高质量渲染
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // 使用橙色表示有更新
    QPen pen(QColor(255, 159, 10));  // #FF9F0A 淡黄色/橙色
    pen.setWidth(2);
    p.setPen(pen);
    
    // 下载箭头 - 向下
    // 箭头杆
    p.drawLine(QPointF(S/2, 4), QPointF(S/2, S-6));
    
    // 箭头头部
    QPolygonF arrowHead;
    arrowHead << QPointF(S/2-4, S-10) 
              << QPointF(S/2, S-6) 
              << QPointF(S/2+4, S-10);
    p.drawPolyline(arrowHead);
    
    // 底部横线（表示下载到本地）
    p.drawLine(QPointF(4, S-3), QPointF(S-4, S-3));
    
    p.end();
    return QIcon(pix);
}

// 显示更新对话框
void MainWindow::showUpdateDialog()
{
    qDebug() << "";
    qDebug() << "=================================================================";
    qDebug() << "=== [MainWindow] showUpdateDialog 被调用 ===";
    qDebug() << "=================================================================";
    qDebug() << "[MainWindow] 有更新标志:" << m_hasUpdate;
    
    if (!m_hasUpdate) {
        qDebug() << "[MainWindow] ⚠️  没有可用更新";
        QMessageBox::information(this, tr("检查更新"), tr("当前已是最新版本"));
        return;
    }
    
    // 构建更新信息 - 使用DownloadUpdateDialog的UpdateInfo
    DownloadUpdateDialog::UpdateInfo updateInfo;
    updateInfo.latestVersion = m_latestVersion;
    updateInfo.currentVersion = LoimReader::AppVersion::getAppVersion();
    updateInfo.updateUrl = m_downloadUrl;
    updateInfo.updateLog = m_releaseNotes;
    updateInfo.checksumMD5 = m_checksumMD5;
    updateInfo.updateSize = m_updateSize;
    updateInfo.forceUpdate = false;
    updateInfo.hasUpdate = true;
    
    qDebug() << "[MainWindow] 更新信息:";
    qDebug() << "[MainWindow]   当前版本:" << updateInfo.currentVersion;
    qDebug() << "[MainWindow]   最新版本:" << updateInfo.latestVersion;
    qDebug() << "[MainWindow]   下载URL:" << updateInfo.updateUrl;
    qDebug() << "[MainWindow]   文件大小:" << updateInfo.updateSize;
    qDebug() << "[MainWindow]   MD5:" << updateInfo.checksumMD5;
    
    // 创建并显示更新对话框
    m_updateDialog = new DownloadUpdateDialog(updateInfo, this);
    
    qDebug() << "[MainWindow] 更新对话框已创建";
    
    // 连接下载相关信号
    connect(m_updateDialog, &DownloadUpdateDialog::requestStartDownload, this, &MainWindow::startDownload);
    
    // 居中显示
    int x = this->x() + (this->width() - m_updateDialog->width()) / 2;
    int y = this->y() + (this->height() - m_updateDialog->height()) / 2;
    m_updateDialog->move(x, y);
    
    qDebug() << "[MainWindow] 准备显示更新对话框";
    m_updateDialog->exec();
    qDebug() << "[MainWindow] 更新对话框已关闭，结果:" << m_updateDialog->getResult();
    
    // 清理对话框
    m_updateDialog->deleteLater();
    m_updateDialog = nullptr;
    
    qDebug() << "=================================================================";
    qDebug() << "=== [MainWindow] showUpdateDialog 完成 ===";
    qDebug() << "=================================================================";
}

// 开始下载更新文件
void MainWindow::startDownload()
{
    qDebug() << "";
    qDebug() << "=================================================================";
    qDebug() << "=== [MainWindow] startDownload 被调用 ===";
    qDebug() << "=================================================================";
    qDebug() << "[MainWindow] 下载URL:" << m_downloadUrl;
    
    if (m_downloadUrl.isEmpty()) {
        qDebug() << "[MainWindow] ❌ 下载URL为空";
        if (m_updateDialog) {
            m_updateDialog->onDownloadError(tr("下载地址无效"));
        }
        return;
    }
    
    // 如果有正在进行的下载，先取消
    if (m_downloadReply) {
        qDebug() << "[MainWindow] 取消之前的下载";
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }
    
    // 确定保存路径
    QString fileName = QFileInfo(m_downloadUrl).fileName();
    if (fileName.isEmpty()) {
        fileName = QString("LoimReader_v%1_macOS.dmg").arg(m_latestVersion);
    }
    QString savePath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/" + fileName;
    
    qDebug() << "[MainWindow] 保存路径:" << savePath;
    
    // 发起下载请求
    QNetworkRequest request(m_downloadUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, LoimReader::AppVersion::getUserAgent());
    
    m_downloadReply = m_downloadManager->get(request);
    
    // 连接信号
    connect(m_downloadReply, &QNetworkReply::downloadProgress, this, &MainWindow::onDownloadProgress);
    connect(m_downloadReply, &QNetworkReply::finished, this, &MainWindow::onDownloadFinished);
    connect(m_downloadReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &MainWindow::onDownloadError);
    
    // 保存文件路径供下载完成后使用
    m_downloadReply->setProperty("savePath", savePath);
    
    qDebug() << "[MainWindow] ✅ 下载请求已发送";
    qDebug() << "=================================================================";
}

// 下载进度更新
void MainWindow::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (!m_updateDialog) return;
    
    int progress = 0;
    if (bytesTotal > 0) {
        progress = static_cast<int>((bytesReceived * 100) / bytesTotal);
    }
    
    m_updateDialog->onDownloadProgress(progress, bytesReceived, bytesTotal);
}

// 下载完成
void MainWindow::onDownloadFinished()
{
    qDebug() << "";
    qDebug() << "=================================================================";
    qDebug() << "=== [MainWindow] onDownloadFinished 被调用 ===";
    qDebug() << "=================================================================";
    
    if (!m_downloadReply) {
        qDebug() << "[MainWindow] ❌ downloadReply 为空";
        return;
    }
    
    // 检查错误
    if (m_downloadReply->error() != QNetworkReply::NoError) {
        qDebug() << "[MainWindow] ❌ 下载出错:" << m_downloadReply->errorString();
        if (m_updateDialog) {
            m_updateDialog->onDownloadError(m_downloadReply->errorString());
        }
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
        return;
    }
    
    // 获取保存路径
    QString savePath = m_downloadReply->property("savePath").toString();
    qDebug() << "[MainWindow] 保存路径:" << savePath;
    
    // 读取数据并保存到文件
    QByteArray data = m_downloadReply->readAll();
    qDebug() << "[MainWindow] 下载数据大小:" << data.size() << "字节";
    
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "[MainWindow] ❌ 无法打开文件:" << savePath;
        if (m_updateDialog) {
            m_updateDialog->onDownloadError(tr("无法保存文件: %1").arg(file.errorString()));
        }
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
        return;
    }
    
    file.write(data);
    file.close();
    
    qDebug() << "[MainWindow] ✅ 文件已保存:" << savePath;
    
    // 通知对话框下载完成
    if (m_updateDialog) {
        m_updateDialog->onDownloadFinished(savePath);
    }
    
    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;
    
    qDebug() << "=================================================================";
    qDebug() << "=== [MainWindow] onDownloadFinished 完成 ===";
    qDebug() << "=================================================================";
}

// 下载错误
void MainWindow::onDownloadError(QNetworkReply::NetworkError error)
{
    qDebug() << "";
    qDebug() << "=================================================================";
    qDebug() << "=== [MainWindow] onDownloadError 被调用 ===";
    qDebug() << "=================================================================";
    qDebug() << "[MainWindow] 错误代码:" << error;
    
    if (!m_downloadReply) return;
    
    QString errorString = m_downloadReply->errorString();
    qDebug() << "[MainWindow] 错误信息:" << errorString;
    
    if (m_updateDialog) {
        m_updateDialog->onDownloadError(errorString);
    }
    
    qDebug() << "=================================================================";
}



