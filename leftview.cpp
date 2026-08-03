#include "leftview.h"
#include <QDebug>
#include <QtGui>

LeftView::LeftView(QWidget *parent)
    : QGraphicsView(parent)
{
    //setDragMode(ScrollHandDrag);
    this->setAcceptDrops(true);
    
    // 初始状态下隐藏滚动条，避免闪烁
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // Jony Ive风格：统一的滚动条样式
    setStyleSheet(
        "QGraphicsView {"
        "    border: none;"
        "}"
        // 垂直滚动条
        "QScrollBar:vertical {"
        "    background: rgba(0, 0, 0, 0.03);"  // 极淡背景
        "    width: 10px;"
        "    margin: 0px;"
        "    border: none;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: rgba(0, 0, 0, 0.15);"  // 半透明灰
        "    min-height: 30px;"
        "    border-radius: 5px;"
        "    margin: 2px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background: rgba(74, 144, 226, 0.4);"  // Hover时iOS蓝
        "}"
        "QScrollBar::handle:vertical:pressed {"
        "    background: rgba(0, 102, 204, 0.6);"   // 按下时深蓝
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    height: 0px;"  // 隐藏箭头按钮
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "    background: none;"
        "}"
        // 水平滚动条
        "QScrollBar:horizontal {"
        "    background: rgba(0, 0, 0, 0.03);"
        "    height: 10px;"
        "    margin: 0px;"
        "    border: none;"
        "}"
        "QScrollBar::handle:horizontal {"
        "    background: rgba(0, 0, 0, 0.15);"
        "    min-width: 30px;"
        "    border-radius: 5px;"
        "    margin: 2px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "    background: rgba(74, 144, 226, 0.4);"
        "}"
        "QScrollBar::handle:horizontal:pressed {"
        "    background: rgba(0, 102, 204, 0.6);"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        "    width: 0px;"
        "}"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        "    background: none;"
        "}"
    );
}

void LeftView::resizeEvent(QResizeEvent *event)
{
    if(this->scene()){
        QSizeF newsize = event->size();
        fitViewPort(newsize.width());
    }
    QGraphicsView::resizeEvent(event);
}

void LeftView::fitViewPort(qreal width)
{
    QRectF rect1 = this->scene()->sceneRect();
    qreal scale =  width * 0.8 / rect1.width();
    setTransform(QTransform::fromScale(scale, scale));
}

void LeftView::enableScrollBars()
{
    // 启用滚动条，当有内容需要滚动时
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void LeftView::disableScrollBars()
{
    // 禁用滚动条，当没有内容时
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void LeftView::dragEnterEvent(QDragEnterEvent *event)
{
    Q_UNUSED(event);
}


void LeftView::dropEvent(QDropEvent *event)
{
    Q_UNUSED(event);
}
