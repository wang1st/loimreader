#ifndef PAGEITEM_H
#define PAGEITEM_H
#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif
#include <QGraphicsItem>
#include <QPixmap>
#include <QList>

class PreviewItem;

class PageItem : public QGraphicsItem
{
public:
    PageItem(const QPixmap& pixmap, const QRectF& rect);
    QRectF boundingRect() const;
    void paint( QPainter *painter,
            const QStyleOptionGraphicsItem *option, QWidget *widget );
    void paintImg( QPainter *painter, const QRectF& target);
    void printImg( QPainter *painter, const QRectF& target);
    quint32 pagenum() const;
    void setPagenum(const quint32 &pagenum);

    quint32 lineWidth() const;
    void setLineWidth(const quint32 &lineWidth);

    PageItem *prevPage() const;
    void setPrevPage(PageItem *prevPage);

    PageItem *nextPage() const;
    void setNextPage(PageItem* page);

    bool onAdjusting() const;
    void setOnAdjusting(bool onAdjusting);

    QRectF splitRect() const;
    void setSplitRect(const QRectF &splitRect);

    QRectF lineRect() const;
    void setLineRect(const QRectF &lineRect);

    QRectF sourceRect() const;
    void setSourceRect(const QRectF &sourceRect);

    QRectF imgRect() const;
    void setImgRect(const QRectF &imgRect);

    void Shrink();
    void Extand();

    PreviewItem *previewItem() const;
    void setPreviewItem(PreviewItem *previewItem);

    void autoCut();

    static void initRatio();
private:
    quint32 Threshold_pro(QImage *inputImage);
    void adjust(qreal dist);
protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
private:
    // Jony Ive风格：绘制分割线的三种状态
    void paintSplitLineDefault(QPainter *painter);     // 默认状态：微妙但可见
    void paintSplitLineHover(QPainter *painter);       // Hover状态：明确可交互
    void paintSplitLineActive(QPainter *painter);      // 拖动状态：清晰反馈
    
    QPixmap m_pixmap;
    QRectF m_originRect, m_sourceRect, m_imgRect, m_lineRect, m_splitRect;
    qreal m_hpadscale, m_vpadscale;
    quint32 m_pagenum;
    quint32 m_lineWidth;
    PageItem *m_prevPage, *m_nextPage;
    QGraphicsLineItem* m_splitLine;
    bool m_onAdjusting;      // 是否正在拖动
    bool m_isHovering;       // 是否鼠标悬停（新增）
    qreal m_cutLength;
    qreal m_curDrift;
    QList<PreviewItem*> m_previewItems;
    static qreal g_ratio;
    qreal g_oriRatio;
};

#endif // PAGEITEM_H
