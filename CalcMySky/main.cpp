#include <iostream>
#include <sstream>
#include <memory>
#include <random>
#include <chrono>
#include <map>
#include <set>

#include <QOffscreenSurface>
#include <QSurfaceFormat>
#include <QApplication>
#include <QRegExp>
#include <QImage>
#include <QFile>

#include "config.h"
#include "data.hpp"
#include "util.hpp"
#include "glinit.hpp"
#include "cmdline.hpp"
#include "shaders.hpp"
#include "../cie-xyzw-functions.hpp"

QOpenGLFunctions_3_3_Core gl;

glm::mat4 radianceToLuminance(const unsigned texIndex)
{
    using glm::mat4;
    const auto diag=[](GLfloat x, GLfloat y, GLfloat z, GLfloat w) { return mat4(x,0,0,0,
                                                                                 0,y,0,0,
                                                                                 0,0,z,0,
                                                                                 0,0,0,w); };
    const auto wlCount = 4*allWavelengths.size();
    // Weights for the trapezoidal quadrature rule
    const mat4 weights = wlCount==4            ? diag(0.5,1,1,0.5) :
                         texIndex==0           ? diag(0.5,1,1,1  ) :
                         texIndex+1==wlCount/4 ? diag(  1,1,1,0.5) :
                                                 diag(  1,1,1,1);
    const mat4 dlambda = weights * abs(allWavelengths.back()[3]-allWavelengths.front()[0]) / (wlCount-1.f);
    // Ref: Rapport BIPM-2019/05. Principles Governing Photometry, 2nd edition. Sections 6.2, 6.3.
    const mat4 maxLuminousEfficacy=diag(683.002f,683.002f,683.002f,1700.13f); // lm/W
    return maxLuminousEfficacy * mat4(wavelengthToXYZW(allWavelengths[texIndex][0]),
                                      wavelengthToXYZW(allWavelengths[texIndex][1]),
                                      wavelengthToXYZW(allWavelengths[texIndex][2]),
                                      wavelengthToXYZW(allWavelengths[texIndex][3])) * dlambda;
}

void saveIrradiance(const unsigned scatteringOrder, const unsigned texIndex)
{
    if(scatteringOrder==scatteringOrdersToCompute)
    {
        saveTexture(GL_TEXTURE_2D,textures[TEX_IRRADIANCE],"irradiance texture",
                    textureOutputDir+"/irradiance-wlset"+std::to_string(texIndex)+".f32",
                    {irradianceTexW, irradianceTexH});
    }

    if(!dbgSaveGroundIrradiance) return;

    saveTexture(GL_TEXTURE_2D,textures[TEX_DELTA_IRRADIANCE],"irradiance texture",
                textureOutputDir+"/irradiance-delta-order"+std::to_string(scatteringOrder-1)+"-wlset"+std::to_string(texIndex)+".f32",
                {irradianceTexW, irradianceTexH});

    saveTexture(GL_TEXTURE_2D,textures[TEX_IRRADIANCE],"irradiance texture",
                textureOutputDir+"/irradiance-accum-order"+std::to_string(scatteringOrder-1)+"-wlset"+std::to_string(texIndex)+".f32",
                {irradianceTexW, irradianceTexH});
}

void saveScatteringDensity(const unsigned scatteringOrder, const unsigned texIndex)
{
    if(!dbgSaveScatDensity) return;
    saveTexture(GL_TEXTURE_3D,textures[TEX_DELTA_SCATTERING_DENSITY],
                "order "+std::to_string(scatteringOrder)+" scattering density",
                textureOutputDir+"/scattering-density"+std::to_string(scatteringOrder)+"-wlset"+std::to_string(texIndex)+".f32",
                {scatteringTextureSize[0], scatteringTextureSize[1], scatteringTextureSize[2], scatteringTextureSize[3]});
}

void render3DTexLayers(QOpenGLShaderProgram& program, const std::string_view whatIsBeingDone)
{
    if(dbgNoSaveTextures) return; // don't take time to do useless computations

    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        std::cerr << "FAILED on entry to render3DTexLayers(): " << openglErrorString(err) << "\n";
        throw MustQuit{};
    }

    std::cerr << indentOutput() << whatIsBeingDone << "... ";
    for(GLsizei layer=0; layer<scatTexDepth(); ++layer)
    {
        std::ostringstream ss;
        ss << layer << " of " << scatTexDepth() << " layers done";
        std::cerr << ss.str();

        program.setUniformValue("layer",layer);
        renderQuad();
        gl.glFinish();

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
    gl.glViewport(0, 0, transmittanceTexW, transmittanceTexH);
    renderQuad();

    gl.glFinish();
    std::cerr << "done\n";

    saveTexture(GL_TEXTURE_2D,textures[TEX_TRANSMITTANCE],"transmittance texture",
                textureOutputDir+"/transmittance-wlset"+std::to_string(texIndex)+".f32",
                {transmittanceTexW, transmittanceTexH});

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

    gl.glViewport(0, 0, irradianceTexW, irradianceTexH);
    renderQuad();

    gl.glFinish();
    std::cerr << "done\n";

    saveIrradiance(1,texIndex);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

static constexpr char renderShaderFileName[]="render.frag";
constexpr char viewDirFuncFileName[]="calc-view-dir.frag";
constexpr char viewDirStubFunc[]="vec3 calcViewDir() { return vec3(0); }";
void saveZeroOrderScatteringRenderingShader(const unsigned texIndex)
{
    std::vector<std::pair<QString, QString>> sourcesToSave;
    virtualSourceFiles[viewDirFuncFileName]=viewDirStubFunc;
    virtualSourceFiles[renderShaderFileName]=getShaderSrc(renderShaderFileName,IgnoreCache{})
            .replace(QRegExp("\\b(RENDERING_ZERO_SCATTERING)\\b"), "1 // \\1")
            .replace(QRegExp("#include \"(phase-functions|single-scattering|single-scattering-eclipsed)\\.h\\.glsl\""), "");
    const auto program=compileShaderProgram(renderShaderFileName,
                                            "zero-order scattering rendering shader program",
                                            UseGeomShader{false}, &sourcesToSave);
    for(const auto& [filename, src] : sourcesToSave)
    {
        if(filename==viewDirFuncFileName) continue;

        const auto filePath=QString("%1/shaders/zero-order-scattering/%2/%3")
                                .arg(textureOutputDir.c_str()).arg(texIndex).arg(filename);
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

void saveMultipleScatteringRenderingShader()
{
    std::vector<std::pair<QString, QString>> sourcesToSave;
    virtualSourceFiles[viewDirFuncFileName]=viewDirStubFunc;
    virtualSourceFiles[renderShaderFileName]=getShaderSrc(renderShaderFileName,IgnoreCache{})
            .replace(QRegExp("\\b(RENDERING_MULTIPLE_SCATTERING)\\b"), "1 // \\1")
            .replace(QRegExp("#include \"(phase-functions|common-functions|texture-sampling-functions|single-scattering|single-scattering-eclipsed|radiance-to-luminance)\\.h\\.glsl\""), "");
    const auto program=compileShaderProgram(renderShaderFileName,
                                            "multiple scattering rendering shader program",
                                            UseGeomShader{false}, &sourcesToSave);
    for(const auto& [filename, src] : sourcesToSave)
    {
        if(filename==viewDirFuncFileName) continue;

        const auto filePath=QString("%1/shaders/multiple-scattering/%2").arg(textureOutputDir.c_str()).arg(filename);
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

void saveSingleScatteringRenderingShader(const unsigned texIndex, ScattererDescription const& scatterer, const SingleScatteringRenderMode renderMode)
{
    virtualSourceFiles[PHASE_FUNCTIONS_SHADER_FILENAME]=makePhaseFunctionsSrc()+
        "vec4 currentPhaseFunction(float dotViewSun) { return phaseFunction_"+scatterer.name+"(dotViewSun); }\n";

    if(scatterer.phaseFunctionType==PhaseFunctionType::Smooth)
        return; // Luminance will be already merged in multiple scattering texture, no need to render it separately

    std::vector<std::pair<QString, QString>> sourcesToSave;
    virtualSourceFiles[viewDirFuncFileName]=viewDirStubFunc;
    const auto renderModeDefine = renderMode==SSRM_ON_THE_FLY ? "RENDERING_SINGLE_SCATTERING_ON_THE_FLY" :
                                  scatterer.phaseFunctionType==PhaseFunctionType::General ? "RENDERING_SINGLE_SCATTERING_PRECOMPUTED_RADIANCE"
                                                                                          : "RENDERING_SINGLE_SCATTERING_PRECOMPUTED_LUMINANCE";
    virtualSourceFiles[renderShaderFileName]=getShaderSrc(renderShaderFileName,IgnoreCache{})
                                                .replace(QRegExp(QString("\\b(%1)\\b").arg(renderModeDefine)), "1 // \\1")
                                                .replace(QRegExp("#include \"(common-functions|texture-sampling-functions|single-scattering-eclipsed)\\.h\\.glsl\""), "");
    const auto program=compileShaderProgram(renderShaderFileName,
                                            "single scattering rendering shader program",
                                            UseGeomShader{false}, &sourcesToSave);
    for(const auto& [filename, src] : sourcesToSave)
    {
        if(filename==viewDirFuncFileName) continue;

        const auto filePath = scatterer.phaseFunctionType==PhaseFunctionType::General || renderMode==SSRM_ON_THE_FLY ?
           QString("%1/shaders/single-scattering/%2/%3/%4/%5").arg(textureOutputDir.c_str()).arg(toString(renderMode)).arg(texIndex).arg(scatterer.name).arg(filename) :
           QString("%1/shaders/single-scattering/%2/%3/%4").arg(textureOutputDir.c_str()).arg(toString(renderMode)).arg(scatterer.name).arg(filename);
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

void saveEclipsedSingleScatteringRenderingShader(const unsigned texIndex, ScattererDescription const& scatterer, const SingleScatteringRenderMode renderMode)
{
    virtualSourceFiles[PHASE_FUNCTIONS_SHADER_FILENAME]=makePhaseFunctionsSrc()+
        "vec4 currentPhaseFunction(float dotViewSun) { return phaseFunction_"+scatterer.name+"(dotViewSun); }\n";

    if(scatterer.phaseFunctionType==PhaseFunctionType::Smooth)
        return; // Luminance will be already merged in multiple scattering texture, no need to render it separately

    std::vector<std::pair<QString, QString>> sourcesToSave;
    static constexpr char renderShaderFileName[]="render.frag";
    const auto renderModeDefine = renderMode==SSRM_ON_THE_FLY ? "RENDERING_ECLIPSED_SINGLE_SCATTERING_ON_THE_FLY" : "WHAAA?!";
    virtualSourceFiles[renderShaderFileName]=getShaderSrc(renderShaderFileName,IgnoreCache{})
                                                .replace(QRegExp(QString("\\b(%1)\\b").arg(renderModeDefine)), "1 // \\1")
                                                .replace(QRegExp("#include \"(common-functions|texture-sampling-functions|single-scattering)\\.h\\.glsl\""), "");
    const auto program=compileShaderProgram(renderShaderFileName,
                                            "single scattering rendering shader program",
                                            UseGeomShader{false}, &sourcesToSave);
    for(const auto& [filename, src] : sourcesToSave)
    {
        if(filename==viewDirFuncFileName) continue;

        const auto filePath = QString("%1/shaders/single-scattering-eclipsed/%2/%3/%4/%5").arg(textureOutputDir.c_str())
                                                                                          .arg(toString(renderMode))
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

void accumulateSingleScattering(const unsigned texIndex, ScattererDescription const& scatterer)
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
        setupTexture(targetTexture, scatTexWidth(),scatTexHeight(),scatTexDepth());
        gl.glDisable(GL_BLEND);
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_SINGLE_SCATTERING]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, targetTexture,0);
    checkFramebufferStatus("framebuffer for accumulation of single scattering radiance");

    const auto program=compileShaderProgram("copy-scattering-texture.frag",
                                            "scattering texture copy-blend shader program",
                                            UseGeomShader{});
    program->bind();
    setUniformTexture(*program,GL_TEXTURE_3D,TEX_DELTA_SCATTERING,0,"tex");
    program->setUniformValue("radianceToLuminance", toQMatrix(radianceToLuminance(texIndex)));
    render3DTexLayers(*program, "Blending single scattering layers into accumulator texture");

    gl.glDisable(GL_BLEND);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);

    if(texIndex+1==allWavelengths.size() && scatterer.phaseFunctionType!=PhaseFunctionType::Smooth)
    {
        saveTexture(GL_TEXTURE_3D,targetTexture, "single scattering texture",
                    textureOutputDir+"/single-scattering/"+scatterer.name.toStdString()+"-xyzw.f32",
                    {scatteringTextureSize[0], scatteringTextureSize[1], scatteringTextureSize[2], scatteringTextureSize[3]});
    }
}

void computeSingleScattering(const unsigned texIndex, ScattererDescription const& scatterer)
{
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_DELTA_SCATTERING]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, textures[TEX_DELTA_SCATTERING],0);
    checkFramebufferStatus("framebuffer for first scattering");

    gl.glViewport(0, 0, scatTexWidth(), scatTexHeight());

    const auto src=makeScattererDensityFunctionsSrc()+
                    "float scattererDensity(float alt) { return scattererNumberDensity_"+scatterer.name+"(alt); }\n"+
                    "vec4 scatteringCrossSection() { return "+toString(scatterer.crossSection(allWavelengths[texIndex]))+"; }\n";
    virtualSourceFiles[DENSITIES_SHADER_FILENAME]=src;
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
        saveTexture(GL_TEXTURE_3D,textures[TEX_DELTA_SCATTERING], "single scattering texture",
                    textureOutputDir+"/single-scattering/"+std::to_string(texIndex)+"/"+scatterer.name.toStdString()+".f32",
                    {scatteringTextureSize[0], scatteringTextureSize[1], scatteringTextureSize[2], scatteringTextureSize[3]});
        break;
    case PhaseFunctionType::Achromatic:
    case PhaseFunctionType::Smooth:
        accumulateSingleScattering(texIndex, scatterer);
        break;
    }

    saveSingleScatteringRenderingShader(texIndex, scatterer, SSRM_ON_THE_FLY);
    saveSingleScatteringRenderingShader(texIndex, scatterer, SSRM_PRECOMPUTED);
    saveEclipsedSingleScatteringRenderingShader(texIndex, scatterer, SSRM_ON_THE_FLY);
}

void computeIndirectIrradianceOrder1(unsigned texIndex, unsigned scattererIndex);
void computeScatteringDensityOrder2(const unsigned texIndex)
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
                                                    .replace(QRegExp("\\bRADIATION_IS_FROM_GROUND_ONLY\\b"), "true")
                                                    .replace(QRegExp("\\bSCATTERING_ORDER\\b"), QString::number(scatteringOrder));
        // recompile the program
        program=compileShaderProgram(COMPUTE_SCATTERING_DENSITY_FILENAME,
                                     "scattering density computation shader program", UseGeomShader{});
    }

    gl.glViewport(0, 0, scatTexWidth(), scatTexHeight());

    program->bind();

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_MULTIPLE_SCATTERING]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,textures[TEX_DELTA_SCATTERING_DENSITY],0);
    checkFramebufferStatus("framebuffer for scattering density");

    setUniformTexture(*program,GL_TEXTURE_2D,TEX_TRANSMITTANCE   ,0,"transmittanceTexture");
    setUniformTexture(*program,GL_TEXTURE_2D,TEX_DELTA_IRRADIANCE,1,"irradianceTexture");

    render3DTexLayers(*program, "Computing scattering density layers for radiation from the ground");

    if(dbgSaveScatDensityOrder2FromGround)
    {
        saveTexture(GL_TEXTURE_3D,textures[TEX_DELTA_SCATTERING_DENSITY],
                    "order 2 scattering density from ground texture",
                    textureOutputDir+"/scattering-density2-from-ground-wlset"+std::to_string(texIndex)+".f32",
                    {scatteringTextureSize[0], scatteringTextureSize[1], scatteringTextureSize[2], scatteringTextureSize[3]});
    }

    gl.glBlendFunc(GL_ONE, GL_ONE);
    for(unsigned scattererIndex=0; scattererIndex<scatterers.size(); ++scattererIndex)
    {
        const auto& scatterer=scatterers[scattererIndex];
        std::cerr << indentOutput() << "Processing scatterer \""+scatterer.name.toStdString()+"\":\n";
        OutputIndentIncrease incr;

        // Current phase function is updated in the single scattering computation while saving the rendering shader
        computeSingleScattering(texIndex, scatterer);

        gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_MULTIPLE_SCATTERING]);

        {
            virtualSourceFiles[COMPUTE_SCATTERING_DENSITY_FILENAME]=getShaderSrc(COMPUTE_SCATTERING_DENSITY_FILENAME,IgnoreCache{})
                                                    .replace(QRegExp("\\bRADIATION_IS_FROM_GROUND_ONLY\\b"), "false")
                                                    .replace(QRegExp("\\bSCATTERING_ORDER\\b"), QString::number(scatteringOrder));
            // recompile the program
            program=compileShaderProgram(COMPUTE_SCATTERING_DENSITY_FILENAME,
                                                        "scattering density computation shader program", UseGeomShader{});
        }
        program->bind();

        setUniformTexture(*program,GL_TEXTURE_3D,TEX_DELTA_SCATTERING,1,"firstScatteringTexture");

        gl.glEnable(GL_BLEND);
        render3DTexLayers(*program, "Computing scattering density layers");

        computeIndirectIrradianceOrder1(texIndex, scattererIndex);
    }
    gl.glDisable(GL_BLEND);
    saveScatteringDensity(scatteringOrder,texIndex);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void computeScatteringDensity(const unsigned scatteringOrder, const unsigned texIndex)
{
    assert(scatteringOrder>2);

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_MULTIPLE_SCATTERING]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,textures[TEX_DELTA_SCATTERING_DENSITY],0);

    virtualSourceFiles[COMPUTE_SCATTERING_DENSITY_FILENAME]=getShaderSrc(COMPUTE_SCATTERING_DENSITY_FILENAME,IgnoreCache{})
                                                    .replace(QRegExp("\\bRADIATION_IS_FROM_GROUND_ONLY\\b"), "false")
                                                    .replace(QRegExp("\\bSCATTERING_ORDER\\b"), QString::number(scatteringOrder));
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

void computeIndirectIrradianceOrder1(const unsigned texIndex, const unsigned scattererIndex)
{
    constexpr unsigned scatteringOrder=2;

    gl.glViewport(0, 0, irradianceTexW, irradianceTexH);

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_IRRADIANCE]);
    gl.glBlendFunc(GL_ONE, GL_ONE);
    if(scattererIndex==0)
        gl.glDisablei(GL_BLEND, 0); // First scatterer overwrites delta-irradiance-texture
    else
        gl.glEnablei(GL_BLEND, 0); // Second and subsequent scatterers blend into delta-irradiance-texture
    gl.glEnablei(GL_BLEND, 1); // Total irradiance is always accumulated

    const auto& scatterer=scatterers[scattererIndex];

    virtualSourceFiles[PHASE_FUNCTIONS_SHADER_FILENAME]=makePhaseFunctionsSrc()+
        "vec4 currentPhaseFunction(float dotViewSun) { return phaseFunction_"+scatterer.name+"(dotViewSun); }\n";

    virtualSourceFiles[COMPUTE_INDIRECT_IRRADIANCE_FILENAME]=getShaderSrc(COMPUTE_INDIRECT_IRRADIANCE_FILENAME,IgnoreCache{})
                                                .replace(QRegExp("\\bSCATTERING_ORDER\\b"), QString::number(scatteringOrder-1));
    std::unique_ptr<QOpenGLShaderProgram> program=compileShaderProgram(COMPUTE_INDIRECT_IRRADIANCE_FILENAME,
                                                                       "indirect irradiance computation shader program");
    program->bind();
    setUniformTexture(*program,GL_TEXTURE_3D,TEX_DELTA_SCATTERING,0,"firstScatteringTexture");

    std::cerr << indentOutput() << "Computing indirect irradiance... ";
    renderQuad();
    gl.glFinish();
    std::cerr << "done\n";

    gl.glDisable(GL_BLEND);
    saveIrradiance(scatteringOrder,texIndex);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void computeIndirectIrradiance(const unsigned scatteringOrder, const unsigned texIndex)
{
    assert(scatteringOrder>2);
    gl.glViewport(0, 0, irradianceTexW, irradianceTexH);

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_IRRADIANCE]);
    gl.glBlendFunc(GL_ONE, GL_ONE);
    gl.glDisablei(GL_BLEND, 0); // Overwrite delta-irradiance-texture
    gl.glEnablei(GL_BLEND, 1); // Accumulate total irradiance

    virtualSourceFiles[COMPUTE_INDIRECT_IRRADIANCE_FILENAME]=getShaderSrc(COMPUTE_INDIRECT_IRRADIANCE_FILENAME,IgnoreCache{})
                                                .replace(QRegExp("\\bSCATTERING_ORDER\\b"), QString::number(scatteringOrder-1));
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

void mergeSmoothSingleScatteringTexture()
{
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_MULTIPLE_SCATTERING]);
    for(const auto& scatterer : scatterers)
    {
        if(scatterer.phaseFunctionType!=PhaseFunctionType::Smooth)
            continue;
        virtualSourceFiles[PHASE_FUNCTIONS_SHADER_FILENAME]=makePhaseFunctionsSrc()+
            "vec4 currentPhaseFunction(float dotViewSun) { return phaseFunction_"+scatterer.name+"(dotViewSun); }\n";
        const auto program=compileShaderProgram("merge-smooth-single-scattering-texture.frag",
                                                "single scattering texture merge shader program",
                                                UseGeomShader{});
        program->bind();
        gl.glBlendFunc(GL_ONE, GL_ONE);
        gl.glEnable(GL_BLEND);
        setUniformTexture(*program,GL_TEXTURE_3D,accumulatedSingleScatteringTextures[scatterer.name],0,"tex");
        render3DTexLayers(*program, "Blending single scattering data for scatterer \""+scatterer.name.toStdString()+
                                    "\" into multiple scattering texture");
    }
    gl.glDisable(GL_BLEND);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void accumulateMultipleScattering(const unsigned scatteringOrder, const unsigned texIndex)
{
    // We didn't render to the accumulating texture when computing delta scattering to avoid holding
    // more than two 4D textures in VRAM at once.
    // Now it's time to do this by only holding the accumulator and delta scattering texture in VRAM.
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBlendFunc(GL_ONE, GL_ONE);
    if(scatteringOrder>2 || (texIndex>0 && !saveResultAsRadiance))
        gl.glEnable(GL_BLEND);
    else
        gl.glDisable(GL_BLEND);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_MULTIPLE_SCATTERING]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, textures[TEX_MULTIPLE_SCATTERING],0);
    checkFramebufferStatus("framebuffer for accumulation of multiple scattering data");

    const auto program=compileShaderProgram("copy-scattering-texture.frag",
                                            "scattering texture copy-blend shader program",
                                            UseGeomShader{});
    program->bind();
    if(!saveResultAsRadiance)
        program->setUniformValue("radianceToLuminance", toQMatrix(radianceToLuminance(texIndex)));
    setUniformTexture(*program,GL_TEXTURE_3D,TEX_DELTA_SCATTERING,0,"tex");
    render3DTexLayers(*program, "Blending multiple scattering layers into accumulator texture");
    gl.glDisable(GL_BLEND);

    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);

    if(dbgSaveAccumScattering)
    {
        saveTexture(GL_TEXTURE_3D,textures[TEX_MULTIPLE_SCATTERING],
                    "multiple scattering accumulator texture",
                    textureOutputDir+"/multiple-scattering-to-order"+std::to_string(scatteringOrder)+"-wlset"+std::to_string(texIndex)+".f32",
                    {scatteringTextureSize[0], scatteringTextureSize[1], scatteringTextureSize[2], scatteringTextureSize[3]});
    }
    if(scatteringOrder==scatteringOrdersToCompute && (texIndex+1==allWavelengths.size() || saveResultAsRadiance))
    {
        mergeSmoothSingleScatteringTexture();

        const auto filename = saveResultAsRadiance ?
            textureOutputDir+"/multiple-scattering-wlset"+std::to_string(texIndex)+".f32" :
            textureOutputDir+"/multiple-scattering-xyzw.f32";
        saveTexture(GL_TEXTURE_3D,textures[TEX_MULTIPLE_SCATTERING],
                    "multiple scattering accumulator texture", filename,
                    {scatteringTextureSize[0], scatteringTextureSize[1], scatteringTextureSize[2], scatteringTextureSize[3]});
    }
}

void computeMultipleScatteringFromDensity(const unsigned scatteringOrder, const unsigned texIndex)
{
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_MULTIPLE_SCATTERING]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, textures[TEX_DELTA_SCATTERING],0);
    checkFramebufferStatus("framebuffer for delta multiple scattering");

    gl.glViewport(0, 0, scatTexWidth(), scatTexHeight());

    {
        const auto program=compileShaderProgram("compute-multiple-scattering.frag",
                                                "multiple scattering computation shader program",
                                                UseGeomShader{});
        program->bind();

        setUniformTexture(*program,GL_TEXTURE_2D,TEX_TRANSMITTANCE,0,"transmittanceTexture");
        setUniformTexture(*program,GL_TEXTURE_3D,TEX_DELTA_SCATTERING_DENSITY,1,"scatteringDensityTexture");

        render3DTexLayers(*program, "Computing multiple scattering layers");

        if(dbgSaveDeltaScattering)
        {
            saveTexture(GL_TEXTURE_3D,textures[TEX_DELTA_SCATTERING],
                        "delta scattering texture",
                        textureOutputDir+"/delta-scattering-order"+std::to_string(scatteringOrder)+"-wlset"+std::to_string(texIndex)+".f32",
                        {scatteringTextureSize[0], scatteringTextureSize[1], scatteringTextureSize[2], scatteringTextureSize[3]});
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

        computeScatteringDensityOrder2(texIndex);
        computeMultipleScatteringFromDensity(2,texIndex);
    }
    for(unsigned scatteringOrder=3; scatteringOrder<=scatteringOrdersToCompute; ++scatteringOrder)
    {
        std::cerr << indentOutput() << "Working on scattering order " << scatteringOrder << ":\n";
        OutputIndentIncrease incr;

        computeScatteringDensity(scatteringOrder,texIndex);
        computeIndirectIrradiance(scatteringOrder,texIndex);
        computeMultipleScatteringFromDensity(scatteringOrder,texIndex);
    }
}

int main(int argc, char** argv)
{
    [[maybe_unused]] UTF8Console utf8console;

    qInstallMessageHandler(qtMessageHandler);
    QApplication app(argc, argv);
    app.setApplicationName("CalcMySky");
    app.setApplicationVersion(APP_VERSION);

    try
    {
        handleCmdLine();

        if(saveResultAsRadiance)
            for(auto& scatterer : scatterers)
                scatterer.phaseFunctionType=PhaseFunctionType::General;

        if(textureOutputDir.length() && textureOutputDir.back()=='/')
            textureOutputDir.pop_back(); // Make the paths a bit nicer (without double slashes)
        for(const auto& scatterer : scatterers)
        {
            for(unsigned texIndex=0; texIndex<allWavelengths.size(); ++texIndex)
            {
                if(scatterer.phaseFunctionType!=PhaseFunctionType::Smooth)
                {
                    createDirs(textureOutputDir+"/shaders/single-scattering/"+singleScatteringRenderModeNames[SSRM_ON_THE_FLY]+"/"+
                               std::to_string(texIndex)+"/"+scatterer.name.toStdString());
                    createDirs(textureOutputDir+"/shaders/single-scattering-eclipsed/"+singleScatteringRenderModeNames[SSRM_ON_THE_FLY]+"/"+
                               std::to_string(texIndex)+"/"+scatterer.name.toStdString());
                }
                if(scatterer.phaseFunctionType==PhaseFunctionType::General)
                {
                    createDirs(textureOutputDir+"/shaders/single-scattering/"+singleScatteringRenderModeNames[SSRM_PRECOMPUTED]+"/"+
                               std::to_string(texIndex)+"/"+scatterer.name.toStdString());
                }
            }
            if(scatterer.phaseFunctionType==PhaseFunctionType::Achromatic)
            {
                createDirs(textureOutputDir+"/shaders/single-scattering/"+singleScatteringRenderModeNames[SSRM_PRECOMPUTED]+"/"+
                           scatterer.name.toStdString());
            }
        }
        for(unsigned texIndex=0; texIndex<allWavelengths.size(); ++texIndex)
        {
            createDirs(textureOutputDir+"/shaders/zero-order-scattering/"+std::to_string(texIndex));
            createDirs(textureOutputDir+"/single-scattering/"+std::to_string(texIndex));
        }
        createDirs(textureOutputDir+"/shaders/multiple-scattering/");

        {
            std::cerr << "Writing parameters to output description file...";
            QFile file((textureOutputDir+"/params.txt").c_str());
            if(!file.open(QFile::WriteOnly))
            {
                std::cerr << " FAILED to open file: " << file.errorString().toStdString() << "\n";
                throw MustQuit{};
            }
            QTextStream out(&file);
            out << "wavelengths: ";
            for(unsigned i=0; i<allWavelengths.size(); ++i)
            {
                const auto& wlset=allWavelengths[i];
                out << wlset[0] << "," << wlset[1] << "," << wlset[2] << "," << wlset[3] << (i+1==allWavelengths.size() ? "\n" : ",");
            }
            out << "atmosphere height: " << atmosphereHeight << "\n";
            out << "Earth radius: " << earthRadius << "\n";
            out << "Earth-Moon distance: " << earthMoonDistance << "\n";
            out << "scatterers: {";
            for(const auto& scatterer : scatterers)
                out << " \"" << scatterer.name << "\" { phase function " << toString(scatterer.phaseFunctionType) << " };";
            out << " }\n";
            out.flush();
            file.close();
            if(file.error())
            {
                std::cerr << " FAILED to write: " << file.errorString().toStdString() << "\n";
                return 1;
            }
            std::cerr << " done\n";
        }

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
                << format.minorVersion() << " context\n";
            return 1;
        }

        QOffscreenSurface surface;
        surface.setFormat(format);
        surface.create();
        if(!surface.isValid())
        {
            std::cerr << "Failed to create OpenGL "
                << format.majorVersion() << '.'
                << format.minorVersion() << " offscreen surface\n";
            return 1;
        }

        context.makeCurrent(&surface);

        if(!gl.initializeOpenGLFunctions())
        {
            std::cerr << "Failed to initialize OpenGL "
                << format.majorVersion() << '.'
                << format.minorVersion() << " functions\n";
            return 1;
        }

        init();

        const auto timeBegin=std::chrono::steady_clock::now();

        for(unsigned texIndex=0;texIndex<allWavelengths.size();++texIndex)
        {
            std::cerr << "Working on wavelengths " << allWavelengths[texIndex][0] << ", "
                                                   << allWavelengths[texIndex][1] << ", "
                                                   << allWavelengths[texIndex][2] << ", "
                                                   << allWavelengths[texIndex][3] << " nm"
                         " (set " << texIndex+1 << " of " << allWavelengths.size() << "):\n";
            OutputIndentIncrease incr;

            initConstHeader(allWavelengths[texIndex]);
            virtualSourceFiles[COMPUTE_TRANSMITTANCE_SHADER_FILENAME]=
                makeTransmittanceComputeFunctionsSrc(allWavelengths[texIndex]);
            virtualSourceFiles[PHASE_FUNCTIONS_SHADER_FILENAME]=makePhaseFunctionsSrc();
            virtualSourceFiles[TOTAL_SCATTERING_COEFFICIENT_SHADER_FILENAME]=makeTotalScatteringCoefSrc();
            virtualHeaderFiles[RADIANCE_TO_LUMINANCE_HEADER_FILENAME]="const mat4 radianceToLuminance=" +
                                                                        toString(radianceToLuminance(texIndex)) + ";\n";

            saveZeroOrderScatteringRenderingShader(texIndex);

            {
                std::cerr << indentOutput() << "Computing parts of scattering order 1:\n";
                OutputIndentIncrease incr;

                computeTransmittance(texIndex);
                // We'll use ground irradiance to take into account the contribution of light scattered by the ground to the
                // sky color. Irradiance will also be needed when we want to draw the ground itself.
                computeDirectGroundIrradiance(texIndex);
            }

            computeMultipleScattering(texIndex);
        }
        saveMultipleScatteringRenderingShader();

        {
            const auto timeEnd=std::chrono::steady_clock::now();
            const auto microsecTaken=std::chrono::duration_cast<std::chrono::microseconds>(timeEnd-timeBegin).count();
            const auto secondsTaken=1e-6*microsecTaken;
            if(secondsTaken<60)
            {
                std::cerr << "Finished in " << secondsTaken << " s\n";
            }
            else
            {
                auto remainder=secondsTaken;
                const auto d = int(remainder/(24*3600));
                remainder -= d*(24*3600);
                const auto h = int(remainder/3600);
                remainder -= h*3600;
                const auto m = int(remainder/60);
                remainder -= m*60;
                const auto s = std::lround(remainder);
                std::cerr << "Finished in ";
                if(d)
                    std::cerr << d << "d";
                if(d || h)
                    std::cerr << h << "h";
                if(d || h || m)
                    std::cerr << m << "m";
                std::cerr << s << "s\n";
            }
        }
    }
    catch(ParsingError const& ex)
    {
        std::cerr << ex.what();
        return 1;
    }
    catch(Error const& ex)
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
