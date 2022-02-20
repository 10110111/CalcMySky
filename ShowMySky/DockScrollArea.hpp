#pragma once

#include <QScrollArea>

class DockScrollArea : public QScrollArea
{
public:
    DockScrollArea(QWidget* parent=nullptr);
    QSize sizeHint() const override;
};
