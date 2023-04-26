#define _USE_MATH_DEFINES // for MSVC to define M_PI etc.
#include <iostream>
#include <iterator>
#include <sstream>
#include <complex>
#include <memory>
#include <random>
#include <chrono>
#include <cmath>
#include <map>
#include <set>

#include <QRegularExpression>
#include <QOffscreenSurface>
#include <QSurfaceFormat>
#include <QApplication>
#include <QImage>
#include <QFile>

#include "config.h"
#include "data.hpp"
#include "util.hpp"
#include "glinit.hpp"
#include "cmdline.hpp"
#include "shaders.hpp"
#include "interpolation-guides.hpp"
#include "../common/EclipsedDoubleScatteringPrecomputer.hpp"
#include "../common/TextureAverageComputer.hpp"
#include "../common/timing.hpp"

QOpenGLFunctions_3_3_Core gl;
using glm::ivec2;
using glm::vec2;
using glm::vec4;
std::vector<glm::vec4> eclipsedDoubleScatteringAccumulatorTexture;

void saveIrradiance(const unsigned scatteringOrder, const unsigned texIndex)
{
    if(scatteringOrder==atmo.scatteringOrdersToCompute)
    {
        saveTexture(GL_TEXTURE_2D,textures[TEX_IRRADIANCE],"irradiance texture",
                    atmo.textureOutputDir+"/irradiance-wlset"+std::to_string(texIndex)+".f32",
                    {atmo.irradianceTexW, atmo.irradianceTexH});
    }

    if(!opts.dbgSaveGroundIrradiance) return;

    saveTexture(GL_TEXTURE_2D,textures[TEX_DELTA_IRRADIANCE],"delta irradiance texture",
                atmo.textureOutputDir+"/irradiance-delta-order"+std::to_string(scatteringOrder-1)+"-wlset"+std::to_string(texIndex)+".f32",
                {atmo.irradianceTexW, atmo.irradianceTexH});

    saveTexture(GL_TEXTURE_2D,textures[TEX_IRRADIANCE],"irradiance texture accumulated to order "+std::to_string(scatteringOrder-1),
                atmo.textureOutputDir+"/irradiance-accum-order"+std::to_string(scatteringOrder-1)+"-wlset"+std::to_string(texIndex)+".f32",
                {atmo.irradianceTexW, atmo.irradianceTexH});
}

void saveScatteringDensity(const unsigned scatteringOrder, const unsigned texIndex)
{
    if(!opts.dbgSaveScatDensity) return;
    saveTexture(GL_TEXTURE_3D,textures[TEX_DELTA_SCATTERING_DENSITY],
                "order "+std::to_string(scatteringOrder)+" scattering density",
                atmo.textureOutputDir+"/scattering-density"+std::to_string(scatteringOrder)+"-wlset"+std::to_string(texIndex)+".f32",
                {atmo.scatteringTextureSize[0], atmo.scatteringTextureSize[1], atmo.scatteringTextureSize[2], atmo.scatteringTextureSize[3]});
}

void render3DTexLayers(QOpenGLShaderProgram& program, const std::string_view whatIsBeingDone)
{
    if(opts.dbgNoSaveTextures) return; // don't take time to do useless computations

    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        std::cerr << "FAILED on entry to render3DTexLayers(): " << openglErrorString(err) << "\n";
        throw MustQuit{};
    }

    std::cerr << indentOutput() << whatIsBeingDone << "... ";
    for(GLsizei layer=0; layer<atmo.scatTexDepth(); ++layer)
    {
        std::ostringstream ss;
        ss << layer << " of " << atmo.scatTexDepth() << " layers done ";
        std::cerr << ss.str();

        program.setUniformValue("layer",layer);
        renderQuad();
        gl.glFinish();
        OPENGL_DEBUG_CHECK_ERROR("glFinish() FAILED in render3DTexLayers()");

        // Clear previous status and reset cursor position
        const auto statusWidth=ss.tellp();
        std::cerr << std::string(statusWidth, '\b') << std::string(statusWidth, ' ')
                  << std::string(statusWidth, '\b');
    }
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        std::cerr << "FAILED: " << openglErrorString(err) << "\n";
        throw MustQuit{};
    }
    std::cerr << "done\n";
}

void computeTransmittance(const unsigned texIndex)
{
    const auto program=compileShaderProgram("compute-transmittance.frag", "transmittance computation shader program");

    std::cerr << indentOutput() << "Computing transmittance... ";

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_TRANSMITTANCE]);
    assert(fbos[FBO_TRANSMITTANCE]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,textures[TEX_TRANSMITTANCE],0);
    checkFramebufferStatus("framebuffer for transmittance texture");

    program->bind();
    gl.glViewport(0, 0, atmo.transmittanceTexW, atmo.transmittanceTexH);
    renderQuad();

    gl.glFinish();
    std::cerr << "done\n";

    saveTexture(GL_TEXTURE_2D,textures[TEX_TRANSMITTANCE],"transmittance texture",
                atmo.textureOutputDir+"/transmittance-wlset"+std::to_string(texIndex)+".f32",
                {atmo.transmittanceTexW, atmo.transmittanceTexH});

    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void computeDirectGroundIrradiance(const unsigned texIndex)
{
    const auto program=compileShaderProgram("compute-direct-irradiance.frag", "direct ground irradiance computation shader program");

    std::cerr << indentOutput() << "Computing direct ground irradiance... ";

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_IRRADIANCE]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,textures[TEX_DELTA_IRRADIANCE],0);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,textures[TEX_IRRADIANCE],0);
    checkFramebufferStatus("framebuffer for irradiance texture");
    setDrawBuffers({GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1});

    program->bind();

    setUniformTexture(*program,GL_TEXTURE_2D,TEX_TRANSMITTANCE,0,"transmittanceTexture");

    gl.glViewport(0, 0, atmo.irradianceTexW, atmo.irradianceTexH);
    renderQuad();

    gl.glFinish();
    std::cerr << "done\n";

    saveIrradiance(1,texIndex);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

static constexpr char renderShaderFileName[]="render.frag";
constexpr char viewDirFuncFileName[]="calc-view-dir.frag";
constexpr char viewDirStubFunc[]="#version 330\nvec3 calcViewDir() { return vec3(0); }";
void saveZeroOrderScatteringRenderingShader(const unsigned texIndex)
{
    std::vector<std::pair<QString, QString>> sourcesToSave;
    virtualSourceFiles[viewDirFuncFileName]=viewDirStubFunc;
    virtualSourceFiles[renderShaderFileName]=getShaderSrc(renderShaderFileName,IgnoreCache{})
            .replace(QRegularExpression("\\b(RENDERING_ANY_ZERO_SCATTERING)\\b"), "1 /*\\1*/")
            .replace(QRegularExpression("\\b(RENDERING_ZERO_SCATTERING)\\b"), "1 /*\\1*/");
    const auto program=compileShaderProgram(renderShaderFileName,
                                            "zero-order scattering rendering shader program",
                                            UseGeomShader{false}, &sourcesToSave);
    for(const auto& [filename, src] : sourcesToSave)
    {
        if(filename==viewDirFuncFileName) continue;

        const auto filePath=QString("%1/shaders/zero-order-scattering/%2/%3")
                                .arg(atmo.textureOutputDir.c_str()).arg(texIndex).arg(filename);
        std::cerr << indentOutput() << "Saving shader \"" << filePath << "\"...";
        QFile file(filePath);
        if(!file.open(QFile::WriteOnly))
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        file.write(src.toUtf8());
        file.flush();
        if(file.error())
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        std::cerr << "done\n";
    }
}

void saveEclipsedZeroOrderScatteringRenderingShader(const unsigned texIndex)
{
    std::vector<std::pair<QString, QString>> sourcesToSave;
    virtualSourceFiles[viewDirFuncFileName]=viewDirStubFunc;
    virtualSourceFiles[renderShaderFileName]=getShaderSrc(renderShaderFileName,IgnoreCache{})
            .replace(QRegularExpression("\\b(RENDERING_ANY_ZERO_SCATTERING)\\b"), "1 /*\\1*/")
            .replace(QRegularExpression("\\b(RENDERING_ECLIPSED_ZERO_SCATTERING)\\b"), "1 /*\\1*/");
    const auto program=compileShaderProgram(renderShaderFileName,
                                            "eclipsed zero-order scattering rendering shader program",
                                            UseGeomShader{false}, &sourcesToSave);
    for(const auto& [filename, src] : sourcesToSave)
    {
        if(filename==viewDirFuncFileName) continue;

        const auto filePath=QString("%1/shaders/eclipsed-zero-order-scattering/%2/%3")
                                .arg(atmo.textureOutputDir.c_str()).arg(texIndex).arg(filename);
        std::cerr << indentOutput() << "Saving shader \"" << filePath << "\"...";
        QFile file(filePath);
        if(!file.open(QFile::WriteOnly))
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        file.write(src.toUtf8());
        file.flush();
        if(file.error())
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        std::cerr << "done\n";
    }
}

void saveMultipleScatteringRenderingShader(const unsigned texIndex)
{
    std::vector<std::pair<QString, QString>> sourcesToSave;
    virtualSourceFiles[viewDirFuncFileName]=viewDirStubFunc;
    const QString macroToReplace = opts.saveResultAsRadiance ? "RENDERING_MULTIPLE_SCATTERING_RADIANCE" : "RENDERING_MULTIPLE_SCATTERING_LUMINANCE";
    virtualSourceFiles[renderShaderFileName]=getShaderSrc(renderShaderFileName,IgnoreCache{})
                                                .replace(QRegularExpression("\\b("+macroToReplace+")\\b"), "1 /*\\1*/");
    const auto program=compileShaderProgram(renderShaderFileName,
                                            "multiple scattering rendering shader program",
                                            UseGeomShader{false}, &sourcesToSave);
    for(const auto& [filename, src] : sourcesToSave)
    {
        if(filename==viewDirFuncFileName) continue;

        const auto filePath = opts.saveResultAsRadiance ? QString("%1/shaders/multiple-scattering/%2/%3").arg(atmo.textureOutputDir.c_str()).arg(texIndex).arg(filename)
                                                        : QString("%1/shaders/multiple-scattering/%2").arg(atmo.textureOutputDir.c_str()).arg(filename);
        std::cerr << indentOutput() << "Saving shader \"" << filePath << "\"...";
        QFile file(filePath);
        if(!file.open(QFile::WriteOnly))
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        file.write(src.toUtf8());
        file.flush();
        if(file.error())
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        std::cerr << "done\n";
    }
}

void saveSingleScatteringRenderingShader(const unsigned texIndex, AtmosphereParameters::Scatterer const& scatterer, const SingleScatteringRenderMode renderMode)
{
    virtualSourceFiles[PHASE_FUNCTIONS_SHADER_FILENAME]=makePhaseFunctionsSrc()+
        "vec4 currentPhaseFunction(float dotViewSun) { return phaseFunction_"+scatterer.name+"(dotViewSun); }\n";

    std::vector<std::pair<QString, QString>> sourcesToSave;
    virtualSourceFiles[viewDirFuncFileName]=viewDirStubFunc;
    const auto renderModeDefine = renderMode==SSRM_ON_THE_FLY ? "RENDERING_SINGLE_SCATTERING_ON_THE_FLY" :
                                  scatterer.phaseFunctionType==PhaseFunctionType::General ? "RENDERING_SINGLE_SCATTERING_PRECOMPUTED_RADIANCE"
                                                                                          : "RENDERING_SINGLE_SCATTERING_PRECOMPUTED_LUMINANCE";
    const bool phaseFuncIsEmbedded = scatterer.phaseFunctionType==PhaseFunctionType::Smooth;
    virtualSourceFiles[renderShaderFileName]=getShaderSrc(renderShaderFileName,IgnoreCache{})
                                 .replace(QRegularExpression("\\b(RENDERING_ANY_SINGLE_SCATTERING)\\b"), "1 /*\\1*/")
                                 .replace(QRegularExpression("\\b(RENDERING_ANY_NORMAL_SINGLE_SCATTERING)\\b"), "1 /*\\1*/")
                                 .replace(QRegularExpression("\\b(PHASE_FUNCTION_IS_EMBEDDED)\\b"), phaseFuncIsEmbedded ? "1" : "0")
                                 .replace(QRegularExpression(QString("\\b(%1)\\b").arg(renderModeDefine)), "1 /*\\1*/");
    const auto program=compileShaderProgram(renderShaderFileName,
                                            "single scattering rendering shader program",
                                            UseGeomShader{false}, &sourcesToSave);
    for(const auto& [filename, src] : sourcesToSave)
    {
        if(filename==viewDirFuncFileName) continue;

        const auto filePath = scatterer.phaseFunctionType==PhaseFunctionType::General || renderMode==SSRM_ON_THE_FLY ?
           QString("%1/shaders/single-scattering/%2/%3/%4/%5").arg(atmo.textureOutputDir.c_str()).arg(toString(renderMode)).arg(texIndex).arg(scatterer.name).arg(filename) :
           QString("%1/shaders/single-scattering/%2/%3/%4").arg(atmo.textureOutputDir.c_str()).arg(toString(renderMode)).arg(scatterer.name).arg(filename);
        std::cerr << indentOutput() << "Saving shader \"" << filePath << "\"...";
        QFile file(filePath);
        if(!file.open(QFile::WriteOnly))
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        file.write(src.toUtf8());
        file.flush();
        if(file.error())
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        std::cerr << "done\n";
    }
}

void saveEclipsedSingleScatteringRenderingShader(const unsigned texIndex, AtmosphereParameters::Scatterer const& scatterer, const SingleScatteringRenderMode renderMode)
{
    virtualSourceFiles.erase(SINGLE_SCATTERING_ECLIPSED_FILENAME); // Need to refresh it after computeEclipsedDoubleScattering()
    virtualSourceFiles[PHASE_FUNCTIONS_SHADER_FILENAME]=makePhaseFunctionsSrc()+
        "vec4 currentPhaseFunction(float dotViewSun) { return phaseFunction_"+scatterer.name+"(dotViewSun); }\n";

    std::vector<std::pair<QString, QString>> sourcesToSave;
    static constexpr char renderShaderFileName[]="render.frag";
    const auto renderModeDefine = renderMode==SSRM_ON_THE_FLY ? "RENDERING_ECLIPSED_SINGLE_SCATTERING_ON_THE_FLY" :
                                  scatterer.phaseFunctionType==PhaseFunctionType::General ? "RENDERING_ECLIPSED_SINGLE_SCATTERING_PRECOMPUTED_RADIANCE"
                                                                                          : "RENDERING_ECLIPSED_SINGLE_SCATTERING_PRECOMPUTED_LUMINANCE";
    virtualSourceFiles[renderShaderFileName]=getShaderSrc(renderShaderFileName,IgnoreCache{})
                                    .replace(QRegularExpression("\\b(RENDERING_ANY_SINGLE_SCATTERING)\\b"), "1 /*\\1*/")
                                    .replace(QRegularExpression("\\b(RENDERING_ANY_ECLIPSED_SINGLE_SCATTERING)\\b"), "1 /*\\1*/")
                                    .replace(QRegularExpression(QString("\\b(%1)\\b").arg(renderModeDefine)), "1 /*\\1*/");
    const auto program=compileShaderProgram(renderShaderFileName,
                                            "single scattering rendering shader program",
                                            UseGeomShader{false}, &sourcesToSave);
    for(const auto& [filename, src] : sourcesToSave)
    {
        if(filename==viewDirFuncFileName) continue;

        const auto filePath = scatterer.phaseFunctionType==PhaseFunctionType::General || renderMode==SSRM_ON_THE_FLY ?
            QString("%1/shaders/single-scattering-eclipsed/%2/%3/%4/%5").arg(atmo.textureOutputDir.c_str()).arg(toString(renderMode)).arg(texIndex).arg(scatterer.name).arg(filename) :
            QString("%1/shaders/single-scattering-eclipsed/%2/%3/%4").arg(atmo.textureOutputDir.c_str()).arg(toString(renderMode)).arg(scatterer.name).arg(filename);
        std::cerr << indentOutput() << "Saving shader \"" << filePath << "\"...";
        QFile file(filePath);
        if(!file.open(QFile::WriteOnly))
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        file.write(src.toUtf8());
        file.flush();
        if(file.error())
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        std::cerr << "done\n";
    }
}

void saveEclipsedSingleScatteringComputationShader(const unsigned texIndex, AtmosphereParameters::Scatterer const& scatterer)
{
    // Not removing SINGLE_SCATTERING_ECLIPSED_FILENAME, since it's refreshed in saveEclipsedSingleScatteringRenderingShader()
    virtualSourceFiles[PHASE_FUNCTIONS_SHADER_FILENAME]=makePhaseFunctionsSrc()+
        "vec4 currentPhaseFunction(float dotViewSun) { return phaseFunction_"+scatterer.name+"(dotViewSun); }\n";

    std::vector<std::pair<QString, QString>> sourcesToSave;
    static constexpr char renderShaderFileName[]="compute-eclipsed-single-scattering.frag";
    virtualSourceFiles[renderShaderFileName]=getShaderSrc(renderShaderFileName,IgnoreCache{})
        .replace(QRegularExpression(QString("\\b(%1)\\b").arg(scatterer.phaseFunctionType==PhaseFunctionType::General ?
                                                                  "COMPUTE_RADIANCE" : "COMPUTE_LUMINANCE")), "1 /*\\1*/");
    const auto program=compileShaderProgram(renderShaderFileName,
                                            "single scattering rendering shader program",
                                            UseGeomShader{false}, &sourcesToSave);
    for(const auto& [filename, src] : sourcesToSave)
    {
        if(filename==viewDirFuncFileName) continue;

        const auto filePath = QString("%1/shaders/single-scattering-eclipsed/precomputation/%2/%3/%4")
                                    .arg(atmo.textureOutputDir.c_str())
                                    .arg(texIndex)
                                    .arg(scatterer.name)
                                    .arg(filename);
        std::cerr << indentOutput() << "Saving shader \"" << filePath << "\"...";
        QFile file(filePath);
        if(!file.open(QFile::WriteOnly))
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        file.write(src.toUtf8());
        file.flush();
        if(file.error())
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        std::cerr << "done\n";
    }
}

void saveEclipsedDoubleScatteringRenderingShader(const unsigned texIndex)
{
    virtualSourceFiles.erase(DOUBLE_SCATTERING_ECLIPSED_FILENAME);

    std::vector<std::pair<QString, QString>> sourcesToSave;
    virtualSourceFiles[viewDirFuncFileName]=viewDirStubFunc;
    const QString macroToReplace = opts.saveResultAsRadiance ? "RENDERING_ECLIPSED_DOUBLE_SCATTERING_PRECOMPUTED_RADIANCE"
                                                             : "RENDERING_ECLIPSED_DOUBLE_SCATTERING_PRECOMPUTED_LUMINANCE";
    virtualSourceFiles[renderShaderFileName]=getShaderSrc(renderShaderFileName,IgnoreCache{})
        .replace(QRegularExpression("\\b("+macroToReplace+")\\b"), "1 /*\\1*/");
    const auto program=compileShaderProgram(renderShaderFileName,
                                            "double scattering rendering shader program",
                                            UseGeomShader{false}, &sourcesToSave);
    for(const auto& [filename, src] : sourcesToSave)
    {
        if(filename==viewDirFuncFileName) continue;

        const auto filePath = opts.saveResultAsRadiance ? QString("%1/shaders/double-scattering-eclipsed/precomputed/%2/%3")
                                                                    .arg(atmo.textureOutputDir.c_str())
                                                                    .arg(texIndex)
                                                                    .arg(filename)
                                                        : QString("%1/shaders/double-scattering-eclipsed/precomputed/%2")
                                                                    .arg(atmo.textureOutputDir.c_str())
                                                                    .arg(filename);
        std::cerr << indentOutput() << "Saving shader \"" << filePath << "\"...";
        QFile file(filePath);
        if(!file.open(QFile::WriteOnly))
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        file.write(src.toUtf8());
        file.flush();
        if(file.error())
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        std::cerr << "done\n";
    }
}

void saveLightPollutionRenderingShader(const unsigned texIndex)
{
    if(!opts.saveResultAsRadiance && texIndex!=0)
    {
        // There's only one luminance shader, so don't re-save it for each texIndex
        return;
    }

    std::vector<std::pair<QString, QString>> sourcesToSave;
    virtualSourceFiles[viewDirFuncFileName]=viewDirStubFunc;
    const QString macroToReplace = opts.saveResultAsRadiance ? "RENDERING_LIGHT_POLLUTION_RADIANCE" : "RENDERING_LIGHT_POLLUTION_LUMINANCE";
    virtualSourceFiles[renderShaderFileName]=getShaderSrc(renderShaderFileName,IgnoreCache{})
                                                .replace(QRegularExpression("\\b("+macroToReplace+")\\b"), "1 /*\\1*/")
                                                .replace(QRegularExpression("\\b(RENDERING_ANY_LIGHT_POLLUTION)\\b"), "1/*\\1*/");
    const auto program=compileShaderProgram(renderShaderFileName,
                                            "light pollution rendering shader program",
                                            UseGeomShader{false}, &sourcesToSave);
    for(const auto& [filename, src] : sourcesToSave)
    {
        if(filename==viewDirFuncFileName) continue;

        const auto filePath = opts.saveResultAsRadiance ? QString("%1/shaders/light-pollution/%2/%3").arg(atmo.textureOutputDir.c_str()).arg(texIndex).arg(filename)
                                                        : QString("%1/shaders/light-pollution/%2").arg(atmo.textureOutputDir.c_str()).arg(filename);
        std::cerr << indentOutput() << "Saving shader \"" << filePath << "\"...";
        QFile file(filePath);
        if(!file.open(QFile::WriteOnly))
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        file.write(src.toUtf8());
        file.flush();
        if(file.error())
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        std::cerr << "done\n";
    }
}


void accumulateSingleScattering(const unsigned texIndex, AtmosphereParameters::Scatterer const& scatterer)
{
    gl.glBlendFunc(GL_ONE, GL_ONE);
    gl.glEnable(GL_BLEND);
    auto& targetTexture=accumulatedSingleScatteringTextures[scatterer.name];
    if(!targetTexture)
    {
        gl.glGenTextures(1, &targetTexture);
        gl.glBindTexture(GL_TEXTURE_3D,targetTexture);
        gl.glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        gl.glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
        setupTexture(targetTexture, atmo.scatTexWidth(),atmo.scatTexHeight(),atmo.scatTexDepth());
        gl.glDisable(GL_BLEND);
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_SINGLE_SCATTERING]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, targetTexture,0);
    checkFramebufferStatus("framebuffer for accumulation of single scattering radiance");

    const auto program=compileShaderProgram("accumulate-single-scattering-texture.frag",
                                            "single scattering accumulation shader program",
                                            UseGeomShader{});
    program->bind();
    setUniformTexture(*program,GL_TEXTURE_3D,TEX_DELTA_SCATTERING,0,"tex");
    program->setUniformValue("radianceToLuminance", toQMatrix(radianceToLuminance(texIndex, atmo.allWavelengths)));
    program->setUniformValue("embedPhaseFunction", scatterer.phaseFunctionType==PhaseFunctionType::Smooth);
    render3DTexLayers(*program, "Blending single scattering layers into accumulator texture");

    gl.glDisable(GL_BLEND);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);

    if(texIndex+1==atmo.allWavelengths.size())
    {
        const auto filePath = atmo.textureOutputDir+"/single-scattering/"+scatterer.name.toStdString()+"-xyzw.f32";
        const std::vector<int> sizes{atmo.scatteringTextureSize[0], atmo.scatteringTextureSize[1],
                                     atmo.scatteringTextureSize[2], atmo.scatteringTextureSize[3]};
        const auto data = saveTexture(GL_TEXTURE_3D,targetTexture, "single scattering texture",
                                      filePath, sizes, ReturnTextureData{true});
        if(scatterer.needsInterpolationGuides && !opts.dbgNoSaveTextures)
            generateInterpolationGuidesForScatteringTexture(filePath, data, sizes);
    }
}

void computeSingleScattering(const unsigned texIndex, AtmosphereParameters::Scatterer const& scatterer)
{
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_DELTA_SCATTERING]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, textures[TEX_DELTA_SCATTERING],0);
    checkFramebufferStatus("framebuffer for first scattering");

    gl.glViewport(0, 0, atmo.scatTexWidth(), atmo.scatTexHeight());

    const auto src=makeScattererDensityFunctionsSrc()+
                    "float scattererDensity(float alt) { return scattererNumberDensity_"+scatterer.name+"(alt); }\n"+
                    "vec4 scatteringCrossSection() { return "+toString(scatterer.scatteringCrossSection(atmo.allWavelengths[texIndex]))+"; }\n";
    virtualSourceFiles[DENSITIES_SHADER_FILENAME]=src;
    virtualSourceFiles[PHASE_FUNCTIONS_SHADER_FILENAME]=makePhaseFunctionsSrc()+
        "vec4 currentPhaseFunction(float dotViewSun) { return phaseFunction_"+scatterer.name+"(dotViewSun); }\n";
    const auto program=compileShaderProgram("compute-single-scattering.frag",
                                            "single scattering computation shader program",
                                            UseGeomShader{});
    program->bind();
    setUniformTexture(*program,GL_TEXTURE_2D,TEX_TRANSMITTANCE,0,"transmittanceTexture");

    render3DTexLayers(*program, "Computing single scattering layers");

    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);

    switch(scatterer.phaseFunctionType)
    {
    case PhaseFunctionType::General:
    {
        const auto filePath = atmo.textureOutputDir+"/single-scattering/"+std::to_string(texIndex)+
                                "/"+scatterer.name.toStdString()+".f32";
        const std::vector<int> sizes{atmo.scatteringTextureSize[0], atmo.scatteringTextureSize[1],
                                     atmo.scatteringTextureSize[2], atmo.scatteringTextureSize[3]};
        const auto data = saveTexture(GL_TEXTURE_3D,textures[TEX_DELTA_SCATTERING], "single scattering texture",
                                      filePath, sizes, ReturnTextureData{true});
        if(scatterer.needsInterpolationGuides && !opts.dbgNoSaveTextures)
            generateInterpolationGuidesForScatteringTexture(filePath, data, sizes);
        break;
    }
    case PhaseFunctionType::Achromatic:
    case PhaseFunctionType::Smooth:
        accumulateSingleScattering(texIndex, scatterer);
        break;
    }

    saveSingleScatteringRenderingShader(texIndex, scatterer, SSRM_ON_THE_FLY);
    saveSingleScatteringRenderingShader(texIndex, scatterer, SSRM_PRECOMPUTED);
    saveEclipsedSingleScatteringRenderingShader(texIndex, scatterer, SSRM_ON_THE_FLY);
    saveEclipsedSingleScatteringRenderingShader(texIndex, scatterer, SSRM_PRECOMPUTED);
    saveEclipsedSingleScatteringComputationShader(texIndex, scatterer);
}

void computeIndirectIrradianceOrder1(unsigned scattererIndex);
void computeScatteringOrder1AndScatteringDensityOrder2(const unsigned texIndex)
{
    constexpr unsigned scatteringOrder=2;

    virtualSourceFiles[DENSITIES_SHADER_FILENAME]=makeScattererDensityFunctionsSrc();
    std::unique_ptr<QOpenGLShaderProgram> program;
    {
        // Make a stub for current phase function. It's not used for ground radiance, but we need it to avoid linking errors.
        virtualSourceFiles[PHASE_FUNCTIONS_SHADER_FILENAME]=makePhaseFunctionsSrc()+
            "vec4 currentPhaseFunction(float dotViewSun) { return vec4(3.4028235e38); }\n";

        // Doing replacements instead of using uniforms is meant to
        //  1) Improve performance by statically avoiding branching
        //  2) Ease debugging by clearing the list of really-used uniforms (this can be printed by dumpActiveUniforms())
        virtualSourceFiles[COMPUTE_SCATTERING_DENSITY_FILENAME]=getShaderSrc(COMPUTE_SCATTERING_DENSITY_FILENAME,IgnoreCache{})
                                               .replace(QRegularExpression("\\bRADIATION_IS_FROM_GROUND_ONLY\\b"), "true")
                                               .replace(QRegularExpression("\\bSCATTERING_ORDER\\b"), QString::number(scatteringOrder));
        // recompile the program
        program=compileShaderProgram(COMPUTE_SCATTERING_DENSITY_FILENAME,
                                     "scattering density computation shader program", UseGeomShader{});
    }

    gl.glViewport(0, 0, atmo.scatTexWidth(), atmo.scatTexHeight());

    program->bind();

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_MULTIPLE_SCATTERING]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,textures[TEX_DELTA_SCATTERING_DENSITY],0);
    checkFramebufferStatus("framebuffer for scattering density");

    setUniformTexture(*program,GL_TEXTURE_2D,TEX_TRANSMITTANCE   ,0,"transmittanceTexture");
    setUniformTexture(*program,GL_TEXTURE_2D,TEX_DELTA_IRRADIANCE,1,"irradianceTexture");

    // Scattering density is only used to compute multiple scattering at the following stages.
    // If multiple scattering is not requested, don't take the time needlessly.
    if(atmo.scatteringOrdersToCompute >= 2)
    {
        render3DTexLayers(*program, "Computing scattering density layers for radiation from the ground");

        if(opts.dbgSaveScatDensityOrder2FromGround)
        {
            saveTexture(GL_TEXTURE_3D,textures[TEX_DELTA_SCATTERING_DENSITY],
                        "order 2 scattering density from ground texture",
                        atmo.textureOutputDir+"/scattering-density2-from-ground-wlset"+std::to_string(texIndex)+".f32",
                        {atmo.scatteringTextureSize[0], atmo.scatteringTextureSize[1], atmo.scatteringTextureSize[2], atmo.scatteringTextureSize[3]});
        }
    }

    gl.glBlendFunc(GL_ONE, GL_ONE);
    for(unsigned scattererIndex=0; scattererIndex<atmo.scatterers.size(); ++scattererIndex)
    {
        const auto& scatterer=atmo.scatterers[scattererIndex];
        std::cerr << indentOutput() << "Processing scatterer \""+scatterer.name.toStdString()+"\":\n";
        OutputIndentIncrease incr;

        // Current phase function is updated in the single scattering computation while saving the rendering shader
        computeSingleScattering(texIndex, scatterer);

        gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_MULTIPLE_SCATTERING]);

        {
            virtualSourceFiles[COMPUTE_SCATTERING_DENSITY_FILENAME]=getShaderSrc(COMPUTE_SCATTERING_DENSITY_FILENAME,IgnoreCache{})
                                                .replace(QRegularExpression("\\bRADIATION_IS_FROM_GROUND_ONLY\\b"), "false")
                                                .replace(QRegularExpression("\\bSCATTERING_ORDER\\b"), QString::number(scatteringOrder));
            // recompile the program
            program=compileShaderProgram(COMPUTE_SCATTERING_DENSITY_FILENAME,
                                                        "scattering density computation shader program", UseGeomShader{});
        }
        program->bind();

        setUniformTexture(*program,GL_TEXTURE_3D,TEX_DELTA_SCATTERING,1,"firstScatteringTexture");

        gl.glEnable(GL_BLEND);
        // Scattering density is only used to compute multiple scattering at the following stages.
        // If multiple scattering is not requested, don't take the time needlessly.
        if(atmo.scatteringOrdersToCompute >= 2)
        {
            render3DTexLayers(*program, "Computing scattering density layers");
        }

        // Disables blending before returning
        computeIndirectIrradianceOrder1(scattererIndex);
    }
    gl.glDisable(GL_BLEND);
    saveIrradiance(scatteringOrder,texIndex);
    saveScatteringDensity(scatteringOrder,texIndex);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void computeScatteringDensity(const unsigned scatteringOrder, const unsigned texIndex)
{
    assert(scatteringOrder>2);

    gl.glViewport(0, 0, atmo.scatTexWidth(), atmo.scatTexHeight());
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_MULTIPLE_SCATTERING]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,textures[TEX_DELTA_SCATTERING_DENSITY],0);

    virtualSourceFiles[COMPUTE_SCATTERING_DENSITY_FILENAME]=getShaderSrc(COMPUTE_SCATTERING_DENSITY_FILENAME,IgnoreCache{})
                                         .replace(QRegularExpression("\\bRADIATION_IS_FROM_GROUND_ONLY\\b"), "false")
                                         .replace(QRegularExpression("\\bSCATTERING_ORDER\\b"), QString::number(scatteringOrder));
    // recompile the program
    const std::unique_ptr<QOpenGLShaderProgram> program=compileShaderProgram(COMPUTE_SCATTERING_DENSITY_FILENAME,
                                                                             "scattering density computation shader program",
                                                                             UseGeomShader{});
    program->bind();

    setUniformTexture(*program,GL_TEXTURE_2D,TEX_TRANSMITTANCE   ,0,"transmittanceTexture");
    setUniformTexture(*program,GL_TEXTURE_2D,TEX_DELTA_IRRADIANCE,1,"irradianceTexture");
    setUniformTexture(*program,GL_TEXTURE_3D,TEX_DELTA_SCATTERING,2,"multipleScatteringTexture");

    render3DTexLayers(*program, "Computing scattering density layers");
    saveScatteringDensity(scatteringOrder,texIndex);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void computeIndirectIrradianceOrder1(const unsigned scattererIndex)
{
    constexpr unsigned scatteringOrder=2;

    gl.glViewport(0, 0, atmo.irradianceTexW, atmo.irradianceTexH);

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_IRRADIANCE]);
    gl.glBlendFunc(GL_ONE, GL_ONE);
    if(scattererIndex==0)
        gl.glDisablei(GL_BLEND, 0); // First scatterer overwrites delta-irradiance-texture
    else
        gl.glEnablei(GL_BLEND, 0); // Second and subsequent scatterers blend into delta-irradiance-texture
    gl.glEnablei(GL_BLEND, 1); // Total irradiance is always accumulated

    const auto& scatterer=atmo.scatterers[scattererIndex];

    virtualSourceFiles[PHASE_FUNCTIONS_SHADER_FILENAME]=makePhaseFunctionsSrc()+
        "vec4 currentPhaseFunction(float dotViewSun) { return phaseFunction_"+scatterer.name+"(dotViewSun); }\n";

    virtualSourceFiles[COMPUTE_INDIRECT_IRRADIANCE_FILENAME]=getShaderSrc(COMPUTE_INDIRECT_IRRADIANCE_FILENAME,IgnoreCache{})
                                           .replace(QRegularExpression("\\bSCATTERING_ORDER\\b"), QString::number(scatteringOrder-1));
    std::unique_ptr<QOpenGLShaderProgram> program=compileShaderProgram(COMPUTE_INDIRECT_IRRADIANCE_FILENAME,
                                                                       "indirect irradiance computation shader program");
    program->bind();
    setUniformTexture(*program,GL_TEXTURE_3D,TEX_DELTA_SCATTERING,0,"firstScatteringTexture");

    std::cerr << indentOutput() << "Computing indirect irradiance... ";
    renderQuad();
    gl.glFinish();
    std::cerr << "done\n";

    gl.glDisable(GL_BLEND);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void computeIndirectIrradiance(const unsigned scatteringOrder, const unsigned texIndex)
{
    assert(scatteringOrder>2);
    gl.glViewport(0, 0, atmo.irradianceTexW, atmo.irradianceTexH);

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_IRRADIANCE]);
    gl.glBlendFunc(GL_ONE, GL_ONE);
    gl.glDisablei(GL_BLEND, 0); // Overwrite delta-irradiance-texture
    gl.glEnablei(GL_BLEND, 1); // Accumulate total irradiance

    virtualSourceFiles[COMPUTE_INDIRECT_IRRADIANCE_FILENAME]=getShaderSrc(COMPUTE_INDIRECT_IRRADIANCE_FILENAME,IgnoreCache{})
                                           .replace(QRegularExpression("\\bSCATTERING_ORDER\\b"), QString::number(scatteringOrder-1));
    std::unique_ptr<QOpenGLShaderProgram> program=compileShaderProgram(COMPUTE_INDIRECT_IRRADIANCE_FILENAME,
                                                                       "indirect irradiance computation shader program");
    program->bind();
    setUniformTexture(*program,GL_TEXTURE_3D,TEX_DELTA_SCATTERING,0,"multipleScatteringTexture");

    std::cerr << indentOutput() << "Computing indirect irradiance... ";
    renderQuad();
    gl.glFinish();
    std::cerr << "done\n";

    gl.glDisable(GL_BLEND);
    saveIrradiance(scatteringOrder,texIndex);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void accumulateMultipleScattering(const unsigned scatteringOrder, const unsigned texIndex)
{
    // We didn't render to the accumulating texture when computing delta scattering to avoid holding
    // more than two 4D textures in VRAM at once.
    // Now it's time to do this by only holding the accumulator and delta scattering texture in VRAM.
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBlendFunc(GL_ONE, GL_ONE);
    if(scatteringOrder>2 || (texIndex>0 && !opts.saveResultAsRadiance))
        gl.glEnable(GL_BLEND);
    else
        gl.glDisable(GL_BLEND);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_MULTIPLE_SCATTERING]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, textures[TEX_MULTIPLE_SCATTERING],0);
    checkFramebufferStatus("framebuffer for accumulation of multiple scattering data");

    const auto program=compileShaderProgram("copy-scattering-texture-3d.frag",
                                            "scattering texture copy-blend shader program",
                                            UseGeomShader{});
    program->bind();
    if(!opts.saveResultAsRadiance)
        program->setUniformValue("radianceToLuminance", toQMatrix(radianceToLuminance(texIndex, atmo.allWavelengths)));
    setUniformTexture(*program,GL_TEXTURE_3D,TEX_DELTA_SCATTERING,0,"tex");
    render3DTexLayers(*program, "Blending multiple scattering layers into accumulator texture");
    gl.glDisable(GL_BLEND);

    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);

    if(opts.dbgSaveAccumScattering)
    {
        saveTexture(GL_TEXTURE_3D,textures[TEX_MULTIPLE_SCATTERING],
                    "multiple scattering accumulator texture",
                    atmo.textureOutputDir+"/multiple-scattering-to-order"+std::to_string(scatteringOrder)+"-wlset"+std::to_string(texIndex)+".f32",
                    {atmo.scatteringTextureSize[0], atmo.scatteringTextureSize[1], atmo.scatteringTextureSize[2], atmo.scatteringTextureSize[3]});
    }
    if(scatteringOrder==atmo.scatteringOrdersToCompute && (texIndex+1==atmo.allWavelengths.size() || opts.saveResultAsRadiance))
    {
        const auto filename = opts.saveResultAsRadiance ?
            atmo.textureOutputDir+"/multiple-scattering-wlset"+std::to_string(texIndex)+".f32" :
            atmo.textureOutputDir+"/multiple-scattering-xyzw.f32";
        saveTexture(GL_TEXTURE_3D,textures[TEX_MULTIPLE_SCATTERING],
                    "multiple scattering accumulator texture", filename,
                    {atmo.scatteringTextureSize[0], atmo.scatteringTextureSize[1], atmo.scatteringTextureSize[2], atmo.scatteringTextureSize[3]});
    }
}

void computeMultipleScatteringFromDensity(const unsigned scatteringOrder, const unsigned texIndex)
{
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_MULTIPLE_SCATTERING]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, textures[TEX_DELTA_SCATTERING],0);
    checkFramebufferStatus("framebuffer for delta multiple scattering");

    gl.glViewport(0, 0, atmo.scatTexWidth(), atmo.scatTexHeight());

    {
        const auto program=compileShaderProgram("compute-multiple-scattering.frag",
                                                "multiple scattering computation shader program",
                                                UseGeomShader{});
        program->bind();

        setUniformTexture(*program,GL_TEXTURE_2D,TEX_TRANSMITTANCE,0,"transmittanceTexture");
        setUniformTexture(*program,GL_TEXTURE_3D,TEX_DELTA_SCATTERING_DENSITY,1,"scatteringDensityTexture");

        render3DTexLayers(*program, "Computing multiple scattering layers");

        if(opts.dbgSaveDeltaScattering)
        {
            saveTexture(GL_TEXTURE_3D,textures[TEX_DELTA_SCATTERING],
                        "delta scattering texture",
                        atmo.textureOutputDir+"/delta-scattering-order"+std::to_string(scatteringOrder)+"-wlset"+std::to_string(texIndex)+".f32",
                        {atmo.scatteringTextureSize[0], atmo.scatteringTextureSize[1], atmo.scatteringTextureSize[2], atmo.scatteringTextureSize[3]});
        }
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
    accumulateMultipleScattering(scatteringOrder, texIndex);
}

void computeMultipleScattering(const unsigned texIndex)
{
    // Due to interleaving of calculations of first scattering for each scatterer with the
    // second-order scattering density and irradiance we have to do this iteration separately.
    {
        std::cerr << indentOutput() << "Working on scattering orders 1 and 2:\n";
        OutputIndentIncrease incr;

        computeScatteringOrder1AndScatteringDensityOrder2(texIndex);
        if(atmo.scatteringOrdersToCompute >= 2)
        {
            computeMultipleScatteringFromDensity(2,texIndex);
        }
    }
    for(unsigned scatteringOrder=3; scatteringOrder<=atmo.scatteringOrdersToCompute; ++scatteringOrder)
    {
        std::cerr << indentOutput() << "Working on scattering order " << scatteringOrder << ":\n";
        OutputIndentIncrease incr;

        computeScatteringDensity(scatteringOrder,texIndex);
        computeIndirectIrradiance(scatteringOrder,texIndex);
        computeMultipleScatteringFromDensity(scatteringOrder,texIndex);
    }
}

// XXX: keep in sync with the GLSL version in texture-coordinates.frag
float unitRangeTexCoordToCosSZA(const float texCoord)
{
    const float distMin=atmo.atmosphereHeight;
    const float distMax=atmo.lengthOfHorizRayFromGroundToBorderOfAtmo;
    // TODO: choose a more descriptive name, same as in cosSZAToUnitRangeTexCoord()
    const float A=atmo.earthRadius/(distMax-distMin);
    // TODO: choose a more descriptive name, same as in cosSZAToUnitRangeTexCoord()
    const float a=(A-A*texCoord)/(1+A*texCoord);
    const float distFromGroundToTopAtmoBorder=distMin+std::min(a,A)*(distMax-distMin);
    return distFromGroundToTopAtmoBorder==0 ? 1 :
        clampCosine((sqr(atmo.lengthOfHorizRayFromGroundToBorderOfAtmo)-sqr(distFromGroundToTopAtmoBorder)) /
                    (2*atmo.earthRadius*distFromGroundToTopAtmoBorder));
}

std::unique_ptr<QOpenGLShaderProgram> saveEclipsedDoubleScatteringComputationShader(const unsigned texIndex)
{
    QString scatCoefDef="vec4 totalScatteringCoefficient=vec4(0);\n";
    for(const auto& scatterer : atmo.scatterers)
    {
        scatCoefDef += "    totalScatteringCoefficient += "
                         " scattererNumberDensity_" + scatterer.name + "(altAtDist)"
                       " * scatteringCrossSection_" + scatterer.name +
                       " * phaseFunction_" + scatterer.name + "(dotViewSun)"
                       ";\n";
    }
    virtualSourceFiles[SINGLE_SCATTERING_ECLIPSED_FILENAME]=getShaderSrc(SINGLE_SCATTERING_ECLIPSED_FILENAME,IgnoreCache{})
                                       .replace(QRegularExpression("\\bCOMPUTE_TOTAL_SCATTERING_COEFFICIENT;"), scatCoefDef)
                                       .replace(QRegularExpression("\\b(ALL_SCATTERERS_AT_ONCE_WITH_PHASE_FUNCTION)\\b"), "1 /*\\1*/");
    std::vector<std::pair<QString, QString>> sourcesToSave;
    auto program=compileShaderProgram(COMPUTE_ECLIPSED_DOUBLE_SCATTERING_FILENAME,
                                      "eclipsed double scattering computation shader program",
                                      UseGeomShader{false}, &sourcesToSave);
    for(const auto& [filename, src] : sourcesToSave)
    {
        if(filename==viewDirFuncFileName) continue;

        const auto filePath = QString("%1/shaders/double-scattering-eclipsed/precomputation/%2/%3").arg(atmo.textureOutputDir.c_str())
                                    .arg(texIndex).arg(filename);
        std::cerr << indentOutput() << "Saving shader \"" << filePath << "\"...";
        QFile file(filePath);
        if(!file.open(QFile::WriteOnly))
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        file.write(src.toUtf8());
        file.flush();
        if(file.error())
        {
            std::cerr << " failed: " << file.errorString().toStdString() << "\"\n";
            throw MustQuit{};
        }
        std::cerr << "done\n";
    }
    return program;
}

void computeEclipsedDoubleScattering(const unsigned texIndex)
{
    const auto program=saveEclipsedDoubleScatteringComputationShader(texIndex);

    if(opts.dbgNoEDSTextures || opts.dbgNoSaveTextures) return;

    std::cerr << indentOutput() << "Computing eclipsed double scattering... ";
    const auto time0=std::chrono::steady_clock::now();

    using namespace glm;
    using std::acos;

    const unsigned texSizeByViewAzimuth = atmo.eclipsedDoubleScatteringTextureSize[0];
    const unsigned texSizeByViewElevation = atmo.eclipsedDoubleScatteringTextureSize[1];
    const unsigned texSizeBySZA = atmo.eclipsedDoubleScatteringTextureSize[2];
    const unsigned texSizeByAltitude = atmo.eclipsedDoubleScatteringTextureSize[3];

    gl.glBindFramebuffer(GL_FRAMEBUFFER, fbos[FBO_ECLIPSED_DOUBLE_SCATTERING]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,textures[TEX_ECLIPSED_DOUBLE_SCATTERING],0);
    checkFramebufferStatus("framebuffer for eclipsed double scattering");
    program->bind();
    int unusedTextureUnitNum=0;
    setUniformTexture(*program,GL_TEXTURE_2D,TEX_TRANSMITTANCE,unusedTextureUnitNum++,"transmittanceTexture");

    EclipsedDoubleScatteringPrecomputer precomputer(gl, atmo, texSizeByViewAzimuth, texSizeByViewElevation,
                                                    texSizeBySZA, texSizeByAltitude);

	gl.glBindVertexArray(vao);
    std::vector<glm::vec4> dataToSave;
    size_t numPointsPerSet=0;
    for(unsigned altIndex=0; altIndex<texSizeByAltitude; ++altIndex)
    {
        // Using the same encoding for altitude as in scatteringTex4DCoordsToTexVars()
        const float distToHorizon = float(altIndex)/(texSizeByAltitude-1)*atmo.lengthOfHorizRayFromGroundToBorderOfAtmo;
        // Rounding errors can result in altitude>max, breaking the code after this calculation, so we have to clamp.
        // To avoid too many zeros that would make log interpolation problematic, we clamp the bottom value at 1 m. The same at the top.
        const float cameraAltitude=clamp(sqrt(sqr(distToHorizon)+sqr(atmo.earthRadius))-atmo.earthRadius, 1.f, atmo.atmosphereHeight-1);

        for(unsigned szaIndex=0; szaIndex<texSizeBySZA; ++szaIndex)
        {
            std::ostringstream ss;
            ss << altIndex*texSizeBySZA+szaIndex << " of " << texSizeBySZA*texSizeByAltitude << " samples done ";
            std::cerr << ss.str();

            const double cosSunZenithAngle=unitRangeTexCoordToCosSZA(float(szaIndex)/(texSizeBySZA-1));
            const double sunZenithAngle=acos(cosSunZenithAngle);

            precomputer.computeRadianceOnCoarseGrid(*program, textures[TEX_ECLIPSED_DOUBLE_SCATTERING], unusedTextureUnitNum,
                                                    cameraAltitude, sunZenithAngle, sunZenithAngle, 0, atmo.earthMoonDistance);
            numPointsPerSet = precomputer.appendCoarseGridSamplesTo(dataToSave);

            // Clear previous status and reset cursor position
            const auto statusWidth=ss.tellp();
            std::cerr << std::string(statusWidth, '\b') << std::string(statusWidth, ' ')
                      << std::string(statusWidth, '\b');
        }
    }
	gl.glBindVertexArray(0);

    const auto time1=std::chrono::steady_clock::now();
    std::cerr << "done in " << formatDeltaTime(time0, time1) << "\n";

    if(!opts.saveResultAsRadiance)
    {
        std::cerr << indentOutput() << "Blending eclipsed double scattering texture into accumulator... ";
        const auto time0=std::chrono::steady_clock::now();
        const auto rad2lum = radianceToLuminance(texIndex, atmo.allWavelengths);
        if(texIndex == 0)
        {
            // Initialize the accumulator with the first layer...
            eclipsedDoubleScatteringAccumulatorTexture = std::move(dataToSave);
            // ... and apply the weight.
            for(auto& v : eclipsedDoubleScatteringAccumulatorTexture)
                v = rad2lum*v;
        }
        else
        {
            // Blend the new texture data into the accumulator.
            auto& accum = eclipsedDoubleScatteringAccumulatorTexture;
            const auto& src = dataToSave;
            for(size_t i = 0; i < accum.size(); ++i)
                accum[i] += rad2lum * src[i];
        }
        const auto time1=std::chrono::steady_clock::now();
        std::cerr << "done in " << formatDeltaTime(time0, time1) << "\n";
    }

    if(opts.saveResultAsRadiance || texIndex+1 == atmo.allWavelengths.size())
    {
        const auto path = atmo.textureOutputDir+"/eclipsed-double-scattering" +
                          (opts.saveResultAsRadiance ? "-wlset"+std::to_string(texIndex) : "-xyzw") +
                          ".f32";
        std::cerr << "Saving eclipsed double scattering texture to \"" << path << "\"... ";
        QFile out(QString::fromStdString(path));
        if(!out.open(QFile::WriteOnly))
        {
            std::cerr << "failed to open file: " << out.errorString().toStdString() << "\n";
            throw MustQuit{};
        }
        for(const uint16_t size : {numPointsPerSet})
            out.write(reinterpret_cast<const char*>(&size), sizeof size);
        auto& texture = opts.saveResultAsRadiance ? dataToSave : eclipsedDoubleScatteringAccumulatorTexture;
        if(opts.textureSavePrecision)
            roundTexData(&texture[0][0], 4*texture.size(), opts.textureSavePrecision);
        out.write(reinterpret_cast<const char*>(texture.data()), texture.size()*sizeof texture[0]);
        out.close();
        if(out.error())
        {
            std::cerr << "failed to write file: " << out.errorString().toStdString() << "\n";
            throw MustQuit{};
        }
        std::cerr << "done\n";
    }
}

void computeLightPollutionSingleScattering(const unsigned texIndex)
{
    std::cerr << indentOutput() << "Computing light pollution single scattering... ";

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_LIGHT_POLLUTION]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, textures[TEX_LIGHT_POLLUTION_SCATTERING],0);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1, textures[TEX_LIGHT_POLLUTION_DELTA_SCATTERING],0);
    checkFramebufferStatus("framebuffer for light pollution");
    setDrawBuffers({GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1});

    gl.glViewport(0, 0, atmo.lightPollutionTextureSize[0], atmo.lightPollutionTextureSize[1]);

    const auto src=makeScattererDensityFunctionsSrc();
    virtualSourceFiles[DENSITIES_SHADER_FILENAME]=src;
    const auto program=compileShaderProgram("compute-light-pollution-single-scattering.frag",
                                            "shader program to compute single scattering of light pollution");
    program->bind();
    setUniformTexture(*program,GL_TEXTURE_2D,TEX_TRANSMITTANCE,0,"transmittanceTexture");
    renderQuad();

    gl.glFinish();
    std::cerr << "done\n";

    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);

    if(!opts.dbgSaveLightPollutionIntermediateTextures) return;

    constexpr unsigned scatteringOrder=1;
    saveTexture(GL_TEXTURE_2D,textures[TEX_LIGHT_POLLUTION_DELTA_SCATTERING],"light pollution single scattering texture",
                atmo.textureOutputDir+"/light-pollution-delta-order"+std::to_string(scatteringOrder)+"-wlset"+std::to_string(texIndex)+".f32",
                {atmo.lightPollutionTextureSize[0], atmo.lightPollutionTextureSize[1]});
}

void computeLightPollutionMultipleScattering(const unsigned texIndex)
{
    std::cerr << indentOutput() << "Computing light pollution multiple scattering...\n";
    OutputIndentIncrease incr;

    const auto src=makeScattererDensityFunctionsSrc();
    virtualSourceFiles[DENSITIES_SHADER_FILENAME]=src;
    const auto program=compileShaderProgram("compute-light-pollution-multiple-scattering.frag",
                                            "shader program to compute higher-order scattering of light pollution");

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_LIGHT_POLLUTION]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, textures[TEX_LIGHT_POLLUTION_SCATTERING],0);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1, textures[TEX_LIGHT_POLLUTION_DELTA_SCATTERING],0);
    checkFramebufferStatus("framebuffer for light pollution");
    gl.glViewport(0, 0, atmo.lightPollutionTextureSize[0], atmo.lightPollutionTextureSize[1]);

    gl.glBlendFunc(GL_ONE, GL_ONE);
    for(unsigned scatteringOrder=2; scatteringOrder<=atmo.scatteringOrdersToCompute; ++scatteringOrder)
    {
        std::cerr << indentOutput() << "Computing light pollution scattering order " << scatteringOrder << "... ";
        {
            // Copy the delta scattering texture of the previous scattering order into a separate
            // texture, because the former will be overwritten with the new scattering order
            gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT2, textures[TEX_LIGHT_POLLUTION_SCATTERING_PREV_ORDER],0);
            gl.glReadBuffer(GL_COLOR_ATTACHMENT1);
            setDrawBuffers({GL_COLOR_ATTACHMENT2});
            const auto width=atmo.lightPollutionTextureSize[0], height=atmo.lightPollutionTextureSize[1];
            gl.glBlitFramebuffer(0,0,width,height, 0,0,width,height, GL_COLOR_BUFFER_BIT,GL_NEAREST);
            gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT2, 0,0);
        }
        setDrawBuffers({GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1});
        gl.glEnablei(GL_BLEND, 0);

        program->bind();
        setUniformTexture(*program,GL_TEXTURE_2D,TEX_TRANSMITTANCE,0,"transmittanceTexture");
        setUniformTexture(*program,GL_TEXTURE_2D,TEX_LIGHT_POLLUTION_SCATTERING_PREV_ORDER,1,"lightPollutionScatteringTexture");
        renderQuad();

        gl.glFinish();
        std::cerr << "done\n";

        if(!opts.dbgSaveLightPollutionIntermediateTextures)
            continue;

        saveTexture(GL_TEXTURE_2D,textures[TEX_LIGHT_POLLUTION_DELTA_SCATTERING],"light pollution delta multiple scattering texture",
                    atmo.textureOutputDir+"/light-pollution-delta-order"+std::to_string(scatteringOrder)+"-wlset"+std::to_string(texIndex)+".f32",
                    {atmo.lightPollutionTextureSize[0], atmo.lightPollutionTextureSize[1]});
    }
    gl.glDisablei(GL_BLEND, 0);

    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void accumulateLightPollutionLuminanceTexture(const unsigned texIndex)
{
    const auto tex = TEX_LIGHT_POLLUTION_SCATTERING_LUMINANCE;
    if(texIndex==0)
    {
        setupTexture(tex, atmo.lightPollutionTextureSize[0], atmo.lightPollutionTextureSize[1]);
    }
    else
    {
        gl.glBlendFunc(GL_ONE, GL_ONE);
        gl.glEnable(GL_BLEND);
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_LIGHT_POLLUTION]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, textures[tex],0);
    checkFramebufferStatus("framebuffer for accumulation of light pollution luminance");
    setDrawBuffers({GL_COLOR_ATTACHMENT0});

    const auto program=compileShaderProgram("copy-scattering-texture-2d.frag",
                                            "light pollution texture copy-blend shader program",
                                            UseGeomShader{});
    program->bind();
    setUniformTexture(*program,GL_TEXTURE_2D,TEX_LIGHT_POLLUTION_SCATTERING,0,"tex");
    program->setUniformValue("radianceToLuminance", toQMatrix(radianceToLuminance(texIndex, atmo.allWavelengths)));
    renderQuad();

    if(texIndex+1==atmo.allWavelengths.size())
    {
        saveTexture(GL_TEXTURE_2D,textures[TEX_LIGHT_POLLUTION_SCATTERING_LUMINANCE],"light pollution texture",
                    atmo.textureOutputDir+"/light-pollution-xyzw.f32",
                    {atmo.lightPollutionTextureSize[0], atmo.lightPollutionTextureSize[1]});
    }

    gl.glDisable(GL_BLEND);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

int main(int argc, char** argv)
{
    [[maybe_unused]] UTF8Console utf8console;

    qInstallMessageHandler(qtMessageHandler);
    QApplication app(argc, argv);
    app.setApplicationName("CalcMySky");
    app.setApplicationVersion(PROJECT_VERSION);

    try
    {
        handleCmdLine();

        std::cerr << qApp->applicationName() << ' ' << qApp->applicationVersion() << '\n';
        std::cerr << "Compiled against Qt " << QT_VERSION_MAJOR << "." << QT_VERSION_MINOR << "." << QT_VERSION_PATCH << "\n";
        std::cerr << "Running on " << QSysInfo::prettyProductName().toStdString() << " " << QSysInfo::currentCpuArchitecture() << "\n";

        [[maybe_unused]] const auto glCtxAndSfc = initOpenGL();

        if(opts.saveResultAsRadiance)
            for(auto& scatterer : atmo.scatterers)
                scatterer.phaseFunctionType=PhaseFunctionType::General;

        if(atmo.textureOutputDir.length() && atmo.textureOutputDir.back()=='/')
            atmo.textureOutputDir.pop_back(); // Make the paths a bit nicer (without double slashes)
        for(const auto& scatterer : atmo.scatterers)
        {
            for(unsigned texIndex=0; texIndex<atmo.allWavelengths.size(); ++texIndex)
            {
                createDirs(atmo.textureOutputDir+"/shaders/single-scattering-eclipsed/precomputation/"+
                           std::to_string(texIndex)+"/"+scatterer.name.toStdString());
                createDirs(atmo.textureOutputDir+"/shaders/single-scattering-eclipsed/"+singleScatteringRenderModeNames[SSRM_ON_THE_FLY]+"/"+
                           std::to_string(texIndex)+"/"+scatterer.name.toStdString());
                createDirs(atmo.textureOutputDir+"/shaders/single-scattering/"+singleScatteringRenderModeNames[SSRM_ON_THE_FLY]+"/"+
                           std::to_string(texIndex)+"/"+scatterer.name.toStdString());
                if(scatterer.phaseFunctionType==PhaseFunctionType::General)
                {
                    createDirs(atmo.textureOutputDir+"/shaders/single-scattering/"+singleScatteringRenderModeNames[SSRM_PRECOMPUTED]+"/"+
                               std::to_string(texIndex)+"/"+scatterer.name.toStdString());
                    createDirs(atmo.textureOutputDir+"/shaders/single-scattering-eclipsed/"+singleScatteringRenderModeNames[SSRM_PRECOMPUTED]+"/"+
                               std::to_string(texIndex)+"/"+scatterer.name.toStdString());
                }
            }
            if(scatterer.phaseFunctionType!=PhaseFunctionType::General)
            {
                createDirs(atmo.textureOutputDir+"/shaders/single-scattering/"+singleScatteringRenderModeNames[SSRM_PRECOMPUTED]+"/"+
                           scatterer.name.toStdString());
                createDirs(atmo.textureOutputDir+"/shaders/single-scattering-eclipsed/"+singleScatteringRenderModeNames[SSRM_PRECOMPUTED]+"/"+
                           scatterer.name.toStdString());
            }
        }
        createDirs(atmo.textureOutputDir+"/shaders/double-scattering-eclipsed/precomputed/");
        for(unsigned texIndex=0; texIndex<atmo.allWavelengths.size(); ++texIndex)
        {
            createDirs(atmo.textureOutputDir+"/shaders/zero-order-scattering/"+std::to_string(texIndex));
            createDirs(atmo.textureOutputDir+"/shaders/eclipsed-zero-order-scattering/"+std::to_string(texIndex));
            if(opts.saveResultAsRadiance)
                createDirs(atmo.textureOutputDir+"/shaders/double-scattering-eclipsed/precomputed/"+std::to_string(texIndex));
            createDirs(atmo.textureOutputDir+"/shaders/double-scattering-eclipsed/precomputation/"+std::to_string(texIndex));
            createDirs(atmo.textureOutputDir+"/single-scattering/"+std::to_string(texIndex));
        }
        createDirs(atmo.textureOutputDir+"/shaders/multiple-scattering/");
        if(opts.saveResultAsRadiance)
            for(unsigned texIndex=0; texIndex<atmo.allWavelengths.size(); ++texIndex)
                createDirs(atmo.textureOutputDir+"/shaders/multiple-scattering/"+std::to_string(texIndex));
        createDirs(atmo.textureOutputDir+"/shaders/light-pollution/");
        if(opts.saveResultAsRadiance)
            for(unsigned texIndex=0; texIndex<atmo.allWavelengths.size(); ++texIndex)
                createDirs(atmo.textureOutputDir+"/shaders/light-pollution/"+std::to_string(texIndex));

        {
            std::cerr << "Writing parameters to output description file...";
            const auto target=atmo.textureOutputDir+"/params.atmo";
            QFile file(target.c_str());
            if(!file.open(QFile::WriteOnly))
            {
                std::cerr << " FAILED to open \"" << target << "\": " << file.errorString() << "\n";
                throw MustQuit{};
            }
            QTextStream out(&file);
            out << "version: " << AtmosphereParameters::FORMAT_VERSION << "\n";
            if(opts.saveResultAsRadiance)
                out << AtmosphereParameters::ALL_TEXTURES_ARE_RADIANCES_DIRECTIVE << "\n";
            if(opts.dbgNoEDSTextures)
                out << AtmosphereParameters::NO_ECLIPSED_DOUBLE_SCATTERING_TEXTURES_DIRECTIVE << "\n";
            out << "# These spectra override the spectra further down the document. This is to make sure\n# we have all the required spectra inlined, rather than just references to files.\n";
            out << AtmosphereParameters::WAVELENGTHS_KEY << ": min=" << atmo.allWavelengths.front().x
                << "nm,max=" << atmo.allWavelengths.back().w << "nm,count=" << 4*atmo.allWavelengths.size() << "\n";
            out << AtmosphereParameters::SOLAR_IRRADIANCE_AT_TOA_KEY << ": "
                << AtmosphereParameters::spectrumToString(atmo.solarIrradianceAtTOA) << "\n";
            out << "\n#Copy of original atmosphere description\n" << atmo.descriptionFileText;
            out.flush();
            file.close();
            if(file.error())
            {
                std::cerr << " FAILED to write to \"" << target << "\": " << file.errorString() << "\n";
                throw MustQuit{};
            }
            std::cerr << " done\n";
        }

        const auto timeBegin=std::chrono::steady_clock::now();

        // Initialize texture averager before anything to make it emit possible
        // warnings not mixing them into computation status reports.
        TextureAverageComputer{gl, 10, 10, GL_RGBA32F, 0};

        for(unsigned texIndex=0;texIndex<atmo.allWavelengths.size();++texIndex)
        {
            std::cerr << "Working on wavelengths " << atmo.allWavelengths[texIndex][0] << ", "
                                                   << atmo.allWavelengths[texIndex][1] << ", "
                                                   << atmo.allWavelengths[texIndex][2] << ", "
                                                   << atmo.allWavelengths[texIndex][3] << " nm"
                         " (set " << texIndex+1 << " of " << atmo.allWavelengths.size() << "):\n";
            OutputIndentIncrease incr;

            initConstHeader(atmo.allWavelengths[texIndex]);
            virtualSourceFiles[COMPUTE_TRANSMITTANCE_SHADER_FILENAME]=
                makeTransmittanceComputeFunctionsSrc(atmo.allWavelengths[texIndex]);
            virtualSourceFiles[PHASE_FUNCTIONS_SHADER_FILENAME]=makePhaseFunctionsSrc();
            virtualSourceFiles[TOTAL_SCATTERING_COEFFICIENT_SHADER_FILENAME]=makeTotalScatteringCoefSrc();
            virtualHeaderFiles[RADIANCE_TO_LUMINANCE_HEADER_FILENAME]="const mat4 radianceToLuminance=" +
                                                  toString(radianceToLuminance(texIndex, atmo.allWavelengths)) + ";\n";

            saveZeroOrderScatteringRenderingShader(texIndex);
            saveEclipsedZeroOrderScatteringRenderingShader(texIndex);

            {
                std::cerr << indentOutput() << "Computing parts of scattering order 1:\n";
                OutputIndentIncrease incr;

                computeTransmittance(texIndex);
                // We'll use ground irradiance to take into account the contribution of light scattered by the ground to the
                // sky color. Irradiance will also be needed when we want to draw the ground itself.
                computeDirectGroundIrradiance(texIndex);
            }

            computeLightPollutionSingleScattering(texIndex);
            computeLightPollutionMultipleScattering(texIndex);
            if(opts.saveResultAsRadiance)
            {
                saveTexture(GL_TEXTURE_2D,textures[TEX_LIGHT_POLLUTION_SCATTERING],"light pollution texture",
                            atmo.textureOutputDir+"/light-pollution-wlset"+std::to_string(texIndex)+".f32",
                            {atmo.lightPollutionTextureSize[0], atmo.lightPollutionTextureSize[1]});
            }
            else
            {
                accumulateLightPollutionLuminanceTexture(texIndex);
            }
            saveLightPollutionRenderingShader(texIndex);

            computeMultipleScattering(texIndex);
            if(opts.saveResultAsRadiance)
            {
                saveMultipleScatteringRenderingShader(texIndex);
                saveEclipsedDoubleScatteringRenderingShader(texIndex);
            }

            computeEclipsedDoubleScattering(texIndex);

        }
        if(!opts.saveResultAsRadiance)
        {
            saveMultipleScatteringRenderingShader(-1);
            saveEclipsedDoubleScatteringRenderingShader(-1);
        }

        const auto timeEnd=std::chrono::steady_clock::now();
        std::cerr << "Finished in " << formatDeltaTime(timeBegin, timeEnd) << "\n";
    }
    catch(ParsingError const& ex)
    {
        std::cerr << ex.what() << "\n";
        return 1;
    }
    catch(ShowMySky::Error const& ex)
    {
        std::cerr << QObject::tr("Error: %1\n").arg(ex.what());
        return 1;
    }
    catch(MustQuit& ex)
    {
        return ex.exitCode;
    }
    catch(std::exception const& ex)
    {
        std::cerr << "Fatal error: " << QString::fromLocal8Bit(ex.what()) << '\n';
        return 111;
    }
}
