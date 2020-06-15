#ifndef INCLUDE_ONCE_60F008D3_578F_4231_998C_EBB05192B4B7
#define INCLUDE_ONCE_60F008D3_578F_4231_998C_EBB05192B4B7

#include <set>
#include <map>
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
inline std::vector<glm::vec4> allWavelengths;
inline std::vector<glm::vec4> solarIrradianceAtTOA;

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

inline std::string textureOutputDir=".";
inline GLint transmittanceTexW, transmittanceTexH;
inline GLint irradianceTexW, irradianceTexH;
inline glm::ivec4 scatteringTextureSize;
// XXX: keep in sync with those in previewer and renderer
inline auto scatTexWidth()  { return GLsizei(scatteringTextureSize[0]); }
inline auto scatTexHeight() { return GLsizei(scatteringTextureSize[1]*scatteringTextureSize[2]); }
inline auto scatTexDepth()  { return GLsizei(scatteringTextureSize[3]); }
inline unsigned scatteringOrdersToCompute;
inline GLint numTransmittanceIntegrationPoints;
inline GLint radialIntegrationPoints;
inline GLint angularIntegrationPointsPerHalfRevolution;
inline GLfloat earthRadius;
inline GLfloat atmosphereHeight;
inline double earthSunDistance;
inline GLfloat sunAngularRadius; // calculated from earthSunDistance
inline std::vector<glm::vec4> groundAlbedo;
inline unsigned wavelengthsIndex(glm::vec4 const& wavelengths)
{
    const auto it=std::find(allWavelengths.begin(), allWavelengths.end(), wavelengths);
    assert(it!=allWavelengths.end());
    return it-allWavelengths.begin();
}
struct ScattererDescription
{
    GLfloat crossSectionAt1um = NaN;
    GLfloat angstromExponent = NaN;
    QString numberDensity;
    QString phaseFunction;
    PhaseFunctionType phaseFunctionType=PhaseFunctionType::General;
    QString name;

    ScattererDescription(QString const& name) : name(name) {}
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
inline std::vector<ScattererDescription> scatterers;
struct AbsorberDescription
{
    QString numberDensity;
    QString name;
    std::vector<glm::vec4> absorptionCrossSection;

    AbsorberDescription(QString const& name) : name(name) {}
    bool valid() const
    {
        return !numberDensity.isEmpty() &&
               absorptionCrossSection.size()==allWavelengths.size() &&
               !name.isEmpty();
    }
    glm::vec4 crossSection(glm::vec4 const wavelengths) const
    {
        const auto i=wavelengthsIndex(wavelengths);
        return absorptionCrossSection[i];
    }
};
inline std::vector<AbsorberDescription> absorbers;

#endif
