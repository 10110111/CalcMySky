#include "EclipsedDoubleScatteringPrecomputer.hpp"

#include <iostream>
#include <chrono>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include <QFile>
#include <QOpenGLShaderProgram>

#include "const.hpp"
#include "TextureAverageComputer.hpp"
#include "fourier-interpolation.hpp"
#include "spline-interpolation.hpp"
#include "timing.hpp"
#include "util.hpp"

using namespace glm;
using std::sin;
using std::cos;
using std::sqrt;
using std::asin;
using std::exp;
using std::log;
using std::ceil;
using std::floor;
using std::lround;
using std::round;

namespace
{

// Returns sides of a rectangle that has the given area or a bit higher, and is
// as close to a square as possible.
std::pair<unsigned, unsigned> computeSidesOfSquarestRect(const unsigned area)
{
    const unsigned a = floor(area / floor(sqrt(area)));
    const unsigned b = ceil(area / a);
    return {a,b};
}

}

float EclipsedDoubleScatteringPrecomputer::cosZenithAngleOfHorizon(const float altitude) const
{
    const float R=atmo.earthRadius;
    const float h=altitude;
    return -std::sqrt(2*h*R+sqr(h))/(R+h);
}

void EclipsedDoubleScatteringPrecomputer::generateElevationsForEclipsedDoubleScattering(const float cameraAltitude)
{
    const auto trueHorizonToZenith = acos(cosZenithAngleOfHorizon(cameraAltitude));
    const auto trueHorizonToNadir = M_PI - trueHorizonToZenith;
    const auto elevationOfHorizon = M_PI/2-trueHorizonToZenith;
    constexpr auto mathHorizonToZenith = M_PI/2;
    constexpr auto mathHorizonToNadir  = M_PI/2;
    std::vector<float> baseElevations;
    const auto kMax = atmo.eclipsedDoubleScatteringNumberOfElevationPairsToSample;
    for(unsigned k=0; k<kMax; ++k)
    {
        constexpr double nMin=0;
        constexpr double nMax=9.14247130714592;
        const double n=nMin+nMax*(k+0.5)/kMax;
        baseElevations.push_back(exp(-5.373792168754 + 0.876897748729422*n - 0.0262205343007567*n*n));
    }
    assert(!baseElevations.empty());
    const double firstBaseElev=0.001*M_PI/180; // resolve the possible almost-jump in the vicinity of the horizon
    if(baseElevations[0] > firstBaseElev)
        baseElevations[0] = firstBaseElev;

    elevationsBelowHorizon.clear();
    // Elevations from horizon to nadir in backward direction, [-PI, -PI/2] at alt==0
    for(const auto baseElev : baseElevations)
        elevationsBelowHorizon.push_back(-M_PI - elevationOfHorizon + baseElev/mathHorizonToNadir*trueHorizonToNadir);
    // Elevations from nadir to horizon in forward direction, [-PI/2, 0] at alt==0
    for(unsigned i=baseElevations.size(); i-->0;)
        elevationsBelowHorizon.push_back(elevationOfHorizon - baseElevations[i]/mathHorizonToNadir*trueHorizonToNadir);

    elevationsAboveHorizon.clear();
    // Elevations from horizon to zenith in forward direction, [0, PI/2] at alt==0
    for(const auto baseElev : baseElevations)
        elevationsAboveHorizon.push_back(elevationOfHorizon + baseElev/mathHorizonToZenith*trueHorizonToZenith);
    // Elevations from zenith to horizon in backward direction, [PI/2, PI] at alt==0
    for(unsigned i=baseElevations.size(); i-->0;)
        elevationsAboveHorizon.push_back(M_PI-(elevationOfHorizon + baseElevations[i]/mathHorizonToZenith*trueHorizonToZenith));
}

// XXX: keep in sync with the GLSL version in texture-coordinates.{frag,h.glsl}
std::pair<float,bool> EclipsedDoubleScatteringPrecomputer::eclipseTexCoordsToTexVars_cosVZA_VRIG(const float vzaTexCoordInUnitRange,
                                                                                                 const float altitude) const
{
    using namespace std;

    const float distToHorizon = sqrt(sqr(altitude)+2*altitude*atmo.earthRadius);

    const bool viewRayIntersectsGround = vzaTexCoordInUnitRange<0.5;
    if(viewRayIntersectsGround)
    {
        // Bring the [0 .. 4.9xx] range to exact [0 .. 1]
        const float vzaTexCoordInDoubleRange = vzaTexCoordInUnitRange * (texSizeByViewElevation-1)/(texSizeByViewElevation/2-1);
        const float cosVZACoord = 1 - vzaTexCoordInDoubleRange;

        const float distMin=altitude;
        const float distMax=distToHorizon;
        const float distToGround=cosVZACoord*(distMax-distMin)+distMin;
        return {distToGround==0 ? -1 :
            clampCosine(-(sqr(distToHorizon)+sqr(distToGround)) / (2*distToGround*(altitude+atmo.earthRadius))),
                viewRayIntersectsGround};
    }
    else
    {
        // [0.50xx .. 1] --reverse--> [0.49xx .. 0]
        const float vzaTexCoordInUnitRangeReversed = 1 - vzaTexCoordInUnitRange;
        // [0.49xx .. 0] -> [1 .. 0]
        const float vzaTexCoordInDoubleRange = vzaTexCoordInUnitRangeReversed * (texSizeByViewElevation-1)/(texSizeByViewElevation/2-1);
        // [1 .. 0] --reverse--> [0 .. 1]
        const float cosVZACoord = 1 - vzaTexCoordInDoubleRange;

        const float distMin=atmo.atmosphereHeight-altitude;
        const float distMax=distToHorizon+atmo.lengthOfHorizRayFromGroundToBorderOfAtmo;
        const float distToTopAtmoBorder=cosVZACoord*(distMax-distMin)+distMin;
        return {distToTopAtmoBorder==0 ? 1 :
            clampCosine((sqr(atmo.lengthOfHorizRayFromGroundToBorderOfAtmo)-sqr(distToHorizon)-sqr(distToTopAtmoBorder)) /
                        (2*distToTopAtmoBorder*(altitude+atmo.earthRadius))),
                viewRayIntersectsGround};
    }
}

EclipsedDoubleScatteringPrecomputer::EclipsedDoubleScatteringPrecomputer(
          QOpenGLFunctions_3_3_Core& gl,
          AtmosphereParameters const& atmo,
          const unsigned texSizeByViewAzimuth, const unsigned texSizeByViewElevation,
          const unsigned texSizeBySZA, const unsigned texSizeByAltitude)
    : gl(gl)
    , atmo(atmo)
    , texSizeByViewAzimuth(texSizeByViewAzimuth)
    , texSizeByViewElevation(texSizeByViewElevation)
    , texSizeBySZA(texSizeBySZA)
    , texture_(texSizeByViewAzimuth*texSizeByViewElevation*texSizeBySZA*texSizeByAltitude)
    , fourierIntermediate(texSizeByViewAzimuth)
{
    // XXX: keep in sync with its use in GLSL computeDoubleScatteringEclipsedDensitySample() and C++ initTexturesAndFramebuffers()
    QSize subTexSize;
    const auto texSize = intermediateTexSize(atmo, &subTexSize);
    subTexW = subTexSize.width();
    subTexH = subTexSize.height();
    texW = texSize.width();
    texH = texSize.height();

    GLint viewport[4];
    gl.glGetIntegerv(GL_VIEWPORT, viewport);
    origViewportWidth=viewport[2];
    origViewportHeight=viewport[3];
    gl.glViewport(0,0, texW,texH);

    const auto nAzimuthPairsToSample=atmo.eclipsedDoubleScatteringNumberOfAzimuthPairsToSample;
    const auto nElevationPairsToSample=atmo.eclipsedDoubleScatteringNumberOfElevationPairsToSample;

    for(auto& s : samplesAboveHorizon)
        s.resize(2*nElevationPairsToSample*nAzimuthPairsToSample);
    for(auto& s : samplesBelowHorizon)
        s.resize(2*nElevationPairsToSample*nAzimuthPairsToSample);

    for(auto& r : radianceInterpolatedOverElevations)
        r.resize(texSizeByViewElevation*2*nAzimuthPairsToSample);
}

EclipsedDoubleScatteringPrecomputer::~EclipsedDoubleScatteringPrecomputer()
{
    gl.glViewport(0,0, origViewportWidth,origViewportHeight);
}

QSize EclipsedDoubleScatteringPrecomputer::intermediateTexSize(AtmosphereParameters const& atmo, QSize* subTexSize)
{
    const auto nAzimuthPairsToSample=atmo.eclipsedDoubleScatteringNumberOfAzimuthPairsToSample;
    const auto nElevationPairsToSample=atmo.eclipsedDoubleScatteringNumberOfElevationPairsToSample;

    const unsigned totalQuadraturePointsToSum = atmo.eclipseAngularIntegrationPoints * atmo.radialIntegrationPoints;
    // We arrange all the directions of camera view together with all the
    // directions of second scattering and radial points in such a way that the
    // second scattering directions that are to be summed over are contained in
    // smaller PoT-sized subtextures, and the full texture is a tiling of these
    // subtextures. We try to make the subtextures as square as possible to
    // reduce the number of summation on the CPU (so that isotropic mipmap
    // could be used at the deepest possible level), and also the grid of these
    // subtextures should be as square as possible, so that we don't hit
    // texture size limits.
    const int pot = std::ceil(std::log2(totalQuadraturePointsToSum));
    // NOTE: we want the subtexture to be horizontal, this will simplify getting data from the sum later.
    const int subTexH = 1 << (pot / 2);
    const int subTexW = (1 << pot) / subTexH;
    // Check that it's horizontal. The above computation should ensure this.
    if(subTexW < subTexH)
    {
        throw std::logic_error("EclipsedDoubleScatteringPrecomputer: subTexW<subTexH (" +
                               std::to_string(subTexW) + "<" + std::to_string(subTexH) + ")");
    }
    const auto [tileW, tileH] = computeSidesOfSquarestRect(4 * nAzimuthPairsToSample * nElevationPairsToSample);
    const int texW = tileW * subTexW;
    const int texH = tileH * subTexH;
    if(subTexSize)
        *subTexSize = {subTexW, subTexH};
    return {texW, texH};
}

void EclipsedDoubleScatteringPrecomputer::computeRadianceOnCoarseGrid(QOpenGLShaderProgram& program,
                                                                      const GLuint intermediateTextureName,
                                                                      const GLuint intermediateTextureTexUnitNum,
                                                                      const double cameraAltitude, const double sunZenithAngle,
                                                                      const double moonZenithAngle, const double moonAzimuthRelativeToSun,
                                                                      const double earthMoonDistance,
                                                                      GLuint& directionsTextureName)
{
    const auto nAzimuthPairsToSample=atmo.eclipsedDoubleScatteringNumberOfAzimuthPairsToSample;

    const dvec3 sunDir(sin(sunZenithAngle), 0, cos(sunZenithAngle));
    const dvec3 moonDir = dmat3(rotate(moonAzimuthRelativeToSun,dvec3(0,0,1)))*dvec3(sin(moonZenithAngle), 0, cos(moonZenithAngle));
    const double cameraMoonDistance=[cameraAltitude,moonZenithAngle,earthMoonDistance, this]{
        const auto hpR=cameraAltitude+atmo.earthRadius;
        const auto moonElevation=M_PI/2-moonZenithAngle;
        return -hpR*sin(moonElevation)+sqrt(sqr(earthMoonDistance)-0.5*sqr(hpR)*(1+cos(2*moonElevation)));
    }();
    const float moonAngularRadius=moonRadius/cameraMoonDistance;
    const dvec3 cameraPos(0,0,cameraAltitude);
    const dvec3 moonPos=cameraPos+cameraMoonDistance*moonDir;
    program.setUniformValue("cameraAltitude", GLfloat(cameraAltitude));
    program.setUniformValue("sunZenithAngle", GLfloat(sunZenithAngle));
    program.setUniformValue("moonAngularRadius", moonAngularRadius);
    program.setUniformValue("moonPositionRelativeToSunAzimuth", toQVector(moonPos));
    program.setUniformValue("eclipsedDoubleScatteringTextureSize", texSizeByViewAzimuth, texSizeByViewElevation, texSizeBySZA);

    // Sample double scattering on a very coarse grid of elevations and azimuths

    // These elevations span from forward horizon to backward horizon. This is to
    // use spline interpolation to compute the value at the zenith.
    generateElevationsForEclipsedDoubleScattering(cameraAltitude);

    const auto azimuths=[nAzimuthPairsToSample]
    {
        const auto step=M_PI/nAzimuthPairsToSample;
        std::vector<float> azimuths;
        for(unsigned i=0; i<nAzimuthPairsToSample; ++i)
            azimuths.push_back(i*step);
        return azimuths;
    }();
    assert(elevationsAboveHorizon.size()==2*atmo.eclipsedDoubleScatteringNumberOfElevationPairsToSample);
    assert(elevationsBelowHorizon.size()==2*atmo.eclipsedDoubleScatteringNumberOfElevationPairsToSample);
    assert(azimuths.size()==nAzimuthPairsToSample);

    const auto tileW = texW / subTexW;
    const auto tileH = texH / subTexH;
    std::vector<vec3> dirsPerTile;
    dirsPerTile.reserve(tileW*tileH);
    const auto elevCount=elevationsAboveHorizon.size(); // for each direction: above and below horizon
    for(unsigned azimIndex=0; azimIndex<azimuths.size(); ++azimIndex)
    {
        const auto azimuth=azimuths[azimIndex];
        for(const auto& elevs : {&elevationsAboveHorizon, &elevationsBelowHorizon})
        {
            for(unsigned elevIndex=0; elevIndex<elevCount; ++elevIndex)
            {
                const auto elev=(*elevs)[elevIndex];
                const auto viewDir=mat3(rotate(azimuth,vec3(0,0,1)))*vec3(cos(elev),0,sin(elev));
                dirsPerTile.push_back(viewDir);
            }
        }
    }

    if(!directionsTextureName)
        gl.glGenTextures(1, &directionsTextureName);
    const auto directionsTextureTexUnitNum = intermediateTextureTexUnitNum + 1;
    gl.glActiveTexture(GL_TEXTURE0 + directionsTextureTexUnitNum);
    gl.glBindTexture(GL_TEXTURE_2D, directionsTextureName);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,tileW,tileH,0,GL_RGB,GL_FLOAT,dirsPerTile.data());
    program.setUniformValue("cameraViewDirs", directionsTextureTexUnitNum);
    gl.glUniform2iv(gl.glGetUniformLocation(program.programId(), "subTexSize"),
                    1, std::array{int(subTexW), int(subTexH)}.data());

    gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl.glActiveTexture(GL_TEXTURE0 + intermediateTextureTexUnitNum);
    gl.glBindTexture(GL_TEXTURE_2D, intermediateTextureName);
    gl.glGenerateMipmap(GL_TEXTURE_2D);
    const float reductionFactor = std::min(subTexW, subTexH);
    const unsigned finalSumElemCount = subTexW*subTexH/sqr(reductionFactor);
    std::vector<vec4> computedData(tileW*tileH*finalSumElemCount);
    const int mipmapLevelToFetch = lround(std::log2(std::min(subTexW, subTexH)));
    gl.glGetTexImage(GL_TEXTURE_2D, mipmapLevelToFetch, GL_RGBA, GL_FLOAT, computedData.data());

    // Our summands are in a PoT subtexture, which is always either a square (aspect ratio 1:1),
    // or a double square (aspect ratio 2:1 (we've made sure that subTexW>=subTexH)), so the deepest
    // usable mip level yields either 1 or 2 subtexels.
    assert(finalSumElemCount==1 || finalSumElemCount==2);

    for(unsigned azimIndex = 0, dirIndex = 0; azimIndex<azimuths.size(); ++azimIndex)
    {
        for(const bool aboveHorizon : {true, false})
        {
            for(unsigned elevIndex = 0; elevIndex<elevCount; ++elevIndex, ++dirIndex)
            {
                const auto& elev = (aboveHorizon ? elevationsAboveHorizon : elevationsBelowHorizon)[elevIndex];

                // Extracting the pixel containing the sum - the integral over the view ray and scattering directions

                const unsigned pos = dirIndex * finalSumElemCount;
                // If we have a single summed texel, it's the average that we want to multiply by
                // the original number of texels to get the sum instead of the average. Otherwise
                // there's two texels to sum, each of which is an average. In this case we also need
                // to multiply by the same factor to get the sum of texels.
                const auto sum = finalSumElemCount==1 ? computedData[pos]
                                                      : computedData[pos]+computedData[pos+1];
                const auto integral = sum * sqr(reductionFactor);

                auto*const samples = aboveHorizon ? samplesAboveHorizon : samplesBelowHorizon;
                for(unsigned i=0; i<VEC_ELEM_COUNT; ++i)
                    samples[i][azimIndex*elevCount+elevIndex] = vec2(elev, integral[i]);
            }
        }
    }
}

void EclipsedDoubleScatteringPrecomputer::generateTextureFromCoarseGridData(const unsigned altIndex, const unsigned szaIndex, const double cameraAltitude)
{
    // 1. EclipsedDoubleScatteringPrecomputer::computeOnCoarseGrid()
    // This must have been called before we get here.

    const auto nAzimuthPairsToSample=atmo.eclipsedDoubleScatteringNumberOfAzimuthPairsToSample;
    const auto elevCount=elevationsAboveHorizon.size(); // for each direction: above and below horizon

    // 2. Apply log to all samples: interpolation works much better in logarithmic scale.
    for(unsigned azimIndex=0; azimIndex<nAzimuthPairsToSample; ++azimIndex)
    {
        for(unsigned elevIndex=0; elevIndex<elevCount; ++elevIndex)
        {
            const auto pos = azimIndex*elevCount+elevIndex;
            static constexpr float ALMOST_LOG_ZERO = -70;

            for(unsigned i=0; i<VEC_ELEM_COUNT; ++i)
            {
                auto& sample = samplesAboveHorizon[i][pos].y;
                sample = sample==0 ? ALMOST_LOG_ZERO : log(sample);
            }
            for(unsigned i=0; i<VEC_ELEM_COUNT; ++i)
            {
                auto& sample = samplesBelowHorizon[i][pos].y;
                sample = sample==0 ? ALMOST_LOG_ZERO : log(sample);
            }
        }
    }

    // 3. Interpolate the samples over the circles of elevations using second order spline interpolation
    for(unsigned azimIndex=0; azimIndex<nAzimuthPairsToSample; ++azimIndex)
    {
        SplineOrder2InterpolationFunction<float,vec2> intFuncsAboveHorizon[VEC_ELEM_COUNT];
        for(unsigned i=0; i<VEC_ELEM_COUNT; ++i)
            intFuncsAboveHorizon[i]=splineInterpolationOrder2(&samplesAboveHorizon[i][azimIndex*elevCount], elevCount);
        SplineOrder2InterpolationFunction<float,vec2> intFuncsBelowHorizon[VEC_ELEM_COUNT];
        for(unsigned i=0; i<VEC_ELEM_COUNT; ++i)
            intFuncsBelowHorizon[i]=splineInterpolationOrder2(&samplesBelowHorizon[i][azimIndex*elevCount], elevCount);
        for(unsigned texElevIndex=0; texElevIndex<texSizeByViewElevation; ++texElevIndex)
        {
            const auto [cosVZA, viewRayIntersectsGround]=
                eclipseTexCoordsToTexVars_cosVZA_VRIG(float(texElevIndex)/(texSizeByViewElevation-1), cameraAltitude);
            const auto& intFuncs =  viewRayIntersectsGround ?  intFuncsBelowHorizon  : intFuncsAboveHorizon;
            const double elevMin = (viewRayIntersectsGround ? elevationsBelowHorizon : elevationsAboveHorizon).front();
            const double elevMax = (viewRayIntersectsGround ? elevationsBelowHorizon : elevationsAboveHorizon).back();
            for(const bool oppositeAzimuth : {false, true})
            {
                auto elevation = oppositeAzimuth ? M_PI-asin(cosVZA) : asin(cosVZA);
                if(viewRayIntersectsGround && elevation > 0)
                    elevation -= 2*M_PI; // bring it to the negative range to match that of intFuncsBelowHorizon
                // We've not sampled too close to horizon to avoid rounding errors, so let's clamp to the edges of available range
                elevation = std::clamp(elevation, elevMin, elevMax);

                const auto index = texElevIndex*2*nAzimuthPairsToSample + azimIndex + (oppositeAzimuth ? nAzimuthPairsToSample : 0);
                for(unsigned i=0; i<VEC_ELEM_COUNT; ++i)
                    radianceInterpolatedOverElevations[i][index]=intFuncs[i].sample(elevation);
            }
        }
    }

    // 4. Interpolate the resulting interpolations over azimuths using Fourier interpolation and save into the final texture
    std::vector<float> interpolated[VEC_ELEM_COUNT];
    for(auto& in : interpolated)
        in.resize(texSizeByViewAzimuth);
    for(unsigned texElevIndex=0; texElevIndex<texSizeByViewElevation; ++texElevIndex)
    {
        const auto indexInPrevStepArray = texElevIndex*2*nAzimuthPairsToSample;
        const auto indexOfLineInTexture = texSizeByViewAzimuth*(texSizeByViewElevation*(texSizeBySZA*altIndex + szaIndex) +
                                                                texElevIndex);
        for(unsigned i=0; i<VEC_ELEM_COUNT; ++i)
            fourierInterpolate(&radianceInterpolatedOverElevations[i][indexInPrevStepArray], 2*nAzimuthPairsToSample,
                               fourierIntermediate.data(),
                               interpolated[i].data(), texSizeByViewAzimuth);
        for(unsigned i=0; i<texSizeByViewAzimuth; ++i)
            texture_[indexOfLineInTexture+i] = vec4(interpolated[0][i],interpolated[1][i],interpolated[2][i],interpolated[3][i]);
    }
}

void EclipsedDoubleScatteringPrecomputer::convertRadianceToLuminance(glm::mat4 const& radianceToLuminance)
{
    const auto nAzimuthPairsToSample=atmo.eclipsedDoubleScatteringNumberOfAzimuthPairsToSample;
    const auto elevCount=elevationsAboveHorizon.size(); // for each direction: above and below horizon

    for(unsigned azimIndex=0; azimIndex<nAzimuthPairsToSample; ++azimIndex)
    {
        for(unsigned elevIndex=0; elevIndex<elevCount; ++elevIndex)
        {
            const auto pos = azimIndex*elevCount+elevIndex;
            const auto v = vec4(samplesAboveHorizon[0][pos].y,
                                samplesAboveHorizon[1][pos].y,
                                samplesAboveHorizon[2][pos].y,
                                samplesAboveHorizon[3][pos].y);
            const auto transformed = radianceToLuminance * v;
            for(unsigned i=0; i<VEC_ELEM_COUNT; ++i)
                samplesAboveHorizon[i][pos].y = transformed[i];
        }
        for(unsigned elevIndex=0; elevIndex<elevCount; ++elevIndex)
        {
            const auto pos = azimIndex*elevCount+elevIndex;
            const auto v = vec4(samplesBelowHorizon[0][pos].y,
                                samplesBelowHorizon[1][pos].y,
                                samplesBelowHorizon[2][pos].y,
                                samplesBelowHorizon[3][pos].y);
            const auto transformed = radianceToLuminance * v;
            for(unsigned i=0; i<VEC_ELEM_COUNT; ++i)
                samplesBelowHorizon[i][pos].y = transformed[i];
        }
    }
}

void EclipsedDoubleScatteringPrecomputer::accumulateLuminance(EclipsedDoubleScatteringPrecomputer const& source,
                                                              glm::mat4 const& sourceRadianceToLuminance)
{
    const auto nAzimuthPairsToSample=atmo.eclipsedDoubleScatteringNumberOfAzimuthPairsToSample;
    const auto elevCount=elevationsAboveHorizon.size(); // for each direction: above and below horizon

    for(unsigned azimIndex=0; azimIndex<nAzimuthPairsToSample; ++azimIndex)
    {
        for(unsigned elevIndex=0; elevIndex<elevCount; ++elevIndex)
        {
            const auto pos = azimIndex*elevCount+elevIndex;
            const auto v = vec4(source.samplesAboveHorizon[0][pos].y,
                                source.samplesAboveHorizon[1][pos].y,
                                source.samplesAboveHorizon[2][pos].y,
                                source.samplesAboveHorizon[3][pos].y);
            const auto transformed = sourceRadianceToLuminance * v;
            for(unsigned i=0; i<VEC_ELEM_COUNT; ++i)
                samplesAboveHorizon[i][pos].y += transformed[i];
        }
        for(unsigned elevIndex=0; elevIndex<elevCount; ++elevIndex)
        {
            const auto pos = azimIndex*elevCount+elevIndex;
            const auto v = vec4(source.samplesBelowHorizon[0][pos].y,
                                source.samplesBelowHorizon[1][pos].y,
                                source.samplesBelowHorizon[2][pos].y,
                                source.samplesBelowHorizon[3][pos].y);
            const auto transformed = sourceRadianceToLuminance * v;
            for(unsigned i=0; i<VEC_ELEM_COUNT; ++i)
                samplesBelowHorizon[i][pos].y += transformed[i];
        }
    }
}

// The only data that's really expensive to compute is radiance. As we want to
// keep storage use as small as possible, we intend the other data like the list
// of elevations to be restored on loading by the renderer.
size_t EclipsedDoubleScatteringPrecomputer::appendCoarseGridSamplesTo(std::vector<glm::vec4>& data) const
{
    const auto initialSize = data.size();
    for(unsigned n=0; n<samplesAboveHorizon[0].size(); ++n)
    {
        data.emplace_back(samplesAboveHorizon[0][n].y,
                          samplesAboveHorizon[1][n].y,
                          samplesAboveHorizon[2][n].y,
                          samplesAboveHorizon[3][n].y);
    }
    for(unsigned n=0; n<samplesBelowHorizon[0].size(); ++n)
    {
        data.emplace_back(samplesBelowHorizon[0][n].y,
                          samplesBelowHorizon[1][n].y,
                          samplesBelowHorizon[2][n].y,
                          samplesBelowHorizon[3][n].y);
    }
    const auto numElementsWritten = data.size() - initialSize;
    return numElementsWritten;
}

void EclipsedDoubleScatteringPrecomputer::loadCoarseGridSamples(const double cameraAltitude,
                                                                glm::vec4 const* data, const size_t numElements)
{
    generateElevationsForEclipsedDoubleScattering(cameraAltitude);

    const auto numPointsPerElevSet = numElements/2;
    unsigned dataOffset = 0;

    for(unsigned i=0; i<VEC_ELEM_COUNT; ++i)
        samplesAboveHorizon[i].resize(numPointsPerElevSet);
    for(unsigned n = 0, elevIndex = 0; n<numPointsPerElevSet; ++n)
    {
        const auto elev = elevationsAboveHorizon[elevIndex];
        samplesAboveHorizon[0][n] = vec2(elev, data[dataOffset][0]);
        samplesAboveHorizon[1][n] = vec2(elev, data[dataOffset][1]);
        samplesAboveHorizon[2][n] = vec2(elev, data[dataOffset][2]);
        samplesAboveHorizon[3][n] = vec2(elev, data[dataOffset][3]);
        ++dataOffset;

        if(elevIndex+1 < elevationsAboveHorizon.size())
            ++elevIndex;
        else
            elevIndex=0;
    }

    for(unsigned i=0; i<VEC_ELEM_COUNT; ++i)
        samplesBelowHorizon[i].resize(numPointsPerElevSet);
    for(unsigned n = 0, elevIndex = 0; n<numPointsPerElevSet; ++n)
    {
        const auto elev = elevationsBelowHorizon[elevIndex];
        samplesBelowHorizon[0][n] = vec2(elev, data[dataOffset][0]);
        samplesBelowHorizon[1][n] = vec2(elev, data[dataOffset][1]);
        samplesBelowHorizon[2][n] = vec2(elev, data[dataOffset][2]);
        samplesBelowHorizon[3][n] = vec2(elev, data[dataOffset][3]);
        ++dataOffset;

        if(elevIndex+1 < elevationsBelowHorizon.size())
            ++elevIndex;
        else
            elevIndex=0;
    }
}
