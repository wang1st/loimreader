#include "customsplitterhandle.h"
#include <QPainter>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QSplitter>

CustomSplitterHandle::CustomSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
    : QSplitterHandle(orientation, parent)
    , m_isHovering(false)
    , m_isDragging(false)
{
    // 设置鼠标追踪以接收hover事件
    setMouseTracking(true);
}

void CustomSplitterHandle::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    // Jony Ive风格：根据状态绘制分割条
    if (m_isDragging) {
        // 拖动状态：清晰反馈
        paintActive(&painter);
    } else if (m_isHovering) {
        // Hover状态：明确可交互
        paintHover(&painter);
    } else {
        // 默认状态：微妙但可见
        paintDefault(&painter);
    }
}

void CustomSplitterHandle::paintDefault(QPainter *painter)
{
    // 默认状态：微妙的渐变 + 中心圆点装饰
    // 设计：创造"凹槽"效果，暗示可以拖动
    
    painter->save();
    
    QRect rect = this->rect();
    
    // 1. 绘制水平渐变背景（左暗→中亮→右暗，创造凹槽效果）
    QLinearGradient gradient(rect.topLeft(), rect.topRight());
    gradient.setColorAt(0.0, QColor(0, 0, 0, 20));    // 左边缘：8% 不透明
    gradient.setColorAt(0.5, QColor(0, 0, 0, 8));     // 中间：3% 不透明
    gradient.setColorAt(1.0, QColor(0, 0, 0, 20));    // 右边缘：8% 不透明
    
    painter->fillRect(rect, gradient);
    
    // 2. 绘制中心6个圆点装饰（⋮⋮，垂直排列，暗示可拖动）
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 255, 255, 100));  // 半透明白色
    
    qreal centerX = rect.center().x();
    qreal centerY = rect.center().y();
    qreal dotRadius = 1.5;  // 3px直径
    qreal dotSpacing = 4.0;  // 4px间距
    
    // 绘制6个圆点（两列，每列3个）
    for (int col = 0; col < 2; ++col) {
        qreal x = centerX + (col == 0 ? -2.5 : 2.5);
        for (int row = -1; row <= 1; ++row) {
            qreal y = centerY + row * dotSpacing;
            painter->drawEllipse(QPointF(x, y), dotRadius, dotRadius);
        }
    }
    
    painter->restore();
}

void CustomSplitterHandle::paintHover(QPainter *painter)
{
    // Hover状态：明确可交互
    // 设计：iOS风格浅蓝色 + 上下双箭头 + 把手装饰
    
    painter->save();
    
    QRect rect = this->rect();
    
    // 1. 绘制浅蓝色背景（iOS风格）
    QColor hoverColor(74, 144, 226, 90);  // #4A90E2，35%不透明
    painter->fillRect(rect, hoverColor);
    
    qreal centerX = rect.center().x();
    qreal centerY = rect.center().y();
    
    // 2. 绘制中心的左右双箭头（垂直分割条用左右箭头）
    painter->setPen(QPen(QColor(255, 255, 255, 230), 2.0));
    painter->setBrush(Qt::NoBrush);
    
    qreal arrowSize = 5.0;  // 箭头大小
    qreal arrowGap = 1.5;   // 左右箭头之间的间距
    
    // 左箭头 ←
    QPointF leftArrowTip(centerX - arrowGap - 1, centerY);
    QPointF leftArrowTop(centerX - arrowGap + arrowSize - 1, centerY - arrowSize);
    QPointF leftArrowBottom(centerX - arrowGap + arrowSize - 1, centerY + arrowSize);
    painter->drawLine(leftArrowTop, leftArrowTip);
    painter->drawLine(leftArrowTip, leftArrowBottom);
    
    // 右箭头 →
    QPointF rightArrowTip(centerX + arrowGap + 1, centerY);
    QPointF rightArrowTop(centerX + arrowGap - arrowSize + 1, centerY - arrowSize);
    QPointF rightArrowBottom(centerX + arrowGap - arrowSize + 1, centerY + arrowSize);
    painter->drawLine(rightArrowTop, rightArrowTip);
    painter->drawLine(rightArrowTip, rightArrowBottom);
    
    // 3. 绘制上下把手装饰（各3个圆点）
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 255, 255, 180));
    
    qreal handleY = 25.0;  // 把手距离边缘的距离
    qreal dotRadius = 1.5;
    qreal dotSpacing = 3.0;
    
    // 上方把手
    for (int i = -1; i <= 1; ++i) {
        painter->drawEllipse(QPointF(centerX + i * dotSpacing, handleY), dotRadius, dotRadius);
    }
    
    // 下方把手
    qreal bottomHandleY = rect.height() - handleY;
    for (int i = -1; i <= 1; ++i) {
        painter->drawEllipse(QPointF(centerX + i * dotSpacing, bottomHandleY), dotRadius, dotRadius);
    }
    
    painter->restore();
}

void CustomSplitterHandle::paintActive(QPainter *painter)
{
    // 拖动状态：清晰反馈
    // 设计：深蓝色背景 + 更大的箭头 + 更多把手点
    
    painter->save();
    
    QRect rect = this->rect();
    
    // 1. 绘制深蓝色背景
    QColor activeColor(0, 102, 204, 215);  // #0066CC，85%不透明
    painter->fillRect(rect, activeColor);
    
    qreal centerX = rect.center().x();
    qreal centerY = rect.center().y();
    
    // 2. 绘制更大的左右双箭头
    painter->setPen(QPen(QColor(255, 255, 255, 255), 2.5));
    painter->setBrush(Qt::NoBrush);
    
    qreal arrowSize = 6.0;  // 箭头更大
    qreal arrowGap = 2.0;
    
    // 左箭头 ←
    QPointF leftArrowTip(centerX - arrowGap - 2, centerY);
    QPointF leftArrowTop(centerX - arrowGap + arrowSize - 2, centerY - arrowSize);
    QPointF leftArrowBottom(centerX - arrowGap + arrowSize - 2, centerY + arrowSize);
    painter->drawLine(leftArrowTop, leftArrowTip);
    painter->drawLine(leftArrowTip, leftArrowBottom);
    
    // 右箭头 →
    QPointF rightArrowTip(centerX + arrowGap + 2, centerY);
    QPointF rightArrowTop(centerX + arrowGap - arrowSize + 2, centerY - arrowSize);
    QPointF rightArrowBottom(centerX + arrowGap - arrowSize + 2, centerY + arrowSize);
    painter->drawLine(rightArrowTop, rightArrowTip);
    painter->drawLine(rightArrowTip, rightArrowBottom);
    
    // 3. 绘制上下更大的把手装饰（各5个圆点）
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 255, 255, 220));
    
    qreal handleY = 20.0;
    qreal dotRadius = 2.0;  // 更大的圆点
    qreal dotSpacing = 4.0;
    
    // 上方把手（5个点）
    for (int i = -2; i <= 2; ++i) {
        painter->drawEllipse(QPointF(centerX + i * dotSpacing, handleY), dotRadius, dotRadius);
    }
    
    // 下方把手（5个点）
    qreal bottomHandleY = rect.height() - handleY;
    for (int i = -2; i <= 2; ++i) {
        painter->drawEllipse(QPointF(centerX + i * dotSpacing, bottomHandleY), dotRadius, dotRadius);
    }
    
    painter->restore();
}

void CustomSplitterHandle::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    m_isHovering = true;
    update();  // 触发重绘
    QSplitterHandle::enterEvent(event);
}

void CustomSplitterHandle::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    m_isHovering = false;
    update();  // 触发重绘
    QSplitterHandle::leaveEvent(event);
}

void CustomSplitterHandle::mousePressEvent(QMouseEvent *event)
{
    m_isDragging = true;
    update();  // 触发重绘
    QSplitterHandle::mousePressEvent(event);
}

void CustomSplitterHandle::mouseReleaseEvent(QMouseEvent *event)
{
    m_isDragging = false;
    update();  // 触发重绘
    QSplitterHandle::mouseReleaseEvent(event);
}

