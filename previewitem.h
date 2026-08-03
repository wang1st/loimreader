#ifndef PREVIEWITEM_H
#define PREVIEWITEM_H
#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif
#include <QGraphicsItem>
#include <QList>

class QMarginsF;
class PageItem;
class PreviewItem;

class PreviewItem : public QGraphicsItem
{
private:
    QRectF m_imageRect;
    QMarginsF m_margins;
    QList<PageItem*> m_pages;
    qreal m_pageSpace;
    PreviewItem *m_prev;
    PreviewItem *m_next;
    quint32 m_marginScale;
    uint32_t m_uuidHash;
    quint32 m_pageNum;
    quint32 m_pageCount;
    static qreal g_maxHeight;
    static bool g_printPageNum;
    QString m_lang;
public:
    void SetLanguage(QString lang);
    static void initMaxHeight();
    PreviewItem(PageItem* item, quint32 count = 1);
    void appendItem(PageItem* item);
    QMarginsF margins() const;
    void setMargins(const QMarginsF &margins);
    QRectF boundingRect() const;
    void paint( QPainter *printMe,
            const QStyleOptionGraphicsItem *option, QWidget *widget );
    void paintImg( QPainter *painter);
    void print( QPainter *painter);
    qreal pageSpace() const;
    void setPageSpace(const qreal &pageSpace);
    PreviewItem *prev() const;
    void setPrev(PreviewItem *prev);

    PreviewItem *next() const;
    void setNext(PreviewItem *next);

    void onAdjusted();
    void setMarginScale(int marginScale);
    int increaseMargins();
    int decreaseMargins();

    QRectF imageRect() const;
    void setImageRect(const QRectF &imageRect);

    void setUuidHash(const uint32_t &uuidHash);

    uint32_t uuidHash() const;

    quint32 pageNum() const;
    void setPageNum(const quint32 &pageNum);
    void switchPageNum();

protected:
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
};

#endif // PREVIEWITEM_H
