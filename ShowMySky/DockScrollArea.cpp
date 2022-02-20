#include "DockScrollArea.hpp"
#include <QScrollBar>

DockScrollArea::DockScrollArea(QWidget* parent)
    : QScrollArea(parent)
{
}

QSize DockScrollArea::sizeHint() const
{
    // Avoid scrollbars when we can simply adjust the size a bit to get rid of them
    auto size = QScrollArea::sizeHint();
    if(verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded)
        size.setWidth(size.width() + verticalScrollBar()->sizeHint().width());
    if(horizontalScrollBarPolicy() == Qt::ScrollBarAsNeeded)
        size.setHeight(size.height() + horizontalScrollBar()->sizeHint().height());
    return size;
}
