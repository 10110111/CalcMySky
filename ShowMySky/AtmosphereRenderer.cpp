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
#include "api/ShowMySky/Settings.hpp"

namespace fs=std::filesystem;

namespace
{

auto newTex(QOpenGLTexture::Target target)
{
    return std::make_unique<QOpenGLTexture>(target);
}

void oglDebugMessageInsert([[maybe_unused]] const char*const message)
{
#if defined GL_DEBUG_OUTPUT && !defined NDEBUG
    static PFNGLDEBUGMESSAGEINSERTPROC glDebugMessageInsert;
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

void AtmosphereRenderer::loadEclipsedDoubleScatteringTexture(QString const& path, const float altitudeCoord)
{
    auto log=qDebug().nospace();

    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        throw DataLoadError{QObject::tr("GL error on entry to loadEclipsedDoubleScatteringTexture(\"%1\"): %2")
                            .arg(path).arg(openglErrorString(err).c_str())};
    }
    log << "Loading texture from " << path << "... ";
    QFile file(path);
    if(!file.open(QFile::ReadOnly))
        throw DataLoadError{QObject::tr("Failed to open file \"%1\": %2").arg(path).arg(file.errorString())};

    uint16_t numPointsPerSet;
    {
        const qint64 sizeToRead=sizeof numPointsPerSet;
        if(file.read(reinterpret_cast<char*>(&numPointsPerSet), sizeToRead) != sizeToRead)
        {
            throw DataLoadError{QObject::tr("Failed to read header from file \"%1\": %2")
                                .arg(path).arg(file.errorString())};
        }
    }
    const auto texSizeByViewAzimuth = params_.eclipsedDoubleScatteringTextureSize[0];
    const auto texSizeByViewElevation = params_.eclipsedDoubleScatteringTextureSize[1];
    const auto texSizeBySZA = params_.eclipsedDoubleScatteringTextureSize[2];
    const auto texSizeByAltitude = params_.eclipsedDoubleScatteringTextureSize[3];
    EclipsedDoubleScatteringPrecomputer precomputer(gl, params_, texSizeByViewAzimuth, texSizeByViewElevation, texSizeBySZA, 2);

    const auto altTexIndex = altitudeCoord==1 ? numAltIntervalsIn4DTexture_-1 : altitudeCoord*numAltIntervalsIn4DTexture_;
    const int floorAltIndex = std::floor(altTexIndex);
    const auto fractAltIndex = altTexIndex-floorAltIndex;
    const auto maxAltIndex = floorAltIndex+1;

    std::vector<glm::vec4> data(numPointsPerSet*texSizeBySZA*2);

    const auto sliceByteSize = numPointsPerSet*sizeof data[0];
    const auto fileReadOffset = uint64_t(sliceByteSize)*texSizeBySZA*floorAltIndex;
    const qint64 absoluteOffset=file.pos()+fileReadOffset;
    log << "skipping to offset " << absoluteOffset << "... ";
    if(!file.seek(absoluteOffset))
    {
        throw DataLoadError{QObject::tr("Failed to seek to offset %1 in file \"%2\": %3")
            .arg(absoluteOffset).arg(path).arg(file.errorString())};
    }

    const qint64 sizeToRead = data.size()*sizeof data[0];
    if(file.read(reinterpret_cast<char*>(data.data()), sizeToRead) != sizeToRead)
    {
        throw DataLoadError{QObject::tr("Failed to read data from file \"%1\": %2")
            .arg(path).arg(file.errorString())};
    }

    size_t readOffset = 0;
    for(int altIndex=floorAltIndex; altIndex<=maxAltIndex; ++altIndex)
    {
        for(int szaIndex=0; szaIndex<texSizeBySZA; ++szaIndex)
        {
            // Using the same encoding for altitude as in scatteringTex4DCoordsToTexVars()
            const float distToHorizon = float(altIndex)/(texSizeByAltitude-1)*params_.lengthOfHorizRayFromGroundToBorderOfAtmo;
            // Rounding errors can result in altitude>max, breaking the code after this calculation, so we have to clamp.
            // To avoid too many zeros that would make log interpolation problematic, we clamp the bottom value at 1 m. The same at the top.
            const float cameraAltitude = std::clamp(float(sqrt(sqr(distToHorizon)+sqr(params_.earthRadius))-params_.earthRadius),
                                                    1.f, params_.atmosphereHeight-1);

            precomputer.loadCoarseGridSamples(cameraAltitude, data.data()+readOffset, numPointsPerSet);
            precomputer.generateTextureFromCoarseGridData(altIndex-floorAltIndex, szaIndex, cameraAltitude);
            readOffset += numPointsPerSet;
        }
    }

    const size_t altSliceSize = texSizeByViewAzimuth * texSizeByViewElevation * texSizeBySZA;
    auto texture = precomputer.texture();
    assert(texture.size() == altSliceSize*2);

    for(size_t n = 0; n < altSliceSize; ++n)
    {
        const auto interpolated = texture[n] + fractAltIndex * (texture[n+altSliceSize] - texture[n]);
        if(std::isnan(interpolated.x))
        {
            std::cerr << "NaN computed from " << texture[n].x << " and " << texture[n+altSliceSize].x << " (n = " << n << ")\n";
        }
        texture[n] = interpolated;
    }

    gl.glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, texSizeByViewAzimuth, texSizeByViewElevation, texSizeBySZA,
                    0, GL_RGBA, GL_FLOAT, texture.data());

    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        throw DataLoadError{QObject::tr("GL error in loadEclipsedDoubleScatteringTexture(\"%1\") after glTexImage3D() call: %2")
                            .arg(path).arg(openglErrorString(err).c_str())};
    }

    log << "done";
}

void AtmosphereRenderer::loadTexture4D(QString const& path, const float altitudeCoord, Texture4DType texType)
{
    auto log=qDebug().nospace();

    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        throw DataLoadError{QObject::tr("GL error on entry to loadTexture4D(\"%1\"): %2")
                            .arg(path).arg(openglErrorString(err).c_str())};
    }
    log << "Loading texture from " << path << "... ";
    QFile file(path);
    if(!file.open(QFile::ReadOnly))
        throw DataLoadError{QObject::tr("Failed to open file \"%1\": %2").arg(path).arg(file.errorString())};

    uint16_t sizes[4];
    {
        const qint64 sizeToRead=sizeof sizes;
        if(file.read(reinterpret_cast<char*>(sizes), sizeToRead) != sizeToRead)
        {
            throw DataLoadError{QObject::tr("Failed to read header from file \"%1\": %2")
                                .arg(path).arg(file.errorString())};
        }
    }
    log << "dimensions from header: " << sizes[0] << "×" << sizes[1] << "×" << sizes[2] << "×" << sizes[3] << "... ";

    const size_t subpixelsPerPixel = texType==Texture4DType::InterpolationGuides ? 1 : 4;
    const size_t subpixelSize = texType==Texture4DType::InterpolationGuides ? sizeof(GLshort) : sizeof(GLfloat);
    const size_t pixelSize = subpixelsPerPixel*subpixelSize;
    const qint64 expectedFileSize = file.pos() + pixelSize*uint64_t(sizes[0])*sizes[1]*sizes[2]*sizes[3];
    if(expectedFileSize != file.size())
    {
        throw DataLoadError{QObject::tr("Size of file \"%1\" (%2 bytes) doesn't match image dimensions %3×%4×%5×%6 from file header.\nThe expected size is %7 bytes.")
                            .arg(path).arg(file.size()).arg(sizes[0]).arg(sizes[1]).arg(sizes[2]).arg(sizes[3]).arg(expectedFileSize)};
    }

    numAltIntervalsIn4DTexture_ = sizes[3]-1;
    const auto altTexIndex = altitudeCoord==1 ? numAltIntervalsIn4DTexture_-1 : altitudeCoord*numAltIntervalsIn4DTexture_;
    const auto floorAltIndex = std::floor(altTexIndex);
    const auto fractAltIndex = altTexIndex-floorAltIndex;

    const auto readOffset = pixelSize*uint64_t(sizes[0])*sizes[1]*sizes[2]*uint64_t(floorAltIndex);
    sizes[3]=2;
    const qint64 sizeToRead = pixelSize*uint64_t(sizes[0])*sizes[1]*sizes[2]*sizes[3];

    const std::unique_ptr<char[]> data(new char[sizeToRead]);

    const qint64 absoluteOffset=file.pos()+readOffset;
    log << "skipping to offset " << absoluteOffset << "... ";
    if(!file.seek(absoluteOffset))
    {
        throw DataLoadError{QObject::tr("Failed to seek to offset %1 in file \"%2\": %3")
                            .arg(absoluteOffset).arg(path).arg(file.errorString())};
    }
    const auto actuallyRead=file.read(data.get(), sizeToRead);
    if(actuallyRead != sizeToRead)
    {
        const auto error = actuallyRead==-1 ? QObject::tr("Failed to read texture data from file \"%1\": %2").arg(path).arg(file.errorString())
                                            : QObject::tr("Failed to read texture data from file \"%1\": requested %2 bytes, read %3").arg(path).arg(sizeToRead).arg(actuallyRead);
        throw DataLoadError{error};
    }

    const auto altSliceSize = size_t(sizes[0])*sizes[1]*sizes[2];
    if(texType == Texture4DType::InterpolationGuides)
    {
        std::unique_ptr<int16_t[]> texData(new int16_t[altSliceSize]);
        for(size_t n = 0; n < altSliceSize; ++n)
        {
            int16_t lower, upper;
            assert(sizeof lower == pixelSize);
            std::memcpy(&lower, data.get() + n * pixelSize, pixelSize);
            std::memcpy(&upper, data.get() + (n+altSliceSize) * pixelSize, pixelSize);
            texData[n] = lower + fractAltIndex*(upper-lower);
        }
        gl.glTexImage3D(GL_TEXTURE_3D, 0, GL_R16_SNORM, sizes[0], sizes[1], sizes[2], 0, GL_RED, GL_SHORT, texData.get());
    }
    else
    {
        std::unique_ptr<glm::vec4[]> texData(new glm::vec4[altSliceSize]);
        for(size_t n = 0; n < altSliceSize; ++n)
        {
            glm::vec4 lower, upper;
            assert(sizeof lower == pixelSize);
            std::memcpy(&lower, data.get() + n * pixelSize, pixelSize);
            std::memcpy(&upper, data.get() + (n+altSliceSize) * pixelSize, pixelSize);
            texData[n] = lower + fractAltIndex*(upper-lower);
        }
        gl.glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, sizes[0], sizes[1], sizes[2], 0, GL_RGBA, GL_FLOAT, texData.get());
    }
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        throw DataLoadError{QObject::tr("GL error in loadTexture4D(\"%1\") after glTexImage3D() call: %2")
                            .arg(path).arg(openglErrorString(err).c_str())};
    }

    log << "done";
}

glm::ivec2 AtmosphereRenderer::loadTexture2D(QString const& path)
{
    auto log=qDebug().nospace();

    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        throw DataLoadError{QObject::tr("GL error on entry to loadTexture2D(\"%1\"): %2")
                            .arg(path).arg(openglErrorString(err).c_str())};
    }
    log << "Loading texture from " << path << "... ";
    QFile file(path);
    if(!file.open(QFile::ReadOnly))
        throw DataLoadError{QObject::tr("Failed to open file \"%1\": %2").arg(path).arg(file.errorString())};

    uint16_t sizes[2];
    {
        const qint64 sizeToRead=sizeof sizes;
        if(file.read(reinterpret_cast<char*>(sizes), sizeToRead) != sizeToRead)
        {
            throw DataLoadError{QObject::tr("Failed to read header from file \"%1\": %2")
                                .arg(path).arg(file.errorString())};
        }
    }
    const auto subpixelCount = 4*uint64_t(sizes[0])*sizes[1];
    log << "dimensions from header: " << sizes[0] << "×" << sizes[1] << "... ";

    if(const qint64 expectedFileSize = subpixelCount*sizeof(GLfloat)+file.pos();
       expectedFileSize != file.size())
    {
        throw DataLoadError{QObject::tr("Size of file \"%1\" (%2 bytes) doesn't match image dimensions %3×%4 from file header.\nThe expected size is %5 bytes.")
                            .arg(path).arg(file.size()).arg(sizes[0]).arg(sizes[1]).arg(expectedFileSize)};
    }

    const std::unique_ptr<GLfloat[]> subpixels(new GLfloat[subpixelCount]);
    {
        const qint64 sizeToRead=subpixelCount*sizeof subpixels[0];
        const auto actuallyRead=file.read(reinterpret_cast<char*>(subpixels.get()), sizeToRead);
        if(actuallyRead != sizeToRead)
        {
            const auto error = actuallyRead==-1 ? QObject::tr("Failed to read texture data from file \"%1\": %2").arg(path).arg(file.errorString())
                                                : QObject::tr("Failed to read texture data from file \"%1\": requested %2 bytes, read %3").arg(path).arg(sizeToRead).arg(actuallyRead);
            throw DataLoadError{error};
        }
    }
    gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,sizes[0],sizes[1],0,GL_RGBA,GL_FLOAT,subpixels.get());
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        throw DataLoadError{QObject::tr("GL error in loadTexture2D(\"%1\") after glTexImage2D() call: %2")
                            .arg(path).arg(openglErrorString(err).c_str())};
    }
    log << "done";
    return {sizes[0], sizes[1]};
}

const float sunPositions[][5]={
    {20,38,24, 298.125713095103, -4.78204363889612},
    {20,40,23, 298.566389633925, -4.99825192683366},
    {20,42,23, 299.011367798508, -5.21536151787467},
    {20,44,23, 299.456957704902, -5.43153738689461},
    {20,46,23, 299.90317000405 , -5.64676515612128},
    {20,48,23, 300.350015188534, -5.86103042802592},
    {20,50,23, 300.797503589762, -6.07431878564055},
    {20,52,30, 301.271807360605, -6.29896887791526},
    {20,54,30, 301.720651533118, -6.51020096163185},
    {20,56,30, 302.170169489007, -6.72041192262251},
    {20,58,30, 302.620370875888, -6.92958726930273},
    {21, 0,30, 303.071265165347, -7.13771249301201},
    {21, 2,30, 303.522861649876, -7.34477306863564},
    {21, 4,37, 304.001576207411, -7.56273641549499},
    {21, 6,37, 304.454646512566, -7.7675598058058},
    {21, 8,37, 304.908446288203, -7.97127403062429},
    {21,10,37, 305.362984067733, -8.17386450554762},
    {21,12,37, 305.818268189398, -8.3753166336876},
    {21,14,37, 306.274306792986, -8.57561580662277},
    {21,16,37, 306.731107816521, -8.77474740540709},
    {21,18,37, 307.188678992924, -8.97269680163653},
    {21,20,37, 307.647027846642, -9.16944935857296},
    {21,22,47, 308.144458501513, -9.38123037761941},
    {21,24,47, 308.604450753166, -9.57544247921417},
    {21,26,47, 309.065242533897, -9.76841257205458},
    {21,28,47, 309.5268404815  , -9.96012599827993},
    {21,30,47, 309.989251007864, -10.1505680983261},
    {21,32,47, 310.452480295499, -10.3397242124137},
    {21,34,47, 310.916534294047, -10.527579682097},
    {21,36,47, 311.381418716797, -10.7141198518758},
    {21,38,47, 311.847139037183, -10.8993300708678},
    {21,41, 4, 312.37986500158 , -11.1091337446718},
    {21,43, 4, 312.847392814971, -11.2914463957731},
    {21,45, 4, 313.315772122329, -11.4723831176707},
    {21,47, 5, 313.788921268333, -11.653419632678},
    {21,49, 5, 314.259023819119, -11.8315489018557},
    {21,51, 5, 314.729990447493, -12.0082583273431},
    {21,53, 5, 315.2018248106  , -12.1835333455945},
    {21,55, 5, 315.674530297558, -12.3573594136586},
    {21,57, 5, 316.148110026093, -12.5297220114264},
    {21,59, 5, 316.622566839208, -12.7006066439435},
    {22, 1, 5, 317.097903301893, -12.8699988437833},
    {22, 3, 4, 317.570149561455, -13.0364913949118},
    {22, 5, 5, 318.051224026393, -13.2042482280447},
    {22, 7, 4, 318.525225102457, -13.3677094519537},
    {22, 9, 5, 319.008087036888, -13.5323550694832},
    {22,11, 5, 319.487850266927, -13.6940692320176},
    {22,13, 5, 319.968502519639, -13.8542048761497},
    {22,15, 5, 320.450044325821, -14.012747798804},
    {22,17, 5, 320.93247591381 , -14.1696838456316},
    {22,19, 5, 321.415797206738, -14.3249989139273},
    {22,21, 5, 321.900007819865, -14.4786789556035},
    {22,23, 5, 322.385107058012, -14.6307099802205},
    {22,25, 5, 322.871093913075, -14.7810780580703},
    {22,27, 5, 323.35796706165 , -14.9297693233158},
    {22,29, 5, 323.84572486276 , -15.0767699771814},
    {22,31, 5, 324.33436535569 , -15.2220662911948},
    {22,33, 5, 324.823886257939, -15.3656446104792},
    {22,35, 5, 325.314284963295, -15.5074913570934},
    {22,37, 5, 325.80555854003 , -15.6475930334191},
    {22,39, 5, 326.297703729235, -15.7859362255927},
    {22,41, 5, 326.790716943282, -15.9225076069814},
    {22,43, 5, 327.284594264436, -16.0572939417008},
    {22,45, 5, 327.779331443604, -16.1902820881719},
    {22,47, 5, 328.274923899246, -16.3214590027161},
    {22,49, 5, 328.771366716436, -16.4508117431866},
    {22,51, 5, 329.268654646087, -16.5783274726327},
    {22,53, 5, 329.766782104342, -16.7039934629958},
    {22,55, 5, 330.265743172134, -16.8277970988349},
    {22,57, 5, 330.76553159493 , -16.9497258810782},
    {22,59, 5, 331.266140782644, -17.0697674307997},
    {23, 1, 5, 331.767563809744, -17.1879094930173},
    {23, 3, 5, 332.269793415546, -17.3041399405096},
    {23, 5, 5, 332.772822004697, -17.41844677765},
    {23, 7, 5, 333.276641647864, -17.5308181442538},
    {23, 9, 5, 333.781244082616, -17.6412423194368},
    {23,11, 5, 334.286620714517, -17.7497077254807},
    {23,13, 5, 334.792762618421, -17.856202931705},
    {23,15, 5, 335.299660539989, -17.9607166583393},
};
void AtmosphereRenderer::loadTextures(const CountStepsOnly countStepsOnly)
{
    OGL_TRACE();

    while(gl.glGetError()!=GL_NO_ERROR);

    if(!countStepsOnly)
        gl.glActiveTexture(GL_TEXTURE0);

    for(unsigned n=0; n<std::size(sunPositions); ++n)
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
            continue;
        }
        if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
            continue;

        if(n==0)
        {
            ladogaTextures_.resize(std::size(sunPositions));
            gl.glGenTextures(ladogaTextures_.size(), ladogaTextures_.data());
        }
        auto const& date=sunPositions[n];
        const auto imgFileName=QString("/home/ruslan/tmp/2019-04-14-twilight-at-ladoga-frames/2019-04-14 %1:%2:%3-merged-srgb.bmp")
                                    .arg(int(date[0]),2,10,QChar('0'))
                                    .arg(int(date[1]),2,10,QChar('0'))
                                    .arg(int(date[2]),2,10,QChar('0'));
        std::cerr << "Loading texture from \"" << imgFileName << "\"...\n";
        const auto img=QImage(imgFileName).convertToFormat(QImage::Format_RGBA8888);
        if(img.isNull())
            qDebug() << "******** Failed to open image " << imgFileName;
        gl.glBindTexture(GL_TEXTURE_2D, ladogaTextures_[n]);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width(), img.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
        gl.glGenerateMipmap(GL_TEXTURE_2D);
        ++loadingStepsDone_; return;
    }

    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
            continue;
        }
        if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
            continue;

        auto& tex=*transmittanceTextures_.emplace_back(newTex(QOpenGLTexture::Target2D));
        tex.setMinificationFilter(QOpenGLTexture::Linear);
        tex.setWrapMode(QOpenGLTexture::ClampToEdge);
        tex.bind();
        loadTexture2D(QString("%1/transmittance-wlset%2.f32").arg(pathToData_).arg(wlSetIndex));
        ++loadingStepsDone_; return;
    }

    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
            continue;
        }
        if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
            continue;

        auto& tex=*irradianceTextures_.emplace_back(newTex(QOpenGLTexture::Target2D));
        tex.setMinificationFilter(QOpenGLTexture::Linear);
        tex.setWrapMode(QOpenGLTexture::ClampToEdge);
        tex.bind();
        loadTexture2D(QString("%1/irradiance-wlset%2.f32").arg(pathToData_).arg(wlSetIndex));
        ++loadingStepsDone_; return;
    }

    altCoordToLoad_=altitudeUnitRangeTexCoord();
    reloadScatteringTextures(countStepsOnly);

    assert(gl.glGetError()==GL_NO_ERROR);
}

double AtmosphereRenderer::altitudeUnitRangeTexCoord() const
{
    const double H = params_.atmosphereHeight;
    const double h = std::clamp(tools_->altitude(), 0., H);
    const double R = params_.earthRadius;
    return std::sqrt(h*(h+2*R) / ( H*(H+2*R) ));
}

void AtmosphereRenderer::reloadScatteringTextures(const CountStepsOnly countStepsOnly)
{
    const auto texFilter = tools_->textureFilteringEnabled() ? QOpenGLTexture::Linear : QOpenGLTexture::Nearest;
    const auto altCoord = altCoordToLoad_;

    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        multipleScatteringTextures_.clear();
        ++loadingStepsDone_; return;
    }
    if(const auto filename=pathToData_+"/multiple-scattering-xyzw.f32"; QFile::exists(filename))
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
        }
        else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
        {
            auto& tex=*multipleScatteringTextures_.emplace_back(newTex(QOpenGLTexture::Target3D));
            tex.setMinificationFilter(texFilter);
            tex.setMagnificationFilter(texFilter);
            tex.setWrapMode(QOpenGLTexture::ClampToEdge);
            tex.bind();
            loadTexture4D(filename, altCoord);
            ++loadingStepsDone_; return;
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
            if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
                continue;

            auto& tex=*multipleScatteringTextures_.emplace_back(newTex(QOpenGLTexture::Target3D));
            tex.setMinificationFilter(texFilter);
            tex.setMagnificationFilter(texFilter);
            tex.setWrapMode(QOpenGLTexture::ClampToEdge);
            tex.bind();
            loadTexture4D(QString("%1/multiple-scattering-wlset%2.f32").arg(pathToData_).arg(wlSetIndex), altCoord);
            ++loadingStepsDone_; return;
        }
    }

    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        singleScatteringTextures_.clear();
        ++loadingStepsDone_; return;
    }

    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        singleScatteringInterpolationGuidesTextures01_.clear();
        singleScatteringInterpolationGuidesTextures02_.clear();
        ++loadingStepsDone_; return;
    }

    for(const auto& scatterer : params_.scatterers)
    {
        auto& texturesPerWLSet=singleScatteringTextures_[scatterer.name];
        switch(scatterer.phaseFunctionType)
        {
        case PhaseFunctionType::General:
        {
            for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
            {
                if(countStepsOnly)
                {
                    ++totalLoadingStepsToDo_;
                    continue;
                }
                if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
                    continue;

                auto& texture=*texturesPerWLSet.emplace_back(newTex(QOpenGLTexture::Target3D));
                texture.setMinificationFilter(texFilter);
                texture.setMagnificationFilter(texFilter);
                texture.setWrapMode(QOpenGLTexture::ClampToEdge);
                texture.bind();
                loadTexture4D(QString("%1/single-scattering/%2/%3.f32").arg(pathToData_).arg(wlSetIndex).arg(scatterer.name), altCoord);
                ++loadingStepsDone_; return;
            }
            for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
            {
                const auto filename=QString("%1/single-scattering/%2/%3-dims01.guides2d").arg(pathToData_).arg(wlSetIndex).arg(scatterer.name);
                if(QFile::exists(filename))
                {
                    if(countStepsOnly)
                    {
                        ++totalLoadingStepsToDo_;
                    }
                    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
                    {
                        auto& guidesPerWLSet=singleScatteringInterpolationGuidesTextures01_[scatterer.name];
                        auto& tex=*guidesPerWLSet.emplace_back(newTex(QOpenGLTexture::Target3D));
                        tex.setMinificationFilter(QOpenGLTexture::Linear);
                        tex.setMagnificationFilter(QOpenGLTexture::Linear);
                        tex.setWrapMode(QOpenGLTexture::ClampToEdge);
                        tex.bind();
                        loadTexture4D(filename, altCoord, Texture4DType::InterpolationGuides);
                        ++loadingStepsDone_; return;
                    }
                }
            }
            for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
            {
                const auto filename=QString("%1/single-scattering/%2/%3-dims02.guides2d").arg(pathToData_).arg(wlSetIndex).arg(scatterer.name);
                if(QFile::exists(filename))
                {
                    if(countStepsOnly)
                    {
                        ++totalLoadingStepsToDo_;
                    }
                    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
                    {
                        auto& guidesPerWLSet=singleScatteringInterpolationGuidesTextures02_[scatterer.name];
                        auto& tex=*guidesPerWLSet.emplace_back(newTex(QOpenGLTexture::Target3D));
                        tex.setMinificationFilter(QOpenGLTexture::Linear);
                        tex.setMagnificationFilter(QOpenGLTexture::Linear);
                        tex.setWrapMode(QOpenGLTexture::ClampToEdge);
                        tex.bind();
                        loadTexture4D(filename, altCoord, Texture4DType::InterpolationGuides);
                        ++loadingStepsDone_; return;
                    }
                }
            }
            if(countStepsOnly)
            {
                ++totalLoadingStepsToDo_;
            }
            else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
            {
                if(singleScatteringInterpolationGuidesTextures02_.size() != singleScatteringInterpolationGuidesTextures01_.size())
                {
                    std::cerr << "Warning: interpolation guides inconsistent: dimensions 0-1 have "
                              << singleScatteringInterpolationGuidesTextures01_.size() << " wavelength sets, while dimensions 0-2 have "
                              << singleScatteringInterpolationGuidesTextures02_.size() << ". Ignoring the guides.\n";
                    singleScatteringInterpolationGuidesTextures01_.clear();
                    singleScatteringInterpolationGuidesTextures02_.clear();
                }
                ++loadingStepsDone_; return;
            }
            break;
        }
        case PhaseFunctionType::Smooth:
        case PhaseFunctionType::Achromatic:
        {
            if(countStepsOnly)
            {
                ++totalLoadingStepsToDo_;
            }
            else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
            {
                auto& texture=*texturesPerWLSet.emplace_back(newTex(QOpenGLTexture::Target3D));
                texture.setMinificationFilter(texFilter);
                texture.setMagnificationFilter(texFilter);
                texture.setWrapMode(QOpenGLTexture::ClampToEdge);
                texture.bind();
                loadTexture4D(QString("%1/single-scattering/%2-xyzw.f32").arg(pathToData_).arg(scatterer.name), altCoord);
                ++loadingStepsDone_; return;
            }

            const auto guidesFilename01 = QString("%1/single-scattering/%2-xyzw-dims01.guides2d").arg(pathToData_).arg(scatterer.name);
            if(QFile::exists(guidesFilename01))
            {
                if(countStepsOnly)
                {
                    ++totalLoadingStepsToDo_;
                }
                else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
                {
                    auto& guidesPerWLSet=singleScatteringInterpolationGuidesTextures01_[scatterer.name];
                    auto& texture=*guidesPerWLSet.emplace_back(newTex(QOpenGLTexture::Target3D));
                    texture.setMinificationFilter(QOpenGLTexture::Linear);
                    texture.setMagnificationFilter(QOpenGLTexture::Linear);
                    texture.setWrapMode(QOpenGLTexture::ClampToEdge);
                    texture.bind();
                    loadTexture4D(guidesFilename01, altCoord, Texture4DType::InterpolationGuides);
                    ++loadingStepsDone_; return;
                }
            }
            const auto guidesFilename02 = QString("%1/single-scattering/%2-xyzw-dims02.guides2d").arg(pathToData_).arg(scatterer.name);
            if(QFile::exists(guidesFilename02))
            {
                if(countStepsOnly)
                {
                    ++totalLoadingStepsToDo_;
                }
                else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
                {
                    auto& guidesPerWLSet=singleScatteringInterpolationGuidesTextures02_[scatterer.name];
                    auto& texture=*guidesPerWLSet.emplace_back(newTex(QOpenGLTexture::Target3D));
                    texture.setMinificationFilter(QOpenGLTexture::Linear);
                    texture.setMagnificationFilter(QOpenGLTexture::Linear);
                    texture.setWrapMode(QOpenGLTexture::ClampToEdge);
                    texture.bind();
                    loadTexture4D(guidesFilename02, altCoord, Texture4DType::InterpolationGuides);
                    ++loadingStepsDone_; return;
                }
            }
            if(countStepsOnly)
            {
                ++totalLoadingStepsToDo_;
            }
            else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
            {
                if(singleScatteringInterpolationGuidesTextures02_.size() != singleScatteringInterpolationGuidesTextures01_.size())
                {
                    std::cerr << "Warning: interpolation guides inconsistent: dimensions 0-1 is "
                              << (singleScatteringInterpolationGuidesTextures01_.empty() ? "lacking" : "present")
                              << ", while dimensions 0-2 is "
                              << (singleScatteringInterpolationGuidesTextures02_.empty() ? "lacking" : "present")
                              << ". Ignoring the guides.\n";
                    singleScatteringInterpolationGuidesTextures01_.clear();
                    singleScatteringInterpolationGuidesTextures02_.clear();
                }
                ++loadingStepsDone_; return;
            }
            break;
        }
        }
    }

    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        eclipsedDoubleScatteringTextures_.clear();
        ++loadingStepsDone_; return;
    }
    if(!params_.noEclipsedDoubleScatteringTextures)
    {
        if(const auto filename=pathToData_+"/eclipsed-double-scattering-xyzw.f32"; QFile::exists(filename))
        {
            if(countStepsOnly)
            {
                ++totalLoadingStepsToDo_;
            }
            else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
            {
                auto& texture=*eclipsedDoubleScatteringTextures_.emplace_back(newTex(QOpenGLTexture::Target3D));
                texture.setMinificationFilter(texFilter);
                texture.setMagnificationFilter(texFilter);
                // relative azimuth
                texture.setWrapMode(QOpenGLTexture::DirectionS, QOpenGLTexture::Repeat);
                // VZA
                texture.setWrapMode(QOpenGLTexture::DirectionT, QOpenGLTexture::ClampToEdge);
                // SZA
                texture.setWrapMode(QOpenGLTexture::DirectionR, QOpenGLTexture::ClampToEdge);

                texture.bind();
                loadEclipsedDoubleScatteringTexture(QString("%1/eclipsed-double-scattering-xyzw.f32")
                                                     .arg(pathToData_), altCoord);

                ++loadingStepsDone_; return;
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
                if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
                    continue;

                auto& texture=*eclipsedDoubleScatteringTextures_.emplace_back(newTex(QOpenGLTexture::Target3D));
                texture.setMinificationFilter(texFilter);
                texture.setMagnificationFilter(texFilter);
                // relative azimuth
                texture.setWrapMode(QOpenGLTexture::DirectionS, QOpenGLTexture::Repeat);
                // VZA
                texture.setWrapMode(QOpenGLTexture::DirectionT, QOpenGLTexture::ClampToEdge);
                // SZA
                texture.setWrapMode(QOpenGLTexture::DirectionR, QOpenGLTexture::ClampToEdge);

                texture.bind();
                loadEclipsedDoubleScatteringTexture(QString("%1/eclipsed-double-scattering-wlset%2.f32")
                                                     .arg(pathToData_).arg(wlSetIndex), altCoord);

                ++loadingStepsDone_; return;
            }
        }
    }

    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        eclipsedDoubleScatteringPrecomputationTargetTextures_.clear();
        ++loadingStepsDone_; return;
    }
    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
            continue;
        }
        if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
            continue;

        auto& tex=*eclipsedDoubleScatteringPrecomputationTargetTextures_.emplace_back(newTex(QOpenGLTexture::Target3D));
        // relative azimuth
        tex.setWrapMode(QOpenGLTexture::DirectionS, QOpenGLTexture::Repeat);
        // cosVZA
        tex.setWrapMode(QOpenGLTexture::DirectionT, QOpenGLTexture::ClampToEdge);
        // dummy dimension
        tex.setWrapMode(QOpenGLTexture::DirectionR, QOpenGLTexture::Repeat);
    }

    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        lightPollutionTextures_.clear();
        ++loadingStepsDone_; return;
    }
    if(const auto filename=pathToData_+"/light-pollution-xyzw.f32"; QFile::exists(filename))
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
        }
        else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
        {
            auto& tex=*lightPollutionTextures_.emplace_back(newTex(QOpenGLTexture::Target2D));
            tex.setMinificationFilter(texFilter);
            tex.setMagnificationFilter(texFilter);
            tex.setWrapMode(QOpenGLTexture::ClampToEdge);
            tex.bind();
            loadTexture2D(filename);
            ++loadingStepsDone_; return;
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
            if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
                continue;

            auto& tex=*lightPollutionTextures_.emplace_back(newTex(QOpenGLTexture::Target2D));
            tex.setMinificationFilter(texFilter);
            tex.setMagnificationFilter(texFilter);
            tex.setWrapMode(QOpenGLTexture::ClampToEdge);
            tex.bind();
            loadTexture2D(QString("%1/light-pollution-wlset%2.f32").arg(pathToData_).arg(wlSetIndex));
            ++loadingStepsDone_; return;
        }
    }
}

void AtmosphereRenderer::loadShaders(const CountStepsOnly countStepsOnly)
{
    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        viewDirVertShader_.reset(new QOpenGLShader(QOpenGLShader::Vertex));
        viewDirFragShader_.reset(new QOpenGLShader(QOpenGLShader::Fragment));
        if(!viewDirVertShader_->compileSourceCode(viewDirVertShaderSrc_))
            throw DataLoadError{QObject::tr("Failed to compile view direction vertex shader:\n%2").arg(viewDirVertShader_->log())};
        if(!viewDirFragShader_->compileSourceCode(viewDirFragShaderSrc_))
            throw DataLoadError{QObject::tr("Failed to compile view direction fragment shader:\n%2").arg(viewDirFragShader_->log())};
        ++loadingStepsDone_; return;
    }

    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;

        // Dummy values so that the loop below doesn't index empty vector
        singleScatteringPrograms_.clear();
        for(int renderMode=0; renderMode<SSRM_COUNT; ++renderMode)
            singleScatteringPrograms_.emplace_back(std::make_unique<std::map<QString,std::vector<ShaderProgPtr>>>());
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        singleScatteringPrograms_.clear();
        for(int renderMode=0; renderMode<SSRM_COUNT; ++renderMode)
            singleScatteringPrograms_.emplace_back(std::make_unique<std::map<QString,std::vector<ShaderProgPtr>>>());

        ++loadingStepsDone_; return;
    }
    for(int renderMode=0; renderMode<SSRM_COUNT; ++renderMode)
    {
        auto& programsPerScatterer=*singleScatteringPrograms_[renderMode];

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
                    if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
                        continue;

                    const auto scatDir=QString("%1/shaders/single-scattering/%2/%3/%4").arg(pathToData_)
                                                                                       .arg(singleScatteringRenderModeNames[renderMode])
                                                                                       .arg(wlSetIndex)
                                                                                       .arg(scatterer.name);
                    qDebug().nospace() << "Loading shaders from " << scatDir << "...";
                    auto& program=*programs.emplace_back(std::make_unique<QOpenGLShaderProgram>());

                    for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
                        addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

                    program.addShader(viewDirFragShader_.get());
                    program.addShader(viewDirVertShader_.get());
                    for(const auto& b : viewDirBindAttribLocations_)
                        program.bindAttributeLocation(b.first.c_str(), b.second);

                    link(program, QObject::tr("shader program for scatterer \"%1\"").arg(scatterer.name));
                    ++loadingStepsDone_; return;
                }
            }
            else
            {
                if(countStepsOnly)
                {
                    ++totalLoadingStepsToDo_;
                    continue;
                }
                if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
                    continue;

                const auto scatDir=QString("%1/shaders/single-scattering/%2/%3").arg(pathToData_)
                                                                                .arg(singleScatteringRenderModeNames[renderMode])
                                                                                .arg(scatterer.name);
                qDebug().nospace() << "Loading shaders from " << scatDir << "...";
                auto& program=*programs.emplace_back(std::make_unique<QOpenGLShaderProgram>());
                for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
                    addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

                program.addShader(viewDirFragShader_.get());
                program.addShader(viewDirVertShader_.get());
                for(const auto& b : viewDirBindAttribLocations_)
                    program.bindAttributeLocation(b.first.c_str(), b.second);

                link(program, QObject::tr("shader program for scatterer \"%1\"").arg(scatterer.name));
                ++loadingStepsDone_; return;
            }
        }
    }

    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;

        // Dummy values so that the loop below doesn't index empty vector
        eclipsedSingleScatteringPrograms_.clear();
        for(int renderMode=SSRM_ON_THE_FLY; renderMode<SSRM_COUNT; ++renderMode)
            eclipsedSingleScatteringPrograms_.emplace_back(std::make_unique<ScatteringProgramsMap>());
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        eclipsedSingleScatteringPrograms_.clear();
        for(int renderMode=SSRM_ON_THE_FLY; renderMode<SSRM_COUNT; ++renderMode)
            eclipsedSingleScatteringPrograms_.emplace_back(std::make_unique<ScatteringProgramsMap>());

        ++loadingStepsDone_; return;
    }
    for(int renderMode=SSRM_ON_THE_FLY; renderMode<SSRM_COUNT; ++renderMode)
    {
        auto& programsPerScatterer=*eclipsedSingleScatteringPrograms_[renderMode];

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
                    if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
                        continue;

                    const auto scatDir=QString("%1/shaders/single-scattering-eclipsed/%2/%3/%4").arg(pathToData_)
                                                                                                .arg(singleScatteringRenderModeNames[renderMode])
                                                                                                .arg(wlSetIndex)
                                                                                                .arg(scatterer.name);
                    qDebug().nospace() << "Loading shaders from " << scatDir << "...";
                    auto& program=*programs.emplace_back(std::make_unique<QOpenGLShaderProgram>());

                    for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
                        addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

                    program.addShader(viewDirFragShader_.get());
                    program.addShader(viewDirVertShader_.get());
                    for(const auto& b : viewDirBindAttribLocations_)
                        program.bindAttributeLocation(b.first.c_str(), b.second);

                    link(program, QObject::tr("shader program for scatterer \"%1\"").arg(scatterer.name));
                    ++loadingStepsDone_; return;
                }
            }
            else
            {
                if(countStepsOnly)
                {
                    ++totalLoadingStepsToDo_;
                    continue;
                }
                if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
                    continue;

                const auto scatDir=QString("%1/shaders/single-scattering-eclipsed/%2/%3").arg(pathToData_)
                                                                                            .arg(singleScatteringRenderModeNames[renderMode])
                                                                                            .arg(scatterer.name);
                qDebug().nospace() << "Loading shaders from " << scatDir << "...";
                auto& program=*programs.emplace_back(std::make_unique<QOpenGLShaderProgram>());

                for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
                    addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

                program.addShader(viewDirFragShader_.get());
                program.addShader(viewDirVertShader_.get());
                for(const auto& b : viewDirBindAttribLocations_)
                    program.bindAttributeLocation(b.first.c_str(), b.second);

                link(program, QObject::tr("shader program for scatterer \"%1\"").arg(scatterer.name));
                ++loadingStepsDone_; return;
            }
        }
    }

    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
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
        precomputationProgramsVertShader_.reset(new QOpenGLShader(QOpenGLShader::Vertex));
        if(!precomputationProgramsVertShader_->compileSourceCode(precomputationProgramsVertShaderSrc))
            throw DataLoadError{QObject::tr("Failed to compile vertex shader for on-the-fly precomputation of eclipsed scattering:\n%2")
                                    .arg(precomputationProgramsVertShader_->log())};
        ++loadingStepsDone_; return;
    }

    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        ladogaFramesProgram_ = std::make_unique<QOpenGLShaderProgram>();
        auto& program = *ladogaFramesProgram_;
        program.addShader(viewDirFragShader_.get());
        program.addShader(viewDirVertShader_.get());
        addShaderFile(program,QOpenGLShader::Fragment,QString("/home/ruslan/Dropbox/myprogs/CalcMySky/shaders/atmosphere-from-photos.frag"));
        for(const auto& b : viewDirBindAttribLocations_)
            program.bindAttributeLocation(b.first.c_str(), b.second);
        link(program, QObject::tr("Ladoga frames display shader program"));
        ++loadingStepsDone_; return;
    }

    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;

        // Dummy value to avoid dereferencing null pointer in the loop below
        eclipsedSingleScatteringPrecomputationPrograms_=std::make_unique<ScatteringProgramsMap>();
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        eclipsedSingleScatteringPrecomputationPrograms_=std::make_unique<ScatteringProgramsMap>();
        ++loadingStepsDone_; return;
    }

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
            if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
                continue;

            const auto scatDir=QString("%1/shaders/single-scattering-eclipsed/precomputation/%3/%4").arg(pathToData_)
                                                                                                    .arg(wlSetIndex)
                                                                                                    .arg(scatterer.name);
            qDebug().nospace() << "Loading shaders from " << scatDir << "...";
            auto& program=*programs.emplace_back(std::make_unique<QOpenGLShaderProgram>());

            for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
                addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

            program.addShader(precomputationProgramsVertShader_.get());

            link(program, QObject::tr("shader program for scatterer \"%1\"").arg(scatterer.name));
            ++loadingStepsDone_; return;
        }
    }

    // Precomputed rendering (with approximate mixing, since textures contain only the data for fully-centered eclipse)
    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        eclipsedDoubleScatteringPrecomputedPrograms_.clear();
        ++loadingStepsDone_; return;
    }
    if(QFile::exists(pathToData_+"/shaders/double-scattering-eclipsed/precomputed/0/"))
    {
        for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
        {
            if(countStepsOnly)
            {
                ++totalLoadingStepsToDo_;
                continue;
            }
            if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
                continue;

            const auto scatDir=QString("%1/shaders/double-scattering-eclipsed/precomputed/%2").arg(pathToData_).arg(wlSetIndex);
            qDebug().nospace() << "Loading shaders from " << scatDir << "...";
            auto& program=*eclipsedDoubleScatteringPrecomputedPrograms_.emplace_back(std::make_unique<QOpenGLShaderProgram>());

            for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
                addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

            program.addShader(viewDirFragShader_.get());
            program.addShader(viewDirVertShader_.get());
            for(const auto& b : viewDirBindAttribLocations_)
                program.bindAttributeLocation(b.first.c_str(), b.second);

            link(program, QObject::tr("precomputed eclipsed double scattering shader program"));
            ++loadingStepsDone_; return;
        }
    }
    else
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
        }
        else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
        {
            const auto scatDir=QString("%1/shaders/double-scattering-eclipsed/precomputed").arg(pathToData_);
            qDebug().nospace() << "Loading shaders from " << scatDir << "...";
            auto& program=*eclipsedDoubleScatteringPrecomputedPrograms_.emplace_back(std::make_unique<QOpenGLShaderProgram>());

            for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
                addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

            program.addShader(viewDirFragShader_.get());
            program.addShader(viewDirVertShader_.get());
            for(const auto& b : viewDirBindAttribLocations_)
                program.bindAttributeLocation(b.first.c_str(), b.second);

            link(program, QObject::tr("precomputed eclipsed double scattering shader program"));
            ++loadingStepsDone_; return;
        }
    }

    // Rendering with on-the-fly precomputation, useful as a reference on slower machines, and as the production mode on very fast ones
    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        eclipsedDoubleScatteringPrecomputationPrograms_.clear();
        ++loadingStepsDone_; return;
    }
    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
            continue;
        }
        if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
            continue;

        const auto scatDir=QString("%1/shaders/double-scattering-eclipsed/precomputation/%2").arg(pathToData_).arg(wlSetIndex);
        qDebug().nospace() << "Loading shaders from " << scatDir << "...";
        auto& program=*eclipsedDoubleScatteringPrecomputationPrograms_.emplace_back(std::make_unique<QOpenGLShaderProgram>());

        for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
            addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

        program.addShader(precomputationProgramsVertShader_.get());

        link(program, QObject::tr("on-the-fly eclipsed double scattering shader program"));
        ++loadingStepsDone_; return;
    }

    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        multipleScatteringPrograms_.clear();
        ++loadingStepsDone_; return;
    }
    if(QFile::exists(pathToData_+"/shaders/multiple-scattering/0/"))
    {
        for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
        {
            if(countStepsOnly)
            {
                ++totalLoadingStepsToDo_;
                continue;
            }
            if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
                continue;

            auto& program=*multipleScatteringPrograms_.emplace_back(std::make_unique<QOpenGLShaderProgram>());
            const auto wlDir=QString("%1/shaders/multiple-scattering/%2").arg(pathToData_).arg(wlSetIndex);
            qDebug().nospace() << "Loading shaders from " << wlDir << "...";
            for(const auto& shaderFile : fs::directory_iterator(fs::u8path(wlDir.toStdString())))
                addShaderFile(program, QOpenGLShader::Fragment, shaderFile.path());
            program.addShader(viewDirFragShader_.get());
            program.addShader(viewDirVertShader_.get());
            for(const auto& b : viewDirBindAttribLocations_)
                program.bindAttributeLocation(b.first.c_str(), b.second);
            link(program, QObject::tr("multiple scattering shader program"));
            ++loadingStepsDone_; return;
        }
    }
    else
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
        }
        else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
        {
            auto& program=*multipleScatteringPrograms_.emplace_back(std::make_unique<QOpenGLShaderProgram>());
            const auto wlDir=pathToData_+"/shaders/multiple-scattering/";
            qDebug().nospace() << "Loading shaders from " << wlDir << "...";
            for(const auto& shaderFile : fs::directory_iterator(fs::u8path(wlDir.toStdString())))
                addShaderFile(program, QOpenGLShader::Fragment, shaderFile.path());
            program.addShader(viewDirFragShader_.get());
            program.addShader(viewDirVertShader_.get());
            for(const auto& b : viewDirBindAttribLocations_)
                program.bindAttributeLocation(b.first.c_str(), b.second);
            link(program, QObject::tr("multiple scattering shader program"));
            ++loadingStepsDone_; return;
        }
    }

    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        zeroOrderScatteringPrograms_.clear();
        ++loadingStepsDone_; return;
    }
    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
            continue;
        }
        if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
            continue;

        auto& program=*zeroOrderScatteringPrograms_.emplace_back(std::make_unique<QOpenGLShaderProgram>());
        const auto wlDir=QString("%1/shaders/zero-order-scattering/%2").arg(pathToData_).arg(wlSetIndex);
        qDebug().nospace() << "Loading shaders from " << wlDir << "...";
        for(const auto& shaderFile : fs::directory_iterator(fs::u8path(wlDir.toStdString())))
            addShaderFile(program, QOpenGLShader::Fragment, shaderFile.path());
        program.addShader(viewDirFragShader_.get());
        program.addShader(viewDirVertShader_.get());
        for(const auto& b : viewDirBindAttribLocations_)
            program.bindAttributeLocation(b.first.c_str(), b.second);
        link(program, QObject::tr("zero-order scattering shader program"));
        ++loadingStepsDone_; return;
    }

    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        eclipsedZeroOrderScatteringPrograms_.clear();
        ++loadingStepsDone_; return;
    }
    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
            continue;
        }
        if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
            continue;

        auto& program=*eclipsedZeroOrderScatteringPrograms_.emplace_back(std::make_unique<QOpenGLShaderProgram>());
        const auto wlDir=QString("%1/shaders/eclipsed-zero-order-scattering/%2").arg(pathToData_).arg(wlSetIndex);
        qDebug().nospace() << "Loading shaders from " << wlDir << "...";
        for(const auto& shaderFile : fs::directory_iterator(fs::u8path(wlDir.toStdString())))
            addShaderFile(program, QOpenGLShader::Fragment, shaderFile.path());
        program.addShader(viewDirFragShader_.get());
        program.addShader(viewDirVertShader_.get());
        for(const auto& b : viewDirBindAttribLocations_)
            program.bindAttributeLocation(b.first.c_str(), b.second);
        link(program, QObject::tr("eclipsed zero-order scattering shader program"));
        ++loadingStepsDone_; return;
    }

    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        viewDirectionGetterProgram_=std::make_unique<QOpenGLShaderProgram>();
        auto& program=*viewDirectionGetterProgram_;
        program.addShader(viewDirFragShader_.get());
        program.addShader(viewDirVertShader_.get());
        for(const auto& b : viewDirBindAttribLocations_)
            program.bindAttributeLocation(b.first.c_str(), b.second);
        addShaderCode(program, QOpenGLShader::Fragment, QObject::tr("fragment shader for view direction getter"), 1+R"(
#version 330

in vec3 position;
out vec3 viewDir;

vec3 calcViewDir();
void main()
{
    viewDir=calcViewDir();
}
)");
        link(program, QObject::tr("view direction getter shader program"));
        ++loadingStepsDone_; return;
    }

    if(countStepsOnly)
    {
        ++totalLoadingStepsToDo_;
    }
    else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
    {
        lightPollutionPrograms_.clear();
        ++loadingStepsDone_; return;
    }
    if(QFile::exists(pathToData_+"/shaders/light-pollution/0/"))
    {
        for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
        {
            if(countStepsOnly)
            {
                ++totalLoadingStepsToDo_;
                continue;
            }
            if(++currentLoadingIterationStepCounter_ <= loadingStepsDone_)
                continue;

            auto& program=*lightPollutionPrograms_.emplace_back(std::make_unique<QOpenGLShaderProgram>());
            const auto wlDir=QString("%1/shaders/light-pollution/%2").arg(pathToData_).arg(wlSetIndex);
            qDebug().nospace() << "Loading shaders from " << wlDir << "...";
            for(const auto& shaderFile : fs::directory_iterator(fs::u8path(wlDir.toStdString())))
                addShaderFile(program, QOpenGLShader::Fragment, shaderFile.path());
            program.addShader(viewDirFragShader_.get());
            program.addShader(viewDirVertShader_.get());
            for(const auto& b : viewDirBindAttribLocations_)
                program.bindAttributeLocation(b.first.c_str(), b.second);
            link(program, QObject::tr("light pollution shader program"));
            ++loadingStepsDone_; return;
        }
    }
    else
    {
        if(countStepsOnly)
        {
            ++totalLoadingStepsToDo_;
        }
        else if(++currentLoadingIterationStepCounter_ > loadingStepsDone_)
        {
            auto& program=*lightPollutionPrograms_.emplace_back(std::make_unique<QOpenGLShaderProgram>());
            const auto wlDir=pathToData_+"/shaders/light-pollution/";
            qDebug().nospace() << "Loading shaders from " << wlDir << "...";
            for(const auto& shaderFile : fs::directory_iterator(fs::u8path(wlDir.toStdString())))
                addShaderFile(program, QOpenGLShader::Fragment, shaderFile.path());
            program.addShader(viewDirFragShader_.get());
            program.addShader(viewDirVertShader_.get());
            for(const auto& b : viewDirBindAttribLocations_)
                program.bindAttributeLocation(b.first.c_str(), b.second);
            link(program, QObject::tr("light pollution shader program"));
            ++loadingStepsDone_; return;
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

double AtmosphereRenderer::sunZenithAngle() const
{
    if(overrideSunZenithAngle_)
        return *overrideSunZenithAngle_;
    return tools_->sunZenithAngle();
}

glm::dvec3 AtmosphereRenderer::sunDirection() const
{
    return glm::dvec3(std::cos(tools_->sunAzimuth())*std::sin(sunZenithAngle()),
                      std::sin(tools_->sunAzimuth())*std::sin(sunZenithAngle()),
                      std::cos(sunZenithAngle()));
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
    return -hpR*sin(moonElevation)+sqrt(sqr(tools_->earthMoonDistance())-sqr(hpR*cos(moonElevation)));
}

QVector4D AtmosphereRenderer::getPixelLuminance(QPoint const& pixelPos)
{
    GLint origFBO=-1;
    gl.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &origFBO);

    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, luminanceRadianceFBO_);
    gl.glReadBuffer(GL_COLOR_ATTACHMENT0);
    glm::vec4 pixel;
    gl.glReadPixels(pixelPos.x(), viewportSize_.height()-pixelPos.y()-1, 1,1, GL_RGBA, GL_FLOAT, &pixel[0]);

    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, origFBO);

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
    const bool haveNoLuminanceOnlySingleScatteringTextures =
        std::find_if(params_.scatterers.begin(), params_.scatterers.end(), [=](auto const& scatterer)
                     { return scatterer.phaseFunctionType!=PhaseFunctionType::General; }) == params_.scatterers.end();
    return haveNoLuminanceOnlySingleScatteringTextures && multipleScatteringTextures_.size()==params_.allWavelengths.size();
}

bool AtmosphereRenderer::canSetSolarSpectrum() const
{
    return canGrabRadiance(); // condition is the same as for radiance grabbing
}

bool AtmosphereRenderer::canRenderPrecomputedEclipsedDoubleScattering() const
{
    return !params_.noEclipsedDoubleScatteringTextures;
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
            prog.setUniformValue("moonPosition", toQVector(moonPosition()));
            prog.setUniformValue("sunDirection", toQVector(sunDirection()));
            prog.setUniformValue("sunAngularRadius", float(tools_->sunAngularRadius()));
            transmittanceTextures_[wlSetIndex]->bind(0);
            prog.setUniformValue("transmittanceTexture", 0);
            prog.setUniformValue("lightPollutionGroundLuminance", float(tools_->lightPollutionGroundLuminance()));
            prog.setUniformValue("pseudoMirrorSkyBelowHorizon", tools_->pseudoMirrorEnabled());
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
            prog.setUniformValue("sunAngularRadius", float(tools_->sunAngularRadius()));
            transmittanceTextures_[wlSetIndex]->bind(0);
            prog.setUniformValue("transmittanceTexture", 0);
            irradianceTextures_[wlSetIndex]->bind(1);
            prog.setUniformValue("irradianceTexture",1);
            prog.setUniformValue("lightPollutionGroundLuminance", float(tools_->lightPollutionGroundLuminance()));
            prog.setUniformValue("pseudoMirrorSkyBelowHorizon", tools_->pseudoMirrorEnabled());
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
            prog.setUniformValue("moonPositionRelativeToSunAzimuth", toQVector(moonPositionRelativeToSunAzimuth()));
            prog.setUniformValue("sunAngularRadius", float(tools_->sunAngularRadius()));
            prog.setUniformValue("sunZenithAngle", float(sunZenithAngle()));
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

void AtmosphereRenderer::renderLadogaFrame()
{
    OGL_TRACE();
    auto& prog = *ladogaFramesProgram_;
    prog.bind();
    gl.glActiveTexture(GL_TEXTURE0);
    prog.setUniformValue("photo", 0);
    gl.glEnable(GL_SCISSOR_TEST);
    for(unsigned n=0; n<std::size(sunPositions); ++n)
    {
        gl.glBindTexture(GL_TEXTURE_2D, ladogaTextures_[n]);
        prog.setUniformValue("sunAzimuthInPhoto", float(sunPositions[n][3]*M_PI/180));
        prog.setUniformValue("frameNum", float(n));

        gl.glScissor(n,0, 1,viewportSize_.height());
        double localBrightness=1;
        gl.glBlendColor(localBrightness, localBrightness, localBrightness, localBrightness);

        drawSurface(prog);
        {
            const auto y = 0.5*(1-49/1080.)*viewportSize_.height();
            const auto p = getPixelLuminance(QPoint(n,y)).y();
            localBrightness=1./p/2;
            gl.glBlendColor(localBrightness, localBrightness, localBrightness, localBrightness);
        }
        gl.glClear(GL_COLOR_BUFFER_BIT);
        gl.glScissor(n,0, 1,viewportSize_.height());
        drawSurface(prog);
    }
    gl.glDisable(GL_SCISSOR_TEST);
}

void AtmosphereRenderer::renderSingleScattering()
{
    OGL_TRACE();

    if(tools_->usingEclipseShader())
        precomputeEclipsedSingleScattering();

    const auto texFilter = tools_->textureFilteringEnabled() ? QOpenGLTexture::Linear : QOpenGLTexture::Nearest;
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
                    prog.setUniformValue("moonPosition", toQVector(moonPosition()));
                    prog.setUniformValue("sunDirection", toQVector(sunDirection()));
                    prog.setUniformValue("sunAngularRadius", float(tools_->sunAngularRadius()));
                    transmittanceTextures_[wlSetIndex]->bind(0);
                    prog.setUniformValue("transmittanceTexture", 0);
                    prog.setUniformValue("pseudoMirrorSkyBelowHorizon", tools_->pseudoMirrorEnabled());
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
                    prog.setUniformValue("sunAngularRadius", float(tools_->sunAngularRadius()));
                    transmittanceTextures_[wlSetIndex]->bind(0);
                    prog.setUniformValue("transmittanceTexture", 0);
                    prog.setUniformValue("pseudoMirrorSkyBelowHorizon", tools_->pseudoMirrorEnabled());
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
                    prog.setUniformValue("sunAngularRadius", float(tools_->sunAngularRadius()));
                    {
                        auto& tex=*eclipsedSingleScatteringPrecomputationTextures_.at(scatterer.name)[wlSetIndex];
                        tex.setMinificationFilter(texFilter);
                        tex.setMagnificationFilter(texFilter);
                        tex.bind(0);
                        prog.setUniformValue("eclipsedScatteringTexture", 0);
                    }
                    prog.setUniformValue("pseudoMirrorSkyBelowHorizon", tools_->pseudoMirrorEnabled());
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
                    prog.setUniformValue("sunAngularRadius", float(tools_->sunAngularRadius()));
                    {
                        auto& tex=*singleScatteringTextures_.at(scatterer.name)[wlSetIndex];
                        tex.setMinificationFilter(texFilter);
                        tex.setMagnificationFilter(texFilter);
                        tex.bind(0);
                        prog.setUniformValue("scatteringTexture", 0);
                    }

                    bool guides01Loaded = false, guides02Loaded = false;
                    {
                        const auto guidesPerWLSetIt = singleScatteringInterpolationGuidesTextures01_.find(scatterer.name);
                        if(guidesPerWLSetIt != singleScatteringInterpolationGuidesTextures01_.end())
                        {
                            auto& tex=guidesPerWLSetIt->second[wlSetIndex];
                            tex->bind(1);
                            prog.setUniformValue("scatteringTextureInterpolationGuides01", 1);
                            guides01Loaded = true;
                        }
                    }
                    {
                        const auto guidesPerWLSetIt = singleScatteringInterpolationGuidesTextures02_.find(scatterer.name);
                        if(guidesPerWLSetIt != singleScatteringInterpolationGuidesTextures02_.end())
                        {
                            auto& tex=guidesPerWLSetIt->second[wlSetIndex];
                            tex->bind(2);
                            prog.setUniformValue("scatteringTextureInterpolationGuides02", 2);
                            guides02Loaded = true;
                        }
                    }
                    prog.setUniformValue("useInterpolationGuides", guides01Loaded && guides02Loaded);

                    prog.setUniformValue("pseudoMirrorSkyBelowHorizon", tools_->pseudoMirrorEnabled());
                    if(!solarIrradianceFixup_.empty())
                        prog.setUniformValue("solarIrradianceFixup", solarIrradianceFixup_[wlSetIndex]);

                    drawSurface(prog);
                }
            }
        }
        else if(!tools_->usingEclipseShader())
        {
            auto& prog=*singleScatteringPrograms_[renderMode]->at(scatterer.name).front();
            prog.bind();
            prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
            prog.setUniformValue("sunDirection", toQVector(sunDirection()));
            prog.setUniformValue("sunAngularRadius", float(tools_->sunAngularRadius()));
            {
                auto& tex=*singleScatteringTextures_.at(scatterer.name).front();
                tex.setMinificationFilter(texFilter);
                tex.setMagnificationFilter(texFilter);
                tex.bind(0);
            }
            prog.setUniformValue("scatteringTexture", 0);
            prog.setUniformValue("pseudoMirrorSkyBelowHorizon", tools_->pseudoMirrorEnabled());

            bool guides01Loaded = false, guides02Loaded = false;
            {
                const auto guidesPerWLSetIt = singleScatteringInterpolationGuidesTextures01_.find(scatterer.name);
                if(guidesPerWLSetIt != singleScatteringInterpolationGuidesTextures01_.end())
                {
                    auto& tex=guidesPerWLSetIt->second.front();
                    tex->bind(1);
                    prog.setUniformValue("scatteringTextureInterpolationGuides01", 1);
                    guides01Loaded = true;
                }
            }
            {
                const auto guidesPerWLSetIt = singleScatteringInterpolationGuidesTextures02_.find(scatterer.name);
                if(guidesPerWLSetIt != singleScatteringInterpolationGuidesTextures02_.end())
                {
                    auto& tex=guidesPerWLSetIt->second.front();
                    tex->bind(2);
                    prog.setUniformValue("scatteringTextureInterpolationGuides02", 2);
                    guides02Loaded = true;
                }
            }
            prog.setUniformValue("useInterpolationGuides", guides01Loaded && guides02Loaded);

            drawSurface(prog);
        }
        else
        {
            auto& prog=*eclipsedSingleScatteringPrograms_[renderMode]->at(scatterer.name).front();
            prog.bind();
            prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
            prog.setUniformValue("sunDirection", toQVector(sunDirection()));
            prog.setUniformValue("sunAngularRadius", float(tools_->sunAngularRadius()));
            {
                auto& tex=*eclipsedSingleScatteringPrecomputationTextures_.at(scatterer.name).front();
                tex.setMinificationFilter(texFilter);
                tex.setMagnificationFilter(texFilter);
                tex.bind(0);
                prog.setUniformValue("eclipsedScatteringTexture", 0);
            }
            prog.setUniformValue("pseudoMirrorSkyBelowHorizon", tools_->pseudoMirrorEnabled());

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
    const bool renderingNeedsLuminance = !canGrabRadiance();
    std::unique_ptr<EclipsedDoubleScatteringPrecomputer> precompAccumulator;
    for(unsigned wlSetIndex=0; wlSetIndex<params_.allWavelengths.size(); ++wlSetIndex)
    {
        auto& prog=*eclipsedDoubleScatteringPrecomputationPrograms_[wlSetIndex];
        prog.bind();
        int unusedTextureUnitNum=0;
        transmittanceTextures_[wlSetIndex]->bind(unusedTextureUnitNum);
        prog.setUniformValue("transmittanceTexture", unusedTextureUnitNum++);
        if(!solarIrradianceFixup_.empty())
            prog.setUniformValue("solarIrradianceFixup", solarIrradianceFixup_[wlSetIndex]);
        prog.setUniformValue("sunAngularRadius", float(tools_->sunAngularRadius()));

        auto precomputer = std::make_unique<EclipsedDoubleScatteringPrecomputer>(gl,
                                                        params_,
                                                        params_.eclipsedDoubleScatteringTextureSize[0],
                                                        params_.eclipsedDoubleScatteringTextureSize[1], 1, 1);
        precomputer->computeRadianceOnCoarseGrid(prog, eclipsedDoubleScatteringPrecomputationScratchTexture_->textureId(),
                                                 unusedTextureUnitNum, tools_->altitude(), sunZenithAngle(),
                                                 tools_->moonZenithAngle(), tools_->moonAzimuth() - tools_->sunAzimuth(),
                                                 tools_->earthMoonDistance());
        if(renderingNeedsLuminance)
        {
            const auto rad2lum = radianceToLuminance(wlSetIndex, params_.allWavelengths);
            if(wlSetIndex==0)
            {
                precompAccumulator = std::move(precomputer);
                precompAccumulator->convertRadianceToLuminance(rad2lum);
            }
            else
            {
                precompAccumulator->accumulateLuminance(*precomputer, rad2lum);
            }
        }

        if(!renderingNeedsLuminance || wlSetIndex+1 == params_.allWavelengths.size())
        {
            auto& generator = renderingNeedsLuminance ? *precompAccumulator : *precomputer;
            generator.generateTextureFromCoarseGridData(0, 0, tools_->altitude());
            eclipsedDoubleScatteringPrecomputationTargetTextures_[renderingNeedsLuminance ? 0 : wlSetIndex]->bind();
            gl.glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA32F,
                            params_.eclipsedDoubleScatteringTextureSize[0], params_.eclipsedDoubleScatteringTextureSize[1], 1,
                            0,GL_RGBA,GL_FLOAT,generator.texture().data());
        }
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
        for(unsigned wlSetIndex=0; wlSetIndex < eclipsedDoubleScatteringPrecomputedPrograms_.size(); ++wlSetIndex)
        {
            if(!radianceRenderBuffers_.empty())
                gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, radianceRenderBuffers_[wlSetIndex]);

            auto& prog=*eclipsedDoubleScatteringPrecomputedPrograms_[wlSetIndex];
            prog.bind();
            prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
            prog.setUniformValue("sunDirection", toQVector(sunDirection()));
            prog.setUniformValue("sunAngularRadius", float(tools_->sunAngularRadius()));
            prog.setUniformValue("pseudoMirrorSkyBelowHorizon", tools_->pseudoMirrorEnabled());
            if(!solarIrradianceFixup_.empty())
                prog.setUniformValue("solarIrradianceFixup", solarIrradianceFixup_[wlSetIndex]);

            if(tools_->onTheFlyPrecompDoubleScatteringEnabled())
            {
                // The same texture is used for upper and lower slices
                auto& tex=*eclipsedDoubleScatteringPrecomputationTargetTextures_[wlSetIndex];
                tex.setMinificationFilter(texFilter);
                tex.setMagnificationFilter(texFilter);
                tex.bind(0);
                prog.setUniformValue("eclipsedDoubleScatteringTexture", 0);
                prog.setUniformValue("eclipsedDoubleScatteringTextureSize", QVector3D(params_.eclipsedDoubleScatteringTextureSize[0],
                                                                                      params_.eclipsedDoubleScatteringTextureSize[1], 1));
            }
            else
            {
                assert(!params_.noEclipsedDoubleScatteringTextures);

                auto& texture=*eclipsedDoubleScatteringTextures_[wlSetIndex];
                texture.setMinificationFilter(texFilter);
                texture.setMagnificationFilter(texFilter);
                texture.bind(0);
                prog.setUniformValue("eclipsedDoubleScatteringTexture", 0);

                prog.setUniformValue("eclipsedDoubleScatteringTextureSize", toQVector(glm::vec3(params_.eclipsedDoubleScatteringTextureSize)));
            }
            drawSurface(prog);
        }
    }
    else
    {
        for(unsigned wlSetIndex = 0; wlSetIndex < multipleScatteringTextures_.size(); ++wlSetIndex)
        {
            if(!radianceRenderBuffers_.empty())
                gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, radianceRenderBuffers_[wlSetIndex]);

            auto& prog=*multipleScatteringPrograms_[wlSetIndex];
            prog.bind();
            prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
            prog.setUniformValue("sunDirection", toQVector(sunDirection()));
            prog.setUniformValue("sunAngularRadius", float(tools_->sunAngularRadius()));
            prog.setUniformValue("pseudoMirrorSkyBelowHorizon", tools_->pseudoMirrorEnabled());
            if(!solarIrradianceFixup_.empty())
                prog.setUniformValue("solarIrradianceFixup", solarIrradianceFixup_[wlSetIndex]);

            auto& tex=*multipleScatteringTextures_[wlSetIndex];
            tex.setMinificationFilter(texFilter);
            tex.setMagnificationFilter(texFilter);
            tex.bind(0);
            prog.setUniformValue("scatteringTexture", 0);
            drawSurface(prog);
        }
    }
}

void AtmosphereRenderer::renderLightPollution()
{
    OGL_TRACE();

    const auto texFilter = tools_->textureFilteringEnabled() ? QOpenGLTexture::Linear : QOpenGLTexture::Nearest;

    for(unsigned wlSetIndex = 0; wlSetIndex < lightPollutionPrograms_.size(); ++wlSetIndex)
    {
        if(!radianceRenderBuffers_.empty())
            gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, radianceRenderBuffers_[wlSetIndex]);

        auto& prog=*lightPollutionPrograms_[wlSetIndex];
        prog.bind();
        prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
        prog.setUniformValue("sunDirection", toQVector(sunDirection()));
        prog.setUniformValue("sunAngularRadius", float(tools_->sunAngularRadius()));
        prog.setUniformValue("pseudoMirrorSkyBelowHorizon", tools_->pseudoMirrorEnabled());
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

int AtmosphereRenderer::initPreparationToDraw()
{
    OGL_TRACE();

    if(state_ == State::ReloadingTextures)
        return totalLoadingStepsToDo_;
    if(state_ != State::ReadyToRender) return -1;

    const auto altCoord=altitudeUnitRangeTexCoord();
    if(altCoord != altCoordToLoad_)
    {
        [[maybe_unused]] OGLTrace t("reloading textures");

        altCoordToLoad_ = altCoord;
        state_ = State::ReloadingTextures;
        currentActivity_=QObject::tr("Reloading textures due to altitude change...");
        loadingStepsDone_=0;
        totalLoadingStepsToDo_=0;
        reloadScatteringTextures(CountStepsOnly{true});
    }

    return totalLoadingStepsToDo_;
}

auto AtmosphereRenderer::stepPreparationToDraw() -> LoadingStatus
{
    OGL_TRACE();

    if(state_ != State::ReloadingTextures)
        return {0, -1};

    const auto altCoord=altitudeUnitRangeTexCoord();
    if(altCoord != altCoordToLoad_)
    {
        std::cerr << "While we were reloading textures, the requested altitude changed again "
                     "(loaded coordinate: " << altCoordToLoad_ << ", requested: " << altCoord
                  << "). Restarting the reloading process\n";
        finalizeLoading();
        initPreparationToDraw();
    }

    currentLoadingIterationStepCounter_=0;
    reloadScatteringTextures(CountStepsOnly{false});

    if(loadingStepsDone_ == totalLoadingStepsToDo_)
        finalizeLoading();

    return {loadingStepsDone_, totalLoadingStepsToDo_};
}

void AtmosphereRenderer::draw(const double brightness, const bool clear)
{
    OGL_TRACE();

    if(const int preparationSteps = initPreparationToDraw(); preparationSteps>0)
    {
        qWarning() << "Calling code hasn't properly prepared the renderer. Doing the preparation synchronously.";
        for(LoadingStatus status = stepPreparationToDraw(); status.stepsDone < status.stepsToDo; )
            status = stepPreparationToDraw();
    }

    if(state_ != State::ReadyToRender) return;

    oglDebugMessageInsert("AtmosphereRenderer::draw() begins drawing");

    GLint targetFBO=-1;
    gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &targetFBO);

    {
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER,luminanceRadianceFBO_);
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
            if(tools_->ladogaFramesEnabled())
            {
                renderLadogaFrame();
            }
            else
            {
                gl.glEnable(GL_SCISSOR_TEST);
                for(unsigned n=0; n<std::size(sunPositions); ++n)
                {
                    overrideSunZenithAngle_=(90-sunPositions[n][4])*(M_PI/180);
                    gl.glScissor(n,0, 1,viewportSize_.height());
                    auto localBrightness=brightness;
                    gl.glBlendColor(localBrightness, localBrightness, localBrightness, localBrightness);
                    if(tools_->singleScatteringEnabled())
                        renderSingleScattering();
                    if(tools_->multipleScatteringEnabled())
                        renderMultipleScattering();
                    if(tools_->lightPollutionGroundLuminance())
                        renderLightPollution();

                    {
                        const auto y = 0.5*(1-49/1080.)*viewportSize_.height();
                        const auto p = getPixelLuminance(QPoint(n,y)).y();
                        localBrightness=brightness/p/5;
                        gl.glBlendColor(localBrightness, localBrightness, localBrightness, localBrightness);
                    }
                    gl.glClear(GL_COLOR_BUFFER_BIT);
                    gl.glScissor(n,0, 1,viewportSize_.height());
                    if(tools_->zeroOrderScatteringEnabled())
                        renderZeroOrderScattering();
                    if(tools_->singleScatteringEnabled())
                        renderSingleScattering();
                    if(tools_->multipleScatteringEnabled())
                        renderMultipleScattering();
                    if(tools_->lightPollutionGroundLuminance())
                        renderLightPollution();
                }
                gl.glScissor(0,0, viewportSize_.width(),viewportSize_.height());
                gl.glDisable(GL_SCISSOR_TEST);
            }
        }
        gl.glDisablei(GL_BLEND, 0);

        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER,targetFBO);
    }
}

void AtmosphereRenderer::setupRenderTarget()
{
    OGL_TRACE();

    GLint origFBO=-1;
    gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &origFBO);

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
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, viewDirectionFBO_);
        gl.glBindRenderbuffer(GL_RENDERBUFFER, viewDirectionRenderBuffer_);
        gl.glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, 1, 1); // dummy size just to initialize the renderbuffer
        gl.glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, viewDirectionRenderBuffer_);
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, origFBO);
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

            if(scatterer.phaseFunctionType!=PhaseFunctionType::General)
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
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, eclipseDoubleScatteringPrecomputationFBO_);
    gl.glFramebufferTexture(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,eclipsedDoubleScatteringPrecomputationScratchTexture_->textureId(),0);
    checkFramebufferStatus(gl, "Eclipsed double scattering precomputation FBO");
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, origFBO);

    GLint viewport[4];
    gl.glGetIntegerv(GL_VIEWPORT, viewport);
    const int width=viewport[2], height=viewport[3];
    resizeEvent(width,height);
}

AtmosphereRenderer::AtmosphereRenderer(QOpenGLFunctions_3_3_Core& gl, QString const& pathToData,
                                       ShowMySky::Settings* tools, std::function<void(QOpenGLShaderProgram& shprog)> const& drawSurface)
    : gl(gl)
    , tools_(tools)
    , drawSurfaceCallback(drawSurface)
    , pathToData_(pathToData)
    , luminanceRenderTargetTexture_(QOpenGLTexture::Target2D)
{
    params_.parse(pathToData + "/params.atmo", AtmosphereParameters::ForceNoEDSTextures{false}, AtmosphereParameters::SkipSpectra{true});
}

void AtmosphereRenderer::setDrawSurfaceCallback(std::function<void(QOpenGLShaderProgram& shprog)> const& drawSurface)
{
    drawSurfaceCallback=drawSurface;
}

int AtmosphereRenderer::initDataLoading(QByteArray viewDirVertShaderSrc, QByteArray viewDirFragShaderSrc,
                                        std::vector<std::pair<std::string,GLuint>> viewDirBindAttribLocations)
{
    try
    {
        state_ = State::LoadingData;
        currentActivity_=QObject::tr("Loading textures and shaders...");
        loadingStepsDone_=0;
        totalLoadingStepsToDo_=0;

        clearResources();

        viewDirVertShaderSrc_=std::move(viewDirVertShaderSrc);
        viewDirFragShaderSrc_=std::move(viewDirFragShaderSrc);
        viewDirBindAttribLocations_=std::move(viewDirBindAttribLocations);

        for(const auto& scatterer : params_.scatterers)
            scatterersEnabledStates_[scatterer.name]=true;

        loadShaders(CountStepsOnly{true});
        loadTextures(CountStepsOnly{true});
    }
    catch(std::exception const& ex)
    {
        throw DataLoadError(ex.what());
    }
    return totalLoadingStepsToDo_;
}

void AtmosphereRenderer::setViewDirShaders(QByteArray viewDirVertShaderSrc, QByteArray viewDirFragShaderSrc,
                                           std::vector<std::pair<std::string,GLuint>> viewDirBindAttribLocations)
{
    viewDirVertShaderSrc_ = viewDirVertShaderSrc;
    viewDirFragShaderSrc_ = viewDirFragShaderSrc;

    std::unique_ptr<QOpenGLShader> newVertShader(new QOpenGLShader(QOpenGLShader::Vertex));
    std::unique_ptr<QOpenGLShader> newFragShader(new QOpenGLShader(QOpenGLShader::Fragment));

    if(!newVertShader->compileSourceCode(viewDirVertShaderSrc_))
        throw DataLoadError{QObject::tr("Failed to compile view direction vertex shader:\n%2").arg(viewDirVertShader_->log())};
    if(!newFragShader->compileSourceCode(viewDirFragShaderSrc_))
        throw DataLoadError{QObject::tr("Failed to compile view direction fragment shader:\n%2").arg(viewDirFragShader_->log())};

    const auto replaceShaders = [oldVert=viewDirVertShader_.get(),
                                 oldFrag=viewDirFragShader_.get(),
                                 newVert=newVertShader.get(),
                                 newFrag=newFragShader.get()](QOpenGLShaderProgram& prog, QString const& name)
                                {
                                    prog.removeShader(oldVert);
                                    prog.removeShader(oldFrag);
                                    prog.addShader(newVert);
                                    prog.addShader(newFrag);
                                    link(prog, name);
                                };

    for(const auto& map : singleScatteringPrograms_)
        for(auto& item : *map)
            for(auto& prog : item.second)
                replaceShaders(*prog, QObject::tr("single scattering shader program"));

    for(const auto& map : eclipsedSingleScatteringPrograms_)
        for(auto& item : *map)
            for(auto& prog : item.second)
                replaceShaders(*prog, QObject::tr("eclipsed single scattering shader program"));

    for(const auto& prog : eclipsedDoubleScatteringPrecomputedPrograms_)
        replaceShaders(*prog, QObject::tr("eclipsed double scattering shader program"));
    for(const auto& prog : lightPollutionPrograms_)
        replaceShaders(*prog, QObject::tr("light pollution shader program"));
    for(const auto& prog : zeroOrderScatteringPrograms_)
        replaceShaders(*prog, QObject::tr("zero-order scattering shader program"));
    for(const auto& prog : eclipsedZeroOrderScatteringPrograms_)
        replaceShaders(*prog, QObject::tr("eclipsed zero-order scattering shader program"));
    for(const auto& prog : multipleScatteringPrograms_)
        replaceShaders(*prog, QObject::tr("multiple scattering shader program"));

    replaceShaders(*viewDirectionGetterProgram_, QObject::tr("view direction getter shader program"));

    viewDirVertShader_ = std::move(newVertShader);
    viewDirFragShader_ = std::move(newFragShader);
    viewDirBindAttribLocations_ = std::move(viewDirBindAttribLocations);

}

auto AtmosphereRenderer::stepDataLoading() -> LoadingStatus
{
    OGL_TRACE();

    if(totalLoadingStepsToDo_ == 0)
        return {0, 0};

    try
    {
        currentLoadingIterationStepCounter_=0;

        const auto stepsDoneBeforeShaders = loadingStepsDone_;
        loadShaders(CountStepsOnly{false});
        if(loadingStepsDone_ == stepsDoneBeforeShaders) // proceed only if previous function has nothing left to do
            loadTextures(CountStepsOnly{false});

        if(loadingStepsDone_ < totalLoadingStepsToDo_)
            return {loadingStepsDone_, totalLoadingStepsToDo_};

        setupRenderTarget();
        setupBuffers();
    }
    catch(std::exception const& ex)
    {
        throw DataLoadError(ex.what());
    }

    if(multipleScatteringPrograms_.size() != multipleScatteringTextures_.size())
    {
        throw DataLoadError{QObject::tr("Numbers of multiple scattering shader programs and textures don't match: %1 vs %2")
                              .arg(multipleScatteringPrograms_.size())
                              .arg(multipleScatteringTextures_.size())};
    }

    finalizeLoading();
    return {loadingStepsDone_, totalLoadingStepsToDo_};
}

void AtmosphereRenderer::finalizeLoading()
{
    currentActivity_.clear();
    totalLoadingStepsToDo_=0;
    loadingStepsDone_=0;
    state_ = State::ReadyToRender;
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

    if(ladogaTextures_.size())
        gl.glDeleteTextures(ladogaTextures_.size(), ladogaTextures_.data());
}

void AtmosphereRenderer::drawSurface(QOpenGLShaderProgram& prog)
{
    OGL_TRACE();
    drawSurfaceCallback(prog);
}

void AtmosphereRenderer::resizeEvent(int width, int height)
{
    OGL_TRACE();

    if(width<=0 || height<=0)
    {
        qWarning().nospace() << "AtmosphereRenderer::resizeEvent(" << width << ", " << height << "): non-positive-area framebuffer specified";
        width  = std::max(1, width);
        height = std::max(1, height);
    }

    viewportSize_=QSize(width,height);
    if(!luminanceRadianceFBO_) return;

    GLint origFBO=-1;
    gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &origFBO);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER,luminanceRadianceFBO_);

    GLint origTex=-1;
    gl.glGetIntegerv(GL_TEXTURE_BINDING_2D, &origTex);
    luminanceRenderTargetTexture_.bind();

    gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl.glFramebufferTexture(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,luminanceRenderTargetTexture_.textureId(),0);
    checkFramebufferStatus(gl, "Atmosphere renderer FBO");

    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, origFBO);
    gl.glBindTexture(GL_TEXTURE_2D, origTex);

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

int AtmosphereRenderer::initShaderReloading()
{
    OGL_TRACE();

    if(state_ != State::NotReady && state_ != State::ReadyToRender)
        return -1;

    state_ = State::ReloadingShaders;
    currentActivity_=QObject::tr("Reloading shaders...");
    loadingStepsDone_=0;
    totalLoadingStepsToDo_=0;
    loadShaders(CountStepsOnly{true});

    return totalLoadingStepsToDo_;
}

auto AtmosphereRenderer::stepShaderReloading() -> LoadingStatus
{
    OGL_TRACE();

    if(!totalLoadingStepsToDo_)
        return {0, -1};

    currentLoadingIterationStepCounter_=0;
    loadShaders(CountStepsOnly{false});

    if(loadingStepsDone_ == totalLoadingStepsToDo_)
        finalizeLoading();

    return {loadingStepsDone_, totalLoadingStepsToDo_};
}
