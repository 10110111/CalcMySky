#ifndef INCLUDE_ONCE_CB7252F0_A962_4452_87E3_0CDE43F88DF9
#define INCLUDE_ONCE_CB7252F0_A962_4452_87E3_0CDE43F88DF9

#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>

class MainWindow : public QMainWindow
{
public:
    MainWindow(QString const& pathToData, QDockWidget* tools, QWidget* parent=nullptr);
    void onLoadProgress(QString const& currentActivity, int stepsDone, int stepsToDo);
    void showFrameRate(long long frameTimeInUS);
    void setWindowDecorationEnabled(bool enabled);
protected:
    bool eventFilter(QObject* object, QEvent* event) override;
private:
    void keyPressEvent(QKeyEvent*) override;
private:
    QDockWidget* tools_;
    QProgressBar* loadProgressBar_=new QProgressBar;
    QLabel* frameRate_=new QLabel;
};

#endif
