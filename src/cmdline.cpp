#include "cmdline.hpp"

#include <iostream>
#include <QCommandLineParser>
#include <QFile>

#include "data.hpp"
#include "util.hpp"

unsigned long long getUInt(QString const& value, unsigned long long min, unsigned long long max, QString const& filename, int lineNumber)
{
    bool ok;
    const auto x=value.toULongLong(&ok);
    if(!ok)
    {
        std::cerr << filename.toStdString() << ":" << lineNumber << ": can't parse integer\n";
    }
    if(x<min || x>max)
    {
        std::cerr << filename.toStdString() << ":" << lineNumber << ": integer out of range. Valid range is [" << min << ".." << max << "]\n";
        throw MustQuit{};
    }
    return x;
}

struct Quantity
{
    virtual std::string name() const = 0;
    virtual std::map<QString, double> units() const = 0;
    virtual QString basicUnit() const = 0;
};

struct LengthQuantity : Quantity
{
    std::string name() const override { return "length"; }
    std::map<QString, double> units() const override
    {
        return {
                {"nm",1e-9},
                {"um",1e-6},
                {"mm",1e-3},
                { "m",1e+0},
                {"km",1e+3},
                {"Mm",1e+6},
                {"Gm",1e+9},
                {"AU",astronomicalUnit},
               };
    }
    QString basicUnit() const override { return "m"; }
};

struct ReciprocalLengthQuantity : Quantity
{
    std::string name() const override { return "reciprocal length"; }
    std::map<QString, double> units() const override
    {
        return {
                {"nm^-1",1e+9},
                {"um^-1",1e+6},
                {"mm^-1",1e+3},
                { "m^-1",1e-0},
                {"km^-1",1e-3},
                {"Mm^-1",1e-6},
                {"Gm^-1",1e-9},
               };
    }
    QString basicUnit() const override { return "m^-1"; }
};

struct AreaQuantity : Quantity
{
    std::string name() const override { return "area"; }
    std::map<QString, double> units() const override
    {
        return {
                {"am^2",1e-36},
                {"fm^2",1e-30},
                {"pm^2",1e-24},
                {"nm^2",1e-18},
                {"um^2",1e-12},
                {"mm^2",1e-6},
                {"cm^2",1e-4},
                { "m^2",1e-0},
                {"km^2",1e+6},
                {"Mm^2",1e+12},
                {"Gm^2",1e+18},
               };
    }
    QString basicUnit() const override { return "m^2"; }
};

struct DimensionlessQuantity {};

double getQuantity(QString const& value, double min, double max, DimensionlessQuantity const&, QString const& filename, int lineNumber)
{
    bool ok;
    const auto x=value.toDouble(&ok);
    if(!ok)
    {
        std::cerr << filename.toStdString() << ":" << lineNumber << ": failed to parse number\n";
        throw MustQuit{};
    }
    if(x<min || x>max)
    {
        std::cerr << filename.toStdString() << ":" << lineNumber << ": value " << x << " is out of range. Valid range is [" << min << ".." << max << "].\n";
        throw MustQuit{};
    }
    return x;
}

double getQuantity(QString const& value, double min, double max, Quantity const& quantity, QString const& filename, int lineNumber)
{
    auto regex=QRegExp("(-?[0-9.]+) *([a-zA-Z][a-zA-Z^-0-9]*)");
    if(!regex.exactMatch(value))
    {
        std::cerr << filename.toStdString() << ":" << lineNumber << ": bad format of " << quantity.name() << " quantity. Must be `NUMBER UNIT', e.g. `30.2 km' (without the quotes).\n";
        throw MustQuit{};
    }
    bool ok;
    const auto x=regex.cap(1).toDouble(&ok);
    if(!ok)
    {
        std::cerr << filename.toStdString() << ":" << lineNumber << ": failed to parse numeric part of the quantity\n";
        throw MustQuit{};
    }
    const auto units=quantity.units();
    const auto unit=regex.cap(2).trimmed();
    const auto scaleIt=units.find(unit);
    if(scaleIt==units.end())
    {
        std::cerr << filename.toStdString() << ":" << lineNumber << ": unrecognized " << quantity.name() << " unit " << unit.toStdString() << ". Can be one of ";
        for(auto it=units.begin(); it!=units.end(); ++it)
        {
            if(it!=units.begin()) std::cerr << ',';
            std::cerr << it->first.toStdString();
        }
        std::cerr << ".\n";
        throw MustQuit{};
    }
    const auto finalX = x * scaleIt->second;
    if(finalX<min || finalX>max)
    {
        std::cerr << filename.toStdString() << ":" << lineNumber << ": value " << finalX << " " << quantity.basicUnit().toStdString()
                  << " is out of range. Valid range is [" << min << ".." << max << "] " << quantity.basicUnit().toStdString() << ".\n";
        throw MustQuit{};
    }
    return finalX;
}

QString readGLSLFunctionBody(QTextStream& stream, const QString filename, int& lineNumber)
{
    QString function;
    const QRegExp startEndMarker("^\\s*```\\s*$");
    bool begun=false;
    for(auto line=stream.readLine(); !line.isNull(); line=stream.readLine(), ++lineNumber)
    {
        if(!begun)
        {
            if(!startEndMarker.exactMatch(line))
            {
                std::cerr << filename.toStdString() << ":" << lineNumber << ": function body must start and end with triple backtick placed on a separate line.\n";
                throw MustQuit{};
            }
            begun=true;
            continue;
        }

        if(!startEndMarker.exactMatch(line))
            function.append(line+'\n');
        else
            break;
    }
    return function;
}

std::vector<glm::vec4> getSpectrum(QString const& line, const GLfloat min, const GLfloat max,
                                 QString const& filename, const int lineNumber, bool checkSize=true)
{
    const auto items=line.split(',');
    if(checkSize && std::size_t(items.size()) != allWavelengths.size()*pointsPerWavelengthItem)
    {
            std::cerr << filename.toStdString() << ":" << lineNumber << ": spectrum has " << items.size() << " entries, but there are " << allWavelengths.size()*pointsPerWavelengthItem << " wavelengths\n";
            throw MustQuit{};
    }
    if(items.size()%4)
    {
            std::cerr << filename.toStdString() << ":" << lineNumber << ": spectrum length must be a multiple of 4\n";
            throw MustQuit{};
    }
    std::vector<GLfloat> values;
    for(int i=0; i<items.size(); ++i)
    {
        bool ok;
        const auto value=items[i].toFloat(&ok);
        if(!ok)
        {
            std::cerr << filename.toStdString() << ":" << lineNumber << ": failed to parse entry #" << i+1 << "\n";
            throw MustQuit{};
        }
        if(value<min)
        {
            std::cerr << filename.toStdString() << ":" << lineNumber << ": spectrum point #" << i+1 << " is less than minimally allowed: " << value << " < " << min << "\n";
            throw MustQuit{};
        }
        if(value>max)
        {
            std::cerr << filename.toStdString() << ":" << lineNumber << ": spectrum point #" << i+1 << " is greater than maximally allowed: " << value << " > " << max << "\n";
            throw MustQuit{};
        }
        values.emplace_back(value);
    }
    std::vector<glm::vec4> spectrum;
    for(unsigned i=0; i<values.size(); i+=4)
        spectrum.emplace_back(values[i+0], values[i+1], values[i+2], values[i+3]);
    return spectrum;
}

ScattererDescription parseScatterer(QTextStream& stream, QString const& name, QString const& filename, int& lineNumber)
{
    ScattererDescription description(name);
    bool begun=false;
    for(auto line=stream.readLine(); !line.isNull(); line=stream.readLine(), ++lineNumber)
    {
        const auto codeAndComment=line.split('#');
        assert(codeAndComment.size());
        if(codeAndComment[0].trimmed().isEmpty())
            continue;
        const auto keyValue=codeAndComment[0].split(':');

        if(!begun)
        {
            if(keyValue.size()!=1 || keyValue[0] != "{")
            {
                std::cerr << filename.toStdString() << ":" << lineNumber << ": scatterer description must begin with a '{'\n";
                throw MustQuit{};
            }
            begun=true;
            continue;
        }
        if(keyValue.size()==1 && keyValue[0]=="}")
            break;

        if(keyValue.size()!=2)
        {
            std::cerr << filename.toStdString() << ":" << lineNumber << ": error: not a key:value pair\n";
            throw MustQuit{};
        }
        const auto key=keyValue[0].simplified().toLower();
        const auto value=keyValue[1].trimmed();

        if(key=="cross section at 1 um")
            description.crossSectionAt1um=getQuantity(value,1e-35,1,AreaQuantity{},filename,lineNumber);
        else if(key=="angstrom exponent")
            description.angstromExponent=getQuantity(value,-10,10,DimensionlessQuantity{},filename,lineNumber);
        else if(key=="number density")
            description.numberDensity=readGLSLFunctionBody(stream,filename,++lineNumber);
        else if(key=="phase function")
            description.phaseFunction=readGLSLFunctionBody(stream,filename,++lineNumber);
    }
    if(!description.valid())
    {
        std::cerr << "Description of scatterer \"" << name.toStdString() << "\" is incomplete\n";
        throw MustQuit{};
    }

    return description;
}

AbsorberDescription parseAbsorber(QTextStream& stream, QString const& name, QString const& filename, int& lineNumber)
{
    AbsorberDescription description(name);

    bool begun=false;
    for(auto line=stream.readLine(); !line.isNull(); line=stream.readLine(), ++lineNumber)
    {
        const auto codeAndComment=line.split('#');
        assert(codeAndComment.size());
        if(codeAndComment[0].trimmed().isEmpty())
            continue;
        const auto keyValue=codeAndComment[0].split(':');

        if(!begun)
        {
            if(keyValue.size()!=1 || keyValue[0] != "{")
            {
                std::cerr << filename.toStdString() << ":" << lineNumber << ": absorber description must begin with a '{'\n";
                throw MustQuit{};
            }
            begun=true;
            continue;
        }
        if(keyValue.size()==1 && keyValue[0]=="}")
            break;

        if(keyValue.size()!=2)
        {
            std::cerr << filename.toStdString() << ":" << lineNumber << ": error: not a key:value pair\n";
            throw MustQuit{};
        }
        const auto key=keyValue[0].simplified().toLower();
        const auto value=keyValue[1].trimmed();

        if(key=="number density")
            description.numberDensity=readGLSLFunctionBody(stream,filename,++lineNumber);
        else if(key=="cross section")
            description.absorptionCrossSection=getSpectrum(value,0,10,filename,lineNumber);
    }
    if(!description.valid())
    {
        std::cerr << "Description of absorber \"" << name.toStdString() << "\" is incomplete\n";
        throw MustQuit{};
    }

    return description;
}

void handleCmdLine()
{
    QCommandLineParser parser;
    const auto atmoDescrOpt="atmo-descr";
    parser.addPositionalArgument(atmoDescrOpt, "Atmosphere description file", "atmosphere-description.atmo");
    parser.addVersionOption();
    parser.addHelpOption();
    const QCommandLineOption textureOutputDirOpt("out-dir","Directory for the textures computed","output directory",".");
    const QCommandLineOption dbgSaveTransmittancePngOpt("save-xmittance-png","Save transmittance textures as PNG (for debugging)");
    const QCommandLineOption dbgSaveGroundIrradianceOpt("save-irradiance","Save ground irradiance textures as F32 and PNG (for debugging)");
    const QCommandLineOption dbgSaveScatDensityOrder2FromGroundOpt("save-scat-density2-from-ground","Save order 2 scattering density from ground (for debugging)");
    const QCommandLineOption dbgSaveScatDensityOpt("save-scat-density","Save scattering density textures (for debugging)");
    const QCommandLineOption dbgSaveDeltaScatteringOpt("save-delta-scattering","Save delta scattering textures for each order (for debugging)");
    const QCommandLineOption dbgSaveAccumScatteringOpt("save-accum-scattering","Save accumulated multiple scattering textures for each order (for debugging)");
    parser.addOptions({textureOutputDirOpt,
                       dbgSaveTransmittancePngOpt,
                       dbgSaveGroundIrradianceOpt,
                       dbgSaveScatDensityOrder2FromGroundOpt,
                       dbgSaveScatDensityOpt,
                       dbgSaveDeltaScatteringOpt,
                       dbgSaveAccumScatteringOpt,
                      });
    parser.process(*qApp);

    if(parser.isSet(textureOutputDirOpt))
        textureOutputDir=parser.value(textureOutputDirOpt).toStdString();
    if(parser.isSet(dbgSaveTransmittancePngOpt))
        dbgSaveTransmittancePng=true;
    if(parser.isSet(dbgSaveGroundIrradianceOpt))
        dbgSaveGroundIrradiance=true;
    if(parser.isSet(dbgSaveScatDensityOrder2FromGroundOpt))
        dbgSaveScatDensityOrder2FromGround=true;
    if(parser.isSet(dbgSaveScatDensityOpt))
        dbgSaveScatDensity=true;
    if(parser.isSet(dbgSaveDeltaScatteringOpt))
        dbgSaveDeltaScattering=true;
    if(parser.isSet(dbgSaveAccumScatteringOpt))
        dbgSaveAccumScattering=true;

    const auto posArgs=parser.positionalArguments();
    if(posArgs.size()>1)
    {
        std::cerr << "Too many arguments\n";
        throw MustQuit{};
    }
    if(posArgs.isEmpty())
    {
        std::cerr << parser.helpText().toStdString();
        throw MustQuit{};
    }

    const auto atmoDescrFileName=posArgs[0];
    QFile atmoDescr(atmoDescrFileName);
    if(!atmoDescr.open(QIODevice::ReadOnly))
    {
        std::cerr << "Failed to open atmosphere description file: " << atmoDescr.errorString().toStdString() << '\n';
        throw MustQuit{};
    }
    QTextStream stream(&atmoDescr);
    int lineNumber=1;
    for(auto line=stream.readLine(); !line.isNull(); line=stream.readLine(), ++lineNumber)
    {
        QRegExp scattererDescriptionKey("^scatterer \"([^\"]+)\"$");
        QRegExp absorberDescriptionKey("^absorber \"([^\"]+)\"$");
        const auto codeAndComment=line.split('#');
        assert(codeAndComment.size());
        if(codeAndComment[0].trimmed().isEmpty())
            continue;
        const auto keyValue=codeAndComment[0].split(':');
        if(keyValue.size()!=2)
        {
            std::cerr << atmoDescrFileName.toStdString() << ":" << lineNumber << ": error: not a key:value pair\n";
            throw MustQuit{};
        }
        const auto key=keyValue[0].simplified().toLower();
        const auto value=keyValue[1].trimmed();
        if(key=="transmittance texture size for cos(vza)")
            transmittanceTexW=getUInt(value,1,std::numeric_limits<GLsizei>::max(), atmoDescrFileName, lineNumber);
        else if(key=="transmittance texture size for altitude")
            transmittanceTexH=getUInt(value,1,std::numeric_limits<GLsizei>::max(), atmoDescrFileName, lineNumber);
        else if(key=="transmittance integration points")
            numTransmittanceIntegrationPoints=getUInt(value,1,INT_MAX, atmoDescrFileName, lineNumber);
        else if(key=="radial integration points")
            radialIntegrationPoints=getUInt(value,1,INT_MAX, atmoDescrFileName, lineNumber);
        else if(key=="angular integration points per half revolution")
            angularIntegrationPointsPerHalfRevolution=getUInt(value,1,INT_MAX, atmoDescrFileName, lineNumber);
        else if(key=="irradiance texture size for cos(sza)")
            irradianceTexW=getUInt(value,1,std::numeric_limits<GLsizei>::max(), atmoDescrFileName, lineNumber);
        else if(key=="irradiance texture size for altitude")
            irradianceTexH=getUInt(value,1,std::numeric_limits<GLsizei>::max(), atmoDescrFileName, lineNumber);
        else if(key=="scattering texture size for cos(vza)")
        {
            const auto integer=getUInt(value,1,std::numeric_limits<GLsizei>::max(), atmoDescrFileName, lineNumber);
            if(integer%2)
            {
                std::cerr << atmoDescrFileName.toStdString() << ":" << lineNumber << ": value for \"" << key.toStdString() << "\" must be even (shaders rely on this)\n";
                throw MustQuit{};
            }
            scatteringTextureSize[0]=integer;
        }
        else if(key=="scattering texture size for dot(view,sun)")
            scatteringTextureSize[1]=getUInt(value,1,std::numeric_limits<GLsizei>::max(), atmoDescrFileName, lineNumber);
        else if(key=="scattering texture size for cos(sza)")
            scatteringTextureSize[2]=getUInt(value,1,std::numeric_limits<GLsizei>::max(), atmoDescrFileName, lineNumber);
        else if(key=="scattering texture size for altitude")
            scatteringTextureSize[3]=getUInt(value,1,std::numeric_limits<GLsizei>::max(), atmoDescrFileName, lineNumber);
        else if(key=="earth radius")
            earthRadius=getQuantity(value,1,1e10,LengthQuantity{},atmoDescrFileName,lineNumber);
        else if(key=="atmosphere height")
            atmosphereHeight=getQuantity(value,1,1e6,LengthQuantity{},atmoDescrFileName,lineNumber);
        else if(key=="earth-sun distance")
        {
            earthSunDistance=getQuantity(value,0.5*AU,1e20*AU,LengthQuantity{},atmoDescrFileName,lineNumber);
            // Here we don't take into account the fact that camera is not in the center of the Earth. It's not too
            // important until we simulate eclipsed atmosphere, when we'll have to recompute Sun angular radius for
            // each camera position.
            sunAngularRadius=sunRadius/earthSunDistance;
        }
        else if(key=="wavelengths")
            allWavelengths=getSpectrum(value,1e2,1e5,atmoDescrFileName,lineNumber,false);
        else if(key=="solar irradiance at toa")
            solarIrradianceAtTOA=getSpectrum(value,0,1e3,atmoDescrFileName,lineNumber);
        else if(key.contains(scattererDescriptionKey))
            scatterers.emplace_back(parseScatterer(stream, scattererDescriptionKey.cap(1), atmoDescrFileName,++lineNumber));
        else if(key.contains(absorberDescriptionKey))
            absorbers.emplace_back(parseAbsorber(stream, absorberDescriptionKey.cap(1), atmoDescrFileName,++lineNumber));
        else if(key=="scattering orders")
            scatteringOrdersToCompute=getQuantity(value,1,100, DimensionlessQuantity{},atmoDescrFileName,lineNumber);
        else if(key=="ground albedo")
        {
            groundAlbedo=getSpectrum(value, 0, 1, atmoDescrFileName, lineNumber);
        }
        else
            std::cerr << "WARNING: Unknown key: " << key.toStdString() << "\n";
    }
    if(!stream.atEnd())
    {
        std::cerr << atmoDescrFileName.toStdString() << ":" << lineNumber << ": error: failed to read file\n";
        throw MustQuit{};
    }
    if(allWavelengths.empty())
    {
        std::cerr << "Wavelengths aren't specified in atmosphere description\n";
        throw MustQuit{};
    }
    if(solarIrradianceAtTOA.empty())
    {
        std::cerr << "Solar irradiance at TOA isn't specified in atmosphere description\n";
        throw MustQuit{};
    }
    if(groundAlbedo.empty())
    {
        std::cerr << "Warning: ground albedo was not specified, assuming 100% white.\n";
        groundAlbedo=std::vector<glm::vec4>(allWavelengths.size(), glm::vec4(1));
    }
}
