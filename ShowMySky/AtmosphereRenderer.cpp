#include "AtmosphereRenderer.hpp"

#include <set>
#include <cmath>
#include <array>
#include <vector>
#include <cstring>
#include <cassert>
#include <iterator>
#include <iostream>
#include <filesystem>
#include <QFile>
#include <QDebug>
#include <QRegularExpression>

#include "util.hpp"
#include "../common/const.hpp"
#include "../common/util.hpp"
#include "../common/EclipsedDoubleScatteringPrecomputer.hpp"
#include "api/Settings.hpp"

namespace fs=std::filesystem;

namespace
{

// XXX: keep in sync with those in CalcMySky
GLsizei scatTexWidth(glm::ivec4 sizes) { return sizes[0]; }
GLsizei scatTexHeight(glm::ivec4 sizes) { return sizes[1]*sizes[2]; }
GLsizei scatTexDepth(glm::ivec4 sizes) { return sizes[3]; }

auto newTex(QOpenGLTexture::Target target)
{
    return std::make_unique<QOpenGLTexture>(target);
}

[[maybe_unused]] PFNGLDEBUGMESSAGEINSERTPROC glDebugMessageInsert;
void oglDebugMessageInsert([[maybe_unused]] const char*const message)
{
#ifndef NDEBUG
    if(!glDebugMessageInsert)
        glDebugMessageInsert=reinterpret_cast<PFNGLDEBUGMESSAGEINSERTPROC>
            (QOpenGLContext::currentContext()->getProcAddress("glDebugMessageInsert"));

    if(!glDebugMessageInsert)
        return;

    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 0, GL_DEBUG_SEVERITY_NOTIFICATION, -1, message);
#endif
}

class OGLTrace
{
    std::string action;
public:
    OGLTrace(const std::string& action)
        : action(action)
    {
        oglDebugMessageInsert(("Begin "+action).c_str());
    }
    ~OGLTrace()
    {
        oglDebugMessageInsert(("End "+action).c_str());
    }
};

#ifndef NDEBUG
# define OGL_TRACE() [[maybe_unused]] OGLTrace t(Q_FUNC_INFO);
#else
# define OGL_TRACE()
#endif

}

void AtmosphereRenderer::updateAltitudeTexCoords(const float altitudeCoord, double* floorAltIndexOut)
{
    const auto altTexIndex = altitudeCoord==1 ? numAltIntervalsIn4DTexture_-1 : altitudeCoord*numAltIntervalsIn4DTexture_;
    const auto floorAltIndex = std::floor(altTexIndex);
    const auto fractAltIndex = altTexIndex-floorAltIndex;

    staticAltitudeTexCoord_ = unitRangeToTexCoord(fractAltIndex, 2);

    if(floorAltIndexOut) *floorAltIndexOut=floorAltIndex;
}

void AtmosphereRenderer::updateEclipsedAltitudeTexCoords(const float altitudeCoord, double* floorAltIndexOut)
{
    const auto altTexIndex = altitudeCoord==1 ? numAltIntervalsInEclipsed4DTexture_-1 : altitudeCoord*numAltIntervalsInEclipsed4DTexture_;
    const auto floorAltIndex = std::floor(altTexIndex);
    const auto fractAltIndex = altTexIndex-floorAltIndex;

    eclipsedDoubleScatteringAltitudeAlphaUpper_ = fractAltIndex;

    if(floorAltIndexOut) *floorAltIndexOut=floorAltIndex;
}

void AtmosphereRenderer::loadTexture4D(QString const& path, const float altitudeCoord)
{
    auto log=qDebug().nospace();

    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        throw DataLoadError{tr("GL error on entry to loadTexture4D(\"%1\"): %2")
                            .arg(path).arg(openglErrorString(err).c_str())};
    }
    log << "Loading texture from " << path << "... ";
    QFile file(path);
    if(!file.open(QFile::ReadOnly))
        throw DataLoadError{tr("Failed to open file \"%1\": %2").arg(path).arg(file.errorString())};

    uint16_t sizes[4];
    {
        const qint64 sizeToRead=sizeof sizes;
        if(file.read(reinterpret_cast<char*>(sizes), sizeToRead) != sizeToRead)
        {
            throw DataLoadError{tr("Failed to read header from file \"%1\": %2")
                                .arg(path).arg(file.errorString())};
        }
    }
    log << "dimensions from header: " << sizes[0] << "×" << sizes[1] << "×" << sizes[2] << "×" << sizes[3] << "... ";

    if(const qint64 expectedFileSize = sizeof(GLfloat)*4*uint64_t(sizes[0])*sizes[1]*sizes[2]*sizes[3] + file.pos();
       expectedFileSize != file.size())
    {
        throw DataLoadError{tr("Size of file \"%1\" (%2 bytes) doesn't match image dimensions %3×%4×%5×%6 from file header.\nThe expected size is %7 bytes.")
                            .arg(path).arg(file.size()).arg(sizes[0]).arg(sizes[1]).arg(sizes[2]).arg(sizes[3]).arg(expectedFileSize)};
    }

    numAltIntervalsIn4DTexture_ = sizes[3]-1;
    double floorAltIndex;
    updateAltitudeTexCoords(altitudeCoord, &floorAltIndex);

    loadedAltitudeURTexCoordRange_[0] = floorAltIndex/numAltIntervalsIn4DTexture_;
    loadedAltitudeURTexCoordRange_[1] = (floorAltIndex+1)/numAltIntervalsIn4DTexture_;

    const auto subpixelReadOffset = 4*uint64_t(sizes[0])*sizes[1]*sizes[2]*uint64_t(floorAltIndex);
    sizes[3]=2;
    const auto subpixelCountToRead = 4*uint64_t(sizes[0])*sizes[1]*sizes[2]*sizes[3];

    const std::unique_ptr<GLfloat[]> subpixels(new GLfloat[subpixelCountToRead]);
    {
        const qint64 offset=file.pos()+subpixelReadOffset*sizeof subpixels[0];
        log << "skipping to offset " << offset << "... ";
        if(!file.seek(offset))
        {
            throw DataLoadError{tr("Failed to seek to offset %1 in file \"%2\": %3")
                                .arg(offset).arg(path).arg(file.errorString())};
        }
        const qint64 sizeToRead=subpixelCountToRead*sizeof subpixels[0];
        const auto actuallyRead=file.read(reinterpret_cast<char*>(subpixels.get()), sizeToRead);
        if(actuallyRead != sizeToRead)
        {
            const auto error = actuallyRead==-1 ? tr("Failed to read texture data from file \"%1\": %2").arg(path).arg(file.errorString())
                                                : tr("Failed to read texture data from file \"%1\": requested %2 bytes, read %3").arg(path).arg(sizeToRead).arg(actuallyRead);
            throw DataLoadError{error};
        }
    }
    const glm::ivec4 size(sizes[0],sizes[1],sizes[2],sizes[3]);
    gl.glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA32F,scatTexWidth(size),scatTexHeight(size),scatTexDepth(size),
                    0,GL_RGBA,GL_FLOAT,subpixels.get());
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        throw DataLoadError{tr("GL error in loadTexture4D(\"%1\") after glTexImage3D() call: %2")
                            .arg(path).arg(openglErrorString(err).c_str())};
    }

    log << "done";
}

void AtmosphereRenderer::load4DTexAltitudeSlicePair(QString const& path, QOpenGLTexture& texLower,
                                                    QOpenGLTexture& texUpper, const float altitudeCoord)
{
    auto log=qDebug().nospace();

    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        throw DataLoadError{tr("GL error on entry to load4DTexAltitudeSlicePair(\"%1\"): %2")
                            .arg(path).arg(openglErrorString(err).c_str())};
    }
    log << "Loading texture from " << path << "... ";
    QFile file(path);
    if(!file.open(QFile::ReadOnly))
        throw DataLoadError{tr("Failed to open file \"%1\": %2").arg(path).arg(file.errorString())};

    uint16_t sizes[4];
    {
        const qint64 sizeToRead=sizeof sizes;
        if(file.read(reinterpret_cast<char*>(sizes), sizeToRead) != sizeToRead)
        {
            throw DataLoadError{tr("Failed to read header from file \"%1\": %2")
                                .arg(path).arg(file.errorString())};
        }
    }
    log << "dimensions from header: " << sizes[0] << "×" << sizes[1] << "×" << sizes[2] << "×" << sizes[3] << "... ";

    if(const qint64 expectedFileSize = sizeof(GLfloat)*4*uint64_t(sizes[0])*sizes[1]*sizes[2]*sizes[3] + file.pos();
       expectedFileSize != file.size())
    {
        throw DataLoadError{tr("Size of file \"%1\" (%2 bytes) doesn't match image dimensions %3×%4×%5×%6"
                               " from file header.\nThe expected size is %7 bytes.").arg(path)
                                                                                    .arg(file.size())
                                                                                    .arg(sizes[0])
                                                                                    .arg(sizes[1])
                                                                                    .arg(sizes[2])
                                                                                    .arg(sizes[3])
                                                                                    .arg(expectedFileSize)};
    }

    numAltIntervalsInEclipsed4DTexture_ = sizes[3]-1;
    double floorAltIndex;
    updateEclipsedAltitudeTexCoords(altitudeCoord, &floorAltIndex);
    loadedEclipsedDoubleScatteringAltitudeURTexCoordRange_[0] = floorAltIndex/numAltIntervalsInEclipsed4DTexture_;
    loadedEclipsedDoubleScatteringAltitudeURTexCoordRange_[1] = (floorAltIndex+1)/numAltIntervalsInEclipsed4DTexture_;

    const auto subpixelReadOffset = 4*uint64_t(sizes[0])*sizes[1]*sizes[2]*uint64_t(floorAltIndex);
    const auto subpixelsInSingleTexSlice = 4*uint64_t(sizes[0])*sizes[1]*sizes[2];
    sizes[3]=2;
    const auto subpixelCountToRead = subpixelsInSingleTexSlice*sizes[3];

    const std::unique_ptr<GLfloat[]> subpixels(new GLfloat[subpixelCountToRead]);
    {
        const qint64 offset=file.pos()+subpixelReadOffset*sizeof subpixels[0];
        log << "skipping to offset " << offset << "... ";
        if(!file.seek(offset))
        {
            throw DataLoadError{tr("Failed to seek to offset %1 in file \"%2\": %3")
                                .arg(offset).arg(path).arg(file.errorString())};
        }
        const qint64 sizeToRead=subpixelCountToRead*sizeof subpixels[0];
        const auto actuallyRead=file.read(reinterpret_cast<char*>(subpixels.get()), sizeToRead);
        if(actuallyRead != sizeToRead)
        {
            const auto error = actuallyRead==-1 ? tr("Failed to read texture data from file \"%1\": %2").arg(path)
                                                                                                        .arg(file.errorString())
                                                : tr("Failed to read texture data from file \"%1\": requested %2 bytes, read %3")
                                                                                                        .arg(path)
                                                                                                        .arg(sizeToRead)
                                                                                                        .arg(actuallyRead);
            throw DataLoadError{error};
        }
    }
    texLower.bind();
    gl.glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA32F,sizes[0],sizes[1],sizes[2],0,GL_RGBA,GL_FLOAT,&subpixels[0]);
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        throw DataLoadError{tr("GL error in load4DTexAltitudeSlicePair(\"%1\") after first glTexImage3D() call: %2")
                            .arg(path).arg(openglErrorString(err).c_str())};
    }
    texUpper.bind();
    gl.glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA32F,sizes[0],sizes[1],sizes[2],0,GL_RGBA,GL_FLOAT,&subpixels[subpixelsInSingleTexSlice]);
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        throw DataLoadError{tr("GL error in load4DTexAltitudeSlicePair(\"%1\") after second glTexImage3D() call: %2")
                            .arg(path).arg(openglErrorString(err).c_str())};
    }

    log << "done";
}

glm::ivec2 AtmosphereRenderer::loadTexture2D(QString const& path)
{
    auto log=qDebug().nospace();

    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        throw DataLoadError{tr("GL error on entry to loadTexture2D(\"%1\"): %2")
                            .arg(path).arg(openglErrorString(err).c_str())};
    }
    log << "Loading texture from " << path << "... ";
    QFile file(path);
    if(!file.open(QFile::ReadOnly))
        throw DataLoadError{tr("Failed to open file \"%1\": %2").arg(path).arg(file.errorString())};

    uint16_t sizes[2];
    {
        const qint64 sizeToRead=sizeof sizes;
        if(file.read(reinterpret_cast<char*>(sizes), sizeToRead) != sizeToRead)
        {
            throw DataLoadError{tr("Failed to read header from file \"%1\": %2")
                                .arg(path).arg(file.errorString())};
        }
    }
    const auto subpixelCount = 4*uint64_t(sizes[0])*sizes[1];
    log << "dimensions from header: " << sizes[0] << "×" << sizes[1] << "... ";

    if(const qint64 expectedFileSize = subpixelCount*sizeof(GLfloat)+file.pos();
       expectedFileSize != file.size())
    {
        throw DataLoadError{tr("Size of file \"%1\" (%2 bytes) doesn't match image dimensions %3×%4 from file header.\nThe expected size is %5 bytes.")
                            .arg(path).arg(file.size()).arg(sizes[0]).arg(sizes[1]).arg(expectedFileSize)};
    }

    const std::unique_ptr<GLfloat[]> subpixels(new GLfloat[subpixelCount]);
    {
        const qint64 sizeToRead=subpixelCount*sizeof subpixels[0];
        const auto actuallyRead=file.read(reinterpret_cast<char*>(subpixels.get()), sizeToRead);
        if(actuallyRead != sizeToRead)
        {
            const auto error = actuallyRead==-1 ? tr("Failed to read texture data from file \"%1\": %2").arg(path).arg(file.errorString())
                                                : tr("Failed to read texture data from file \"%1\": requested %2 bytes, read %3").arg(path).arg(sizeToRead).arg(actuallyRead);
            throw DataLoadError{error};
        }
    }
    gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,sizes[0],sizes[1],0,GL_RGBA,GL_FLOAT,subpixels.get());
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        throw DataLoadError{tr("GL error in loadTexture2D(\"%1\") after glTexImage2D() call: %2")
                            .arg(path).arg(openglErrorString(err).c_str())};
    }
    log << "done";
    return {sizes[0], sizes[1]};
}

void AtmosphereRenderer::loadTextures(const CountStepsOnly countStepsOnly)
{
    OGL_TRACE();

    while(gl.glGetError()!=GL_NO_ERROR);

    gl.glActiveTexture(GL_TEXTURE0);

    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
            continue;
        }

        auto& tex=*transmittanceTextures_.emplace_back(newTex(QOpenGLTexture::Target2D));
        tex.setMinificationFilter(QOpenGLTexture::Linear);
        tex.setWrapMode(QOpenGLTexture::ClampToEdge);
        tex.bind();
        loadTexture2D(QString("%1/transmittance-wlset%2.f32").arg(pathToData_).arg(wlSetIndex));
        tick(++loadingStepsDone_);
    }

    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
            continue;
        }

        auto& tex=*irradianceTextures_.emplace_back(newTex(QOpenGLTexture::Target2D));
        tex.setMinificationFilter(QOpenGLTexture::Linear);
        tex.setWrapMode(QOpenGLTexture::ClampToEdge);
        tex.bind();
        loadTexture2D(QString("%1/irradiance-wlset%2.f32").arg(pathToData_).arg(wlSetIndex));
        tick(++loadingStepsDone_);
    }

    reloadScatteringTextures(countStepsOnly);

    assert(gl.glGetError()==GL_NO_ERROR);
}

double AtmosphereRenderer::altitudeUnitRangeTexCoord() const
{
    const auto h = tools_->altitude();
    const auto H = params_.atmosphereHeight;
    const auto R = params_.earthRadius;
    return std::sqrt(h*(h+2*R) / ( H*(H+2*R) ));
}

void AtmosphereRenderer::reloadScatteringTextures(const CountStepsOnly countStepsOnly)
{
    const auto texFilter = tools_->textureFilteringEnabled() ? QOpenGLTexture::Linear : QOpenGLTexture::Nearest;
    const auto altCoord = altitudeUnitRangeTexCoord();

    multipleScatteringTextures_.clear();
    if(const auto filename=pathToData_+"/multiple-scattering-xyzw.f32"; QFile::exists(filename))
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
        }
        else
        {
            auto& tex=*multipleScatteringTextures_.emplace_back(newTex(QOpenGLTexture::Target3D));
            tex.setMinificationFilter(texFilter);
            tex.setMagnificationFilter(texFilter);
            tex.setWrapMode(QOpenGLTexture::ClampToEdge);
            tex.bind();
            loadTexture4D(filename, altCoord);
            tick(++loadingStepsDone_);
        }
    }
    else
    {
        for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
        {
            if(countStepsOnly)
            {
                ++totalLoadingStepsToDo_;
                continue;
            }

            auto& tex=*multipleScatteringTextures_.emplace_back(newTex(QOpenGLTexture::Target3D));
            tex.setMinificationFilter(texFilter);
            tex.setMagnificationFilter(texFilter);
            tex.setWrapMode(QOpenGLTexture::ClampToEdge);
            tex.bind();
            loadTexture4D(QString("%1/multiple-scattering-wlset%2.f32").arg(pathToData_).arg(wlSetIndex), altCoord);
            tick(++loadingStepsDone_);
        }
    }

    singleScatteringTextures_.clear();
    for(const auto& scatterer : params_.scatterers)
    {
        auto& texturesPerWLSet=singleScatteringTextures_[scatterer.name];
        switch(scatterer.phaseFunctionType)
        {
        case PhaseFunctionType::General:
            for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
            {
                if(countStepsOnly)
                {
                    ++totalLoadingStepsToDo_;
                    continue;
                }

                auto& texture=*texturesPerWLSet.emplace_back(newTex(QOpenGLTexture::Target3D));
                texture.setMinificationFilter(texFilter);
                texture.setMagnificationFilter(texFilter);
                texture.setWrapMode(QOpenGLTexture::ClampToEdge);
                texture.bind();
                loadTexture4D(QString("%1/single-scattering/%2/%3.f32").arg(pathToData_).arg(wlSetIndex).arg(scatterer.name), altCoord);
                tick(++loadingStepsDone_);
            }
            break;
        case PhaseFunctionType::Achromatic:
        {
            if(countStepsOnly)
            {
                ++totalLoadingStepsToDo_;
                continue;
            }

            auto& texture=*texturesPerWLSet.emplace_back(newTex(QOpenGLTexture::Target3D));
            texture.setMinificationFilter(texFilter);
            texture.setMagnificationFilter(texFilter);
            texture.setWrapMode(QOpenGLTexture::ClampToEdge);
            texture.bind();
            loadTexture4D(QString("%1/single-scattering/%2-xyzw.f32").arg(pathToData_).arg(scatterer.name), altCoord);
            tick(++loadingStepsDone_);
            break;
        }
        case PhaseFunctionType::Smooth:
            break;
        }
    }

    eclipsedDoubleScatteringTexturesLower_.clear();
    eclipsedDoubleScatteringTexturesUpper_.clear();
    if(!params_.noEclipsedDoubleScatteringTextures)
    {
        for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
        {
            if(countStepsOnly)
            {
                ++totalLoadingStepsToDo_;
                continue;
            }

            auto& texL=*eclipsedDoubleScatteringTexturesLower_.emplace_back(newTex(QOpenGLTexture::Target3D));
            texL.setMinificationFilter(texFilter);
            texL.setMagnificationFilter(texFilter);
            // relative azimuth
            texL.setWrapMode(QOpenGLTexture::DirectionS, QOpenGLTexture::Repeat);
            // VZA
            texL.setWrapMode(QOpenGLTexture::DirectionT, QOpenGLTexture::ClampToEdge);
            // SZA
            texL.setWrapMode(QOpenGLTexture::DirectionR, QOpenGLTexture::ClampToEdge);

            auto& texU=*eclipsedDoubleScatteringTexturesUpper_.emplace_back(newTex(QOpenGLTexture::Target3D));
            texU.setMinificationFilter(texFilter);
            texU.setMagnificationFilter(texFilter);
            // relative azimuth
            texU.setWrapMode(QOpenGLTexture::DirectionS, QOpenGLTexture::Repeat);
            // VZA
            texU.setWrapMode(QOpenGLTexture::DirectionT, QOpenGLTexture::ClampToEdge);
            // SZA
            texU.setWrapMode(QOpenGLTexture::DirectionR, QOpenGLTexture::ClampToEdge);
            load4DTexAltitudeSlicePair(QString("%1/eclipsed-double-scattering-wlset%2.f32").arg(pathToData_).arg(wlSetIndex), texL, texU, altCoord);
            tick(++loadingStepsDone_);
        }
    }

    eclipsedDoubleScatteringPrecomputationTargetTextures_.clear();
    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
            continue;
        }

        auto& tex=*eclipsedDoubleScatteringPrecomputationTargetTextures_.emplace_back(newTex(QOpenGLTexture::Target3D));
        tex.setMinificationFilter(QOpenGLTexture::Linear);
        tex.setMagnificationFilter(QOpenGLTexture::Linear);
        // relative azimuth
        tex.setWrapMode(QOpenGLTexture::DirectionS, QOpenGLTexture::Repeat);
        // cosVZA
        tex.setWrapMode(QOpenGLTexture::DirectionT, QOpenGLTexture::ClampToEdge);
        // dummy dimension
        tex.setWrapMode(QOpenGLTexture::DirectionR, QOpenGLTexture::Repeat);
    }

    lightPollutionTextures_.clear();
    if(const auto filename=pathToData_+"/light-pollution-xyzw.f32"; QFile::exists(filename))
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
        }
        else
        {
            auto& tex=*lightPollutionTextures_.emplace_back(newTex(QOpenGLTexture::Target2D));
            tex.setMinificationFilter(texFilter);
            tex.setMagnificationFilter(texFilter);
            tex.setWrapMode(QOpenGLTexture::ClampToEdge);
            tex.bind();
            loadTexture2D(filename);
            tick(++loadingStepsDone_);
        }
    }
    else
    {
        for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
        {
            if(countStepsOnly)
            {
                ++totalLoadingStepsToDo_;
                continue;
            }

            auto& tex=*lightPollutionTextures_.emplace_back(newTex(QOpenGLTexture::Target2D));
            tex.setMinificationFilter(texFilter);
            tex.setMagnificationFilter(texFilter);
            tex.setWrapMode(QOpenGLTexture::ClampToEdge);
            tex.bind();
            loadTexture2D(QString("%1/light-pollution-wlset%2.f32").arg(pathToData_).arg(wlSetIndex));
            tick(++loadingStepsDone_);
        }
    }
}

void AtmosphereRenderer::loadShaders(const CountStepsOnly countStepsOnly)
{
    QOpenGLShader viewDirVertShader(QOpenGLShader::Vertex);
    QOpenGLShader viewDirFragShader(QOpenGLShader::Fragment);
    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else
    {
        if(!viewDirVertShader.compileSourceCode(viewDirVertShaderSrc_))
            throw DataLoadError{QObject::tr("Failed to compile view direction vertex shader:\n%2").arg(viewDirVertShader.log())};
        if(!viewDirFragShader.compileSourceCode(viewDirFragShaderSrc_))
            throw DataLoadError{QObject::tr("Failed to compile view direction fragment shader:\n%2").arg(viewDirFragShader.log())};
        tick(++loadingStepsDone_);
    }

    singleScatteringPrograms_.clear();
    for(int renderMode=0; renderMode<SSRM_COUNT; ++renderMode)
    {
        auto& programsPerScatterer=*singleScatteringPrograms_.emplace_back(std::make_unique<std::map<QString,std::vector<ShaderProgPtr>>>());

        for(const auto& scatterer : params_.scatterers)
        {
            auto& programs=programsPerScatterer[scatterer.name];
            if(scatterer.phaseFunctionType==PhaseFunctionType::General || (scatterer.phaseFunctionType!=PhaseFunctionType::Smooth && renderMode==SSRM_ON_THE_FLY))
            {
                for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
                {
                    if(countStepsOnly)
                    {
                        ++totalLoadingStepsToDo_;
                        continue;
                    }

                    const auto scatDir=QString("%1/shaders/single-scattering/%2/%3/%4").arg(pathToData_)
                                                                                       .arg(singleScatteringRenderModeNames[renderMode])
                                                                                       .arg(wlSetIndex)
                                                                                       .arg(scatterer.name);
                    qDebug().nospace() << "Loading shaders from " << scatDir << "...";
                    auto& program=*programs.emplace_back(std::make_unique<QOpenGLShaderProgram>());

                    for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
                        addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

                    program.addShader(&viewDirFragShader);
                    program.addShader(&viewDirVertShader);
                    for(const auto& b : viewDirBindAttribLocations_)
                        program.bindAttributeLocation(b.first.c_str(), b.second);

                    link(program, tr("shader program for scatterer \"%1\"").arg(scatterer.name));
                    tick(++loadingStepsDone_);
                }
            }
            else if(scatterer.phaseFunctionType==PhaseFunctionType::Achromatic)
            {
                if(countStepsOnly)
                {
                    ++totalLoadingStepsToDo_;
                    continue;
                }

                const auto scatDir=QString("%1/shaders/single-scattering/%2/%3").arg(pathToData_)
                                                                                .arg(singleScatteringRenderModeNames[renderMode])
                                                                                .arg(scatterer.name);
                qDebug().nospace() << "Loading shaders from " << scatDir << "...";
                auto& program=*programs.emplace_back(std::make_unique<QOpenGLShaderProgram>());
                for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
                    addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

                program.addShader(&viewDirFragShader);
                program.addShader(&viewDirVertShader);
                for(const auto& b : viewDirBindAttribLocations_)
                    program.bindAttributeLocation(b.first.c_str(), b.second);

                link(program, tr("shader program for scatterer \"%1\"").arg(scatterer.name));
                tick(++loadingStepsDone_);
            }
        }
    }

    eclipsedSingleScatteringPrograms_.clear();
    for(int renderMode=SSRM_ON_THE_FLY; renderMode<SSRM_COUNT; ++renderMode)
    {
        auto& programsPerScatterer=*eclipsedSingleScatteringPrograms_.emplace_back(std::make_unique<ScatteringProgramsMap>());

        for(const auto& scatterer : params_.scatterers)
        {
            auto& programs=programsPerScatterer[scatterer.name];
            if(scatterer.phaseFunctionType==PhaseFunctionType::General || renderMode==SSRM_ON_THE_FLY)
            {
                for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
                {
                    if(countStepsOnly)
                    {
                        ++totalLoadingStepsToDo_;
                        continue;
                    }

                    const auto scatDir=QString("%1/shaders/single-scattering-eclipsed/%2/%3/%4").arg(pathToData_)
                                                                                                .arg(singleScatteringRenderModeNames[renderMode])
                                                                                                .arg(wlSetIndex)
                                                                                                .arg(scatterer.name);
                    qDebug().nospace() << "Loading shaders from " << scatDir << "...";
                    auto& program=*programs.emplace_back(std::make_unique<QOpenGLShaderProgram>());

                    for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
                        addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

                    program.addShader(&viewDirFragShader);
                    program.addShader(&viewDirVertShader);
                    for(const auto& b : viewDirBindAttribLocations_)
                        program.bindAttributeLocation(b.first.c_str(), b.second);

                    link(program, tr("shader program for scatterer \"%1\"").arg(scatterer.name));
                    tick(++loadingStepsDone_);
                }
            }
            else
            {
                if(countStepsOnly)
                {
                    ++totalLoadingStepsToDo_;
                    continue;
                }

                const auto scatDir=QString("%1/shaders/single-scattering-eclipsed/%2/%3").arg(pathToData_)
                                                                                            .arg(singleScatteringRenderModeNames[renderMode])
                                                                                            .arg(scatterer.name);
                qDebug().nospace() << "Loading shaders from " << scatDir << "...";
                auto& program=*programs.emplace_back(std::make_unique<QOpenGLShaderProgram>());

                for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
                    addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

                program.addShader(&viewDirFragShader);
                program.addShader(&viewDirVertShader);
                for(const auto& b : viewDirBindAttribLocations_)
                    program.bindAttributeLocation(b.first.c_str(), b.second);

                link(program, tr("shader program for scatterer \"%1\"").arg(scatterer.name));
                tick(++loadingStepsDone_);
            }
        }
    }

    static constexpr const char* precomputationProgramsVertShaderSrc=1+R"(
#version 330
in vec3 vertex;
out vec3 position;
void main()
{
    position=vertex;
    gl_Position=vec4(position,1);
}
)";
    QOpenGLShader precomputationProgramsVertShader(QOpenGLShader::Vertex);
    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else
    {
        if(!precomputationProgramsVertShader.compileSourceCode(precomputationProgramsVertShaderSrc))
            throw DataLoadError{QObject::tr("Failed to compile vertex shader for on-the-fly precomputation of eclipsed scattering:\n%2")
                                    .arg(precomputationProgramsVertShader.log())};
        tick(++loadingStepsDone_);
    }

    eclipsedSingleScatteringPrecomputationPrograms_=std::make_unique<ScatteringProgramsMap>();
    for(const auto& scatterer : params_.scatterers)
    {
        auto& programs=(*eclipsedSingleScatteringPrecomputationPrograms_)[scatterer.name];
        for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
        {
            if(countStepsOnly)
            {
                ++totalLoadingStepsToDo_;
                continue;
            }

            const auto scatDir=QString("%1/shaders/single-scattering-eclipsed/precomputation/%3/%4").arg(pathToData_)
                                                                                                    .arg(wlSetIndex)
                                                                                                    .arg(scatterer.name);
            qDebug().nospace() << "Loading shaders from " << scatDir << "...";
            auto& program=*programs.emplace_back(std::make_unique<QOpenGLShaderProgram>());

            for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
                addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

            program.addShader(&precomputationProgramsVertShader);

            link(program, tr("shader program for scatterer \"%1\"").arg(scatterer.name));
            tick(++loadingStepsDone_);
        }
    }

    // Precomputed rendering (with approximate mixing, since textures contain only the data for fully-centered eclipse)
    eclipsedDoubleScatteringPrecomputedPrograms_.clear();
    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
            continue;
        }

        const auto scatDir=QString("%1/shaders/double-scattering-eclipsed/precomputed/%2").arg(pathToData_).arg(wlSetIndex);
        qDebug().nospace() << "Loading shaders from " << scatDir << "...";
        auto& program=*eclipsedDoubleScatteringPrecomputedPrograms_.emplace_back(std::make_unique<QOpenGLShaderProgram>());

        for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
            addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

        program.addShader(&viewDirFragShader);
        program.addShader(&viewDirVertShader);
        for(const auto& b : viewDirBindAttribLocations_)
            program.bindAttributeLocation(b.first.c_str(), b.second);

        link(program, tr("precomputed eclipsed double scattering shader program"));
        tick(++loadingStepsDone_);
    }

    // Rendering with on-the-fly precomputation, useful as a reference on slower machines, and as the production mode on very fast ones
    eclipsedDoubleScatteringPrecomputationPrograms_.clear();
    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
            continue;
        }

        const auto scatDir=QString("%1/shaders/double-scattering-eclipsed/precomputation/%2").arg(pathToData_).arg(wlSetIndex);
        qDebug().nospace() << "Loading shaders from " << scatDir << "...";
        auto& program=*eclipsedDoubleScatteringPrecomputationPrograms_.emplace_back(std::make_unique<QOpenGLShaderProgram>());

        for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
            addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

        program.addShader(&precomputationProgramsVertShader);

        link(program, tr("on-the-fly eclipsed double scattering shader program"));
        tick(++loadingStepsDone_);
    }

    multipleScatteringPrograms_.clear();
    if(QFile::exists(pathToData_+"/shaders/multiple-scattering/0/"))
    {
        for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
        {
            if(countStepsOnly)
            {
                ++totalLoadingStepsToDo_;
                continue;
            }

            auto& program=*multipleScatteringPrograms_.emplace_back(std::make_unique<QOpenGLShaderProgram>());
            const auto wlDir=QString("%1/shaders/multiple-scattering/%2").arg(pathToData_).arg(wlSetIndex);
            qDebug().nospace() << "Loading shaders from " << wlDir << "...";
            for(const auto& shaderFile : fs::directory_iterator(fs::u8path(wlDir.toStdString())))
                addShaderFile(program, QOpenGLShader::Fragment, shaderFile.path());
            program.addShader(&viewDirFragShader);
            program.addShader(&viewDirVertShader);
            for(const auto& b : viewDirBindAttribLocations_)
                program.bindAttributeLocation(b.first.c_str(), b.second);
            link(program, tr("multiple scattering shader program"));
            tick(++loadingStepsDone_);
        }
    }
    else
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
        }
        else
        {
            auto& program=*multipleScatteringPrograms_.emplace_back(std::make_unique<QOpenGLShaderProgram>());
            const auto wlDir=pathToData_+"/shaders/multiple-scattering/";
            qDebug().nospace() << "Loading shaders from " << wlDir << "...";
            for(const auto& shaderFile : fs::directory_iterator(fs::u8path(wlDir.toStdString())))
                addShaderFile(program, QOpenGLShader::Fragment, shaderFile.path());
            program.addShader(&viewDirFragShader);
            program.addShader(&viewDirVertShader);
            for(const auto& b : viewDirBindAttribLocations_)
                program.bindAttributeLocation(b.first.c_str(), b.second);
            link(program, tr("multiple scattering shader program"));
            tick(++loadingStepsDone_);
        }
    }

    zeroOrderScatteringPrograms_.clear();
    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
            continue;
        }

        auto& program=*zeroOrderScatteringPrograms_.emplace_back(std::make_unique<QOpenGLShaderProgram>());
        const auto wlDir=QString("%1/shaders/zero-order-scattering/%2").arg(pathToData_).arg(wlSetIndex);
        qDebug().nospace() << "Loading shaders from " << wlDir << "...";
        for(const auto& shaderFile : fs::directory_iterator(fs::u8path(wlDir.toStdString())))
            addShaderFile(program, QOpenGLShader::Fragment, shaderFile.path());
        program.addShader(&viewDirFragShader);
        program.addShader(&viewDirVertShader);
        for(const auto& b : viewDirBindAttribLocations_)
            program.bindAttributeLocation(b.first.c_str(), b.second);
        link(program, tr("zero-order scattering shader program"));
        tick(++loadingStepsDone_);
    }

    eclipsedZeroOrderScatteringPrograms_.clear();
    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
            continue;
        }

        auto& program=*eclipsedZeroOrderScatteringPrograms_.emplace_back(std::make_unique<QOpenGLShaderProgram>());
        const auto wlDir=QString("%1/shaders/eclipsed-zero-order-scattering/%2").arg(pathToData_).arg(wlSetIndex);
        qDebug().nospace() << "Loading shaders from " << wlDir << "...";
        for(const auto& shaderFile : fs::directory_iterator(fs::u8path(wlDir.toStdString())))
            addShaderFile(program, QOpenGLShader::Fragment, shaderFile.path());
        program.addShader(&viewDirFragShader);
        program.addShader(&viewDirVertShader);
        for(const auto& b : viewDirBindAttribLocations_)
            program.bindAttributeLocation(b.first.c_str(), b.second);
        link(program, tr("eclipsed zero-order scattering shader program"));
        tick(++loadingStepsDone_);
    }

    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else
    {
        viewDirectionGetterProgram_=std::make_unique<QOpenGLShaderProgram>();
        auto& program=*viewDirectionGetterProgram_;
        program.addShader(&viewDirFragShader);
        program.addShader(&viewDirVertShader);
        for(const auto& b : viewDirBindAttribLocations_)
            program.bindAttributeLocation(b.first.c_str(), b.second);
        addShaderCode(program, QOpenGLShader::Fragment, tr("fragment shader for view direction getter"), 1+R"(
#version 330

in vec3 position;
out vec3 viewDir;

vec3 calcViewDir();
void main()
{
    viewDir=calcViewDir();
}
)");
        link(program, tr("view direction getter shader program"));
        tick(++loadingStepsDone_);
    }

    lightPollutionPrograms_.clear();
    if(QFile::exists(pathToData_+"/shaders/light-pollution/0/"))
    {
        for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
        {
            if(countStepsOnly)
            {
                ++totalLoadingStepsToDo_;
                continue;
            }

            auto& program=*lightPollutionPrograms_.emplace_back(std::make_unique<QOpenGLShaderProgram>());
            const auto wlDir=QString("%1/shaders/light-pollution/%2").arg(pathToData_).arg(wlSetIndex);
            qDebug().nospace() << "Loading shaders from " << wlDir << "...";
            for(const auto& shaderFile : fs::directory_iterator(fs::u8path(wlDir.toStdString())))
                addShaderFile(program, QOpenGLShader::Fragment, shaderFile.path());
            program.addShader(&viewDirFragShader);
            program.addShader(&viewDirVertShader);
            for(const auto& b : viewDirBindAttribLocations_)
                program.bindAttributeLocation(b.first.c_str(), b.second);
            link(program, tr("light pollution shader program"));
            tick(++loadingStepsDone_);
        }
    }
    else
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
        }
        else
        {
            auto& program=*lightPollutionPrograms_.emplace_back(std::make_unique<QOpenGLShaderProgram>());
            const auto wlDir=pathToData_+"/shaders/light-pollution/";
            qDebug().nospace() << "Loading shaders from " << wlDir << "...";
            for(const auto& shaderFile : fs::directory_iterator(fs::u8path(wlDir.toStdString())))
                addShaderFile(program, QOpenGLShader::Fragment, shaderFile.path());
            program.addShader(&viewDirFragShader);
            program.addShader(&viewDirVertShader);
            for(const auto& b : viewDirBindAttribLocations_)
                program.bindAttributeLocation(b.first.c_str(), b.second);
            link(program, tr("light pollution shader program"));
            tick(++loadingStepsDone_);
        }
    }
}

void AtmosphereRenderer::setupBuffers()
{
    OGL_TRACE();

    gl.glGenVertexArrays(1, &vao_);
    gl.glBindVertexArray(vao_);
    gl.glGenBuffers(1, &vbo_);
    gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    const GLfloat vertices[]=
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

glm::dvec3 AtmosphereRenderer::cameraPosition() const
{
    return glm::dvec3(0,0,tools_->altitude());
}

glm::dvec3 AtmosphereRenderer::sunDirection() const
{
    return glm::dvec3(std::cos(tools_->sunAzimuth())*std::sin(tools_->sunZenithAngle()),
                      std::sin(tools_->sunAzimuth())*std::sin(tools_->sunZenithAngle()),
                      std::cos(tools_->sunZenithAngle()));
}

glm::dvec3 AtmosphereRenderer::moonPosition() const
{
    const auto moonDir=glm::dvec3(std::cos(tools_->moonAzimuth())*std::sin(tools_->moonZenithAngle()),
                                  std::sin(tools_->moonAzimuth())*std::sin(tools_->moonZenithAngle()),
                                  std::cos(tools_->moonZenithAngle()));
    return cameraPosition()+moonDir*cameraMoonDistance();
}

glm::dvec3 AtmosphereRenderer::moonPositionRelativeToSunAzimuth() const
{
    const auto moonDir=glm::dvec3(std::cos(tools_->moonAzimuth() - tools_->sunAzimuth())*std::sin(tools_->moonZenithAngle()),
                                  std::sin(tools_->moonAzimuth() - tools_->sunAzimuth())*std::sin(tools_->moonZenithAngle()),
                                  std::cos(tools_->moonZenithAngle()));
    return cameraPosition()+moonDir*cameraMoonDistance();
}

double AtmosphereRenderer::cameraMoonDistance() const
{
    using namespace std;
    const auto hpR=tools_->altitude()+params_.earthRadius;
    const auto moonElevation=M_PI/2-tools_->moonZenithAngle();
    return -hpR*sin(moonElevation)+sqrt(sqr(params_.earthMoonDistance)-0.5*sqr(hpR)*(1+cos(2*moonElevation)));
}

double AtmosphereRenderer::moonAngularRadius() const
{
    return moonRadius/cameraMoonDistance();
}

QVector4D AtmosphereRenderer::getPixelLuminance(QPoint const& pixelPos)
{
    GLint origFBO=-1;
    gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &origFBO);

    gl.glBindFramebuffer(GL_FRAMEBUFFER, luminanceRadianceFBO_);
    gl.glReadBuffer(GL_COLOR_ATTACHMENT0);
    glm::vec4 pixel;
    gl.glReadPixels(pixelPos.x(), viewportSize_.height()-pixelPos.y()-1, 1,1, GL_RGBA, GL_FLOAT, &pixel[0]);

    gl.glBindFramebuffer(GL_FRAMEBUFFER, origFBO);

    return toQVector(pixel);
}

auto AtmosphereRenderer::getPixelSpectralRadiance(QPoint const& pixelPos) -> SpectralRadiance
{
    if(radianceRenderBuffers_.empty()) return {};
    if(pixelPos.x()<0 || pixelPos.y()<0 || pixelPos.x()>=viewportSize_.width() || pixelPos.y()>=viewportSize_.height())
        return {};

    constexpr unsigned wavelengthsPerPixel=4;
    SpectralRadiance output;
    for(const auto wlSet : params_.allWavelengths)
        for(unsigned i=0; i<wavelengthsPerPixel; ++i)
            output.wavelengths.emplace_back(wlSet[i]);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, luminanceRadianceFBO_);
    gl.glReadBuffer(GL_COLOR_ATTACHMENT1);
    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, radianceRenderBuffers_[wlSetIndex]);
        GLfloat data[wavelengthsPerPixel]={NAN,NAN,NAN,NAN};
        gl.glReadPixels(pixelPos.x(), viewportSize_.height()-pixelPos.y()-1, 1,1, GL_RGBA, GL_FLOAT, data);
        for(unsigned i=0; i<wavelengthsPerPixel; ++i)
            output.radiances.emplace_back(data[i]);
    }
    assert(output.wavelengths.size()==output.radiances.size());

    const auto dir=getViewDirection(pixelPos);
    output.azimuth=dir.azimuth;
    output.elevation=dir.elevation;

    return output;
}

std::vector<float> AtmosphereRenderer::getWavelengths()
{
    constexpr unsigned wavelengthsPerPixel=4;
    std::vector<float> wavelengths;
    for(const auto wlSet : params_.allWavelengths)
        for(unsigned i=0; i<wavelengthsPerPixel; ++i)
            wavelengths.emplace_back(wlSet[i]);
    return wavelengths;
}

void AtmosphereRenderer::setSolarSpectrum(std::vector<float> const& solarIrradianceAtTOA)
{
    solarIrradianceFixup_.clear();
    for(unsigned n=0; n<solarIrradianceAtTOA.size()/4; ++n)
    {
        const auto newIrrad = QVector4D(solarIrradianceAtTOA[4*n+0],solarIrradianceAtTOA[4*n+1],
                                        solarIrradianceAtTOA[4*n+2],solarIrradianceAtTOA[4*n+3]);
        const auto origIrrad = toQVector(params_.solarIrradianceAtTOA[n]);
        solarIrradianceFixup_.emplace_back(newIrrad/origIrrad);
    }
}

void AtmosphereRenderer::resetSolarSpectrum()
{
    // Simple clear() won't work because we want to reset the uniform in the programs where it's been already altered
    std::fill(solarIrradianceFixup_.begin(), solarIrradianceFixup_.end(), QVector4D(1,1,1,1));
}

auto AtmosphereRenderer::getViewDirection(QPoint const& pixelPos) -> Direction
{
    viewDirectionGetterProgram_->bind();
    gl.glBindFramebuffer(GL_FRAMEBUFFER, viewDirectionFBO_);
    drawSurface(*viewDirectionGetterProgram_);
    GLfloat viewDir[3]={NAN,NAN,NAN};
    gl.glReadPixels(pixelPos.x(), viewportSize_.height()-pixelPos.y()-1, 1,1, GL_RGB, GL_FLOAT, viewDir);

    const float azimuth = 180/M_PI * (viewDir[0]!=0 || viewDir[1]!=0 ? std::atan2(viewDir[1], viewDir[0]) : 0);
    const float elevation = 180/M_PI * std::asin(viewDir[2]);

    return Direction{azimuth, elevation};
}

void AtmosphereRenderer::prepareRadianceFrames(const bool clear)
{
    if(radianceRenderBuffers_.empty()) return;

    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, radianceRenderBuffers_[wlSetIndex]);
        gl.glDrawBuffers(2, std::array<GLenum,2>{GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1}.data());
        if(clear)
            gl.glClearBufferfv(GL_COLOR, 1, std::array<GLfloat,4>{0,0,0,0}.data());
    }
}

bool AtmosphereRenderer::canGrabRadiance() const
{
    const auto& sst=singleScatteringTextures_;
    const bool haveNoLuminanceOnlySingleScatteringTextures = std::find_if(sst.begin(), sst.end(), [=](auto const& texSet)
                                                                          { return texSet.second.size()==1; }) == sst.end();
    return haveNoLuminanceOnlySingleScatteringTextures && multipleScatteringTextures_.size()==params_.allWavelengths.size();
}

bool AtmosphereRenderer::canSetSolarSpectrum() const
{
    return canGrabRadiance(); // condition is the same as for radiance grabbing
}

void AtmosphereRenderer::renderZeroOrderScattering()
{
    OGL_TRACE();
    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        if(!radianceRenderBuffers_.empty())
            gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, radianceRenderBuffers_[wlSetIndex]);
        if(tools_->usingEclipseShader())
        {
            auto& prog=*eclipsedZeroOrderScatteringPrograms_[wlSetIndex];
            prog.bind();
            prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
            prog.setUniformValue("moonAngularRadius", float(moonAngularRadius()));
            prog.setUniformValue("moonPosition", toQVector(moonPosition()));
            prog.setUniformValue("sunDirection", toQVector(sunDirection()));
            transmittanceTextures_[wlSetIndex]->bind(0);
            prog.setUniformValue("transmittanceTexture", 0);
            prog.setUniformValue("lightPollutionGroundLuminance", float(tools_->lightPollutionGroundLuminance()));
            if(!solarIrradianceFixup_.empty())
                prog.setUniformValue("solarIrradianceFixup", solarIrradianceFixup_[wlSetIndex]);
            drawSurface(prog);
        }
        else
        {
            auto& prog=*zeroOrderScatteringPrograms_[wlSetIndex];
            prog.bind();
            prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
            prog.setUniformValue("sunDirection", toQVector(sunDirection()));
            transmittanceTextures_[wlSetIndex]->bind(0);
            prog.setUniformValue("transmittanceTexture", 0);
            irradianceTextures_[wlSetIndex]->bind(1);
            prog.setUniformValue("irradianceTexture",1);
            prog.setUniformValue("lightPollutionGroundLuminance", float(tools_->lightPollutionGroundLuminance()));
            if(!solarIrradianceFixup_.empty())
                prog.setUniformValue("solarIrradianceFixup", solarIrradianceFixup_[wlSetIndex]);
            drawSurface(prog);
        }
    }
}


void AtmosphereRenderer::precomputeEclipsedSingleScattering()
{
    OGL_TRACE();

    gl.glBindVertexArray(vao_);
    // TODO: avoid redoing it if Sun elevation and Moon elevation and relative azimuth haven't changed
    for(const auto& scatterer : params_.scatterers)
    {
        auto& textures=eclipsedSingleScatteringPrecomputationTextures_[scatterer.name];
        const auto& programs=eclipsedSingleScatteringPrecomputationPrograms_->at(scatterer.name);
        gl.glDisablei(GL_BLEND, 0); // First wavelength set overwrites old contents, regardless of subsequent blending modes
        const bool needBlending = scatterer.phaseFunctionType==PhaseFunctionType::Achromatic || scatterer.phaseFunctionType==PhaseFunctionType::Smooth;
        for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
        {
            auto& prog=*programs[wlSetIndex];
            prog.bind();
            prog.setUniformValue("altitude", float(tools_->altitude()));
            prog.setUniformValue("moonAngularRadius", float(moonAngularRadius()));
            prog.setUniformValue("moonPositionRelativeToSunAzimuth", toQVector(moonPositionRelativeToSunAzimuth()));
            prog.setUniformValue("sunZenithAngle", float(tools_->sunZenithAngle()));
            transmittanceTextures_[wlSetIndex]->bind(0);
            prog.setUniformValue("transmittanceTexture", 0);
            if(!solarIrradianceFixup_.empty())
                prog.setUniformValue("solarIrradianceFixup", solarIrradianceFixup_[wlSetIndex]);

            auto& tex = needBlending ? *textures.front() : *textures[wlSetIndex];
            gl.glBindFramebuffer(GL_FRAMEBUFFER, eclipseSingleScatteringPrecomputationFBO_);
            gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,tex.textureId(),0);
            checkFramebufferStatus(gl, "Eclipsed single scattering precomputation FBO");
            gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            if(needBlending)
                gl.glEnablei(GL_BLEND, 0);
        }
    }
    gl.glBindVertexArray(0);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,luminanceRadianceFBO_);
    gl.glEnablei(GL_BLEND, 0);
}

void AtmosphereRenderer::renderSingleScattering()
{
    OGL_TRACE();

    if(tools_->usingEclipseShader())
        precomputeEclipsedSingleScattering();

    const auto renderMode = tools_->onTheFlySingleScatteringEnabled() ? SSRM_ON_THE_FLY : SSRM_PRECOMPUTED;
    for(const auto& scatterer : params_.scatterers)
    {
        if(!scatterersEnabledStates_.at(scatterer.name))
            continue;

        if(renderMode==SSRM_ON_THE_FLY)
        {
            if(tools_->usingEclipseShader())
            {
                for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
                {
                    if(!radianceRenderBuffers_.empty())
                        gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, radianceRenderBuffers_[wlSetIndex]);

                    auto& prog=*eclipsedSingleScatteringPrograms_[renderMode]->at(scatterer.name)[wlSetIndex];
                    prog.bind();
                    prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
                    prog.setUniformValue("moonAngularRadius", float(moonAngularRadius()));
                    prog.setUniformValue("moonPosition", toQVector(moonPosition()));
                    prog.setUniformValue("sunDirection", toQVector(sunDirection()));
                    transmittanceTextures_[wlSetIndex]->bind(0);
                    prog.setUniformValue("transmittanceTexture", 0);
                    if(!solarIrradianceFixup_.empty())
                        prog.setUniformValue("solarIrradianceFixup", solarIrradianceFixup_[wlSetIndex]);

                    drawSurface(prog);
                }
            }
            else
            {
                if(scatterer.phaseFunctionType==PhaseFunctionType::Smooth)
                    continue;

                for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
                {
                    if(!radianceRenderBuffers_.empty())
                        gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, radianceRenderBuffers_[wlSetIndex]);

                    auto& prog=*singleScatteringPrograms_[renderMode]->at(scatterer.name)[wlSetIndex];
                    prog.bind();
                    prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
                    prog.setUniformValue("sunDirection", toQVector(sunDirection()));
                    transmittanceTextures_[wlSetIndex]->bind(0);
                    prog.setUniformValue("transmittanceTexture", 0);
                    if(!solarIrradianceFixup_.empty())
                        prog.setUniformValue("solarIrradianceFixup", solarIrradianceFixup_[wlSetIndex]);

                    drawSurface(prog);
                }
            }
        }
        else if(scatterer.phaseFunctionType==PhaseFunctionType::General)
        {
            if(tools_->usingEclipseShader())
            {
                for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
                {
                    if(!radianceRenderBuffers_.empty())
                        gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, radianceRenderBuffers_[wlSetIndex]);

                    auto& prog=*eclipsedSingleScatteringPrograms_[renderMode]->at(scatterer.name)[wlSetIndex];
                    prog.bind();
                    prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
                    prog.setUniformValue("sunDirection", toQVector(sunDirection()));
                    {
                        auto& tex=*eclipsedSingleScatteringPrecomputationTextures_.at(scatterer.name)[wlSetIndex];
                        const auto texFilter = tools_->textureFilteringEnabled() ? QOpenGLTexture::Linear : QOpenGLTexture::Nearest;
                        tex.setMinificationFilter(texFilter);
                        tex.setMagnificationFilter(texFilter);
                        tex.bind(0);
                        prog.setUniformValue("eclipsedScatteringTexture", 0);
                    }
                    if(!solarIrradianceFixup_.empty())
                        prog.setUniformValue("solarIrradianceFixup", solarIrradianceFixup_[wlSetIndex]);

                    drawSurface(prog);
                }
            }
            else
            {
                for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
                {
                    if(!radianceRenderBuffers_.empty())
                        gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, radianceRenderBuffers_[wlSetIndex]);

                    auto& prog=*singleScatteringPrograms_[renderMode]->at(scatterer.name)[wlSetIndex];
                    prog.bind();
                    prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
                    prog.setUniformValue("sunDirection", toQVector(sunDirection()));
                    {
                        auto& tex=*singleScatteringTextures_.at(scatterer.name)[wlSetIndex];
                        const auto texFilter = tools_->textureFilteringEnabled() ? QOpenGLTexture::Linear : QOpenGLTexture::Nearest;
                        tex.setMinificationFilter(texFilter);
                        tex.setMagnificationFilter(texFilter);
                        tex.bind(0);
                        prog.setUniformValue("scatteringTexture", 0);
                        prog.setUniformValue("staticAltitudeTexCoord", staticAltitudeTexCoord_);
                    }
                    if(!solarIrradianceFixup_.empty())
                        prog.setUniformValue("solarIrradianceFixup", solarIrradianceFixup_[wlSetIndex]);

                    drawSurface(prog);
                }
            }
        }
        else if(scatterer.phaseFunctionType==PhaseFunctionType::Achromatic && !tools_->usingEclipseShader())
        {
            auto& prog=*singleScatteringPrograms_[renderMode]->at(scatterer.name).front();
            prog.bind();
            prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
            prog.setUniformValue("sunDirection", toQVector(sunDirection()));
            {
                auto& tex=*singleScatteringTextures_.at(scatterer.name).front();
                const auto texFilter = tools_->textureFilteringEnabled() ? QOpenGLTexture::Linear : QOpenGLTexture::Nearest;
                tex.setMinificationFilter(texFilter);
                tex.setMagnificationFilter(texFilter);
                tex.bind(0);
            }
            prog.setUniformValue("scatteringTexture", 0);
            prog.setUniformValue("staticAltitudeTexCoord", staticAltitudeTexCoord_);

            drawSurface(prog);
        }
        else if(tools_->usingEclipseShader())
        {
            auto& prog=*eclipsedSingleScatteringPrograms_[renderMode]->at(scatterer.name).front();
            prog.bind();
            prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
            prog.setUniformValue("sunDirection", toQVector(sunDirection()));
            {
                auto& tex=*eclipsedSingleScatteringPrecomputationTextures_.at(scatterer.name).front();
                const auto texFilter = tools_->textureFilteringEnabled() ? QOpenGLTexture::Linear : QOpenGLTexture::Nearest;
                tex.setMinificationFilter(texFilter);
                tex.setMagnificationFilter(texFilter);
                tex.bind(0);
                prog.setUniformValue("eclipsedScatteringTexture", 0);
            }

            drawSurface(prog);
        }
    }
}

void AtmosphereRenderer::precomputeEclipsedDoubleScattering()
{
    // TODO: avoid redoing it if Sun elevation and Moon elevation and relative azimuth haven't changed

    gl.glBindFramebuffer(GL_FRAMEBUFFER, eclipseDoubleScatteringPrecomputationFBO_);
    gl.glDisablei(GL_BLEND, 0);
    gl.glBindVertexArray(vao_);
    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        auto& prog=*eclipsedDoubleScatteringPrecomputationPrograms_[wlSetIndex];
        prog.bind();
        int unusedTextureUnitNum=0;
        transmittanceTextures_[wlSetIndex]->bind(unusedTextureUnitNum);
        prog.setUniformValue("transmittanceTexture", unusedTextureUnitNum++);
        if(!solarIrradianceFixup_.empty())
            prog.setUniformValue("solarIrradianceFixup", solarIrradianceFixup_[wlSetIndex]);

        EclipsedDoubleScatteringPrecomputer precomputer(prog, gl, eclipsedDoubleScatteringPrecomputationScratchTexture_->textureId(),
                                                        unusedTextureUnitNum, params_,
                                                        params_.eclipsedDoubleScatteringTextureSize[0],
                                                        params_.eclipsedDoubleScatteringTextureSize[1], 1, 1);
        precomputer.compute(0, 0, tools_->altitude(), tools_->sunZenithAngle(),
                            tools_->moonZenithAngle(), tools_->moonAzimuth() - tools_->sunAzimuth());
        eclipsedDoubleScatteringPrecomputationTargetTextures_[wlSetIndex]->bind();
        gl.glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA32F,
                        params_.eclipsedDoubleScatteringTextureSize[0], params_.eclipsedDoubleScatteringTextureSize[1], 1,
                        0,GL_RGBA,GL_FLOAT,precomputer.texture().data());
    }
    gl.glBindVertexArray(0);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,luminanceRadianceFBO_);
    gl.glEnablei(GL_BLEND, 0);
}

void AtmosphereRenderer::renderMultipleScattering()
{
    OGL_TRACE();

    const auto texFilter = tools_->textureFilteringEnabled() ? QOpenGLTexture::Linear : QOpenGLTexture::Nearest;
    if(tools_->usingEclipseShader())
    {
        if(tools_->onTheFlyPrecompDoubleScatteringEnabled())
            precomputeEclipsedDoubleScattering();
        for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
        {
            if(!radianceRenderBuffers_.empty())
                gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, radianceRenderBuffers_[wlSetIndex]);

            auto& prog=*eclipsedDoubleScatteringPrecomputedPrograms_[wlSetIndex];
            prog.bind();
            prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
            prog.setUniformValue("sunDirection", toQVector(sunDirection()));
            if(!solarIrradianceFixup_.empty())
                prog.setUniformValue("solarIrradianceFixup", solarIrradianceFixup_[wlSetIndex]);

            if(tools_->onTheFlyPrecompDoubleScatteringEnabled())
            {
                // The same texture is used for upper and lower slices
                auto& tex=*eclipsedDoubleScatteringPrecomputationTargetTextures_[wlSetIndex];
                tex.setMinificationFilter(texFilter);
                tex.setMagnificationFilter(texFilter);
                tex.bind(0);
                prog.setUniformValue("eclipsedDoubleScatteringTextureLower", 0);
                prog.setUniformValue("eclipsedDoubleScatteringTextureUpper", 0);
                prog.setUniformValue("eclipsedDoubleScatteringAltitudeAlphaUpper", 0.f);
                prog.setUniformValue("eclipsedDoubleScatteringTextureSize", QVector3D(params_.eclipsedDoubleScatteringTextureSize[0],
                                                                                      params_.eclipsedDoubleScatteringTextureSize[1], 1));
            }
            else
            {
                assert(!params_.noEclipsedDoubleScatteringTextures);

                auto& texLower=*eclipsedDoubleScatteringTexturesLower_[wlSetIndex];
                texLower.setMinificationFilter(texFilter);
                texLower.setMagnificationFilter(texFilter);
                texLower.bind(0);
                prog.setUniformValue("eclipsedDoubleScatteringTextureLower", 0);

                auto& texUpper=*eclipsedDoubleScatteringTexturesUpper_[wlSetIndex];
                texUpper.setMinificationFilter(texFilter);
                texUpper.setMagnificationFilter(texFilter);
                texUpper.bind(1);
                prog.setUniformValue("eclipsedDoubleScatteringTextureUpper", 1);

                prog.setUniformValue("eclipsedDoubleScatteringAltitudeAlphaUpper", eclipsedDoubleScatteringAltitudeAlphaUpper_);
                prog.setUniformValue("eclipsedDoubleScatteringTextureSize", toQVector(glm::vec3(params_.eclipsedDoubleScatteringTextureSize)));
            }
            drawSurface(prog);
        }
    }
    else
    {
        if(multipleScatteringTextures_.size()==1)
        {
            auto& prog=*multipleScatteringPrograms_.front();
            prog.bind();
            prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
            prog.setUniformValue("sunDirection", toQVector(sunDirection()));

            auto& tex=*multipleScatteringTextures_.front();
            tex.setMinificationFilter(texFilter);
            tex.setMagnificationFilter(texFilter);
            tex.bind(0);
            prog.setUniformValue("scatteringTexture", 0);
            prog.setUniformValue("staticAltitudeTexCoord", staticAltitudeTexCoord_);
            drawSurface(prog);
        }
        else
        {
            for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
            {
                if(!radianceRenderBuffers_.empty())
                    gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, radianceRenderBuffers_[wlSetIndex]);

                auto& prog=*multipleScatteringPrograms_[wlSetIndex];
                prog.bind();
                prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
                prog.setUniformValue("sunDirection", toQVector(sunDirection()));
                if(!solarIrradianceFixup_.empty())
                    prog.setUniformValue("solarIrradianceFixup", solarIrradianceFixup_[wlSetIndex]);

                auto& tex=*multipleScatteringTextures_[wlSetIndex];
                tex.setMinificationFilter(texFilter);
                tex.setMagnificationFilter(texFilter);
                tex.bind(0);
                prog.setUniformValue("scatteringTexture", 0);
                prog.setUniformValue("staticAltitudeTexCoord", staticAltitudeTexCoord_);
                drawSurface(prog);
            }
        }
    }
}

void AtmosphereRenderer::renderLightPollution()
{
    OGL_TRACE();

    const auto texFilter = tools_->textureFilteringEnabled() ? QOpenGLTexture::Linear : QOpenGLTexture::Nearest;

    if(lightPollutionPrograms_.size()==1)
    {
        auto& prog=*lightPollutionPrograms_.front();
        prog.bind();
        prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
        prog.setUniformValue("sunDirection", toQVector(sunDirection()));

        auto& tex=*lightPollutionTextures_.front();
        tex.setMinificationFilter(texFilter);
        tex.setMagnificationFilter(texFilter);
        tex.bind(0);
        prog.setUniformValue("lightPollutionScatteringTexture", 0);
        prog.setUniformValue("lightPollutionGroundLuminance", float(tools_->lightPollutionGroundLuminance()));
        drawSurface(prog);
    }
    else
    {
        for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
        {
            if(!radianceRenderBuffers_.empty())
                gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, radianceRenderBuffers_[wlSetIndex]);

            auto& prog=*lightPollutionPrograms_[wlSetIndex];
            prog.bind();
            prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
            prog.setUniformValue("sunDirection", toQVector(sunDirection()));
            if(!solarIrradianceFixup_.empty())
                prog.setUniformValue("solarIrradianceFixup", solarIrradianceFixup_[wlSetIndex]);

            auto& tex=*lightPollutionTextures_[wlSetIndex];
            tex.setMinificationFilter(texFilter);
            tex.setMagnificationFilter(texFilter);
            tex.bind(0);
            prog.setUniformValue("lightPollutionScatteringTexture", 0);
            prog.setUniformValue("lightPollutionGroundLuminance", float(tools_->lightPollutionGroundLuminance()));
            drawSurface(prog);
        }
    }
}

void AtmosphereRenderer::draw(const double brightness, const bool clear)
{
    OGL_TRACE();

    if(!readyToRender_) return;
    // Don't try to draw while we're still loading something. We can come here in
    // this state when e.g. progress reporting code results in a resize event.
    if(totalLoadingStepsToDo_!=0) return;

    const auto altCoord=altitudeUnitRangeTexCoord();
    if(altCoord < loadedAltitudeURTexCoordRange_[0] || altCoord > loadedAltitudeURTexCoordRange_[1])
    {
        [[maybe_unused]] OGLTrace t("reloading textures");

        currentActivity_=tr("Reloading textures due to altitude getting out of the currently loaded layers...");
        totalLoadingStepsToDo_=0;
        reloadScatteringTextures(CountStepsOnly{true});
        loadingStepsDone_=0;
        reloadScatteringTextures(CountStepsOnly{false});
        reportLoadingFinished();
    }
    else
    {
        if(!params_.noEclipsedDoubleScatteringTextures)
            assert(numAltIntervalsInEclipsed4DTexture_==numAltIntervalsIn4DTexture_); // if we want to support them being different, then altCoord must be calculated separately
        updateAltitudeTexCoords(altCoord);
        updateEclipsedAltitudeTexCoords(altCoord);
    }

    oglDebugMessageInsert("AtmosphereRenderer::draw() begins drawing");

    GLint targetFBO=-1;
    gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &targetFBO);

    {
        gl.glBindFramebuffer(GL_FRAMEBUFFER,luminanceRadianceFBO_);
        if(canGrabRadiance())
        {
            prepareRadianceFrames(clear);
            gl.glEnablei(GL_BLEND, 1);
        }
        if(clear)
        {
            gl.glClearColor(0,0,0,0);
            gl.glClear(GL_COLOR_BUFFER_BIT);
        }
        gl.glEnablei(GL_BLEND, 0);
        {
            gl.glBlendFunc(GL_CONSTANT_COLOR, GL_ONE);
            gl.glBlendColor(brightness, brightness, brightness, brightness);
            if(tools_->zeroOrderScatteringEnabled())
                renderZeroOrderScattering();
            if(tools_->singleScatteringEnabled())
                renderSingleScattering();
            if(tools_->multipleScatteringEnabled())
                renderMultipleScattering();
            if(tools_->lightPollutionGroundLuminance())
                renderLightPollution();
        }
        gl.glDisablei(GL_BLEND, 0);

        gl.glBindFramebuffer(GL_FRAMEBUFFER,targetFBO);
    }
}

void AtmosphereRenderer::setupRenderTarget()
{
    OGL_TRACE();

    gl.glGenFramebuffers(1,&luminanceRadianceFBO_);
    luminanceRenderTargetTexture_.setMinificationFilter(QOpenGLTexture::Nearest);
    luminanceRenderTargetTexture_.setMagnificationFilter(QOpenGLTexture::Nearest);
    luminanceRenderTargetTexture_.setWrapMode(QOpenGLTexture::ClampToEdge);

    if(canGrabRadiance())
    {
        assert(radianceRenderBuffers_.empty());
        radianceRenderBuffers_.resize(params_.allWavelengths.size());
        gl.glGenRenderbuffers(radianceRenderBuffers_.size(), radianceRenderBuffers_.data());

        gl.glGenFramebuffers(1, &viewDirectionFBO_);
        gl.glGenRenderbuffers(1, &viewDirectionRenderBuffer_);
        gl.glBindFramebuffer(GL_FRAMEBUFFER, viewDirectionFBO_);
        gl.glBindRenderbuffer(GL_RENDERBUFFER, viewDirectionRenderBuffer_);
        gl.glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, 1, 1); // dummy size just to initialize the renderbuffer
        gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, viewDirectionRenderBuffer_);
        gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        gl.glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    gl.glGenFramebuffers(1,&eclipseSingleScatteringPrecomputationFBO_);
    eclipsedSingleScatteringPrecomputationTextures_.clear();
    for(const auto& scatterer : params_.scatterers)
    {
        auto& textures=eclipsedSingleScatteringPrecomputationTextures_[scatterer.name];
        for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
        {
            auto& tex=*textures.emplace_back(newTex(QOpenGLTexture::Target2D));
            tex.setMinificationFilter(QOpenGLTexture::Linear);
            tex.setMagnificationFilter(QOpenGLTexture::Linear);
            // relative azimuth
            tex.setWrapMode(QOpenGLTexture::DirectionS, QOpenGLTexture::Repeat);
            // cosVZA
            tex.setWrapMode(QOpenGLTexture::DirectionT, QOpenGLTexture::ClampToEdge);
            tex.bind();
            const auto width=params_.eclipsedSingleScatteringTextureSize[0];
            const auto height=params_.eclipsedSingleScatteringTextureSize[1];
            gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);

            if(scatterer.phaseFunctionType==PhaseFunctionType::Achromatic || scatterer.phaseFunctionType==PhaseFunctionType::Smooth)
                break;
        }
    }

    gl.glGenFramebuffers(1,&eclipseDoubleScatteringPrecomputationFBO_);
    eclipsedDoubleScatteringPrecomputationScratchTexture_=newTex(QOpenGLTexture::Target2D);
    eclipsedDoubleScatteringPrecomputationScratchTexture_->create();
    eclipsedDoubleScatteringPrecomputationScratchTexture_->bind();
    gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,
                    params_.eclipseAngularIntegrationPoints, params_.radialIntegrationPoints,
                    0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, eclipseDoubleScatteringPrecomputationFBO_);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,eclipsedDoubleScatteringPrecomputationScratchTexture_->textureId(),0);
    checkFramebufferStatus(gl, "Eclipsed double scattering precomputation FBO");
    gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GLint viewport[4];
    gl.glGetIntegerv(GL_VIEWPORT, viewport);
    const int width=viewport[2], height=viewport[3];
    resizeEvent(width,height);
}

AtmosphereRenderer::AtmosphereRenderer(QOpenGLFunctions_3_3_Core& gl, QString const& pathToData,
                                       ShowMySky::Settings* tools, std::function<void(QOpenGLShaderProgram&)> const& drawSurface)
    : gl(gl)
    , tools_(tools)
    , drawSurfaceCallback(drawSurface)
    , pathToData_(pathToData)
    , luminanceRenderTargetTexture_(QOpenGLTexture::Target2D)
{
    params_.parse(pathToData + "/params.atmo", AtmosphereParameters::SkipSpectra{true});
}

void AtmosphereRenderer::setDrawSurfaceCallback(std::function<void(QOpenGLShaderProgram&)> const& drawSurface)
{
    drawSurfaceCallback=drawSurface;
}

void AtmosphereRenderer::loadData(QByteArray viewDirVertShaderSrc, QByteArray viewDirFragShaderSrc,
                                  std::vector<std::pair<std::string,GLuint>> viewDirBindAttribLocations)
{
    try
    {
        readyToRender_=false;
        loadingStepsDone_=0;
        totalLoadingStepsToDo_=0;

        clearResources();

        viewDirVertShaderSrc_=std::move(viewDirVertShaderSrc);
        viewDirFragShaderSrc_=std::move(viewDirFragShaderSrc);
        viewDirBindAttribLocations_=std::move(viewDirBindAttribLocations);

        for(const auto& scatterer : params_.scatterers)
            scatterersEnabledStates_[scatterer.name]=true;

        currentActivity_=tr("Loading textures and shaders...");
        // The longest actions should be progress-tracked
        loadShaders(CountStepsOnly{true});
        loadTextures(CountStepsOnly{true});
        // Now they can be actually executed, reporting the progress
        loadShaders(CountStepsOnly{false});
        loadTextures(CountStepsOnly{false});
        reportLoadingFinished();

        setupRenderTarget();
        setupBuffers();
    }
    catch(std::exception const& ex)
    {
        throw DataLoadError(ex.what());
    }

    if(multipleScatteringPrograms_.size() != multipleScatteringTextures_.size())
    {
        throw DataLoadError{tr("Numbers of multiple scattering shader programs and textures don't match: %1 vs %2")
                              .arg(multipleScatteringPrograms_.size())
                              .arg(multipleScatteringTextures_.size())};
    }

    readyToRender_=true;
}

void AtmosphereRenderer::tick(const int loadingStepsDone)
{
    emit loadProgress(currentActivity_, loadingStepsDone, totalLoadingStepsToDo_);
}

void AtmosphereRenderer::reportLoadingFinished()
{
    currentActivity_.clear();
    totalLoadingStepsToDo_=0;
    loadingStepsDone_=0;
    tick(0);
}

AtmosphereRenderer::~AtmosphereRenderer()
{
    clearResources();
}

void AtmosphereRenderer::clearResources()
{
    if(vbo_)
    {
        gl.glDeleteBuffers(1, &vbo_);
        vbo_=0;
    }
    if(vao_)
    {
        gl.glDeleteVertexArrays(1, &vao_);
        vao_=0;
    }
    if(luminanceRadianceFBO_)
    {
        gl.glDeleteFramebuffers(1, &luminanceRadianceFBO_);
        luminanceRadianceFBO_=0;
    }
    if(eclipseSingleScatteringPrecomputationFBO_)
    {
        gl.glDeleteFramebuffers(1, &eclipseSingleScatteringPrecomputationFBO_);
        eclipseSingleScatteringPrecomputationFBO_=0;
    }
    if(!radianceRenderBuffers_.empty())
        gl.glDeleteRenderbuffers(radianceRenderBuffers_.size(), radianceRenderBuffers_.data());
}

void AtmosphereRenderer::drawSurface(QOpenGLShaderProgram& prog)
{
    OGL_TRACE();
    drawSurfaceCallback(prog);
}

void AtmosphereRenderer::resizeEvent(const int width, const int height)
{
    OGL_TRACE();

    viewportSize_=QSize(width,height);
    if(!luminanceRadianceFBO_) return;

    luminanceRenderTargetTexture_.bind();
    gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,luminanceRadianceFBO_);
    gl.glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,luminanceRenderTargetTexture_.textureId(),0);
    checkFramebufferStatus(gl, "Atmosphere renderer FBO");
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);

    if(!radianceRenderBuffers_.empty())
    {
        for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
        {
            gl.glBindRenderbuffer(GL_RENDERBUFFER, radianceRenderBuffers_[wlSetIndex]);
            gl.glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, width, height);
        }
        gl.glBindRenderbuffer(GL_RENDERBUFFER, viewDirectionRenderBuffer_);
        gl.glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, width, height);
    }
}

void AtmosphereRenderer::setScattererEnabled(QString const& name, const bool enable)
{
    scatterersEnabledStates_[name]=enable;
}

void AtmosphereRenderer::reloadShaders()
{
    OGL_TRACE();

    currentActivity_=tr("Reloading shaders...");
    totalLoadingStepsToDo_=0;
    loadShaders(CountStepsOnly{true});
    loadingStepsDone_=0;
    loadShaders(CountStepsOnly{false});
    reportLoadingFinished();
}
