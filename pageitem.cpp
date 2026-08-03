#include "pageitem.h"
#include "previewitem.h"
#include <QPainter>
#include <QtDebug>
#include <QList>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>
#include <QtMath>
#include <QLinearGradient>
#include <QFont>

qreal PageItem::g_ratio = 0.70707071;

void PageItem::initRatio()
{
    g_ratio = 0.70707071;
}

PageItem::PageItem(const QPixmap& pixmap, const QRectF& rect)
        : m_pixmap(pixmap), m_sourceRect(rect), m_originRect(rect)
{
    m_hpadscale = 1.05;
    m_vpadscale = 1.05;
    m_pagenum = 0;
    this->setPos(rect.x(), rect.y());

    m_lineWidth = 20;  // 从12增加到20，为把手和箭头留出更多空间
    m_prevPage = NULL;
    m_nextPage = NULL;
    m_splitLine = NULL;
    setAcceptHoverEvents(true);

    m_cutLength = 0.0;
    m_onAdjusting = false;
    m_isHovering = false;  // 初始化Hover状态
    m_curDrift = 0.0;

    m_imgRect = QRectF(0, 0, m_sourceRect.width(), m_sourceRect.height());
    m_lineRect = QRectF(0, m_imgRect.height(), m_imgRect.width(), m_lineWidth);
}


QRectF PageItem::boundingRect() const
{
    QRectF rect = m_imgRect.united(m_lineRect);
    return rect;
}


void PageItem::setNextPage(PageItem *nextPage)
{
    m_nextPage = nextPage;
    nextPage->setPrevPage(this);
    QPointF pos = this->pos();
    nextPage->setPos(pos.x(), pos.y() + boundingRect().height());

}
PageItem *PageItem::nextPage() const
{
    return m_nextPage;
}

PageItem *PageItem::prevPage() const
{
    return m_prevPage;
}

void PageItem::setPrevPage(PageItem *prevPage)
{
    m_prevPage = prevPage;
}


void PageItem::paint(QPainter *painter,
            const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    
    // 绘制图像内容
    QRectF target = m_sourceRect;
    target.moveTo(0.0, 0.0);
    this->paintImg(painter, target);
    
    // Jony Ive风格：根据状态绘制分割线
    painter->setRenderHint(QPainter::Antialiasing, true);
    
    if (m_onAdjusting) {
        // 拖动状态：清晰反馈
        paintSplitLineActive(painter);
    } else if (m_isHovering) {
        // Hover状态：明确可交互
        paintSplitLineHover(painter);
    } else {
        // 默认状态：微妙但可见
        paintSplitLineDefault(painter);
    }
}

void PageItem::paintImg( QPainter * painter, const QRectF& target)
{
    QPixmap map2 = m_pixmap.copy(m_sourceRect.toRect());
    QRectF rect2 = m_sourceRect;
    rect2.moveTopLeft(QPointF(0,0));
    painter->drawPixmap(target, map2, rect2);
//    QGraphicsItem::paint(paint, option, widget);
}

void PageItem::printImg( QPainter * painter, const QRectF& target)
{
    QRectF realRect = target;
    qreal width = target.height() * g_ratio;
    qreal height = width * m_sourceRect.height() / m_sourceRect.width();
    realRect.setWidth(width);
    realRect.setHeight(height);
    realRect.translate((target.width() - width) / 2.0, (target.height() - height) / 2.0);
//    qDebug() << "No." << this->pagenum() << ", source:w=" << m_sourceRect.width() << ",h=" << m_sourceRect.height()
//             << ", ratio:" << g_ratio << ", target:w=" << target.width() << ",h=" << target.height()
//             << ", real:w=" << realRect.width() << ",h=" << realRect.height();
    QPixmap map2 = m_pixmap.copy(m_sourceRect.toRect());
    QRectF rect2 = m_sourceRect;
    rect2.moveTopLeft(QPointF(0,0));
    painter->drawPixmap(realRect, map2, rect2);
//    qDebug() << "page:" << m_pagenum << "source:" << m_sourceRect << ", target:" << realRect;

//    QPixmap map2 = m_pixmap.copy(m_sourceRect.toRect());
//    QString file = "/Users/wangzhen/Documents/qt/page%1.png";
//    map2.save(file.arg(pagenum()));
}

quint32 PageItem::pagenum() const
{
    return m_pagenum;
}

void PageItem::setPagenum(const quint32 &pagenum)
{
    m_pagenum = pagenum;
}

quint32 PageItem::lineWidth() const
{
    return m_lineWidth;
}

void PageItem::setLineWidth(const quint32 &lineWidth)
{
    m_lineWidth = lineWidth;
}



void PageItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    //qDebug() << event->pos();
    if(m_onAdjusting && m_nextPage){
        qreal dist = event->pos().y();
        qreal max_dist = imgRect().height() + m_nextPage->imgRect().height();
        if(dist/max_dist < 0.4 || dist/max_dist > 0.6) return;
        m_splitRect.setRect(0, event->pos().y() - m_curDrift, m_lineRect.width(), m_lineRect.height());
        if(event->pos().y() > m_lineRect.top()){
            //qDebug() << event->pos().y() << m_lineRect.bottom();
            if(m_nextPage){
                m_nextPage->setOnAdjusting(true);
                QRectF splitRect(0, event->pos().y() - m_lineRect.bottom() - m_curDrift,
                                m_lineRect.width(), m_lineRect.height());
                m_nextPage->setSplitRect(splitRect);
                m_nextPage->update();
            }
        }
        else{
            if(m_nextPage){
                m_nextPage->setOnAdjusting(true);
                m_nextPage->setSplitRect(m_nextPage->lineRect());
                m_nextPage->update();
            }
        }
        update();
    }
}

void PageItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if(hasCursor()){
        m_onAdjusting = true;
        m_curDrift = event->pos().y() - m_lineRect.top();
        update();
    }
}

void PageItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if(!m_onAdjusting) return;

    unsetCursor();
    m_onAdjusting = false;
    m_nextPage->setOnAdjusting(false);
    qreal max_dist = imgRect().height() + m_nextPage->imgRect().height();
    qreal dist1 = event->pos().y();
    if(dist1/max_dist < 0.4){
        dist1 = max_dist * 0.4;
    }
    else if (dist1/max_dist > 0.6) {
        dist1 = max_dist * 0.6;
    }
    qreal dist = dist1 - m_lineRect.y() - m_curDrift;
    //qDebug() << dist;
    adjust(dist);
    update();
    this->prepareGeometryChange();

}

void PageItem::adjust(qreal dist)
{
    m_imgRect.setBottom(m_imgRect.bottom() + dist);
    m_sourceRect.setHeight(m_imgRect.height());
    m_lineRect.moveTopLeft(QPointF(0, m_imgRect.bottom()));
    m_splitRect = m_lineRect;
    qreal ratio = m_sourceRect.width() / m_sourceRect.height();
    if(ratio < g_ratio)
        g_ratio = ratio;
    if(m_nextPage){
        QRectF source = m_nextPage->sourceRect();
        source.setTop(source.top() + dist);
        ratio = source.width() / source.height();
        if(ratio < g_ratio)
           g_ratio = ratio;
        m_nextPage->setSourceRect(source);
        QRectF img = m_nextPage->sourceRect();
        img.moveTopLeft(QPointF(0,0));
        m_nextPage->setImgRect(img);
        QRectF line = m_nextPage->lineRect();
        line.moveTopLeft(QPointF(0, img.bottom()));
        m_nextPage->setLineRect(line);
        m_nextPage->setSplitRect(line);
        m_nextPage->setOnAdjusting(false);
        //m_nextPage->update();
        QPointF pos = m_nextPage->pos();
        pos.setY(pos.y() + dist);
        m_nextPage->setPos(pos);
    }
    if(!m_previewItems.isEmpty()){
        for (PreviewItem* preview : m_previewItems) {
            preview->onAdjusted();
        }
    }
}

void PageItem::autoCut()
{
    if(!this->nextPage())
        return;
    QRectF boardRect(m_sourceRect);
    boardRect.setHeight(m_sourceRect.height() * 0.3);
    boardRect.moveTop(m_sourceRect.bottom() - 0.5 * boardRect.height());
    QPixmap boardPix = m_pixmap.copy(boardRect.toAlignedRect());
    QImage  boardImg = boardPix.toImage();
    quint32 blankLine = Threshold_pro(&boardImg);
    if(m_cutLength > 0.01 || m_cutLength < -0.01){
        m_cutLength = 0.0 - m_cutLength;
    }
    else{
        m_cutLength  = blankLine - 0.5 * boardRect.height();
        g_oriRatio = g_ratio;
    }
    adjust(m_cutLength);
    update();
}


//图像二值化，判断空隙
quint32 PageItem::Threshold_pro(QImage *inputImage)
{
    uint8_t bt;
    QColor oldColor;

    quint32 maxline = 0;
    quint64 maxColor = 0;
    for(int i = 0; i<inputImage->height(); i++)
    {
        quint64 color = 0;
        for(int j = 0; j<inputImage->width(); j++)
        {
            oldColor = QColor(inputImage->pixel(j,i));
            bt = oldColor.red();
            if(bt < 192)
            {
                bt = 0;
            }else {
                bt = 255;
            }
            color = color + bt;
        }
        if(color > maxColor){
            maxline = i;
            maxColor = color;
        }
    }
    //qDebug() << maxline;
    return maxline;
}


void PageItem::Shrink()
{

}

void PageItem::Extand()
{

}


void PageItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
}

void PageItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    bool wasHovering = m_isHovering;
    
    if(m_lineRect.contains(event->pos())){
        m_splitRect = m_lineRect;
        m_isHovering = true;
        setCursor(Qt::SplitVCursor);
    }
    else{
        m_isHovering = false;
        unsetCursor();
    }
    
    // 如果Hover状态改变，触发重绘
    if (wasHovering != m_isHovering) {
        update();
    }
    
    if(m_prevPage){
        m_prevPage->unsetCursor();
    }
}

PreviewItem *PageItem::previewItem() const
{
    return m_previewItems.first();
}

void PageItem::setPreviewItem(PreviewItem *previewItem)
{
    m_previewItems.append(previewItem);
}

QRectF PageItem::imgRect() const
{
    return m_imgRect;
}

void PageItem::setImgRect(const QRectF &imgRect)
{
    m_imgRect = imgRect;
}

QRectF PageItem::sourceRect() const
{
    return m_sourceRect;
}

void PageItem::setSourceRect(const QRectF &sourceRect)
{
    m_sourceRect = sourceRect;
}

QRectF PageItem::lineRect() const
{
    return m_lineRect;
}

void PageItem::setLineRect(const QRectF &lineRect)
{
    m_lineRect = lineRect;
}

QRectF PageItem::splitRect() const
{
    return m_splitRect;
}

void PageItem::setSplitRect(const QRectF &splitRect)
{
    m_splitRect = splitRect;
}

bool PageItem::onAdjusting() const
{
    return m_onAdjusting;
}

void PageItem::setOnAdjusting(bool onAdjusting)
{
    m_onAdjusting = onAdjusting;
}

// ============================================================================
// Jony Ive风格：三态分割线绘制方法
// ============================================================================

void PageItem::paintSplitLineDefault(QPainter *painter)
{
    // 默认状态：微妙但可见
    // 设计：渐变背景 + 中心三点装饰
    
    painter->save();
    
    // 1. 绘制渐变背景（上下边缘稍淡，中间稍深，创造微妙层次感）
    QLinearGradient gradient(m_lineRect.topLeft(), m_lineRect.bottomLeft());
    gradient.setColorAt(0.0, QColor(0, 0, 0, 13));    // 顶部：5% 不透明度
    gradient.setColorAt(0.5, QColor(0, 0, 0, 20));    // 中间：8% 不透明度
    gradient.setColorAt(1.0, QColor(0, 0, 0, 13));    // 底部：5% 不透明度
    
    painter->fillRect(m_lineRect, gradient);
    
    // 2. 绘制中心三点装饰（••• 提示可交互）
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(153, 153, 153));  // #999999，中性灰
    
    qreal centerX = m_lineRect.center().x();
    qreal centerY = m_lineRect.center().y();
    qreal dotRadius = 2.0;  // 4px直径的圆点
    qreal dotSpacing = 8.0;  // 圆点间距
    
    // 绘制三个圆点
    painter->drawEllipse(QPointF(centerX - dotSpacing, centerY), dotRadius, dotRadius);
    painter->drawEllipse(QPointF(centerX, centerY), dotRadius, dotRadius);
    painter->drawEllipse(QPointF(centerX + dotSpacing, centerY), dotRadius, dotRadius);
    
    painter->restore();
}

void PageItem::paintSplitLineHover(QPainter *painter)
{
    // Hover状态：明确可交互
    // 设计：浅蓝色背景 + 居中的上下箭头图标 + 左右把手装饰
    
    painter->save();
    
    // 1. 绘制浅蓝色背景（iOS风格）
    QColor hoverColor(74, 144, 226, 230);  // #4A90E2，稍微透明
    painter->fillRect(m_lineRect, hoverColor);
    
    qreal centerX = m_lineRect.center().x();
    qreal centerY = m_lineRect.center().y();
    
    // 2. 绘制中心的上下箭头图标（加大尺寸，更清晰）
    painter->setPen(QPen(QColor(255, 255, 255, 240), 2.5));  // 白色，2.5px粗
    painter->setBrush(Qt::NoBrush);
    
    qreal arrowSize = 6.0;  // 箭头大小增加到6px
    qreal arrowGap = 2.0;   // 上下箭头之间的间距
    
    // 上箭头 ↑
    QPointF upArrowTop(centerX, centerY - arrowGap - 1);
    QPointF upArrowLeft(centerX - arrowSize, centerY - arrowGap + arrowSize - 1);
    QPointF upArrowRight(centerX + arrowSize, centerY - arrowGap + arrowSize - 1);
    painter->drawLine(upArrowLeft, upArrowTop);
    painter->drawLine(upArrowTop, upArrowRight);
    
    // 下箭头 ↓
    QPointF downArrowBottom(centerX, centerY + arrowGap + 1);
    QPointF downArrowLeft(centerX - arrowSize, centerY + arrowGap - arrowSize + 1);
    QPointF downArrowRight(centerX + arrowSize, centerY + arrowGap - arrowSize + 1);
    painter->drawLine(downArrowLeft, downArrowBottom);
    painter->drawLine(downArrowBottom, downArrowRight);
    
    // 3. 绘制左右两侧的把手装饰（三点纵向排列）
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 255, 255, 180));
    
    qreal handleX = 30.0;  // 把手距离边缘的距离
    qreal dotRadius = 2.0;
    qreal dotSpacing = 4.0;
    
    // 左侧把手
    for (int i = -1; i <= 1; ++i) {
        painter->drawEllipse(QPointF(handleX, centerY + i * dotSpacing), dotRadius, dotRadius);
    }
    
    // 右侧把手
    qreal rightHandleX = m_lineRect.width() - handleX;
    for (int i = -1; i <= 1; ++i) {
        painter->drawEllipse(QPointF(rightHandleX, centerY + i * dotSpacing), dotRadius, dotRadius);
    }
    
    painter->restore();
}

void PageItem::paintSplitLineActive(QPainter *painter)
{
    // 拖动状态：清晰反馈
    // 设计：深蓝色背景 + 更大的把手图标 + 中心箭头 + 预览线
    
    painter->save();
    
    // 1. 绘制深蓝色背景
    QColor activeColor(0, 102, 204, 250);  // #0066CC，高不透明度
    painter->fillRect(m_lineRect, activeColor);
    
    qreal centerX = m_lineRect.center().x();
    qreal centerY = m_lineRect.center().y();
    
    // 2. 绘制中心的上下双箭头（拖动时更明显）
    painter->setPen(QPen(QColor(255, 255, 255, 255), 3.0));  // 白色，3px粗
    painter->setBrush(Qt::NoBrush);
    
    qreal arrowSize = 7.0;
    qreal arrowGap = 2.5;
    
    // 上箭头 ↑
    QPointF upArrowTop(centerX, centerY - arrowGap - 2);
    QPointF upArrowLeft(centerX - arrowSize, centerY - arrowGap + arrowSize - 2);
    QPointF upArrowRight(centerX + arrowSize, centerY - arrowGap + arrowSize - 2);
    painter->drawLine(upArrowLeft, upArrowTop);
    painter->drawLine(upArrowTop, upArrowRight);
    
    // 下箭头 ↓
    QPointF downArrowBottom(centerX, centerY + arrowGap + 2);
    QPointF downArrowLeft(centerX - arrowSize, centerY + arrowGap - arrowSize + 2);
    QPointF downArrowRight(centerX + arrowSize, centerY + arrowGap - arrowSize + 2);
    painter->drawLine(downArrowLeft, downArrowBottom);
    painter->drawLine(downArrowBottom, downArrowRight);
    
    // 3. 绘制左右两侧更大的把手图标
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 255, 255, 220));
    
    qreal handleX = 25.0;
    qreal dotRadius = 2.5;  // 增大圆点
    qreal dotSpacing = 5.0;  // 增大间距
    
    // 左侧把手（5个点，更明显）
    for (int i = -2; i <= 2; ++i) {
        painter->drawEllipse(QPointF(handleX, centerY + i * dotSpacing), dotRadius, dotRadius);
    }
    
    // 右侧把手
    qreal rightHandleX = m_lineRect.width() - handleX;
    for (int i = -2; i <= 2; ++i) {
        painter->drawEllipse(QPointF(rightHandleX, centerY + i * dotSpacing), dotRadius, dotRadius);
    }
    
    // 4. 绘制当前拖动位置的预览线（更粗更明显）
    if (m_onAdjusting) {
        QPen previewPen(QColor(255, 255, 255, 180), 3.0, Qt::DashLine);
        painter->setPen(previewPen);
        painter->drawLine(m_splitRect.topLeft(), m_splitRect.topRight());
    }
    
    painter->restore();
}

