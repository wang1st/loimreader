#include "previewitem.h"
#include "pageitem.h"
#include "logindialog.h"
#include "protection.h"

#include <QPainter>
#include <QDebug>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <complex>

qreal PreviewItem::g_maxHeight = 0.0;
bool PreviewItem::g_printPageNum = false;

void PreviewItem::initMaxHeight()
{
    g_maxHeight = 0.0;
}

QMarginsF PreviewItem::margins() const
{
    return m_margins;
}

void PreviewItem::setMargins(const QMarginsF &margins)
{
    m_margins = margins;
}

qreal PreviewItem::pageSpace() const
{
    return m_pageSpace;
}

void PreviewItem::setPageSpace(const qreal &pageSpace)
{
    m_pageSpace = pageSpace;
}

PreviewItem *PreviewItem::prev() const
{
    return m_prev;
}

void PreviewItem::setPrev(PreviewItem *prev)
{
    m_prev = prev;
}

PreviewItem *PreviewItem::next() const
{
    return m_next;
}

void PreviewItem::setNext(PreviewItem *next)
{
    m_next = next;
}

QRectF PreviewItem::imageRect() const
{
    return m_imageRect;
}

void PreviewItem::setImageRect(const QRectF &imageRect)
{
    m_imageRect = imageRect;
}

void PreviewItem::setUuidHash(const uint32_t &uuidHash)
{
    m_uuidHash = uuidHash;
}

uint32_t PreviewItem::uuidHash() const
{
    return m_uuidHash;
}

quint32 PreviewItem::pageNum() const
{
    return m_pageNum;
}

void PreviewItem::setPageNum(const quint32 &pageNum)
{
    m_pageNum = pageNum;
}

void PreviewItem::SetLanguage(QString lang)
{
    m_lang = lang;
}

PreviewItem::PreviewItem(PageItem* item, quint32 count)
{
    m_prev = NULL;
    m_next = NULL;
    m_pageSpace = 32.0;
    m_marginScale = 10;
    m_pages.append(item);
    qreal hmargin = item->boundingRect().height() * m_marginScale / 100;
    qreal wmargin = item->boundingRect().width() * m_marginScale / 100;
    setMargins(QMarginsF(wmargin, hmargin, wmargin, hmargin));
    m_imageRect = item->imgRect();
    m_pageCount = count;
    qreal y = (item->pagenum() / count) * (this->boundingRect().height() + m_pageSpace);
    //qDebug() << "pagenum:" << item->pagenum() << ", y:" << y;
    this->setPos(0.0, y);
    item->setPreviewItem(this);
    setFlags(QGraphicsItem::ItemIsSelectable);
}

void PreviewItem::appendItem(PageItem *item)
{
    m_pages.append(item);
    item->setPreviewItem(this);
}

QRectF PreviewItem::boundingRect() const
{
    QRectF rect = m_imageRect + m_margins;
    rect.moveTo(0.0, 0.0);
    return rect;
}

void PreviewItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    switchPageNum();
}

void PreviewItem::switchPageNum()
{
    if(g_printPageNum){
        g_printPageNum = false;
    }
    else{
        g_printPageNum = true;
    }
    update();
    if(m_next){
        m_next->update();
    }
}

void PreviewItem::paintImg( QPainter *painter)
{
    painter->fillRect(boundingRect(), QColor(255, 255, 255));
    QRectF target = boundingRect();
 //   qDebug() << "target:" << target;
    target = target - m_margins;
    target.moveTo(m_margins.left(), m_margins.top());
    if(m_pageCount == 1){
        m_pages.first()->printImg(painter, target);
    }
    else if(m_pageCount == 4){ //目前只支持1和4两种排列模式
        quint32 height = 0;
        quint32 i = 0;
        for (PageItem* p : m_pages)
        {
            height = height + p->imgRect().height();
            if(height > g_maxHeight){
                g_maxHeight = height;
            }
            i = i + 1;
            if (i == 2)
                height = 0;
        }
        qreal ratio = target.height() / g_maxHeight;
 //       qDebug() << "ratio:" << ratio;
        if(ratio >= 1.0) ratio = 0.5; //防止全部文件只有一张图片
        qreal width = m_pages.first()->imgRect().width() * ratio;

        qreal widthPadding = (target.width() / 2 - width) / 2;
        qreal x = target.left();
        qreal y = target.top();
        i = 0;
        for (PageItem* p : m_pages)
        {
            qreal x0 = x + widthPadding;
            qreal y0 = y;
            qreal width0 = width;
            qreal height0 = p->imgRect().height() * ratio;
            y = y + height0;
            i = i + 1;
            if(i == 2){
                y = target.top();
                x = target.left() + target.width() / 2 + widthPadding;
            }

            QRectF target0 = QRectF(x0, y0, width0, height0);

            p->paintImg(painter, target0);
        }
    }
    if(g_printPageNum){
        QString text = QString("%1").arg(pageNum() + 1);
        int fontSize = 16, spacing = 2;
        QFont font("Times New Roman", fontSize, QFont::Thin);
        font.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
        QRect rect(0, 0, 180, 60);
        QPixmap pixmap(rect.size());
        pixmap.fill(Qt::transparent);

        QPainter corner(&pixmap);
        corner.setFont(font);
        corner.setPen(Qt::gray);
        corner.drawText(rect, Qt::AlignCenter, text);
        QRect rectTarget(0, 0, boundingRect().width() / 5, boundingRect().width() / 15); //宽是高的3倍
        rectTarget.moveTopLeft(QPoint(boundingRect().right() - rectTarget.width(),
                               boundingRect().bottom() - rectTarget.height()));
        painter->drawPixmap(rectTarget, pixmap, rect);
    }

    // 使用保护机制检查是否应该显示水印
    bool shouldShowWatermark = false;
    
    // 反调试检查
    if (!ANTI_DEBUG_CHECK()) {
        shouldShowWatermark = true; // 检测到调试器，强制显示水印
    }
    
    // 完整性检查
    if (!INTEGRITY_CHECK()) {
        shouldShowWatermark = true; // 文件被修改，强制显示水印
    }
    
    // 用户类型检查
    if (LoginDialog::s_isTrialUser) {
        shouldShowWatermark = true;
    }
    
    // 使用控制流混淆保护水印逻辑
    if (shouldShowWatermark) {
        // 动态获取水印文本
        QString text = Protection::getWatermarkText();
        
        // 添加虚假操作
        volatile int dummy = 0;
        for (int i = 0; i < 50; ++i) {
            dummy += i * 2;
        }
        
        int fontSize = 25, spacing = 10;
        QFont font("微软雅黑", fontSize, QFont::Thin);
        font.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
        QRect rect = boundingRect().toRect();
        QPixmap pixmap(rect.size());
        pixmap.fill(Qt::transparent);

        QPainter shadow(&pixmap);
        shadow.setFont(font);
        shadow.setPen(QColor(180, 180, 180));
        shadow.translate(rect.width() / 2, - rect.width() / 2);
        shadow.rotate(45);
        int squareEdgeSize = target.width() * sin(45.0) + target.height() * sin(45.0);
        int hCount = squareEdgeSize / ((fontSize + spacing) * (text.size() + 1)) + 1;
        int x = squareEdgeSize / hCount + (fontSize + spacing) * 3;
        int y = x / 2;
        
        // 使用混淆的条件判断
        bool condition1 = (this->m_pageCount == 1 && this->pageNum() > 5);
        bool condition2 = (this->m_pageCount == 4 && this->pageNum() > 1);
        
        if (condition1 || condition2) {
            for (int i = 0; i < hCount; i++)
            {
                for (int j = 0; j < hCount * 2; j++)
                {
                   shadow.drawText(x * i, y * j, text);
                }
            }
        }
        painter->drawPixmap(0, 0, pixmap);
        
        // 保护水印逻辑
        Protection::protectWatermarkLogic();
    } else {
        // 虚假分支 - 不显示水印
        volatile int dummy = 0;
        for (int i = 0; i < 100; ++i) {
            dummy += i * 3;
        }
    }
}

void PreviewItem::paint( QPainter *painter,
        const QStyleOptionGraphicsItem *option, QWidget *widget )
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    paintImg(painter);
    if(this->isSelected()){
        QPen penIn, penOut;  // creates a default pen

        penIn.setWidth(2);
        penIn.setBrush(Qt::black);
        painter->setPen(penIn);
        painter->drawRect(boundingRect());
        QMarginsF margins(2.0, 2.0, 2.0, 2.0);
        penOut.setWidth(2);
        penOut.setBrush(Qt::lightGray);
        painter->setPen(penOut);
        painter->drawRect(boundingRect() + margins);
    }

}

void PreviewItem::print( QPainter *painter)
{
    paintImg(painter);

}

void PreviewItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    QGraphicsScene* scene = m_pages.first()->scene();
    QList<QGraphicsView *> list = scene->views();
    list.at(0)->centerOn(m_pages.first());
}

void PreviewItem::onAdjusted()
{
    update();
    if(m_next){
 //       QPointF pos = this->pos();
 //       qreal y = pos.y() + boundingRect().height() + m_pageSpace;
 //       qDebug() << "y=" << y;
 //       m_next->setPos(0.0, y);
        m_next->update();
    }
    prepareGeometryChange();
}


int PreviewItem::increaseMargins()
{
    if(m_marginScale < 30){
        QRectF oriRect = boundingRect();
        m_marginScale += 5;
        //qDebug() << "margins scale:" << m_marginScale;
        qreal hmargin = m_pages.first()->boundingRect().height() * m_marginScale / 100;
        qreal wmargin = m_pages.first()->boundingRect().width() * m_marginScale / 100;
        setMargins(QMarginsF(wmargin, hmargin, wmargin, hmargin));
        m_imageRect = oriRect - m_margins;
        update();
    }
    return m_marginScale;
}


void PreviewItem::setMarginScale(int marginScale)
{
    m_marginScale = marginScale;
    QRectF oriRect = boundingRect();
    qreal hmargin = m_pages.first()->boundingRect().height() * m_marginScale / 100;
    qreal wmargin = m_pages.first()->boundingRect().width() * m_marginScale / 100;
    setMargins(QMarginsF(wmargin, hmargin, wmargin, hmargin));
    m_imageRect = oriRect - m_margins;
}


int PreviewItem::decreaseMargins()
{
    if(m_marginScale == 0) return 0;
    m_marginScale -= 5;
    if(m_marginScale < 0) m_marginScale = 0;

    QRectF oriRect = boundingRect();
    //qDebug() << "margins scale:" << m_marginScale;
    qreal hmargin = m_pages.first()->boundingRect().height() * m_marginScale / 100;
    qreal wmargin = m_pages.first()->boundingRect().width() * m_marginScale / 100;
    setMargins(QMarginsF(wmargin, hmargin, wmargin, hmargin));
    m_imageRect = oriRect - m_margins;
    update();
    return m_marginScale;
}
