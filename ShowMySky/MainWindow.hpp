/*
 * CalcMySky - a simulator of light scattering in planetary atmospheres
 * Copyright © 2025 Ruslan Kabatsayev
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

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
