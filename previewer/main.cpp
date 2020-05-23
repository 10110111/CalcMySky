#include <iostream>

#include <QScreen>
#include <QMainWindow>
#include <QApplication>
#include <QSurfaceFormat>
#include <QCommandLineParser>

#include "config.h"
#include "../common/util.hpp"
#include "GLWidget.hpp"
#include "ToolsWidget.hpp"

std::string pathToData;

void handleCmdLine()
{
    QCommandLineParser parser;
    parser.addPositionalArgument("path to data", "Path to atmosphere textures");
    parser.addVersionOption();
    parser.addHelpOption();

    parser.process(*qApp);

    const auto posArgs=parser.positionalArguments();
    if(posArgs.size()>1)
    {
        std::cerr << "Too many arguments\n";
        throw MustQuit{};
    }
    if(posArgs.isEmpty())
    {
        std::cerr << parser.helpText();
        throw MustQuit{};
    }

    pathToData=posArgs[0].toStdString();
}

int main(int argc, char** argv)
{
    [[maybe_unused]] UTF8Console utf8console;

    QApplication app(argc, argv);
    app.setApplicationName("Atmosphere textures preview");
    app.setApplicationVersion(APP_VERSION);

    QSurfaceFormat format;
    format.setVersion(3,3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);

    try
    {
        handleCmdLine();
        const double maxAltitude=50e3; // TODO: read from precomputed data

        QMainWindow mainWin;
        const auto glWidget=new GLWidget(pathToData);
        const auto tools=new ToolsWidget(maxAltitude);

        QObject::connect(tools, &ToolsWidget::altitudeChanged, glWidget, &GLWidget::setAltitude);
        QObject::connect(tools, &ToolsWidget::exposureLogChanged, glWidget, &GLWidget::setExposureLog);
        QObject::connect(tools, &ToolsWidget::sunElevationChanged, glWidget, &GLWidget::setSunElevation);
        QObject::connect(tools, &ToolsWidget::sunAzimuthChanged, glWidget, &GLWidget::setSunAzimuth);
        QObject::connect(glWidget, &GLWidget::sunElevationChanged, tools, &ToolsWidget::showSunElevation);
        QObject::connect(glWidget, &GLWidget::sunAzimuthChanged, tools, &ToolsWidget::showSunAzimuth);
        QObject::connect(glWidget, &GLWidget::frameFinished, tools, &ToolsWidget::showFrameRate);

        mainWin.setCentralWidget(glWidget);
        mainWin.addDockWidget(Qt::RightDockWidgetArea, tools);
        mainWin.resize(app.primaryScreen()->size()/1.6);
        mainWin.show();
        return app.exec();
    }
    catch(MustQuit&)
    {
        return 1;
    }
    catch(std::exception const& ex)
    {
#if defined Q_OS_WIN && !defined __GNUC__
        // MSVCRT-generated exceptions can contain localized messages
        // in OEM codepage, so restore CP before printing them.
        utf8console.restore();
#endif
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 111;
    }
}
