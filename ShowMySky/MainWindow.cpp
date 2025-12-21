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

#include "MainWindow.hpp"
#include <cmath>
#include <QDir>
#include <QKeyEvent>
#include <QStatusBar>
#include <QDockWidget>
#include <QApplication>

MainWindow::MainWindow(QString const& pathToData, QDockWidget* tools, QWidget* parent)
    : QMainWindow(parent)
    , tools_(tools)
{
    installEventFilter(this);
    addDockWidget(Qt::RightDockWidgetArea, tools);

    const auto sb = statusBar();

    loadProgressBar_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    sb->showMessage(tr("Loading..."));
    sb->addPermanentWidget(loadProgressBar_);

    sb->addPermanentWidget(frameRate_);

    setWindowTitle(tr("%1 - %2","window title: File Name - App Name")
                   .arg(QDir(pathToData).dirName()).arg(qApp->applicationDisplayName()));
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

void MainWindow::onLoadProgress(QString const& currentActivity, const int stepsDone, const int stepsToDo)
{
    statusBar()->showMessage(currentActivity);
    if(stepsToDo)
    {
        loadProgressBar_->setMaximum(stepsToDo);
        loadProgressBar_->setValue(stepsDone);
        loadProgressBar_->setVisible(true);
    }
    else
    {
        loadProgressBar_->setVisible(false);
    }
    frameRate_->setVisible(stepsToDo==0);
}

void MainWindow::showFrameRate(const long long frameTimeInUS)
{
    if(frameTimeInUS<=1e6)
    {
        const auto fps = 1e6/frameTimeInUS;
        if(1e3<fps && fps<1e5) // avoid exponential notation on very fast machines
            frameRate_->setText(tr("%1 FPS").arg(std::lround(fps)));
        else
            frameRate_->setText(tr("%1 FPS").arg(fps, 0, 'g', 3));
    }
    else
        frameRate_->setText(tr("%1 FPS (%2 s/frame)").arg(1e6/frameTimeInUS, 0, 'g', 3)
                                                    .arg(frameTimeInUS/1e6, 0, 'g', 3));
}

void MainWindow::setWindowDecorationEnabled(const bool enabled)
{
    const bool windowWasVisible = isVisible();
    statusBar()->setVisible(enabled);
    setWindowFlag(Qt::FramelessWindowHint, !enabled);
    if(windowWasVisible)
        show();
    if(!enabled)
        centralWidget()->update();
}
