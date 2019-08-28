#include <iostream>
#include <fstream>
#include <memory>
#include <map>
#include <set>

#include <QOpenGLFunctions_3_3_Core>
#include <QCommandLineParser>
#include <QOffscreenSurface>
#include <QProgressDialog>
#include <QSurfaceFormat>
#include <QOpenGLShader>
#include <QApplication>
#include <QVector4D>
#include <QRegExp>
#include <QFile>
#include <QDir>

#include <glm/glm.hpp>

#include "config.h"

QString withHeadersIncluded(QString src, QString filename);

constexpr double astronomicalUnit=149'597'870'700.; // m
constexpr double AU=astronomicalUnit;

constexpr auto allWavelengths=[]() constexpr
{
    constexpr float wlMin=360, wlMax=830;
    std::array<float, 16> arr{};
    const auto step=(wlMax-wlMin)/(arr.size()-1);
    for(unsigned n=0; n<arr.size(); ++n)
        arr[n]=wlMin+step*n;
    return arr;
}();
/* Data taken from https://www.nrel.gov/grid/solar-resource/assets/data/astmg173.zip
 * which is linked to at https://www.nrel.gov/grid/solar-resource/spectra-am1.5.html .
 * Values are in W/(m^2*nm).
 */
constexpr decltype(allWavelengths) solarIrradiance=
   {1.037,1.249,1.684,1.975,
    1.968,1.877,1.854,1.818,
    1.723,1.604,1.516,1.408,
    1.309,1.23,1.142,1.062};
/* Data taken from http://www.iup.uni-bremen.de/gruppen/molspec/downloads/serdyuchenkogorshelevversionjuly2013.zip
 * which is linked to at http://www.iup.uni-bremen.de/gruppen/molspec/databases/referencespectra/o3spectra2011/index.html .
 * Data are for 233K. Values are in m^2/molecule.
 */
constexpr decltype(allWavelengths) ozoneAbsCrossSection=
   {1.394e-26,6.052e-28,4.923e-27,2.434e-26,
    7.361e-26,1.831e-25,3.264e-25,4.514e-25,
    4.544e-25,2.861e-25,1.571e-25,7.902e-26,
    4.452e-26,2.781e-26,1.764e-26,5.369e-27};
static_assert(allWavelengths.size()%4==0,"Non-round number of wavelengths");

constexpr double sunRadius=696350e3; /* m */

std::map<QString, std::unique_ptr<QOpenGLShader>> allShaders;
constexpr char DENSITIES_SHADER_FILENAME[]="densities.frag";
std::set<QString> internalShaders
{
    DENSITIES_SHADER_FILENAME
};
GLuint vao, vbo;
enum
{
    FBO_TRANSMITTANCE,
    FBO_IRRADIANCE,

    FBO_COUNT
};
GLuint fbos[FBO_COUNT];
enum
{
    TEX_TRANSMITTANCE0,
    TEX_TRANSMITTANCE_LAST=TEX_TRANSMITTANCE0+allWavelengths.size()/4-1,
    TEX_IRRADIANCE0,
    TEX_IRRADIANCE_LAST=TEX_IRRADIANCE0+allWavelengths.size()/4-1,

    TEX_COUNT
};
GLuint textures[TEX_COUNT];

std::string textureOutputDir=".";
GLint transmittanceTexW, transmittanceTexH;
GLint irradianceTexW, irradianceTexH;
GLint numTransmittanceIntegrationPoints;
GLfloat earthRadius;
GLfloat atmosphereHeight;
GLfloat rayleighScatteringCoefficientAt1um;
GLfloat mieScatteringCoefficientAt1um;
GLfloat mieAngstromExponent;
GLfloat mieSingleScatteringAlbedo;
QString rayleighScattererRelativeDensity;
QString mieScattererRelativeDensity;
QString ozoneDensity;
double earthSunDistance;
GLfloat sunAngularRadius; // calculated from earthSunDistance

QOpenGLFunctions_3_3_Core gl;

#include "debug.h"

struct MustQuit{};

glm::vec4 Vec(QVector4D v) { return glm::vec4(v.x(), v.y(), v.z(), v.w()); }
QVector4D QVec(glm::vec4 v) { return QVector4D(v.x, v.y, v.z, v.w); }

QVector4D rayleighScatteringCoefficient(QVector4D wavelengths)
{
    constexpr float refWL=1000; // nm
    return QVec(rayleighScatteringCoefficientAt1um*pow(Vec(wavelengths/refWL), glm::vec4(-4)));
}

QVector4D mieScatteringCoefficient(QVector4D wavelengths)
{
    constexpr float refWL=1000; // nm
    return QVec(mieScatteringCoefficientAt1um*pow(Vec(wavelengths/refWL), glm::vec4(mieAngstromExponent)));
}

void checkFramebufferStatus(const char*const fboDescription)
{
    GLenum status=gl.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status!=GL_FRAMEBUFFER_COMPLETE)
    {
        std::string errorDescription;
        switch(status)
        {
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            errorDescription="incomplete attachment";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            errorDescription="missing attachment";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            errorDescription="invalid framebuffer operation";
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            errorDescription="framebuffer unsupported";
            break;
        default:
            errorDescription="Unknown error "+std::to_string(status);
            break;
        }
        std::cerr << "Error: " << fboDescription << " is incomplete: " << errorDescription << "\n";
        throw MustQuit{};
    }
}

void initBuffers()
{
	gl.glGenVertexArrays(1, &vao);
	gl.glBindVertexArray(vao);
	gl.glGenBuffers(1, &vbo);
	gl.glBindBuffer(GL_ARRAY_BUFFER, vbo);
	static constexpr GLfloat vertices[]=
	{
		-1, -1,
		 1, -1,
		-1,  1,
		 1,  1,
	};
	gl.glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);
	constexpr GLuint attribIndex=0;
	constexpr int coordsPerVertex=2;
	gl.glVertexAttribPointer(attribIndex, coordsPerVertex, GL_FLOAT, false, 0, 0);
	gl.glEnableVertexAttribArray(attribIndex);
	gl.glBindVertexArray(0);
}

void initTexturesAndFramebuffers()
{
    gl.glGenTextures(TEX_COUNT,textures);
    for(int texIndex=0;texIndex<allWavelengths.size()/4;++texIndex)
    {
        gl.glBindTexture(GL_TEXTURE_2D,textures[TEX_TRANSMITTANCE0+texIndex]);
        gl.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        gl.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

        gl.glBindTexture(GL_TEXTURE_2D,textures[TEX_IRRADIANCE0+texIndex]);
        gl.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        gl.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    }

    gl.glGenFramebuffers(FBO_COUNT,fbos);
}

void init()
{
    initBuffers();
    initTexturesAndFramebuffers();
}

void renderUntexturedQuad()
{
	gl.glBindVertexArray(vao);
	gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	gl.glBindVertexArray(0);
}

QString addConstDefinitions(QString src)
{
    const auto constants="const float earthRadius="+QString::number(earthRadius)+"; // must be in meters\n"
                         "const float atmosphereHeight="+QString::number(atmosphereHeight)+"; // must be in meters\n"
                         R"(
const vec3 earthCenter=vec3(0,0,-earthRadius);

const float dobsonUnit = 2.687e20; // molecules/m^2
const float PI=3.1415926535897932;
const float km=1000;
#define sqr(x) ((x)*(x))
)";
    src.replace("\n#include \"const.h.glsl\"\n", constants);
    return src;
}

QString makeDensitiesSrc()
{
    return withHeadersIncluded(addConstDefinitions(1+R"(
#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "densities.h.glsl"

float rayleighScattererRelativeDensity(float altitude)
{
)" + rayleighScattererRelativeDensity.trimmed() + R"(
}
float mieScattererRelativeDensity(float altitude)
{
)" + mieScattererRelativeDensity.trimmed() + R"(
}
float ozoneDensity(float altitude)
{
)" + ozoneDensity.trimmed() + R"(
}
float density(float altitude, int whichDensity)
{
    switch(whichDensity)
    {
    case DENSITY_ABS_OZONE:
        return ozoneDensity(altitude);
    case DENSITY_REL_RAYLEIGH:
        return rayleighScattererRelativeDensity(altitude);
    case DENSITY_REL_MIE:
        return mieScattererRelativeDensity(altitude);
    }
}
)"), "(virtual)densities.frag");
}

QString getShaderSrc(QString const& fileName)
{
    QFile file;
    bool opened=false;
    const auto appBinDir=QDir(qApp->applicationDirPath()+"/").canonicalPath();
    if(appBinDir==QDir(INSTALL_BINDIR).canonicalPath())
    {
        file.setFileName(DATA_ROOT_DIR + fileName);
        opened=file.open(QIODevice::ReadOnly);
    }
    else if(appBinDir==QDir(BUILD_BINDIR).canonicalPath())
    {
        file.setFileName(SOURCE_DIR + fileName);
        opened = file.open(QIODevice::ReadOnly);
    }

    if(!opened)
    {
        std::cerr << "Error opening shader \"" << fileName.toStdString() << "\"\n";
        throw MustQuit{};
    }
    return addConstDefinitions(file.readAll());
}

std::unique_ptr<QOpenGLShader> compileShader(QOpenGLShader::ShaderType type, QString source, QString description)
{
    auto shader=std::make_unique<QOpenGLShader>(type);
    source=withHeadersIncluded(source, description);
    if(!shader->compileSourceCode(source))
    {
        // Qt prints compilation errors to stderr, so don't print them again
        std::cerr << "Failed to compile " << description.toStdString() << "\n";
        throw MustQuit{};
    }
    if(!shader->log().isEmpty())
    {
        std::cerr << "Warnings while compiling " << description.toStdString() << ": " << shader->log().toStdString() << "\n";
    }
    return shader;
}

std::unique_ptr<QOpenGLShader> compileShader(QOpenGLShader::ShaderType type, QString filename)
{
    const auto src=getShaderSrc(filename);
    return compileShader(type, src, filename);
}

QOpenGLShader& getOrCompileShader(QOpenGLShader::ShaderType type, QString filename)
{
    const auto it=allShaders.find(filename);
    if(it!=allShaders.end()) return *it->second;
    return *allShaders.emplace(filename, compileShader(type, filename)).first->second;
}

QString withHeadersIncluded(QString src, QString filename)
{
    QTextStream srcStream(&src);
    int lineNumber=1;
    int headerNumber=1;
    QString newSrc;
    for(auto line=srcStream.readLine(); !line.isNull(); line=srcStream.readLine(), ++lineNumber)
    {
        if(!line.simplified().startsWith("#include \""))
        {
            newSrc.append(line+'\n');
            continue;
        }
        auto includePattern=QRegExp("^#include \"([^\"]+)\"$");
        if(!includePattern.exactMatch(line))
        {
            std::cerr << filename.toStdString() << ":" << lineNumber << ": syntax error in #include directive\n";
            throw MustQuit{};
        }
        const auto includeFileName=includePattern.cap(1);
        static const char headerSuffix[]=".h.glsl";
        if(!includeFileName.endsWith(headerSuffix))
        {
            std::cerr << filename.toStdString() << ":" << lineNumber << ": file to include must have suffix \""
                      << headerSuffix << "\"\n";
            throw MustQuit{};
        }
        const auto header=getShaderSrc(includeFileName);
        newSrc.append(QString("#line 1 %1\n").arg(headerNumber++));
        newSrc.append(header);
        newSrc.append(QString("#line %1 0\n").arg(lineNumber));
    }
    return newSrc;
}

std::set<QString> getShaderFileNamesToLinkWith(QString shaderSrc, int recursionDepth=0)
{
    constexpr int maxRecursionDepth=50;
    if(recursionDepth>maxRecursionDepth)
    {
        std::cerr << "Include recursion depth exceeded " << maxRecursionDepth << "\n";
        throw MustQuit{};
    }
    std::set<QString> filenames;
    QTextStream srcStream(&shaderSrc);
    for(auto line=srcStream.readLine(); !line.isNull(); line=srcStream.readLine())
    {
        auto includePattern=QRegExp("^#include \"([^\"]+)\\.h\\.glsl\"$");
        if(!includePattern.exactMatch(line))
            continue;
        const auto includeFileBaseName=includePattern.cap(1);
        const auto shaderFileNameToLinkWith=includeFileBaseName+".frag";
        filenames.insert(shaderFileNameToLinkWith);
        if(!internalShaders.count(shaderFileNameToLinkWith))
        {
            const auto extraFileNames=getShaderFileNamesToLinkWith(getShaderSrc(shaderFileNameToLinkWith), recursionDepth+1);
            filenames.insert(extraFileNames.begin(), extraFileNames.end());
        }
    }
    return filenames;
}

std::unique_ptr<QOpenGLShaderProgram> compileShaderProgram(QString mainSrcFileName, const char* description)
{
    auto program=std::make_unique<QOpenGLShaderProgram>();

    auto mainSrc=getShaderSrc(mainSrcFileName);
    for(const auto filename : getShaderFileNamesToLinkWith(mainSrc))
        program->addShader(&getOrCompileShader(QOpenGLShader::Fragment, filename));

    mainSrc=withHeadersIncluded(mainSrc, mainSrcFileName);
    const auto mainShader=compileShader(QOpenGLShader::Fragment, mainSrc, mainSrcFileName);
    program->addShader(mainShader.get());

    program->addShader(&getOrCompileShader(QOpenGLShader::Vertex, "shader.vert"));

    if(!program->link())
    {
        // Qt prints linking errors to stderr, so don't print them again
        std::cerr << "Failed to link " << description << "\n";
        throw MustQuit{};
    }
    return program;
}

unsigned long long getUInt(QString value, unsigned long long min, unsigned long long max, QString filename, int lineNumber)
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

struct DimensionlessQuantity {};

double getQuantity(QString value, double min, double max, DimensionlessQuantity const& quantity, QString filename, int lineNumber)
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

double getQuantity(QString value, double min, double max, Quantity const& quantity, QString filename, int lineNumber)
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
        std::cerr << filename.toStdString() << ":" << lineNumber << ": unrecognized unit " << unit.toStdString() << ". Can be one of ";
        for(auto it=units.begin(); it!=units.end(); ++it)
        {
            if(it!=units.begin()) std::cerr << ',';
            std::cerr << it->first.toStdString();
        }
        std::cerr << ".\n";
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

QString readGLSLFunctionBody(QString value, QTextStream& stream, const QString filename, int& lineNumber)
{
    if(!value.isEmpty() && !value.startsWith("```"))
    {
        std::cerr << filename.toStdString() << ":" << lineNumber << ": bad value format. Must start and end with triple backtick \"```\".\n";
        throw MustQuit{};
    }
    for(auto line=stream.readLine(); !line.isNull(); line=stream.readLine(), ++lineNumber)
    {
        if(value.isEmpty())
        {
            if(!line.startsWith("```"))
            {
                std::cerr << filename.toStdString() << ":" << lineNumber << ": value must start with triple backtick.\n";
                throw MustQuit{};
            }
            value.append(line+'\n');
            continue;
        }

        if(!line.contains("```"))
            value.append(line+'\n');
        else if(line.endsWith("```"))
        {
            value.append(line+'\n');
            break;
        }
        else
        {
            std::cerr << filename.toStdString() << ":" << lineNumber << ": trailing characters after triple backtick.\n";
            throw MustQuit{};
        }
    }
    return value.mid(3,value.size()-(2*3+1));
}

void handleCmdLine()
{
    QCommandLineParser parser;
    const auto atmoDescrOpt="atmo-descr";
    parser.addPositionalArgument(atmoDescrOpt, "Atmosphere description file", "atmosphere-description.atmo");
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
        if(key=="transmittance texture width")
            transmittanceTexW=getUInt(value,1,std::numeric_limits<GLsizei>::max(), atmoDescrFileName, lineNumber);
        else if(key=="transmittance texture height")
            transmittanceTexH=getUInt(value,1,std::numeric_limits<GLsizei>::max(), atmoDescrFileName, lineNumber);
        else if(key=="transmittance integration points")
            numTransmittanceIntegrationPoints=getUInt(value,1,INT_MAX, atmoDescrFileName, lineNumber);
        else if(key=="irradiance texture width")
            irradianceTexW=getUInt(value,1,std::numeric_limits<GLsizei>::max(), atmoDescrFileName, lineNumber);
        else if(key=="irradiance texture height")
            irradianceTexH=getUInt(value,1,std::numeric_limits<GLsizei>::max(), atmoDescrFileName, lineNumber);
        else if(key=="earth radius")
            earthRadius=getQuantity(value,1,1e10,LengthQuantity{},atmoDescrFileName,lineNumber);
        else if(key=="atmosphere height")
            atmosphereHeight=getQuantity(value,1,1e6,LengthQuantity{},atmoDescrFileName,lineNumber);
        else if(key=="rayleigh scattering coefficient at 1 um")
            rayleighScatteringCoefficientAt1um=getQuantity(value,1e-20,100,ReciprocalLengthQuantity{},atmoDescrFileName,lineNumber);
        else if(key=="mie scattering coefficient at 1 um")
            mieScatteringCoefficientAt1um=getQuantity(value,1e-20,100,ReciprocalLengthQuantity{},atmoDescrFileName,lineNumber);
        else if(key=="mie angstrom exponent")
            mieAngstromExponent=getQuantity(value,-10,10,DimensionlessQuantity{},atmoDescrFileName,lineNumber);
        else if(key=="mie single scattering albedo")
            mieSingleScatteringAlbedo=getQuantity(value,0,1,DimensionlessQuantity{},atmoDescrFileName,lineNumber);
        else if(key=="rayleigh scatterer relative density")
            rayleighScattererRelativeDensity=readGLSLFunctionBody(value, stream,atmoDescrFileName,lineNumber);
        else if(key=="mie scatterer relative density")
            mieScattererRelativeDensity=readGLSLFunctionBody(value, stream,atmoDescrFileName,lineNumber);
        else if(key=="ozone density")
            ozoneDensity=readGLSLFunctionBody(value, stream,atmoDescrFileName,lineNumber);
        else if(key=="earth-sun distance")
        {
            earthSunDistance=getQuantity(value,0.5*AU,1e20*AU,LengthQuantity{},atmoDescrFileName,lineNumber);
            // Here we don't take into account the fact that camera is not in the center of the Earth. It's not too
            // important until we simulate eclipsed atmosphere, when we'll have to recompute Sun angular radius for
            // each camera position.
            sunAngularRadius=sunRadius/earthSunDistance;
        }
    }
    if(!stream.atEnd())
    {
        std::cerr << atmoDescrFileName.toStdString() << ":" << lineNumber << ": error: failed to read file\n";
        throw MustQuit{};
    }
}

void computeTransmittance()
{
    const auto program=compileShaderProgram("compute-transmittance.frag", "transmittance computation shader program");

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_TRANSMITTANCE]);
    for(int texIndex=0;texIndex<allWavelengths.size()/4;++texIndex)
    {
        const QVector4D wavelengths(allWavelengths[texIndex*4+0],
                                    allWavelengths[texIndex*4+1],
                                    allWavelengths[texIndex*4+2],
                                    allWavelengths[texIndex*4+3]);
        const QVector4D ozoneCS(ozoneAbsCrossSection[texIndex*4+0],
                                ozoneAbsCrossSection[texIndex*4+1],
                                ozoneAbsCrossSection[texIndex*4+2],
                                ozoneAbsCrossSection[texIndex*4+3]);

        assert(fbos[FBO_TRANSMITTANCE]);
        gl.glBindTexture(GL_TEXTURE_2D,textures[TEX_TRANSMITTANCE0+texIndex]);
        gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F_ARB,transmittanceTexW,transmittanceTexH,
                        0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        gl.glBindTexture(GL_TEXTURE_2D,0);
        gl.glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,
                                  textures[TEX_TRANSMITTANCE0+texIndex],0);
        checkFramebufferStatus("framebuffer for transmittance texture");

        program->bind();
        program->setUniformValue("visibleAtmosphereHeight",atmosphereHeight);
        program->setUniformValue("wavelengths",wavelengths);
        program->setUniformValue("rayleighScatteringCoefficient",rayleighScatteringCoefficient(wavelengths));
        program->setUniformValue("mieScatteringCoefficient",mieScatteringCoefficient(wavelengths));
        program->setUniformValue("mieSingleScatteringAlbedo",mieSingleScatteringAlbedo);
        program->setUniformValue("ozoneAbsorptionCrossSection",ozoneCS);
        program->setUniformValue("numTransmittanceIntegrationPoints",numTransmittanceIntegrationPoints);
        program->setUniformValue("transmittanceTextureSize",transmittanceTexW,transmittanceTexH);
        gl.glViewport(0, 0, transmittanceTexW, transmittanceTexH);
        renderUntexturedQuad();

        std::vector<glm::vec4> pixels(transmittanceTexW*transmittanceTexH);
        gl.glReadPixels(0,0,transmittanceTexW,transmittanceTexH,GL_RGBA,GL_FLOAT,pixels.data());
        std::ofstream out(textureOutputDir+"/transmittance-"+std::to_string(texIndex)+".f32");
        const std::uint32_t w=transmittanceTexW, h=transmittanceTexH;
        out.write(reinterpret_cast<const char*>(&w), sizeof w);
        out.write(reinterpret_cast<const char*>(&h), sizeof h);
        out.write(reinterpret_cast<const char*>(pixels.data()), pixels.size()*sizeof pixels[0]);

        if(false) // for debugging
        {
            QImage image(transmittanceTexW, transmittanceTexH, QImage::Format_RGBA8888);
            image.fill(Qt::magenta);
            gl.glReadPixels(0,0,transmittanceTexW,transmittanceTexH,GL_RGBA,GL_UNSIGNED_BYTE,image.bits());
            image.mirrored().save(QString("/tmp/transmittance-png-%1.png").arg(texIndex));
        }
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void computeGroundIrradiance()
{
    const auto program=compileShaderProgram("compute-irradiance.frag", "irradiance computation shader program");

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_IRRADIANCE]);
    for(int texIndex=0;texIndex<allWavelengths.size()/4;++texIndex)
    {
        const QVector4D solarIrradiance(::solarIrradiance[texIndex*4+0],
                                        ::solarIrradiance[texIndex*4+1],
                                        ::solarIrradiance[texIndex*4+2],
                                        ::solarIrradiance[texIndex*4+3]);
        gl.glBindTexture(GL_TEXTURE_2D,textures[TEX_IRRADIANCE0+texIndex]);
        gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F_ARB,irradianceTexW,irradianceTexH,
                        0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        gl.glBindTexture(GL_TEXTURE_2D,0);
        gl.glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,
                                  textures[TEX_IRRADIANCE0+texIndex],0);
        checkFramebufferStatus("framebuffer for irradiance texture");

        gl.glActiveTexture(GL_TEXTURE0);
        gl.glBindTexture(GL_TEXTURE_2D,textures[TEX_TRANSMITTANCE0+texIndex]);

        program->bind();

        program->setUniformValue("irradianceTextureSize",QVector2D(irradianceTexW,irradianceTexH));
        program->setUniformValue("transmittanceTexture",0);
        program->setUniformValue("solarIrradiance",solarIrradiance);
        program->setUniformValue("sunAngularRadius",sunAngularRadius);

        gl.glViewport(0, 0, irradianceTexW, irradianceTexH);
        renderUntexturedQuad();

        std::vector<glm::vec4> pixels(irradianceTexW*irradianceTexH);
        gl.glReadPixels(0,0,irradianceTexW,irradianceTexH,GL_RGBA,GL_FLOAT,pixels.data());
        std::ofstream out(textureOutputDir+"/irradiance-"+std::to_string(texIndex)+".f32");
        const std::uint32_t w=irradianceTexW, h=irradianceTexH;
        out.write(reinterpret_cast<const char*>(&w), sizeof w);
        out.write(reinterpret_cast<const char*>(&h), sizeof h);
        out.write(reinterpret_cast<const char*>(pixels.data()), pixels.size()*sizeof pixels[0]);

        if(false) // for debugging
        {
            QImage image(irradianceTexW, irradianceTexH, QImage::Format_RGBA8888);
            image.fill(Qt::magenta);
            gl.glReadPixels(0,0,irradianceTexW,irradianceTexH,GL_RGBA,GL_UNSIGNED_BYTE,image.bits());
            image.mirrored().save(QString("/tmp/irradiance-png-%1.png").arg(texIndex));
        }
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName("Atmosphere textures generator");
    app.setApplicationVersion(APP_VERSION);

    try
    {
        handleCmdLine();

        QSurfaceFormat format;
        format.setMajorVersion(3);
        format.setMinorVersion(3);
        format.setProfile(QSurfaceFormat::CoreProfile);

        QOpenGLContext context;
        context.setFormat(format);
        context.create();
        if(!context.isValid())
        {
            std::cerr << "Failed to create OpenGL "
                << format.majorVersion() << '.'
                << format.minorVersion() << " context";
            return 1;
        }

        QOffscreenSurface surface;
        surface.setFormat(format);
        surface.create();
        if(!surface.isValid())
        {
            std::cerr << "Failed to create OpenGL "
                << format.majorVersion() << '.'
                << format.minorVersion() << " offscreen surface";
            return 1;
        }

        context.makeCurrent(&surface);

        if(!gl.initializeOpenGLFunctions())
        {
            std::cerr << "Failed to initialize OpenGL "
                << format.majorVersion() << '.'
                << format.minorVersion() << " functions";
            return 1;
        }

        init();
        allShaders.emplace(DENSITIES_SHADER_FILENAME, compileShader(QOpenGLShader::Fragment, makeDensitiesSrc(),
                                                           "shader for calculating scatterer and absorber densities"));
        computeTransmittance();
        // We'll use ground irradiance to take into account the contribution of light scattered by the ground to the
        // sky color. Irradiance will also be needed when we want to draw the ground itself.
        computeGroundIrradiance();
    }
    catch(MustQuit&)
    {
        return 1;
    }
    catch(std::exception const& ex)
    {
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 111;
    }
}
