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
#include "GLWidget.hpp"
#include "ToolsWidget.hpp"
#include "util.hpp"

QString pathToData;

AtmosphereRenderer::Parameters parseParams(QString const& pathToData)
{
    const auto filename=pathToData+"/params.txt";
    QFile file(filename);
    if(!file.open(QFile::ReadOnly))
        throw DataLoadError{QObject::tr("Failed to open \"%1\": %2")
                                        .arg(filename).arg(file.errorString())};

    AtmosphereRenderer::Parameters params;
    int lineNumber=1;
    for(auto line=file.readLine(); !line.isEmpty() && !file.error(); line=file.readLine(), ++lineNumber)
    {
        if(line=="\n") continue;
        const auto keyval=line.split(':');
        if(keyval.size()!=2)
            throw DataLoadError{QObject::tr("Bad entry in \"%1\": must be a key:value pair")
                                            .arg(filename)};

        const QString &key=keyval[0], &value=keyval[1].trimmed();
        if(key=="wavelengths")
        {
            const auto wlStr=value.split(',');
            if(wlStr.isEmpty() || wlStr.size()%4 != 0)
            {
                throw DataLoadError{QObject::tr("Bad wavelengths entry in \"%1\": value must be non-empty "
                                                "and contain a multiple of 4 numbers").arg(filename)};
            }

            params.wavelengthSetCount=wlStr.size()/4;
        }
        else if(key=="atmosphere height")
        {
            bool ok=false;
            params.atmosphereHeight=value.toFloat(&ok);
            if(!ok) throw DataLoadError{QObject::tr("Failed to parse atmosphere height in \"%1\"")
                                                    .arg(filename)};

            if(params.atmosphereHeight<=0)
                throw DataLoadError{QObject::tr("Atmosphere height must be positive")};
        }
        else if(key=="Earth radius")
        {
            bool ok=false;
            params.earthRadius=value.toFloat(&ok);
            if(!ok) throw DataLoadError{QObject::tr("Failed to parse Earth radius in \"%1\"")
                                                    .arg(filename)};

            if(params.earthRadius<=0)
                throw DataLoadError{QObject::tr("Earth radius must be positive")};
        }
        else if(key=="Earth-Moon distance")
        {
            bool ok=false;
            params.earthMoonDistance=value.toFloat(&ok);
            if(!ok) throw DataLoadError{QObject::tr("Failed to parse Earth-Moon distance in \"%1\"")
                                                    .arg(filename)};

            if(params.earthMoonDistance<=0)
                throw DataLoadError{QObject::tr("Earth-Moon distance must be positive")};
        }
        else if(key=="scatterers")
        {
            if(!value.startsWith("{ ") || !value.endsWith("; }"))
            {
                throw DataLoadError{QObject::tr("Failed to parse parameters of scatterers in \"%1\": expected "
                                                "value with format \"{ information; }\", got\n%2")
                                                    .arg(filename).arg(value)};
            }
            const auto list=value.mid(2, value.size()-5).split(";");
            const QRegularExpression expr("^ *\"([^\"]+)\" *{ *phase function ([a-z]+) *} *$");
            for(const auto& scattererParams : list)
            {
                const auto match=expr.match(scattererParams);
                if(!match.hasMatch())
                {
                    throw DataLoadError{QObject::tr("Failed to parse parameters of scatterers in \"%1\": "
                                                    "\"%2\" doesn't match expected pattern \"{ phase function TYPE }\"")
                                                    .arg(filename).arg(scattererParams)};
                }
                params.scatterers.emplace(match.captured(1), parsePhaseFunctionType(match.captured(2), filename, lineNumber));
            }
        }
        else
        {
            throw DataLoadError{QObject::tr("Unknown key \"%1\" in \"%2\"")
                                            .arg(key).arg(filename)};
        }
    }

    if(file.error())
    {
        throw DataLoadError{QObject::tr("Failed to read parameters from \"%1\": %2")
                                        .arg(filename).arg(file.errorString())};
    }
    if(!params.wavelengthSetCount)
        throw DataLoadError{QObject::tr("Failed to find wavelengths in \"%1\"").arg(filename)};
    if(std::isnan(params.atmosphereHeight))
        throw DataLoadError{QObject::tr("Failed to find atmosphere height in \"%1\"").arg(filename)};
    if(std::isnan(params.earthRadius))
        throw DataLoadError{QObject::tr("Failed to find Earth radius in \"%1\"").arg(filename)};
    if(std::isnan(params.earthMoonDistance))
        throw DataLoadError{QObject::tr("Failed to find Earth-Moon distance in \"%1\"").arg(filename)};

    return params;
}

void handleCmdLine()
{
    QCommandLineParser parser;
    parser.addPositionalArgument("path to data", "Path to atmosphere textures");
    parser.addVersionOption();
    parser.addHelpOption();

    parser.process(*qApp);

    const auto posArgs=parser.positionalArguments();
    if(posArgs.size()>1)
        throw BadCommandLine{QObject::tr("Too many arguments")};
    if(posArgs.isEmpty())
        throw BadCommandLine{parser.helpText()};

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
        handleCmdLine();
        const auto params=parseParams(pathToData);

        QMainWindow mainWin;
        const auto tools=new ToolsWidget(params.atmosphereHeight);
        const auto glWidget=new GLWidget(pathToData, params, tools);

        mainWin.setCentralWidget(glWidget);
        mainWin.addDockWidget(Qt::RightDockWidgetArea, tools);
        mainWin.resize(app.primaryScreen()->size()/1.6);
        mainWin.show();
        return app.exec();
    }
    catch(Error const& ex)
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
