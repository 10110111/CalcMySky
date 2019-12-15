#include <iostream>
#include <fstream>
#include <memory>
#include <random>
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

QString withHeadersIncluded(QString src, QString const& filename);

constexpr auto NaN=std::numeric_limits<float>::quiet_NaN();

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
constexpr decltype(allWavelengths) solarIrradianceAtTOA=
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
QString constantsHeader;
QString densitiesHeader;
constexpr char DENSITIES_SHADER_FILENAME[]="densities.frag";
constexpr char PHASE_FUNCTIONS_SHADER_FILENAME[]="phase-functions.frag";
constexpr char COMPUTE_TRANSMITTANCE_SHADER_FILENAME[]="compute-transmittance-functions.frag";
constexpr char CONSTANTS_HEADER_FILENAME[]="const.h.glsl";
constexpr char DENSITIES_HEADER_FILENAME[]="densities.h.glsl";
std::set<QString> internalShaders
{
    DENSITIES_SHADER_FILENAME,
    PHASE_FUNCTIONS_SHADER_FILENAME,
    COMPUTE_TRANSMITTANCE_SHADER_FILENAME,
};
GLuint vao, vbo;
enum
{
    FBO_TRANSMITTANCE,
    FBO_IRRADIANCE,
    FBO_SINGLE_SCATTERING,

    FBO_COUNT
};
GLuint fbos[FBO_COUNT];
enum
{
    TEX_TRANSMITTANCE,
    TEX_IRRADIANCE,
    TEX_FIRST_SCATTERING,

    TEX_COUNT
};
GLuint textures[TEX_COUNT];

std::string textureOutputDir=".";
GLint transmittanceTexW, transmittanceTexH;
GLint irradianceTexW, irradianceTexH;
glm::vec4 scatteringTextureSize;
GLint numTransmittanceIntegrationPoints;
GLint radialIntegrationPoints;
GLfloat earthRadius;
GLfloat atmosphereHeight;
double earthSunDistance;
GLfloat sunAngularRadius; // calculated from earthSunDistance
struct ScattererDescription
{
    GLfloat crossSectionAt1um = NaN;
    GLfloat angstromExponent = NaN;
    QString numberDensity;
    QString phaseFunction;
    QString name;

    ScattererDescription(QString const& name) : name(name) {}
    bool valid() const
    {
        return std::isfinite(crossSectionAt1um) &&
               std::isfinite(angstromExponent) &&
               !numberDensity.isEmpty() &&
               !phaseFunction.isEmpty() &&
               !name.isEmpty();
    }
    glm::vec4 crossSection(glm::vec4 const wavelengths) const
    {
        constexpr float refWL=1000; // nm
        return crossSectionAt1um*pow(wavelengths/refWL, glm::vec4(-angstromExponent));
    }
};
std::vector<ScattererDescription> scatterers;
struct AbsorberDescription
{
    QString numberDensity;
    QString name;
    std::array<float,allWavelengths.size()> absorptionCrossSection{};

    AbsorberDescription(QString const& name) : name(name) {}
    bool valid() const
    {
        return !numberDensity.isEmpty() &&
               std::accumulate(absorptionCrossSection.begin(),absorptionCrossSection.end(), 0.) != 0 &&
               !name.isEmpty();
    }
    glm::vec4 crossSection(glm::vec4 const wavelengths) const
    {
        const auto wlIt=std::find(allWavelengths.begin(), allWavelengths.end(), wavelengths[0]);
        assert(wlIt!=allWavelengths.end());
        const auto*const wl0=&*wlIt;
        assert(glm::vec4(wl0[0],wl0[1],wl0[2],wl0[3])==wavelengths);
        const auto i=wl0-&allWavelengths[0];
        return {absorptionCrossSection[i],absorptionCrossSection[i+1],absorptionCrossSection[i+2],absorptionCrossSection[i+3]};
    }
};
std::vector<AbsorberDescription> absorbers;

QOpenGLFunctions_3_3_Core gl;

#include "debug.h"

struct MustQuit{};

QVector4D QVec(glm::vec4 v) { return QVector4D(v.x, v.y, v.z, v.w); }
QString toString(int x) { return QString::number(x); }
QString toString(double x) { return QString::number(x, 'g', 17); }
QString toString(float x) { return QString::number(x, 'g', 9); }
QString toString(glm::vec2 v) { return QString("vec2(%1,%2)").arg(double(v.x), 0,'g',9)
                                                             .arg(double(v.y), 0,'g',9); }
QString toString(glm::vec4 v) { return QString("vec4(%1,%2,%3,%4)").arg(double(v.x), 0,'g',9)
                                                                   .arg(double(v.y), 0,'g',9)
                                                                   .arg(double(v.z), 0,'g',9)
                                                                   .arg(double(v.w), 0,'g',9); }

// Function useful only for debugging
void dumpActiveUniforms(const GLuint program)
{
    int uniformCount=0, maxLen=0;
    gl.glGetProgramiv(program,GL_ACTIVE_UNIFORMS,&uniformCount);
    gl.glGetProgramiv(program,GL_ACTIVE_UNIFORM_MAX_LENGTH,&maxLen);
    std::cerr << "Active uniforms:\n";
    for(int uniformIndex=0;uniformIndex<uniformCount;++uniformIndex)
    {
        std::vector<char> name(maxLen);
        GLsizei size;
        GLenum type;
        gl.glGetActiveUniform(program,uniformIndex,maxLen,nullptr,&size,&type,name.data());
        std::cerr << ' ' << name.data() << "\n";
    }
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
    for(const auto tex : {TEX_TRANSMITTANCE,TEX_IRRADIANCE})
    {
        gl.glBindTexture(GL_TEXTURE_2D,textures[tex]);
        gl.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        gl.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    }

    for(const auto tex : {TEX_FIRST_SCATTERING})
    {
        gl.glBindTexture(GL_TEXTURE_3D,textures[tex]);
        gl.glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        gl.glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
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

void initConstHeader()
{
    constantsHeader="const float earthRadius="+QString::number(earthRadius)+"; // must be in meters\n"
                         "const float atmosphereHeight="+QString::number(atmosphereHeight)+"; // must be in meters\n"
                         R"(
const vec3 earthCenter=vec3(0,0,-earthRadius);

const float dobsonUnit = 2.687e20; // molecules/m^2
const float PI=3.1415926535897932;
const float km=1000;
#define sqr(x) ((x)*(x))

const float sunAngularRadius=)" + toString(sunAngularRadius) + R"(;
const vec4 scatteringTextureSize=)" + toString(scatteringTextureSize) + R"(;
const vec2 irradianceTextureSize=)" + toString(glm::vec2(irradianceTexW, irradianceTexH)) + R"(;
const vec2 transmittanceTextureSize=)" + toString(glm::vec2(transmittanceTexW,transmittanceTexH)) + R"(;
const int radialIntegrationPoints=)" + toString(radialIntegrationPoints) + R"(;
const int numTransmittanceIntegrationPoints=)" + toString(numTransmittanceIntegrationPoints) + R"(;
)";
}

QString makeDensitiesFunctions()
{
    QString header;
    QString src;
    for(auto const& scatterer : scatterers)
    {
        src += "float scattererNumberDensity_"+scatterer.name+"(float altitude)\n"
               "{\n"
               +scatterer.numberDensity+
               "}\n";
        header += "float scattererNumberDensity_"+scatterer.name+"(float altitude);\n";
    }
    for(auto const& absorber : absorbers)
    {
        src += "float absorberNumberDensity_"+absorber.name+"(float altitude)\n"
               "{\n"
               +absorber.numberDensity+
               "}\n";
        header += "float absorberNumberDensity_"+absorber.name+"(float altitude);\n";
    }

    header += "vec4 scatteringCoefficient();\n"
              "float scattererDensity(float altitude);\n";

    if(densitiesHeader.isEmpty())
        densitiesHeader=header;

    return src;
}

QString makeTransmittanceComputeFunctionsSrc(glm::vec4 const& wavelengths)
{
    const QString head=1+R"(
#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "common-functions.h.glsl"
)";
    const QString opticalDepthFunctionTemplate=R"(
vec4 opticalDepthToAtmosphereBorder_##agentSpecies(float altitude, float cosZenithAngle, vec4 crossSection)
{
    const float integrInterval=distanceToAtmosphereBorder(cosZenithAngle, altitude);

    const float R=earthRadius;
    const float r1=R+altitude;
    const float l=integrInterval;
    const float mu=cosZenithAngle;
    /* From law of cosines: r₂²=r₁²+l²+2r₁lμ */
    const float endAltitude=-R+sqrt(sqr(r1)+sqr(l)+2*r1*l*mu);

    const float dl=integrInterval/(numTransmittanceIntegrationPoints-1);

    /* Using trapezoid rule on a uniform grid: f0/2+f1+f2+...+f(N-2)+f(N-1)/2. */
    float sum=(agent##NumberDensity_##agentSpecies(altitude)+
               agent##NumberDensity_##agentSpecies(endAltitude))/2;
    for(int n=1;n<numTransmittanceIntegrationPoints-1;++n)
    {
        const float dist=n*dl;
        const float currAlt=-R+sqrt(sqr(r1)+sqr(dist)+2*r1*dist*mu);
        sum+=agent##NumberDensity_##agentSpecies(currAlt);
    }
    return sum*dl*crossSection;
}
)";
    QString opticalDepthFunctions;
    QString computeFunction = R"(
// This assumes that ray doesn't intersect Earth
vec4 computeTransmittanceToAtmosphereBorder(float cosZenithAngle, float altitude)
{
    const vec4 depth=
)";
    for(auto const& scatterer : scatterers)
    {
        opticalDepthFunctions += QString(opticalDepthFunctionTemplate).replace("##agentSpecies",scatterer.name).replace("agent##","scatterer");
        computeFunction += "        +opticalDepthToAtmosphereBorder_"+scatterer.name+
                             "(altitude,cosZenithAngle,"+toString(scatterer.crossSection(wavelengths))+")\n";
    }
    for(auto const& absorber : absorbers)
    {
        opticalDepthFunctions += QString(opticalDepthFunctionTemplate).replace("##agentSpecies",absorber.name).replace("agent##","absorber");
        computeFunction += "        +opticalDepthToAtmosphereBorder_"+absorber.name+
                             "(altitude,cosZenithAngle,"+toString(absorber.crossSection(wavelengths))+")\n";
    }
    computeFunction += R"(      ;
    return exp(-depth);
}
)";
    constexpr char mainFunc[]=R"(
)";
    return withHeadersIncluded(head+makeDensitiesFunctions()+opticalDepthFunctions+computeFunction,
                               QString("(virtual)%1").arg(COMPUTE_TRANSMITTANCE_SHADER_FILENAME));
}

QString makeScattererDensityFunctionsSrc(glm::vec4 const& wavelengths)
{
    const QString head=1+R"(
#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
)";
    return withHeadersIncluded(head+makeDensitiesFunctions(),
                               QString("(virtual)%1").arg(DENSITIES_SHADER_FILENAME));
}

QString makePhaseFunctionsSrc(QString const& source)
{
    return withHeadersIncluded(1+R"(
#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"

vec4 phaseFunction(float dotViewSun)
{
)" + source.trimmed() + R"(
}
)", QString("(virtual)%1").arg(PHASE_FUNCTIONS_SHADER_FILENAME));
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
    return file.readAll();
}

std::unique_ptr<QOpenGLShader> compileShader(QOpenGLShader::ShaderType type, QString source, QString const& description)
{
    auto shader=std::make_unique<QOpenGLShader>(type);
    source=withHeadersIncluded(source, description);
    if(!shader->compileSourceCode(source))
    {
        std::cerr << "Failed to compile " << description.toStdString() << ":\n"
                  << shader->log().toStdString() << "\n";
        std::cerr << "Source of the shader:\n________________________________________________\n";
        const auto lineCount=source.count(QChar('\n'));
        QTextStream srcStream(&source);
        int lineNumber=1;
        for(auto line=srcStream.readLine(); !line.isNull(); line=srcStream.readLine(), ++lineNumber)
        {
            QRegExp lineChanger("^\\s*#\\s*line\\s+([0-9]+)\\b.*");
            std::cerr << std::setw(std::ceil(std::log10(lineCount))) << lineNumber << " " << line.toStdString() << "\n";
            if(lineChanger.exactMatch(line))
            {
                lineNumber = lineChanger.cap(1).toInt() - 1;
                continue;
            }
        }
        std::cerr << "________________________________________________\n";
        throw MustQuit{};
    }
    if(!shader->log().isEmpty())
    {
        std::cerr << "Warnings while compiling " << description.toStdString() << ":\n"
                  << shader->log().toStdString() << "\n";
    }
    return shader;
}

std::unique_ptr<QOpenGLShader> compileShader(QOpenGLShader::ShaderType type, QString const& filename)
{
    const auto src=getShaderSrc(filename);
    return compileShader(type, src, filename);
}

QOpenGLShader& getOrCompileShader(QOpenGLShader::ShaderType type, QString const& filename)
{
    const auto it=allShaders.find(filename);
    if(it!=allShaders.end()) return *it->second;
    return *allShaders.emplace(filename, compileShader(type, filename)).first->second;
}

QString withHeadersIncluded(QString src, QString const& filename)
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
        const auto header = includeFileName==CONSTANTS_HEADER_FILENAME ? constantsHeader :
                            includeFileName==DENSITIES_HEADER_FILENAME ? densitiesHeader :
                                getShaderSrc(includeFileName);
        newSrc.append(QString("#line 1 %1 // %2\n").arg(headerNumber++).arg(includeFileName));
        newSrc.append(header);
        newSrc.append(QString("#line %1 0 // %2\n").arg(lineNumber+1).arg(filename));
    }
    return newSrc;
}

std::set<QString> getShaderFileNamesToLinkWith(QString const& filename, int recursionDepth=0)
{
    constexpr int maxRecursionDepth=50;
    if(recursionDepth>maxRecursionDepth)
    {
        std::cerr << "Include recursion depth exceeded " << maxRecursionDepth << "\n";
        throw MustQuit{};
    }
    std::set<QString> filenames;
    auto shaderSrc=getShaderSrc(filename);
    QTextStream srcStream(&shaderSrc);
    for(auto line=srcStream.readLine(); !line.isNull(); line=srcStream.readLine())
    {
        auto includePattern=QRegExp("^#include \"([^\"]+)(\\.h\\.glsl)\"$");
        if(!includePattern.exactMatch(line))
            continue;
        const auto includeFileBaseName=includePattern.cap(1);
        if(includeFileBaseName+includePattern.cap(2) == CONSTANTS_HEADER_FILENAME) // no companion source for constants header
            continue;
        const auto shaderFileNameToLinkWith=includeFileBaseName+".frag";
        filenames.insert(shaderFileNameToLinkWith);
        if(!internalShaders.count(shaderFileNameToLinkWith) && shaderFileNameToLinkWith!=filename)
        {
            const auto extraFileNames=getShaderFileNamesToLinkWith(shaderFileNameToLinkWith, recursionDepth+1);
            filenames.insert(extraFileNames.begin(), extraFileNames.end());
        }
    }
    return filenames;
}

std::unique_ptr<QOpenGLShaderProgram> compileShaderProgram(QString const& mainSrcFileName, const char* description,
                                                           const bool useGeomShader=false)
{
    auto program=std::make_unique<QOpenGLShaderProgram>();

    auto shaderFileNames=getShaderFileNamesToLinkWith(mainSrcFileName);
    shaderFileNames.insert(mainSrcFileName);

    for(const auto filename : shaderFileNames)
        program->addShader(&getOrCompileShader(QOpenGLShader::Fragment, filename));

    program->addShader(&getOrCompileShader(QOpenGLShader::Vertex, "shader.vert"));
    if(useGeomShader)
        program->addShader(&getOrCompileShader(QOpenGLShader::Geometry, "shader.geom"));

    if(!program->link())
    {
        // Qt prints linking errors to stderr, so don't print them again
        std::cerr << "Failed to link " << description << "\n";
        throw MustQuit{};
    }
    return program;
}

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

double getQuantity(QString const& value, double min, double max, DimensionlessQuantity const& quantity, QString const& filename, int lineNumber)
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
    if(name=="ozone")
        description.absorptionCrossSection=ozoneAbsCrossSection;

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
        else if(key=="irradiance texture size for altitude")
            irradianceTexW=getUInt(value,1,std::numeric_limits<GLsizei>::max(), atmoDescrFileName, lineNumber);
        else if(key=="irradiance texture size for cos(sza)")
            irradianceTexH=getUInt(value,1,std::numeric_limits<GLsizei>::max(), atmoDescrFileName, lineNumber);
        else if(key=="scattering texture size for cos(vza)")
            scatteringTextureSize[0]=getUInt(value,1,std::numeric_limits<GLsizei>::max(), atmoDescrFileName, lineNumber);
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
        else if(key.contains(scattererDescriptionKey))
            scatterers.emplace_back(parseScatterer(stream, scattererDescriptionKey.cap(1), atmoDescrFileName,++lineNumber));
        else if(key.contains(absorberDescriptionKey))
            absorbers.emplace_back(parseAbsorber(stream, absorberDescriptionKey.cap(1), atmoDescrFileName,++lineNumber));
        else
            std::cerr << "WARNING: Unknown key: " << key.toStdString() << "\n";
    }
    if(!stream.atEnd())
    {
        std::cerr << atmoDescrFileName.toStdString() << ":" << lineNumber << ": error: failed to read file\n";
        throw MustQuit{};
    }
}

void computeTransmittance(glm::vec4 const& wavelengths, int texIndex)
{
    const auto program=compileShaderProgram("compute-transmittance.frag", "transmittance computation shader program");

    std::cerr << "Computing transmittance... ";
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_TRANSMITTANCE]);
    assert(fbos[FBO_TRANSMITTANCE]);
    gl.glBindTexture(GL_TEXTURE_2D,textures[TEX_TRANSMITTANCE]);
    gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F_ARB,transmittanceTexW,transmittanceTexH,
                    0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl.glBindTexture(GL_TEXTURE_2D,0);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,textures[TEX_TRANSMITTANCE],0);
    checkFramebufferStatus("framebuffer for transmittance texture");

    program->bind();
    gl.glViewport(0, 0, transmittanceTexW, transmittanceTexH);
    renderUntexturedQuad();

    std::vector<glm::vec4> pixels(transmittanceTexW*transmittanceTexH);
    gl.glReadPixels(0,0,transmittanceTexW,transmittanceTexH,GL_RGBA,GL_FLOAT,pixels.data());
    std::ofstream out(textureOutputDir+"/transmittance-"+std::to_string(texIndex)+".f32");
    const std::uint16_t w=transmittanceTexW, h=transmittanceTexH;
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
    gl.glFinish();
    std::cerr << "done\n";
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void computeGroundIrradiance(QVector4D const& solarIrradianceAtTOA, const int texIndex)
{
    const auto program=compileShaderProgram("compute-irradiance.frag", "direct ground irradiance computation shader program");

    std::cerr << "Computing direct ground irradiance... ";
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_IRRADIANCE]);
    gl.glBindTexture(GL_TEXTURE_2D,textures[TEX_IRRADIANCE]);
    gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F_ARB,irradianceTexW,irradianceTexH,
                    0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl.glBindTexture(GL_TEXTURE_2D,0);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,textures[TEX_IRRADIANCE],0);
    checkFramebufferStatus("framebuffer for irradiance texture");

    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D,textures[TEX_TRANSMITTANCE]);

    program->bind();

    program->setUniformValue("transmittanceTexture",0);
    program->setUniformValue("solarIrradianceAtTOA",solarIrradianceAtTOA);

    gl.glViewport(0, 0, irradianceTexW, irradianceTexH);
    renderUntexturedQuad();

    std::vector<glm::vec4> pixels(irradianceTexW*irradianceTexH);
    gl.glReadPixels(0,0,irradianceTexW,irradianceTexH,GL_RGBA,GL_FLOAT,pixels.data());
    std::ofstream out(textureOutputDir+"/irradiance-"+std::to_string(texIndex)+".f32");
    const std::uint16_t w=irradianceTexW, h=irradianceTexH;
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
    gl.glFinish();
    std::cerr << "done\n";
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void computeSingleScattering(glm::vec4 const& wavelengths, QVector4D const& solarIrradianceAtTOA, const int texIndex)
{
    const auto scatTexWidth=scatteringTextureSize[0];
    const auto scatTexHeight=scatteringTextureSize[1]*scatteringTextureSize[2];
    const auto scatTexDepth=scatteringTextureSize[3];
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_SINGLE_SCATTERING]);

    gl.glViewport(0, 0, scatTexWidth, scatTexHeight);

    // TODO: try rendering multiple scatterers to multiple render targets at
    // once - as VRAM size allows. This may improve performance.
    for(const auto& scatterer : scatterers)
    {
        {
            const auto src=makeScattererDensityFunctionsSrc(wavelengths)+
                            "float scattererDensity(float alt) { return scattererNumberDensity_"+scatterer.name+"(alt); }\n"+
                            "vec4 scatteringCoefficient() { return "+toString(scatterer.crossSection(wavelengths))+"; }\n";
            allShaders[DENSITIES_SHADER_FILENAME]=compileShader(QOpenGLShader::Fragment, src,
                                                                "scatterer density computation functions");
        }
        const auto program=compileShaderProgram("compute-single-scattering.frag",
                                                "single scattering computation shader program",
                                                true);
        program->bind();
        const GLfloat altitudeMin=0, altitudeMax=atmosphereHeight; // TODO: implement splitting of calculations over altitude blocks
        program->setUniformValue("solarIrradianceAtTOA",solarIrradianceAtTOA);
        program->setUniformValue("altitudeMin", altitudeMin);
        program->setUniformValue("altitudeMax", altitudeMax);

        gl.glActiveTexture(GL_TEXTURE0);
        gl.glBindTexture(GL_TEXTURE_3D,textures[TEX_FIRST_SCATTERING]);
        gl.glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA32F_ARB,scatTexWidth,scatTexHeight,scatTexDepth,
                        0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,
                                textures[TEX_FIRST_SCATTERING],0);
        checkFramebufferStatus("framebuffer for one-order Rayleigh scattering");

        gl.glActiveTexture(GL_TEXTURE0);
        gl.glBindTexture(GL_TEXTURE_2D, textures[TEX_TRANSMITTANCE]);
        program->setUniformValue("transmittanceTexture", 0);

        std::cerr << "Computing single scattering layers for scatterer \"" << scatterer.name.toStdString() << "\"... ";
        for(int layer=0; layer<scatTexDepth; ++layer)
        {
            std::cerr << layer;
            program->setUniformValue("layer",layer);
            renderUntexturedQuad();
            gl.glFinish();
            if(layer+1<scatTexDepth) std::cerr << ',';
        }
        std::cerr << "; done\n";

        if(false) // for debugging
        {
            std::cerr << "Saving texture...";
            const uint16_t w=scatteringTextureSize[0], h=scatteringTextureSize[1],
                           d=scatteringTextureSize[2], q=scatteringTextureSize[3];
            std::vector<glm::vec4> pixels(w*h*d*q);
            gl.glActiveTexture(GL_TEXTURE0);
            gl.glBindTexture(GL_TEXTURE_3D,textures[TEX_FIRST_SCATTERING]);
            gl.glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_FLOAT, pixels.data());
            std::ofstream out(textureOutputDir+"/single-scattering-"+scatterer.name.toStdString()+"-"+std::to_string(texIndex)+".f32");
            out.write(reinterpret_cast<const char*>(&w), sizeof w);
            out.write(reinterpret_cast<const char*>(&h), sizeof h);
            out.write(reinterpret_cast<const char*>(&d), sizeof d);
            out.write(reinterpret_cast<const char*>(&q), sizeof q);
            out.write(reinterpret_cast<const char*>(pixels.data()), pixels.size()*sizeof pixels[0]);
            std::cerr << " done\n";
        }
    }
}

void qtMessageHandler(const QtMsgType type, QMessageLogContext const&, QString const& message)
{
    switch(type)
    {
    case QtDebugMsg:
        std::cerr << "[DEBUG] " << message.toStdString() << "\n";
        break;
    case QtWarningMsg:
        if(message.startsWith("*** Problematic Fragment shader source code ***"))
            break;
        if(message.startsWith("QOpenGLShader::compile("))
            break;
        std::cerr << "[WARN] " << message.toStdString() << "\n";
        break;
    case QtCriticalMsg:
        std::cerr << "[ERROR] " << message.toStdString() << "\n";
        break;
    case QtFatalMsg:
        std::cerr << "[FATAL] " << message.toStdString() << "\n";
        break;
    }
}

int main(int argc, char** argv)
{
    qInstallMessageHandler(qtMessageHandler);
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
        initConstHeader();
        for(int texIndex=0;texIndex<allWavelengths.size()/4;++texIndex)
        {
            allShaders.clear();
            const glm::vec4 wavelengths(allWavelengths[texIndex*4+0],
                                        allWavelengths[texIndex*4+1],
                                        allWavelengths[texIndex*4+2],
                                        allWavelengths[texIndex*4+3]);
            const QVector4D solarIrradianceAtTOA(::solarIrradianceAtTOA[texIndex*4+0],
                                                 ::solarIrradianceAtTOA[texIndex*4+1],
                                                 ::solarIrradianceAtTOA[texIndex*4+2],
                                                 ::solarIrradianceAtTOA[texIndex*4+3]);
            allShaders.emplace(COMPUTE_TRANSMITTANCE_SHADER_FILENAME,
                               compileShader(QOpenGLShader::Fragment, makeTransmittanceComputeFunctionsSrc(wavelengths),
                                             "transmittance computation functions"));
            computeTransmittance(wavelengths, texIndex);
            // We'll use ground irradiance to take into account the contribution of light scattered by the ground to the
            // sky color. Irradiance will also be needed when we want to draw the ground itself.
            computeGroundIrradiance(solarIrradianceAtTOA, texIndex);

            computeSingleScattering(wavelengths, solarIrradianceAtTOA, texIndex);
        }
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
