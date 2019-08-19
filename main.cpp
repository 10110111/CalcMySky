#include <iostream>
#include <fstream>

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

#include <glm/glm.hpp>

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
constexpr decltype(allWavelengths) fullSunSpectrum={1.037,1.249,1.684,1.975,
    1.968,1.877,1.854,1.818,
    1.723,1.604,1.516,1.408,
    1.309,1.23,1.142,1.062};
constexpr decltype(allWavelengths) fullOzoneAbsCrossSection=
{1.394e-26,6.052e-28,4.923e-27,2.434e-26,
    7.361e-26,1.831e-25,3.264e-25,4.514e-25,
    4.544e-25,2.861e-25,1.571e-25,7.902e-26,
    4.452e-26,2.781e-26,1.764e-26,5.369e-27};
static_assert(allWavelengths.size()%4==0,"Non-round number of wavelengths");

GLuint vao, vbo;
enum
{
    FBO_TRANSMITTANCE,

    FBO_COUNT
};
GLuint fbos[FBO_COUNT];
enum
{
    TEX_TRANSMITTANCE0,
    TEX_TRANSMITTANCE_LAST=TEX_TRANSMITTANCE0+allWavelengths.size()/4-1,

    TEX_COUNT
};
GLuint textures[TEX_COUNT];

std::string transmittanceTexDir=".";
GLint transmittanceTexW=128, transmittanceTexH=64;
GLint numTransmittanceIntegrationPoints=500;
GLfloat earthRadius=6371e3; // m
GLfloat atmosphereHeight=120e3; // m
GLfloat rayleighCoefficientNumerator=1.24062e+6f; // cross-section * numberDensityAtSeaLevel * lambda^4 => nm^4/m;
GLfloat mieCoefficientNumerator=4.44e-6f; // cross-section * numberDensityAtSeaLevel * (lambda/lambdaRef)^-mieAngstromExponent => m^-1;
GLfloat mieAngstromExponentReferenceWavelength=1000; // nm
GLfloat mieAngstromExponent=0;
GLfloat mieSingleScatteringAlbedo=1.0;
QString rayleighScattererRelativeDensity=1+R"(
const float rayleighScaleHeight=8000.;
return exp(-1/rayleighScaleHeight * altitude);
)";
QString mieScattererRelativeDensity=1+R"(
const float mieScaleHeight=1200;
return exp(-1/mieScaleHeight*altitude);
)";
QString ozoneDensity=1+R"(
const float km=1e3;
const float totalOzoneAmount=300*dobsonUnit;

if(altitude<10*km || altitude>40*km) return 0;
if(altitude<25*km)
    return (altitude-10*km)/sqr(25*km-10*km) * totalOzoneAmount;
return (40*km-altitude)/sqr(40*km-25*km) * totalOzoneAmount;
)";

QOpenGLFunctions_3_3_Core gl;

struct MustQuit{};

glm::vec4 Vec(QVector4D v) { return glm::vec4(v.x(), v.y(), v.z(), v.w()); }
QVector4D QVec(glm::vec4 v) { return QVector4D(v.x, v.y, v.z, v.w); }

QVector4D rayleighCoefficient(QVector4D wavelengths)
{
    return QVec(rayleighCoefficientNumerator*pow(Vec(wavelengths), glm::vec4(-4)));
}

QVector4D mieCoefficient(QVector4D wavelengths)
{
    return QVec(mieCoefficientNumerator*pow(Vec(wavelengths)/mieAngstromExponentReferenceWavelength, glm::vec4(mieAngstromExponent)));
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

void handleCompileStatus(bool success, QOpenGLShader const& shader, const char* what)
{
    if(!success)
    {
        // Qt prints compilation errors to stderr, so don't print them again
        std::cerr << "Failed to compile " << what << "\n";
        throw MustQuit{};
    }
    if(!shader.log().isEmpty())
    {
        std::cerr << "Warnings while compiling " << what << ": " << shader.log().toStdString() << "\n";
    }
};

QString addConstDefinitions(QString src)
{
    const auto constants="const float earthRadius="+QString::number(earthRadius)+"; // must be in meters\n"
                         "const float atmosphereHeight="+QString::number(atmosphereHeight)+"; // must be in meters\n"
                         R"(
const vec3 earthCenter=vec3(0,0,-earthRadius);

// These are like enum entries. They are used to choose which density to calculate.
const int DENSITY_ABS_OZONE=1;
const int DENSITY_REL_RAYLEIGH=2;
const int DENSITY_REL_MIE=3;

const float dobsonUnit = 2.687e20; // molecules/m^2
const float PI=3.1415926535897932;
#define sqr(x) ((x)*(x))
)";
    src.replace("\n#include \"const.h.glsl\"\n", constants);
    return src;
}

QString makeDensitiesSrc()
{
    return addConstDefinitions(1+R"(
#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"

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
)");
}

QString getShaderSrc(QString const& fileName)
{
    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly))
    {
        std::cerr << "Error opening shader \"" << fileName.toStdString() << "\"\n";
        throw MustQuit{};
    }
    return addConstDefinitions(file.readAll());
}

void handleFloatOption(float& dest, QString const& str, float min, float max, QString const& optionName)
{
    bool ok;
    dest = str.toFloat(&ok);
    if(!ok || dest<min || dest>max)
    {
        std::cerr << "Bad value for option " << optionName.toStdString() << "\n";
        throw MustQuit{};
    }
}

void handleCmdLine()
{
    QCommandLineParser parser;
    const auto earthRadiusOpt=QCommandLineOption{"earth-radius", "Radius of the Earth surface", "value in meters"};
    const auto atmoHeightOpt=QCommandLineOption{"atmosphere-height", "Cutoff altitude beyond which atmospheric density is neglected", "value in meters"};
    parser.addOptions({earthRadiusOpt, atmoHeightOpt});
    parser.addVersionOption();
    parser.addHelpOption();
    parser.process(*qApp);

    if(parser.isSet(earthRadiusOpt))
        handleFloatOption(earthRadius,parser.value(earthRadiusOpt), 1, 100e3, earthRadiusOpt.names().front());
    if(parser.isSet(atmoHeightOpt))
        handleFloatOption(atmosphereHeight,parser.value(atmoHeightOpt), 1e-2, 1000, atmoHeightOpt.names().front());
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName("Atmosphere textures generator");
    app.setApplicationVersion(APP_VERSION);

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

    try
    {
        init();

        QOpenGLShader commonVertShader(QOpenGLShader::Vertex);
        handleCompileStatus(commonVertShader.compileSourceCode(getShaderSrc(":shader.vert")),
                            commonVertShader, "common vertex shader");
        QOpenGLShader commonFunctionsShader(QOpenGLShader::Fragment);
        handleCompileStatus(commonFunctionsShader.compileSourceCode(getShaderSrc(":/common-functions.frag")),
                            commonFunctionsShader, "common functions shader");
        QOpenGLShader texCoordsShader(QOpenGLShader::Fragment);
        handleCompileStatus(texCoordsShader.compileSourceCode(getShaderSrc(":/texture-coordinates.frag")),
                            texCoordsShader, "texture coordinates transformation shader");
        QOpenGLShader densitiesShader(QOpenGLShader::Fragment);
        handleCompileStatus(densitiesShader.compileSourceCode(makeDensitiesSrc()),
                            densitiesShader, "shader for calculating scatterer and absorber densities");

        {
            QOpenGLShaderProgram computeTransmittanceProg;
            {
                QOpenGLShader computeTransmittanceShader(QOpenGLShader::Fragment);
                handleCompileStatus(computeTransmittanceShader.compileSourceCode(getShaderSrc(":/compute-transmittance.frag")),
                                    computeTransmittanceShader, "transmittance computation shader");
                computeTransmittanceProg.addShader(&commonVertShader);
                computeTransmittanceProg.addShader(&texCoordsShader);
                computeTransmittanceProg.addShader(&densitiesShader);
                computeTransmittanceProg.addShader(&commonFunctionsShader);
                computeTransmittanceProg.addShader(&computeTransmittanceShader);

                if(!computeTransmittanceProg.link())
                {
                    // Qt prints linking errors to stderr, so don't print them again
                    std::cerr << "Failed to link transmittance computation shader program\n";
                    return 1;
                }
            }

            gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_TRANSMITTANCE]);
            for(int texIndex=0;texIndex<allWavelengths.size()/4;++texIndex)
            {
                const QVector4D wavelengths(allWavelengths[texIndex*4+0],
                                            allWavelengths[texIndex*4+1],
                                            allWavelengths[texIndex*4+2],
                                            allWavelengths[texIndex*4+3]);
                const QVector4D ozoneCS(fullOzoneAbsCrossSection[texIndex*4+0],
                                        fullOzoneAbsCrossSection[texIndex*4+1],
                                        fullOzoneAbsCrossSection[texIndex*4+2],
                                        fullOzoneAbsCrossSection[texIndex*4+3]);

                assert(fbos[FBO_TRANSMITTANCE]);
                gl.glBindTexture(GL_TEXTURE_2D,textures[TEX_TRANSMITTANCE0+texIndex]);
                gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F_ARB,transmittanceTexW,transmittanceTexH,
                                0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
                gl.glBindTexture(GL_TEXTURE_2D,0);
                gl.glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,
                                          textures[TEX_TRANSMITTANCE0+texIndex],0);
                checkFramebufferStatus("framebuffer for transmittance texture");

                computeTransmittanceProg.bind();
                computeTransmittanceProg.setUniformValue("visibleAtmosphereHeight",atmosphereHeight);
                computeTransmittanceProg.setUniformValue("wavelengths",wavelengths);
                computeTransmittanceProg.setUniformValue("rayleighScatteringCoefficient",rayleighCoefficient(wavelengths));
                computeTransmittanceProg.setUniformValue("mieScatteringCoefficient",mieCoefficient(wavelengths));
                computeTransmittanceProg.setUniformValue("mieSingleScatteringAlbedo",mieSingleScatteringAlbedo);
                computeTransmittanceProg.setUniformValue("ozoneAbsorptionCrossSection",ozoneCS);
                computeTransmittanceProg.setUniformValue("numTransmittanceIntegrationPoints",numTransmittanceIntegrationPoints);
                computeTransmittanceProg.setUniformValue("transmittanceTextureSize",transmittanceTexW,transmittanceTexH);
                gl.glViewport(0, 0, transmittanceTexW, transmittanceTexH);
                renderUntexturedQuad();

                std::vector<glm::vec4> pixels(transmittanceTexW*transmittanceTexH);
                gl.glReadPixels(0,0,transmittanceTexW,transmittanceTexH,GL_RGBA,GL_FLOAT,pixels.data());
                std::ofstream out(transmittanceTexDir+"/transmittance-"+std::to_string(texIndex)+".f32");
                const std::uint32_t w=transmittanceTexW, h=transmittanceTexH;
                out.write(reinterpret_cast<const char*>(&w), sizeof w);
                out.write(reinterpret_cast<const char*>(&h), sizeof h);
                out.write(reinterpret_cast<const char*>(pixels.data()), pixels.size()*sizeof pixels[0]);

                if(false) // for debugging
                {
                    QImage image(transmittanceTexW, transmittanceTexH, QImage::Format_RGBA8888);
                    image.fill(Qt::magenta);
                    gl.glReadPixels(0,0,transmittanceTexW,transmittanceTexH,GL_RGBA,GL_UNSIGNED_BYTE,image.bits());
                    image.save(QString("/tmp/transmittance-png-%1.png").arg(texIndex));
                }
            }
            gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
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
