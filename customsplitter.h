#ifndef CUSTOMSPLITTER_H
#define CUSTOMSPLITTER_H

#include <QSplitter>

class CustomSplitter : public QSplitter
{
    Q_OBJECT
public:
    explicit CustomSplitter(QWidget *parent = nullptr);
    explicit CustomSplitter(Qt::Orientation orientation, QWidget *parent = nullptr);
    
protected:
    QSplitterHandle *createHandle() override;
};

#endif // CUSTOMSPLITTER_H

