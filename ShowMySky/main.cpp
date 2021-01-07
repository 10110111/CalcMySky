#include <iostream>

#include <QFile>
#include <QScreen>
#include <QMessageBox>
#include <QMainWindow>
#include <QApplication>
#include <QSurfaceFormat>
#include <QCommandLineParser>

#include "config.h"
#include "../common/util.hpp"
#include "../common/AtmosphereParameters.hpp"
#include "GLWidget.hpp"
#include "MainWindow.hpp"
#include "ToolsWidget.hpp"
#include "util.hpp"

QString pathToData;

QSize windowSize;
bool detachedTools=false;
bool frameless=false;
void handleCmdLine()
{
    QCommandLineParser parser;
    parser.addPositionalArgument("path to data", "Path to atmosphere textures");
    parser.addVersionOption();
    parser.addHelpOption();
    QCommandLineOption winSizeOpt("win-size", "Window size", "WIDTHxHEIGHT");
    parser.addOption(winSizeOpt);
    QCommandLineOption detachedToolsOpt("detached-tools", "Start with tools dock detached");
    parser.addOption(detachedToolsOpt);
    QCommandLineOption framelessOpt("frameless", "Make main window frameless");
    parser.addOption(framelessOpt);

    parser.process(*qApp);

    const auto posArgs=parser.positionalArguments();
    if(posArgs.size()>1)
        throw BadCommandLine{QObject::tr("Too many arguments")};
    if(posArgs.isEmpty())
        throw BadCommandLine{parser.helpText()};

    if(parser.isSet(winSizeOpt))
    {
        QRegularExpression pattern("^([0-9]+)x([0-9]+)$");
        QRegularExpressionMatch match;
        const auto value=parser.value(winSizeOpt);
        if(value.contains(pattern, &match))
        {
            bool okW=false;
            const auto width=match.captured(1).toUInt(&okW);
            bool okH=false;
            const auto height=match.captured(2).toUInt(&okH);
            if(!okW || !okH)
                throw BadCommandLine{QObject::tr("Can't parse window size specification \"%1\"").arg(value)};
            windowSize=QSize(width,height);
        }
    }

    if(parser.isSet(detachedToolsOpt))
        detachedTools=true;

    if(parser.isSet(framelessOpt))
        frameless=true;

    pathToData=posArgs[0];
}

int main(int argc, char** argv)
{
    [[maybe_unused]] UTF8Console utf8console;

    QApplication app(argc, argv);
    app.setApplicationName("ShowMySky");
    app.setApplicationVersion(APP_VERSION);

    QSurfaceFormat format;
    format.setVersion(3,3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);

    try
    {
        windowSize=app.primaryScreen()->size()/1.6;
        handleCmdLine();

        const auto tools=new ToolsWidget;
        const auto glWidget=new GLWidget(pathToData, tools);
        const auto mainWin=new MainWindow(tools);

        mainWin->setAttribute(Qt::WA_DeleteOnClose);
        if(frameless)
            mainWin->setWindowFlag(Qt::FramelessWindowHint);
        mainWin->setCentralWidget(glWidget);
        mainWin->resize(windowSize);
        mainWin->show();
        if(detachedTools)
            tools->setFloating(true);
        return app.exec();
    }
    catch(ShowMySky::Error const& ex)
    {
        QMessageBox::critical(nullptr, ex.errorType(), ex.what());
        return 1;
    }
    catch(MustQuit&)
    {
        return 2;
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
