#ifndef MAINVIEW_H
#define MAINVIEW_H
#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif
#include <QGraphicsView>
#include <QLabel>

class MainView : public QGraphicsView
{
     Q_OBJECT

public:
    MainView(QWidget *parent=0);
    void fitViewPort(qreal width);
    qreal scale() const;
    void setScale(const qreal &scale);
    void enableScrollBars();
    void disableScrollBars();

protected:
    void resizeEvent(QResizeEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

private:
    QPoint m_lastMousePos;
    qreal m_scale;
};

#endif // MAINVIEW_H
