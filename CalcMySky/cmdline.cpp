#include "cmdline.hpp"

#include <iomanip>
#include <optional>
#include <iostream>
#include <QFileInfo>
#include <QCommandLineParser>
#include <QRegularExpression>
#include <QFile>
#ifdef Q_OS_UNIX
#   include <sys/ioctl.h>
#   include <unistd.h>
#   include <stdio.h>
#elif defined Q_OS_WIN
#   include <windows.h>
#endif

#include "data.hpp"
#include "util.hpp"
#include "../ShowMySky/api/ShowMySky/AtmosphereRenderer.hpp"

namespace
{

QStringList wordWrap(QString const& longLine, const int maxWidth)
{
    const auto words=longLine.split(' ');
    QStringList lines;
    int col=0;
    QString currentLine;
    const auto endCurrentLine=[&lines, &currentLine, &col, maxWidth]
    {
        currentLine.remove(QRegularExpression(" +$"));
        if(currentLine.length() < maxWidth)
            currentLine += '\n';
        lines << std::move(currentLine);
        currentLine.clear();
        col=0;
    };
    for(const auto& word : words)
    {
        if(col+word.length()+1 < maxWidth)
        {
            currentLine += word;
            currentLine += ' ';
            col += word.length()+1;
        }
        else if(col+word.length()+1 == maxWidth)
        {
            currentLine += word;
            endCurrentLine();
        }
        else if(col+word.length() == maxWidth)
        {
            currentLine += word;
            endCurrentLine();
        }
        else if(word.length()+1 < maxWidth)
        {
            if(!currentLine.isEmpty())
                endCurrentLine();
            currentLine = word+' ';
            col = word.length()+1;
        }
        else if(word.length()+1 == maxWidth)
        {
            if(!currentLine.isEmpty())
                endCurrentLine();
            lines << word+'\n';
        }
        else if(word.length() == maxWidth)
        {
            if(!currentLine.isEmpty())
                endCurrentLine();
            lines << word;
        }
        else
        {
            if(!currentLine.isEmpty())
                endCurrentLine();
            for(int i=0; i<word.length(); i+=maxWidth)
                lines << word.mid(i, maxWidth);
            if(lines.back().length() < maxWidth)
            {
                currentLine = lines.takeLast()+' ';
                col=currentLine.length();
            }
        }
    }
    if(!currentLine.isEmpty())
        endCurrentLine();
    return lines;
}

int getConsoleWidth(std::ostream& s)
{
    int width=INT_MAX; // fallback is to not wrap any output
#ifdef Q_OS_UNIX
    struct winsize w;
    if(ioctl(&s==&std::cout ? STDOUT_FILENO : STDERR_FILENO, TIOCGWINSZ, &w)<0)
        return width;
    width=w.ws_col;
#elif defined Q_OS_WIN
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(&s==&std::cout ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE), &csbi);
    width=csbi.dwSize.X;
#endif
    return width;
}

void showHelp(std::ostream& s, QList<QCommandLineOption> const& options, QString const& positionalArgSyntax)
{
    s << "Usage: " << qApp->arguments()[0] << " [OPTION]... " << positionalArgSyntax << " --out-dir /path/to/output/dir\n";
    s << "\nOptions:\n";

    std::vector<std::pair<QString,QString>> allOptionsFormatted;
    int maxNameLen=0;
    for(const auto& option : options)
    {
        if(option.flags() & QCommandLineOption::HiddenFromHelp)
            continue;
        QString namesFormatted="  ";
        for(const auto& name : option.names())
        {
            const int numDashes = name.size()==1 ? 1 : 2;
            namesFormatted += QLatin1String("--", numDashes) + name + ", ";
        }
        if(!namesFormatted.isEmpty())
            namesFormatted.chop(2); // remove trailing ", "
        const auto valueName=option.valueName();
        if(!valueName.isEmpty())
            namesFormatted += " <" + valueName + ">";
        if(namesFormatted.size() > maxNameLen)
            maxNameLen=namesFormatted.size();
        allOptionsFormatted.emplace_back(std::make_pair(namesFormatted, option.description()));
    }

    const auto consoleWidth=getConsoleWidth(s);
    for(const auto& [name, explanation] : allOptionsFormatted)
    {
        const auto namesColumnWidth=maxNameLen+2;
        if(consoleWidth <= namesColumnWidth*3/2) // Attempts to wrap are useless in this case
        {
            s << name.mid(1) << "\n    " << explanation << '\n';
            continue;
        }
        s << std::setw(namesColumnWidth) << std::left << name;
        // We'll indent wrapped lines to make it easy to spot where the next option begins
        s << explanation.left(1);
        const auto wrappedExplanation=wordWrap(explanation.mid(1), consoleWidth-namesColumnWidth-1);
        for(const auto& line : wrappedExplanation)
        {
            s << line;
            if(&line != &wrappedExplanation.back())
                s << std::string(namesColumnWidth+1, ' ');
        }
    }
}

}

void handleCmdLine()
{
    QCommandLineParser parser;
    // QCommandLineParser::addHelpOption() results in ugly help wrapped at 79 columns, so not using it.
    const QCommandLineOption helpOpt({"h","help"}, "Display this help and exit");
    // We do it a bit differently from QCommandLineParser, so not using addVersionOption()
    const QCommandLineOption versionOpt({"v","version"}, "Display version and exit");
    const QCommandLineOption openglDebug("opengl-debug","Install a GL_KHR_debug message callback and print all the messages from OpenGL");
    const QCommandLineOption openglDebugFull("opengl-debug-full","Like --opengl-debug, but don't hide notification-level messages");
    const QCommandLineOption printOpenGLInfoAndQuit("opengl-info","Print OpenGL info and quit");
    const QCommandLineOption textureOutputDirOpt("out-dir","Directory for the textures computed","output directory",".");
    const QCommandLineOption saveResultAsRadianceOpt("radiance","Save result as radiance instead of XYZW components");
    const QCommandLineOption textureSavePrecisionOpt("texture-save-precision","Number of bits of precision when saving 3D textures, from 1 to 24. Smaller number improves compressibility. Too small destroys fidelity.","bits");
    const QCommandLineOption dbgNoSaveTexturesOpt("no-save-tex","Don't save textures, only save shaders and other fast-to-compute data; don't run the long 4D "
                                                                "textures computations (for debugging)");
    const QCommandLineOption dbgNoEDSTexturesOpt("no-eds-tex","Don't compute/save eclipsed double scattering textures (for debugging)");
    const QCommandLineOption dbgSaveGroundIrradianceOpt("save-irradiance","Save intermediate ground irradiance textures (for debugging)");
    const QCommandLineOption dbgSaveScatDensityOrder2FromGroundOpt("save-scat-density2-from-ground","Save order 2 scattering density from ground (for debugging)");
    const QCommandLineOption dbgSaveScatDensityOpt("save-scat-density","Save scattering density textures (for debugging)");
    const QCommandLineOption dbgSaveDeltaScatteringOpt("save-delta-scattering","Save delta scattering textures for each order (for debugging)");
    const QCommandLineOption dbgSaveAccumScatteringOpt("save-accum-scattering","Save accumulated multiple scattering textures for each order (for debugging)");
    const QCommandLineOption dbgSaveLightPollutionIntermediateOpt("save-light-pollution","Save intermediate light pollution textures (for debugging)");
    const QList options{
                        helpOpt,
                        versionOpt,
                        textureOutputDirOpt,
                        saveResultAsRadianceOpt,
                        textureSavePrecisionOpt,
                        dbgNoEDSTexturesOpt,
                        dbgNoSaveTexturesOpt,
                        printOpenGLInfoAndQuit,
                        openglDebug,
                        openglDebugFull,
                        dbgSaveGroundIrradianceOpt,
                        dbgSaveScatDensityOrder2FromGroundOpt,
                        dbgSaveScatDensityOpt,
                        dbgSaveDeltaScatteringOpt,
                        dbgSaveAccumScatteringOpt,
                        dbgSaveLightPollutionIntermediateOpt,
                       };
    parser.addOptions(options);
    const std::pair<QString, QString> positionalArgument("atmosphere-description.atmo",
                                                         "Atmosphere description file");
    parser.addPositionalArgument("atmo-descr", positionalArgument.second, positionalArgument.first);
    parser.process(*qApp);

    if(parser.isSet(helpOpt))
    {
        showHelp(std::cout, options, positionalArgument.first);
        throw MustQuit{0};
    }
    if(parser.isSet(versionOpt))
    {
        std::cout << qApp->applicationName() << ' ' << qApp->applicationVersion() << '\n';
        std::cout << "Compiled against Qt " << QT_VERSION_MAJOR << "." << QT_VERSION_MINOR << "." << QT_VERSION_PATCH << "\n";
        std::cout << "ABI version: " << ShowMySky_ABI_version << "\n";
        std::cout << "Atmosphere description format version: " << AtmosphereParameters::FORMAT_VERSION << "\n";
        throw MustQuit{0};
    }
    if(parser.isSet(textureOutputDirOpt))
        atmo.textureOutputDir=parser.value(textureOutputDirOpt).toStdString();
    if(parser.isSet(dbgNoSaveTexturesOpt))
        opts.dbgNoSaveTextures=true;
    if(parser.isSet(dbgNoEDSTexturesOpt))
        opts.dbgNoEDSTextures=true;
    if(parser.isSet(saveResultAsRadianceOpt))
        opts.saveResultAsRadiance=true;
    if(parser.isSet(dbgSaveGroundIrradianceOpt))
        opts.dbgSaveGroundIrradiance=true;
    if(parser.isSet(dbgSaveScatDensityOrder2FromGroundOpt))
        opts.dbgSaveScatDensityOrder2FromGround=true;
    if(parser.isSet(dbgSaveScatDensityOpt))
        opts.dbgSaveScatDensity=true;
    if(parser.isSet(dbgSaveDeltaScatteringOpt))
        opts.dbgSaveDeltaScattering=true;
    if(parser.isSet(dbgSaveAccumScatteringOpt))
        opts.dbgSaveAccumScattering=true;
    if(parser.isSet(dbgSaveLightPollutionIntermediateOpt))
        opts.dbgSaveLightPollutionIntermediateTextures=true;
    if(parser.isSet(openglDebug))
        opts.openglDebug=true;
    if(parser.isSet(openglDebugFull))
        opts.openglDebugFull=true;
    if(parser.isSet(printOpenGLInfoAndQuit))
        opts.printOpenGLInfoAndQuit=true;
    if(parser.isSet(textureSavePrecisionOpt))
    {
        bool ok=false;
        opts.textureSavePrecision=parser.value(textureSavePrecisionOpt).toUInt(&ok);
        if(!ok)
        {
            std::cerr << "Failed to parse texture save precision\n";
            throw MustQuit{};
        }
        if(opts.textureSavePrecision < 1 || opts.textureSavePrecision > 24)
        {
            std::cerr << "Texture save precision must be from 1 to 24.\n";
            throw MustQuit{};
        }
    }

    const auto posArgs=parser.positionalArguments();
    if(posArgs.size()>1)
    {
        std::cerr << "Too many arguments\n";
        throw MustQuit{};
    }
    if(!posArgs.isEmpty())
    {
        const auto atmoDescrFileName=posArgs[0];
        atmo.parse(atmoDescrFileName, AtmosphereParameters::ForceNoEDSTextures{opts.dbgNoEDSTextures});
    }
    else if(!opts.printOpenGLInfoAndQuit)
    {
        showHelp(std::cerr, options, positionalArgument.first);
        throw MustQuit{};
    }
}
