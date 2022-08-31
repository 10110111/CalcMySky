#ifndef INCLUDE_ONCE_D1C5F1EB_B6E7_4260_8AC3_57C35BB9B0DF
#define INCLUDE_ONCE_D1C5F1EB_B6E7_4260_8AC3_57C35BB9B0DF

#include <cmath>
#include <glm/glm.hpp>
#include <QtGui>
#include "types.hpp"
#include "util.hpp"

struct AtmosphereParameters
{
    DEFINE_EXPLICIT_BOOL(ForceNoEDSTextures);
    // We don't copy external spectra into output directory, so the renderer needs to skip loading them
    DEFINE_EXPLICIT_BOOL(SkipSpectra);

    struct Scatterer
    {
        QString name;
        AtmosphereParameters const& atmo;

        GLfloat scatteringCrossSectionAt1um = NAN;
        GLfloat angstromExponent = NAN;
        std::vector<glm::vec4> scatteringCrossSection_;
        QString numberDensity;
        QString phaseFunction;
        PhaseFunctionType phaseFunctionType=PhaseFunctionType::General;
        bool needsInterpolationGuides = false;

        explicit Scatterer(QString const& name, AtmosphereParameters const& atmo)
            : name(name)
            , atmo(atmo)
        {}
        bool valid(const SkipSpectra spectrumSkipped) const
        {
            return (spectrumSkipped || scatteringCrossSection_.size()==atmo.allWavelengths.size()) &&
                   !numberDensity.isEmpty() &&
                   !phaseFunction.isEmpty() &&
                   !name.isEmpty();
        }
        void finalizeLoading()
        {
            if(!(scatteringCrossSection_.empty() && std::isfinite(scatteringCrossSectionAt1um) && std::isfinite(angstromExponent)))
                return;

            for(const auto wavelengths : atmo.allWavelengths)
            {
                constexpr float refWL=1000; // nm
                const auto angstromFactor = pow(wavelengths/refWL, glm::vec4(-angstromExponent));
                const auto wlSetCrossSection = scatteringCrossSectionAt1um * angstromFactor;
                scatteringCrossSection_.push_back(wlSetCrossSection);
            }
        }
        glm::vec4 scatteringCrossSection(glm::vec4 const wavelengths) const
        {
            const auto i=atmo.wavelengthsIndex(wavelengths);
            return scatteringCrossSection_[i];
        }
    };
    struct Absorber
    {
        QString numberDensity;
        QString name;
        std::vector<glm::vec4> absorptionCrossSection;

        AtmosphereParameters const& atmo;

        Absorber(QString const& name, AtmosphereParameters const& atmo)
            : name(name)
            , atmo(atmo)
        {}
        bool valid(const SkipSpectra spectrumSkipped) const
        {
            return !numberDensity.isEmpty() &&
                   (spectrumSkipped || absorptionCrossSection.size()==atmo.allWavelengths.size()) &&
                   !name.isEmpty();
        }
        glm::vec4 crossSection(glm::vec4 const wavelengths) const
        {
            const auto i=atmo.wavelengthsIndex(wavelengths);
            return absorptionCrossSection[i];
        }
    };

    QString descriptionFileText;
    std::vector<glm::vec4> allWavelengths;
    std::vector<glm::vec4> solarIrradianceAtTOA;
    std::vector<glm::vec4> lightPollutionRelativeRadiance;
    std::string textureOutputDir=".";
    GLint transmittanceTexW, transmittanceTexH;
    GLint irradianceTexW, irradianceTexH;
    glm::ivec4 scatteringTextureSize;
    glm::ivec2 eclipsedSingleScatteringTextureSize;
    glm::ivec4 eclipsedDoubleScatteringTextureSize;
    glm::ivec2 lightPollutionTextureSize;
    unsigned eclipsedDoubleScatteringNumberOfAzimuthPairsToSample;
    unsigned eclipsedDoubleScatteringNumberOfElevationPairsToSample;
    unsigned scatteringOrdersToCompute;
    GLint numTransmittanceIntegrationPoints;
    GLint radialIntegrationPoints;
    GLint angularIntegrationPoints;
    GLint eclipseAngularIntegrationPoints;
    GLint lightPollutionAngularIntegrationPoints;
    GLfloat earthRadius;
    GLfloat atmosphereHeight;
    double earthSunDistance;
    double earthMoonDistance;
    GLfloat sunAngularRadius; // calculated from earthSunDistance
    float lengthOfHorizRayFromGroundToBorderOfAtmo; // calculated from atmosphereHeight and earthRadius
    // moonAngularRadius is calculated from earthMoonDistance and other parameters on the fly, so isn't kept here
    std::vector<glm::vec4> groundAlbedo;
    std::vector<Scatterer> scatterers;
    std::vector<Absorber> absorbers;
    bool allTexturesAreRadiance=false;
    bool noEclipsedDoubleScatteringTextures=false;
    static constexpr unsigned pointsPerWavelengthItem=4;
    static constexpr unsigned FORMAT_VERSION = 5;
    static constexpr char ALL_TEXTURES_ARE_RADIANCES_DIRECTIVE[]="all textures are radiances";
    static constexpr char NO_ECLIPSED_DOUBLE_SCATTERING_TEXTURES_DIRECTIVE[]="no eclipsed double scattering textures";
    static constexpr char SOLAR_IRRADIANCE_AT_TOA_KEY[]="solar irradiance at toa";
    static constexpr char WAVELENGTHS_KEY[]="wavelengths";

    void parse(QString const& atmoDescrFileName,
               ForceNoEDSTextures forceNoEDSTextures=ForceNoEDSTextures{false},
               SkipSpectra skipSpectra=SkipSpectra{false});
    // XXX: keep in sync with those in previewer and renderer
    auto scatTexWidth()  const { return GLsizei(scatteringTextureSize[0]); }
    auto scatTexHeight() const { return GLsizei(scatteringTextureSize[1]*scatteringTextureSize[2]); }
    auto scatTexDepth()  const { return GLsizei(scatteringTextureSize[3]); }
    unsigned wavelengthsIndex(glm::vec4 const& wavelengths) const
    {
        const auto it=std::find(allWavelengths.begin(), allWavelengths.end(), wavelengths);
        assert(it!=allWavelengths.end());
        return it-allWavelengths.begin();
    }
    static QString spectrumToString(std::vector<glm::vec4> const& spectrum);
};

#endif
