#ifndef CUSTOMSPLITTERHANDLE_H
#define CUSTOMSPLITTERHANDLE_H

#include <QSplitterHandle>
#include <QWidget>
#include <QEnterEvent>

class CustomSplitterHandle : public QSplitterHandle
{
    Q_OBJECT
public:
    explicit CustomSplitterHandle(Qt::Orientation orientation, QSplitter *parent);
    
protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    
private:
    // Jony Ive风格：三态绘制方法
    void paintDefault(QPainter *painter);      // 默认状态：微妙渐变 + 圆点
    void paintHover(QPainter *painter);        // Hover状态：浅蓝色 + 箭头
    void paintActive(QPainter *painter);       // 拖动状态：深蓝色 + 大箭头
    
    bool m_isHovering;   // 是否悬停
    bool m_isDragging;   // 是否拖动
};

#endif // CUSTOMSPLITTERHANDLE_H

