#ifndef LEFTVIEW_H
#define LEFTVIEW_H
#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif
#include <QGraphicsView>

class LeftView: public QGraphicsView
{
    Q_OBJECT

public:
    LeftView(QWidget *parent=0);
    void fitViewPort(qreal width);
    void enableScrollBars();
    void disableScrollBars();
protected:
    void resizeEvent(QResizeEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
};

#endif // LEFTVIEW_H
