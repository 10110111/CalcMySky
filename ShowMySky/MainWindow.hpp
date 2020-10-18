#ifndef INCLUDE_ONCE_CB7252F0_A962_4452_87E3_0CDE43F88DF9
#define INCLUDE_ONCE_CB7252F0_A962_4452_87E3_0CDE43F88DF9

#include <QMainWindow>

class MainWindow : public QMainWindow
{
public:
    MainWindow(QDockWidget* tools, QWidget* parent=nullptr);
private:
    void keyPressEvent(QKeyEvent*) override;
private:
    QDockWidget* tools_;
};

#endif
