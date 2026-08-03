#include "customsplitter.h"
#include "customsplitterhandle.h"

CustomSplitter::CustomSplitter(QWidget *parent)
    : QSplitter(parent)
{
    setChildrenCollapsible(false);
    setHandleWidth(6);  // Jony Ive风格：细窄优雅的分割条
}

CustomSplitter::CustomSplitter(Qt::Orientation orientation, QWidget *parent)
    : QSplitter(orientation, parent)
{
    setChildrenCollapsible(false);
    setHandleWidth(6);  // Jony Ive风格：细窄优雅的分割条
}

QSplitterHandle *CustomSplitter::createHandle()
{
    return new CustomSplitterHandle(orientation(), this);
}

