#include "MainWindow.hpp"
#include <QKeyEvent>
#include <QDockWidget>

MainWindow::MainWindow(QDockWidget* tools, QWidget* parent)
    : QMainWindow(parent)
    , tools_(tools)
{
    addDockWidget(Qt::RightDockWidgetArea, tools);
}

void MainWindow::keyPressEvent(QKeyEvent*const event)
{
    switch(event->key())
    {
    case Qt::Key_Tab:
        tools_->setVisible(!tools_->isVisible());
        break;
    }
}
