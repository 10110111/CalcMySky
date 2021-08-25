#include "MainWindow.hpp"
#include <QKeyEvent>
#include <QDockWidget>

MainWindow::MainWindow(QDockWidget* tools, QWidget* parent)
    : QMainWindow(parent)
    , tools_(tools)
{
    installEventFilter(this);
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

bool MainWindow::eventFilter(QObject* object, QEvent* event)
{
    if(event->type() == QEvent::WindowActivate || event->type() == QEvent::WindowDeactivate)
    {
        // Prevent repaints of GLWidget due to the window becoming active or inactive. This must be combined
        // with filtering out FocusIn/FocusOut events in the GLWidget itself.
        return true;
    }
    return QMainWindow::eventFilter(object, event);
}
