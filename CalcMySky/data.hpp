#ifndef INCLUDE_ONCE_60F008D3_578F_4231_998C_EBB05192B4B7
#define INCLUDE_ONCE_60F008D3_578F_4231_998C_EBB05192B4B7

#include <set>
#include <map>
#include <cmath>
#include <array>
#include <vector>
#include <memory>
#include <QOpenGLShader>
#include <glm/glm.hpp>
#include "const.hpp"
#include "../common/types.hpp"

class OutputIndentIncrease
{
    inline static unsigned outputIndent=0;
    friend std::string indentOutput();
public:
     OutputIndentIncrease() { ++outputIndent; }
    ~OutputIndentIncrease() { --outputIndent; }
};
inline std::string indentOutput()
{
    return std::string(OutputIndentIncrease::outputIndent, ' ');
}

inline bool saveResultAsRadiance=false;
inline bool dbgNoSaveTextures=false;
inline bool dbgSaveGroundIrradiance=false;
inline bool dbgSaveScatDensityOrder2FromGround=false;
inline bool dbgSaveScatDensity=false;
inline bool dbgSaveDeltaScattering=false;
inline bool dbgSaveAccumScattering=false;

constexpr unsigned pointsPerWavelengthItem=4;

inline std::map<QString, QString> virtualSourceFiles;
inline std::map<QString, QString> virtualHeaderFiles;

inline GLuint vao, vbo;
enum FBOId
{
    FBO_FOR_TEXTURE_SAVING,
    FBO_TRANSMITTANCE,
    FBO_IRRADIANCE,
    FBO_DELTA_SCATTERING,
    FBO_SINGLE_SCATTERING,
    FBO_MULTIPLE_SCATTERING,

    FBO_COUNT
};
inline GLuint fbos[FBO_COUNT];
enum TextureId
{
    TEX_TRANSMITTANCE,
    TEX_IRRADIANCE,
    TEX_DELTA_IRRADIANCE,
    TEX_DELTA_SCATTERING,
    TEX_MULTIPLE_SCATTERING,
    TEX_DELTA_SCATTERING_DENSITY,

    TEX_COUNT
};
inline GLuint textures[TEX_COUNT];
// Accumulation of radiance to yield luminance
inline std::map<QString/*scatterer name*/, GLuint> accumulatedSingleScatteringTextures;

struct AtmosphereParameters
{
    struct Scatterer
    {
        GLfloat crossSectionAt1um = NAN;
        GLfloat angstromExponent = NAN;
        QString numberDensity;
        QString phaseFunction;
        PhaseFunctionType phaseFunctionType=PhaseFunctionType::General;
        QString name;

        explicit Scatterer(QString const& name) : name(name) {}
        bool valid() const
        {
            return std::isfinite(crossSectionAt1um) &&
                   std::isfinite(angstromExponent) &&
                   !numberDensity.isEmpty() &&
                   !phaseFunction.isEmpty() &&
                   !name.isEmpty();
        }
        glm::vec4 crossSection(glm::vec4 const wavelengths) const
        {
            constexpr float refWL=1000; // nm
            return crossSectionAt1um*pow(wavelengths/refWL, glm::vec4(-angstromExponent));
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
        bool valid() const
        {
            return !numberDensity.isEmpty() &&
                   absorptionCrossSection.size()==atmo.allWavelengths.size() &&
                   !name.isEmpty();
        }
        glm::vec4 crossSection(glm::vec4 const wavelengths) const
        {
            const auto i=atmo.wavelengthsIndex(wavelengths);
            return absorptionCrossSection[i];
        }
    };

    std::vector<glm::vec4> allWavelengths;
    std::vector<glm::vec4> solarIrradianceAtTOA;
    std::string textureOutputDir=".";
    GLint transmittanceTexW, transmittanceTexH;
    GLint irradianceTexW, irradianceTexH;
    glm::ivec4 scatteringTextureSize;
    glm::ivec2 eclipsedSingleScatteringTextureSize;
    unsigned scatteringOrdersToCompute;
    GLint numTransmittanceIntegrationPoints;
    GLint radialIntegrationPoints;
    GLint angularIntegrationPointsPerHalfRevolution;
    GLfloat earthRadius;
    GLfloat atmosphereHeight;
    double earthSunDistance;
    double earthMoonDistance;
    GLfloat sunAngularRadius; // calculated from earthSunDistance
    // moonAngularRadius is calculated from earthMoonDistance and other parameters on the fly, so isn't kept here
    std::vector<glm::vec4> groundAlbedo;
    std::vector<Scatterer> scatterers;
    std::vector<Absorber> absorbers;

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
};

inline AtmosphereParameters atmo;

#endif
