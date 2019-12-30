#include <iostream>
#include <fstream>
#include <memory>
#include <random>
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

QOpenGLFunctions_3_3_Core gl;

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

    gl.glFinish();
    std::cerr << "done\n";

    saveTexture(GL_TEXTURE_2D,textures[TEX_TRANSMITTANCE],"transmittance texture",
                textureOutputDir+"/transmittance-"+std::to_string(texIndex)+".f32",
                {float(transmittanceTexW), float(transmittanceTexH)});

    if(dbgSaveTransmittancePng)
    {
        QImage image(transmittanceTexW, transmittanceTexH, QImage::Format_RGBA8888);
        image.fill(Qt::magenta);
        gl.glReadPixels(0,0,transmittanceTexW,transmittanceTexH,GL_RGBA,GL_UNSIGNED_BYTE,image.bits());
        image.mirrored().save(QString("%1/transmittance-png-%2.png").arg(textureOutputDir.c_str()).arg(texIndex));
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void computeDirectGroundIrradiance(QVector4D const& solarIrradianceAtTOA, const int texIndex)
{
    const auto program=compileShaderProgram("compute-irradiance.frag", "direct ground irradiance computation shader program");

    std::cerr << "Computing direct ground irradiance... ";
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_IRRADIANCE]);
    gl.glBindTexture(GL_TEXTURE_2D,textures[TEX_DELTA_IRRADIANCE]);
    gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F_ARB,irradianceTexW,irradianceTexH,
                    0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl.glBindTexture(GL_TEXTURE_2D,0);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,textures[TEX_DELTA_IRRADIANCE],0);
    checkFramebufferStatus("framebuffer for irradiance texture");

    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D,textures[TEX_TRANSMITTANCE]);

    program->bind();

    program->setUniformValue("transmittanceTexture",0);
    program->setUniformValue("solarIrradianceAtTOA",solarIrradianceAtTOA);

    gl.glViewport(0, 0, irradianceTexW, irradianceTexH);
    renderUntexturedQuad();

    gl.glFinish();
    std::cerr << "done\n";

    saveTexture(GL_TEXTURE_2D,textures[TEX_DELTA_IRRADIANCE],"irradiance texture",
                textureOutputDir+"/irradiance-"+std::to_string(texIndex)+".f32",
                {float(irradianceTexW), float(irradianceTexH)});

    if(dbgSaveDirectGroundIrradiancePng)
    {
        QImage image(irradianceTexW, irradianceTexH, QImage::Format_RGBA8888);
        image.fill(Qt::magenta);
        gl.glReadPixels(0,0,irradianceTexW,irradianceTexH,GL_RGBA,GL_UNSIGNED_BYTE,image.bits());
        image.mirrored().save(QString("%1/irradiance-png-%2.png").arg(textureOutputDir.c_str()).arg(texIndex));
    }
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
                            "vec4 scatteringCrossSection() { return "+toString(scatterer.crossSection(wavelengths))+"; }\n";
            allShaders.erase(DENSITIES_SHADER_FILENAME);
            virtualSourceFiles[DENSITIES_SHADER_FILENAME]=src;
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

        if(dbgSaveSingleScattering)
        {
            saveTexture(GL_TEXTURE_3D,textures[TEX_FIRST_SCATTERING],
                        "single scattering texture",
                        textureOutputDir+"/single-scattering-"+scatterer.name.toStdString()+"-"+std::to_string(texIndex)+".f32",
                        {scatteringTextureSize[0], scatteringTextureSize[1], scatteringTextureSize[2], scatteringTextureSize[3]});
        }
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
        initConstHeader();
        for(int texIndex=0;texIndex<allWavelengths.size();++texIndex)
        {
            allShaders.clear();
            virtualSourceFiles[COMPUTE_TRANSMITTANCE_SHADER_FILENAME]=
                makeTransmittanceComputeFunctionsSrc(allWavelengths[texIndex]);
            computeTransmittance(allWavelengths[texIndex], texIndex);
            // We'll use ground irradiance to take into account the contribution of light scattered by the ground to the
            // sky color. Irradiance will also be needed when we want to draw the ground itself.
            computeDirectGroundIrradiance(QVec(solarIrradianceAtTOA[texIndex]), texIndex);

            computeSingleScattering(allWavelengths[texIndex], QVec(solarIrradianceAtTOA[texIndex]), texIndex);
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
