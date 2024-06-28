#ifndef INCLUDE_ONCE_9100E17F_B7DD_4CC0_8D2F_9DBB66C7D23D
#define INCLUDE_ONCE_9100E17F_B7DD_4CC0_8D2F_9DBB66C7D23D

#include <vector>
#include <utility>
#include <complex>
#include <glm/glm.hpp>
#include <QtOpenGL>
#include "AtmosphereParameters.hpp"

class EclipsedDoubleScatteringPrecomputer
{
    QOpenGLFunctions_3_3_Core& gl;
    AtmosphereParameters const& atmo;
    const unsigned texSizeByViewAzimuth;
    const unsigned texSizeByViewElevation;
    const unsigned texSizeBySZA;

    const double texW, texH; // size of the intermediate texture we are rendering to
    std::vector<glm::vec4> texture_; // output 4D texture data
    std::vector<std::complex<float>> fourierIntermediate;
    std::vector<float> elevationsAboveHorizon, elevationsBelowHorizon;

    static constexpr unsigned VEC_ELEM_COUNT=4; // number of components in the partial radiance vector
    // The samples of radiance, one container per vec4 component. These containers are re-used for different altitudes and Sun elevations.
    // The separation into above-horizon and below-horizon parts is because at some altitudes there's a jump (or simply rapid change) in
    // radiance at the horizon, so spline interpolation would misbehave near this point if done without separation.
    std::vector<glm::vec2> samplesAboveHorizon[VEC_ELEM_COUNT];
    std::vector<glm::vec2> samplesBelowHorizon[VEC_ELEM_COUNT];
    // The samples of radiance interpolated over view elevations but not yet over view azimuths, one container per vec4 component.
    // These containers are re-used for different altitudes and Sun elevations.
    std::vector<float> radianceInterpolatedOverElevations[VEC_ELEM_COUNT];

    GLint origViewportWidth, origViewportHeight;

    float cosZenithAngleOfHorizon(const float altitude) const;
    std::pair<float,bool> eclipseTexCoordsToTexVars_cosVZA_VRIG(float vzaTexCoordInUnitRange, float altitude) const;
    void generateElevationsForEclipsedDoubleScattering(float cameraAltitude);
public:
    /* Preconditions:
     *   * Rendering FBO is bound, and the target texture is attached to it
     *   * program is bound
     *   * Transmittance texture uniform is set for program
     *   * VAO for a quad is bound
     */
    EclipsedDoubleScatteringPrecomputer(QOpenGLFunctions_3_3_Core& gl,
                                        AtmosphereParameters const& atmo,
                                        unsigned texSizeByViewAzimuth, unsigned texSizeByViewElevation,
                                        unsigned texSizeBySZA, unsigned texSizeByAltitude);
    ~EclipsedDoubleScatteringPrecomputer();

    void computeRadianceOnCoarseGrid(QOpenGLShaderProgram& program,
                                     std::vector<std::unique_ptr<QOpenGLTexture>> const& dataTextures,
                                     GLuint intermediateTextureTexUnitNum,
                                     double cameraAltitude, double sunZenithAngle, double moonZenithAngle,
                                     double moonAzimuthRelativeToSun, double earthMoonDistance);
    void convertRadianceToLuminance(glm::mat4 const& radianceToLuminance);
    void accumulateLuminance(EclipsedDoubleScatteringPrecomputer const& source, glm::mat4 const& sourceRadianceToLuminance);
    void generateTextureFromCoarseGridData(unsigned altIndex, unsigned szaIndex, double cameraAltitude);

    size_t appendCoarseGridSamplesTo(std::vector<glm::vec4>& data) const;
    void loadCoarseGridSamples(double cameraAltitude, glm::vec4 const* data, size_t numElements);
    unsigned numDataTextures() const;

    std::vector<glm::vec4> const& texture() const { return texture_; }
};

#endif
