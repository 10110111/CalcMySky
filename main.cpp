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

void computeTransmittance(const int texIndex)
{
    const auto program=compileShaderProgram("compute-transmittance.frag", "transmittance computation shader program");

    std::cerr << "Computing transmittance... ";
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_TRANSMITTANCE]);
    assert(fbos[FBO_TRANSMITTANCE]);
    setupTexture(TEX_TRANSMITTANCE,transmittanceTexW,transmittanceTexH);
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
    const auto program=compileShaderProgram("compute-direct-irradiance.frag", "direct ground irradiance computation shader program");

    std::cerr << "Computing direct ground irradiance... ";
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_IRRADIANCE]);
    setupTexture(TEX_DELTA_IRRADIANCE,irradianceTexW,irradianceTexH);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,textures[TEX_DELTA_IRRADIANCE],0);
    setupTexture(TEX_IRRADIANCE,irradianceTexW,irradianceTexH);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,textures[TEX_IRRADIANCE],0);
    checkFramebufferStatus("framebuffer for irradiance texture");
    setDrawBuffers({GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1});

    program->bind();

    setUniformTexture(*program,GL_TEXTURE_2D,TEX_TRANSMITTANCE,0,"transmittanceTexture");
    program->setUniformValue("solarIrradianceAtTOA",solarIrradianceAtTOA);

    gl.glViewport(0, 0, irradianceTexW, irradianceTexH);
    renderUntexturedQuad();

    gl.glFinish();
    std::cerr << "done\n";

    if(dbgSaveGroundIrradiance)
    {
        saveTexture(GL_TEXTURE_2D,textures[TEX_DELTA_IRRADIANCE],"irradiance texture",
                    textureOutputDir+"/irradiance-delta-order0-"+std::to_string(texIndex)+".f32",
                    {float(irradianceTexW), float(irradianceTexH)});
        QImage image(irradianceTexW, irradianceTexH, QImage::Format_RGBA8888);
        image.fill(Qt::magenta);
        gl.glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
        image.mirrored().save(QString("%1/irradiance-delta-order0-%2.png").arg(textureOutputDir.c_str()).arg(texIndex));

        saveTexture(GL_TEXTURE_2D,textures[TEX_IRRADIANCE],"irradiance texture",
                    textureOutputDir+"/irradiance-accum-order0-"+std::to_string(texIndex)+".f32",
                    {float(irradianceTexW), float(irradianceTexH)});
        image.fill(Qt::magenta);
        gl.glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
        image.mirrored().save(QString("%1/irradiance-accum-order0-%2.png").arg(textureOutputDir.c_str()).arg(texIndex));
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void computeSingleScattering(glm::vec4 const& wavelengths, QVector4D const& solarIrradianceAtTOA, const int texIndex)
{
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_DELTA_SCATTERING]);
    setupTexture(TEX_DELTA_SCATTERING,scatTexWidth(),scatTexHeight(),scatTexDepth());
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, textures[TEX_DELTA_SCATTERING],0);
    checkFramebufferStatus("framebuffer for first scattering");

    gl.glViewport(0, 0, scatTexWidth(), scatTexHeight());

    // TODO: try rendering multiple scatterers to multiple render targets at
    // once - as VRAM size allows. This may improve performance.
    for(const auto& scatterer : scatterers)
    {
        {
            const auto src=makeScattererDensityFunctionsSrc()+
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

        setUniformTexture(*program,GL_TEXTURE_2D,TEX_TRANSMITTANCE,0,"transmittanceTexture");

        std::cerr << "Computing single scattering layers for scatterer \"" << scatterer.name.toStdString() << "\"... ";
        for(int layer=0; layer<scatTexDepth(); ++layer)
        {
            std::cerr << layer;
            program->setUniformValue("layer",layer);
            renderUntexturedQuad();
            gl.glFinish();
            if(layer+1<scatTexDepth()) std::cerr << ',';
        }
        std::cerr << "; done\n";

        saveTexture(GL_TEXTURE_3D,textures[TEX_DELTA_SCATTERING],
                    "single scattering texture",
                    textureOutputDir+"/single-scattering-"+scatterer.name.toStdString()+"-"+std::to_string(texIndex)+".f32",
                    {scatteringTextureSize[0], scatteringTextureSize[1], scatteringTextureSize[2], scatteringTextureSize[3]});
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void computeScatteringDensityOrder2(const int texIndex)
{
    constexpr int scatteringOrder=2;

    std::unique_ptr<QOpenGLShaderProgram> program;
    {
        // Make a stub for current phase function. It's not used for ground radiance, but we need it to avoid linking errors.
        const auto src=makePhaseFunctionsSrc()+
            "vec4 currentPhaseFunction(float dotViewSun) { return vec4(3.4028235e38); }\n";
        allShaders.erase(PHASE_FUNCTIONS_SHADER_FILENAME);
        virtualSourceFiles[PHASE_FUNCTIONS_SHADER_FILENAME]=src;

        // Doing replacements instead of using uniforms is meant to
        //  1) Improve performance by statically avoiding branching
        //  2) Ease debugging by clearing the list of really-used uniforms (this can be printed by dumpActiveUniforms())
        allShaders.erase(COMPUTE_SCATTERING_DENSITY_FILENAME);
        virtualSourceFiles.erase(COMPUTE_SCATTERING_DENSITY_FILENAME);
        virtualSourceFiles.emplace(COMPUTE_SCATTERING_DENSITY_FILENAME,
            getShaderSrc(COMPUTE_SCATTERING_DENSITY_FILENAME).replace(QRegExp("\\bRADIATION_IS_FROM_GROUND_ONLY\\b"), "true")
                                                             .replace(QRegExp("\\bSCATTERING_ORDER\\b"), QString::number(scatteringOrder)));
        // recompile the program
        program=compileShaderProgram(COMPUTE_SCATTERING_DENSITY_FILENAME,
                                     "scattering density computation shader program", true);
    }

    gl.glViewport(0, 0, scatTexWidth(), scatTexHeight());

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_MULTIPLE_SCATTERING]);
    program->bind();

    const GLfloat altitudeMin=0, altitudeMax=atmosphereHeight; // TODO: implement splitting of calculations over altitude blocks
    program->setUniformValue("altitudeMin", altitudeMin);
    program->setUniformValue("altitudeMax", altitudeMax);

    setupTexture(TEX_DELTA_SCATTERING_DENSITY,scatTexWidth(),scatTexHeight(),scatTexDepth());
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,textures[TEX_DELTA_SCATTERING_DENSITY],0);
    checkFramebufferStatus("framebuffer for scattering density");

    setUniformTexture(*program,GL_TEXTURE_2D,TEX_TRANSMITTANCE   ,0,"transmittanceTexture");
    setUniformTexture(*program,GL_TEXTURE_2D,TEX_DELTA_IRRADIANCE,1,"irradianceTexture");

    std::cerr << " Computing scattering density layers for radiation from the ground... ";
    for(int layer=0; layer<scatTexDepth(); ++layer)
    {
        std::cerr << layer;
        program->setUniformValue("layer",layer);
        renderUntexturedQuad();
        gl.glFinish();
        if(layer+1<scatTexDepth()) std::cerr << ',';
    }
    std::cerr << "; done\n";

    if(dbgSaveScatDensityOrder2FromGround)
    {
        saveTexture(GL_TEXTURE_3D,textures[TEX_DELTA_SCATTERING_DENSITY],
                    "order 2 scattering density from ground texture",
                    textureOutputDir+"/scattering-density2-from-ground-"+std::to_string(texIndex)+".f32",
                    {scatteringTextureSize[0], scatteringTextureSize[1], scatteringTextureSize[2], scatteringTextureSize[3]},
                    1);
    }

    gl.glBlendFunc(GL_ONE, GL_ONE);
    gl.glEnable(GL_BLEND);
    for(const auto& scatterer : scatterers)
    {
        {
            const auto src=makePhaseFunctionsSrc()+
                "vec4 currentPhaseFunction(float dotViewSun) { return phaseFunction_"+scatterer.name+"(dotViewSun); }\n";
            allShaders.erase(PHASE_FUNCTIONS_SHADER_FILENAME);
            virtualSourceFiles[PHASE_FUNCTIONS_SHADER_FILENAME]=src;

            allShaders.erase(COMPUTE_SCATTERING_DENSITY_FILENAME);
            virtualSourceFiles.erase(COMPUTE_SCATTERING_DENSITY_FILENAME);
            virtualSourceFiles.emplace(COMPUTE_SCATTERING_DENSITY_FILENAME,
                                       getShaderSrc(COMPUTE_SCATTERING_DENSITY_FILENAME)
                                            .replace(QRegExp("\\bRADIATION_IS_FROM_GROUND_ONLY\\b"), "false")
                                            .replace(QRegExp("\\bSCATTERING_ORDER\\b"), QString::number(scatteringOrder)));
            // recompile the program
            program=compileShaderProgram(COMPUTE_SCATTERING_DENSITY_FILENAME,
                                                        "scattering density computation shader program", true);
        }
        program->bind();

        const auto firstScatteringTextureSampler=1;
        gl.glActiveTexture(GL_TEXTURE0+firstScatteringTextureSampler);
        gl.glBindTexture(GL_TEXTURE_3D, textures[TEX_DELTA_SCATTERING]);
        loadTexture(textureOutputDir+"/single-scattering-"+scatterer.name.toStdString()+"-"+std::to_string(texIndex)+".f32",
                    scatTexWidth(),scatTexHeight(),scatTexDepth());
        program->setUniformValue("firstScatteringTexture",firstScatteringTextureSampler);

        program->setUniformValue("altitudeMin", altitudeMin);
        program->setUniformValue("altitudeMax", altitudeMax);
        std::cerr << " Computing scattering density layers for scatterer \"" << scatterer.name.toStdString() << "\"... ";
        for(int layer=0; layer<scatTexDepth(); ++layer)
        {
            std::cerr << layer;
            program->setUniformValue("layer",layer);
            renderUntexturedQuad();
            gl.glFinish();
            if(layer+1<scatTexDepth()) std::cerr << ',';
        }
        std::cerr << "; done\n";
    }
    gl.glDisable(GL_BLEND);
    if(dbgSaveScatDensity)
    {
        saveTexture(GL_TEXTURE_3D,textures[TEX_DELTA_SCATTERING_DENSITY],
                    "order "+std::to_string(scatteringOrder)+" scattering density",
                    textureOutputDir+"/scattering-density"+std::to_string(scatteringOrder)+"-"+std::to_string(texIndex)+".f32",
                    {scatteringTextureSize[0], scatteringTextureSize[1], scatteringTextureSize[2], scatteringTextureSize[3]},
                    1);
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void computeScatteringDensity(const int scatteringOrder, const int texIndex)
{
    if(scatteringOrder==2)
    {
        computeScatteringDensityOrder2(texIndex);
        return;
    }

    allShaders.erase(COMPUTE_SCATTERING_DENSITY_FILENAME);
    virtualSourceFiles.erase(COMPUTE_SCATTERING_DENSITY_FILENAME);
    virtualSourceFiles.emplace(COMPUTE_SCATTERING_DENSITY_FILENAME,
                               getShaderSrc(COMPUTE_SCATTERING_DENSITY_FILENAME)
                                    .replace(QRegExp("\\bRADIATION_IS_FROM_GROUND_ONLY\\b"), "false")
                                    .replace(QRegExp("\\bSCATTERING_ORDER\\b"), QString::number(scatteringOrder)));
    // recompile the program
    const std::unique_ptr<QOpenGLShaderProgram> program=compileShaderProgram(COMPUTE_SCATTERING_DENSITY_FILENAME,
                                                                             "scattering density computation shader program",
                                                                             true);
    program->bind();

    setUniformTexture(*program,GL_TEXTURE_2D,TEX_TRANSMITTANCE   ,0,"transmittanceTexture");
    setUniformTexture(*program,GL_TEXTURE_2D,TEX_DELTA_IRRADIANCE,1,"irradianceTexture");
    setUniformTexture(*program,GL_TEXTURE_3D,TEX_DELTA_SCATTERING,2,"multipleScatteringTexture");

    const GLfloat altitudeMin=0, altitudeMax=atmosphereHeight; // TODO: implement splitting of calculations over altitude blocks
    program->setUniformValue("altitudeMin", altitudeMin);
    program->setUniformValue("altitudeMax", altitudeMax);

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_MULTIPLE_SCATTERING]);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,textures[TEX_DELTA_SCATTERING_DENSITY],0);
    std::cerr << " Computing scattering density layers... ";
    for(int layer=0; layer<scatTexDepth(); ++layer)
    {
        std::cerr << layer;
        program->setUniformValue("layer",layer);
        renderUntexturedQuad();
        gl.glFinish();
        if(layer+1<scatTexDepth()) std::cerr << ',';
    }
    std::cerr << "; done\n";
    if(dbgSaveScatDensity)
    {
        saveTexture(GL_TEXTURE_3D,textures[TEX_DELTA_SCATTERING_DENSITY],
                    "order "+std::to_string(scatteringOrder)+" scattering density",
                    textureOutputDir+"/scattering-density"+std::to_string(scatteringOrder)+"-"+std::to_string(texIndex)+".f32",
                    {scatteringTextureSize[0], scatteringTextureSize[1], scatteringTextureSize[2], scatteringTextureSize[3]},
                    1);
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void computeIndirectIrradianceOrder1(const int texIndex)
{
    constexpr int scatteringOrder=2;

    const GLfloat altitudeMin=0, altitudeMax=atmosphereHeight; // TODO: implement splitting of calculations over altitude blocks

    gl.glViewport(0, 0, irradianceTexW, irradianceTexH);

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_IRRADIANCE]);
    gl.glDisablei(GL_BLEND, 0); // First scatterer overwrites delta-irradiance-texture
    gl.glEnablei(GL_BLEND, 1); // Total irradiance is always accumulated
    for(const auto& scatterer : scatterers)
    {
        allShaders.erase(PHASE_FUNCTIONS_SHADER_FILENAME);
        virtualSourceFiles[PHASE_FUNCTIONS_SHADER_FILENAME]=makePhaseFunctionsSrc()+
            "vec4 currentPhaseFunction(float dotViewSun) { return phaseFunction_"+scatterer.name+"(dotViewSun); }\n";

        allShaders.erase(COMPUTE_INDIRECT_IRRADIANCE_FILENAME);
        virtualSourceFiles.erase(COMPUTE_INDIRECT_IRRADIANCE_FILENAME);
        virtualSourceFiles.emplace(COMPUTE_INDIRECT_IRRADIANCE_FILENAME,
                                   getShaderSrc(COMPUTE_INDIRECT_IRRADIANCE_FILENAME)
                                        .replace(QRegExp("\\bSCATTERING_ORDER\\b"), QString::number(scatteringOrder-1)));
        std::unique_ptr<QOpenGLShaderProgram> program=compileShaderProgram(COMPUTE_INDIRECT_IRRADIANCE_FILENAME,
                                                                           "indirect irradiance computation shader program");
        program->bind();

        const auto firstScatteringTextureSampler=1;
        gl.glActiveTexture(GL_TEXTURE0+firstScatteringTextureSampler);
        gl.glBindTexture(GL_TEXTURE_3D, textures[TEX_DELTA_SCATTERING]);
        loadTexture(textureOutputDir+"/single-scattering-"+scatterer.name.toStdString()+"-"+std::to_string(texIndex)+".f32",
                    scatTexWidth(),scatTexHeight(),scatTexDepth());
        program->setUniformValue("firstScatteringTexture",firstScatteringTextureSampler);

        program->setUniformValue("altitudeMin", altitudeMin);
        program->setUniformValue("altitudeMax", altitudeMax);

        std::cerr << " Computing indirect irradiance for scatterer \"" << scatterer.name.toStdString() << "\"... ";
        renderUntexturedQuad();
        gl.glFinish();
        std::cerr << "done\n";

        gl.glEnablei(GL_BLEND, 0); // Second and subsequent scatterers blend into delta-irradiance-texture
    }
    gl.glDisable(GL_BLEND);
    if(dbgSaveGroundIrradiance)
    {
        saveTexture(GL_TEXTURE_2D,textures[TEX_DELTA_IRRADIANCE],"irradiance texture",
                    textureOutputDir+"/irradiance-delta-order"+std::to_string(scatteringOrder-1)+"-"+std::to_string(texIndex)+".f32",
                    {float(irradianceTexW), float(irradianceTexH)}, 1);
        QImage image(irradianceTexW, irradianceTexH, QImage::Format_RGBA8888);
        image.fill(Qt::magenta);
        gl.glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
        image.mirrored().save(QString("%1/irradiance-delta-order%2-%3.png").arg(textureOutputDir.c_str()).arg(scatteringOrder-1).arg(texIndex));

        saveTexture(GL_TEXTURE_2D,textures[TEX_IRRADIANCE],"irradiance texture",
                    textureOutputDir+"/irradiance-accum-order"+std::to_string(scatteringOrder-1)+"-"+std::to_string(texIndex)+".f32",
                    {float(irradianceTexW), float(irradianceTexH)}, 1);
        image.fill(Qt::magenta);
        gl.glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
        image.mirrored().save(QString("%1/irradiance-accum-order%2-%3.png").arg(textureOutputDir.c_str()).arg(scatteringOrder-1).arg(texIndex));
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void computeIndirectIrradiance(const int scatteringOrder, const int texIndex)
{
    if(scatteringOrder==2)
    {
        computeIndirectIrradianceOrder1(texIndex);
        return;
    }
    const GLfloat altitudeMin=0, altitudeMax=atmosphereHeight; // TODO: implement splitting of calculations over altitude blocks

    gl.glViewport(0, 0, irradianceTexW, irradianceTexH);

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_IRRADIANCE]);
    gl.glDisablei(GL_BLEND, 0); // Overwrite delta-irradiance-texture
    gl.glEnablei(GL_BLEND, 1); // Accumulate total irradiance

    allShaders.erase(COMPUTE_INDIRECT_IRRADIANCE_FILENAME);
    virtualSourceFiles.erase(COMPUTE_INDIRECT_IRRADIANCE_FILENAME);
    virtualSourceFiles.emplace(COMPUTE_INDIRECT_IRRADIANCE_FILENAME,
                               getShaderSrc(COMPUTE_INDIRECT_IRRADIANCE_FILENAME)
                                    .replace(QRegExp("\\bSCATTERING_ORDER\\b"), QString::number(scatteringOrder-1)));
    std::unique_ptr<QOpenGLShaderProgram> program=compileShaderProgram(COMPUTE_INDIRECT_IRRADIANCE_FILENAME,
                                                                       "indirect irradiance computation shader program");
    program->bind();

    program->setUniformValue("altitudeMin", altitudeMin);
    program->setUniformValue("altitudeMax", altitudeMax);

    setUniformTexture(*program,GL_TEXTURE_3D,TEX_DELTA_SCATTERING,0,"multipleScatteringTexture");

    std::cerr << " Computing indirect irradiance... ";
    renderUntexturedQuad();
    gl.glFinish();
    std::cerr << "done\n";

    gl.glDisable(GL_BLEND);
    if(dbgSaveGroundIrradiance)
    {
        saveTexture(GL_TEXTURE_2D,textures[TEX_DELTA_IRRADIANCE],"irradiance texture",
                    textureOutputDir+"/irradiance-delta-order"+std::to_string(scatteringOrder-1)+"-"+std::to_string(texIndex)+".f32",
                    {float(irradianceTexW), float(irradianceTexH)}, 1);
        QImage image(irradianceTexW, irradianceTexH, QImage::Format_RGBA8888);
        image.fill(Qt::magenta);
        gl.glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
        image.mirrored().save(QString("%1/irradiance-delta-order%2-%3.png").arg(textureOutputDir.c_str()).arg(scatteringOrder-1).arg(texIndex));

        saveTexture(GL_TEXTURE_2D,textures[TEX_IRRADIANCE],"irradiance texture",
                    textureOutputDir+"/irradiance-accum-order"+std::to_string(scatteringOrder-1)+"-"+std::to_string(texIndex)+".f32",
                    {float(irradianceTexW), float(irradianceTexH)}, 1);
        image.fill(Qt::magenta);
        gl.glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
        image.mirrored().save(QString("%1/irradiance-accum-order%2-%3.png").arg(textureOutputDir.c_str()).arg(scatteringOrder-1).arg(texIndex));
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void accumulateMultipleScattering(const int scatteringOrder, const int texIndex)
{
    // We didn't render to the accumulating texture to avoid holding more than two 4D textures in VRAM at once.
    // Now it's time to do this by only holding the accumulator and delta scattering texture in VRAM.
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_MULTIPLE_SCATTERING]);
    setupTexture(TEX_MULTIPLE_SCATTERING,scatTexWidth(),scatTexHeight(),scatTexDepth());
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, textures[TEX_MULTIPLE_SCATTERING],0);
    checkFramebufferStatus("framebuffer for accumulation of multiple scattering data");

    const auto program=compileShaderProgram("copy-scattering-texture.frag",
                                            "scattering texture copy-blend shader program",
                                            true);
    program->bind();
    setUniformTexture(*program,GL_TEXTURE_3D,TEX_DELTA_SCATTERING,0,"tex");
    if(scatteringOrder>2)
        gl.glEnable(GL_BLEND);
    else
        gl.glDisable(GL_BLEND);
    std::cerr << "Blending multiple scattering layers into accumulator texture... ";
    for(int layer=0; layer<scatTexDepth(); ++layer)
    {
        std::cerr << layer;
        program->setUniformValue("layer",layer);
        renderUntexturedQuad();
        gl.glFinish();
        if(layer+1<scatTexDepth()) std::cerr << ',';
    }
    std::cerr << "; done\n";
    gl.glDisable(GL_BLEND);

    if(dbgSaveAccumScattering)
    {
        saveTexture(GL_TEXTURE_3D,textures[TEX_MULTIPLE_SCATTERING],
                    "multiple scattering accumulator texture",
                    textureOutputDir+"/multiple-scattering-to-order"+std::to_string(scatteringOrder)+"-"+std::to_string(texIndex)+".f32",
                    {scatteringTextureSize[0], scatteringTextureSize[1], scatteringTextureSize[2], scatteringTextureSize[3]});
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void computeMultipleScatteringFromDensity(const int scatteringOrder, const int texIndex)
{
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_MULTIPLE_SCATTERING]);
    setupTexture(TEX_DELTA_SCATTERING,scatTexWidth(),scatTexHeight(),scatTexDepth());
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, textures[TEX_DELTA_SCATTERING],0);
    checkFramebufferStatus("framebuffer for delta multiple scattering");

    gl.glViewport(0, 0, scatTexWidth(), scatTexHeight());

    {
        const auto program=compileShaderProgram("compute-multiple-scattering.frag",
                                                "multiple scattering computation shader program",
                                                true);
        program->bind();

        const GLfloat altitudeMin=0, altitudeMax=atmosphereHeight; // TODO: implement splitting of calculations over altitude blocks
        program->setUniformValue("altitudeMin", altitudeMin);
        program->setUniformValue("altitudeMax", altitudeMax);

        setUniformTexture(*program,GL_TEXTURE_2D,TEX_TRANSMITTANCE,0,"transmittanceTexture");
        setUniformTexture(*program,GL_TEXTURE_3D,TEX_DELTA_SCATTERING_DENSITY,1,"scatteringDensityTexture");

        std::cerr << "Computing multiple scattering layers... ";
        for(int layer=0; layer<scatTexDepth(); ++layer)
        {
            std::cerr << layer;
            program->setUniformValue("layer",layer);
            renderUntexturedQuad();
            gl.glFinish();
            if(layer+1<scatTexDepth()) std::cerr << ',';
        }
        std::cerr << "; done\n";

        if(dbgSaveDeltaScattering)
        {
            saveTexture(GL_TEXTURE_3D,textures[TEX_DELTA_SCATTERING],
                        "delta scattering texture",
                        textureOutputDir+"/delta-scattering-order"+std::to_string(scatteringOrder)+"-"+std::to_string(texIndex)+".f32",
                        {scatteringTextureSize[0], scatteringTextureSize[1], scatteringTextureSize[2], scatteringTextureSize[3]});
        }
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
    accumulateMultipleScattering(scatteringOrder, texIndex);
}

void computeMultipleScattering(const int texIndex)
{
    for(int scatteringOrder=2; scatteringOrder<=scatteringOrdersToCompute; ++scatteringOrder)
    {
        std::cerr << "Computing scattering order " << scatteringOrder << "...\n";
        computeScatteringDensity(scatteringOrder,texIndex);
        computeIndirectIrradiance(scatteringOrder,texIndex);
        computeMultipleScatteringFromDensity(scatteringOrder,texIndex);
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
        for(unsigned texIndex=0;texIndex<allWavelengths.size();++texIndex)
        {
            allShaders.clear();
            initConstHeader(allWavelengths[texIndex]);
            virtualSourceFiles[COMPUTE_TRANSMITTANCE_SHADER_FILENAME]=
                makeTransmittanceComputeFunctionsSrc(allWavelengths[texIndex]);
            virtualSourceFiles[PHASE_FUNCTIONS_SHADER_FILENAME]=makePhaseFunctionsSrc();
            virtualSourceFiles[TOTAL_SCATTERING_COEFFICIENT_SHADER_FILENAME]=makeTotalScatteringCoefSrc();
            computeTransmittance(texIndex);
            // We'll use ground irradiance to take into account the contribution of light scattered by the ground to the
            // sky color. Irradiance will also be needed when we want to draw the ground itself.
            computeDirectGroundIrradiance(QVec(solarIrradianceAtTOA[texIndex]), texIndex);

            computeSingleScattering(allWavelengths[texIndex], QVec(solarIrradianceAtTOA[texIndex]), texIndex);
            computeMultipleScattering(texIndex);
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
