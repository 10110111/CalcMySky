#ifndef INCLUDE_ONCE_CB7252F0_A962_4452_87E3_0CDE43F88DF9
#define INCLUDE_ONCE_CB7252F0_A962_4452_87E3_0CDE43F88DF9

#include <QMainWindow>
#include <QProgressBar>

class MainWindow : public QMainWindow
{
public:
    MainWindow(QDockWidget* tools, QWidget* parent=nullptr);
    void onLoadProgress(QString const& currentActivity, int stepsDone, int stepsToDo);
protected:
    bool eventFilter(QObject* object, QEvent* event) override;
private:
    void keyPressEvent(QKeyEvent*) override;
private:
    QDockWidget* tools_;
    QProgressBar* loadProgressBar_=new QProgressBar;
};

#endif
