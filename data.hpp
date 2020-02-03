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

inline bool dbgSaveTransmittancePng=false;
inline bool dbgSaveDirectGroundIrradiance=false;
inline bool dbgSaveScatDensityOrder2FromGround=false;
inline bool dbgSaveScatDensityOrder2Full=false;

constexpr unsigned pointsPerWavelengthItem=4;
inline std::vector<glm::vec4> allWavelengths;
inline std::vector<glm::vec4> solarIrradianceAtTOA;

inline std::map<QString, std::unique_ptr<QOpenGLShader>> allShaders;
inline std::map<QString, QString> virtualSourceFiles;
inline std::map<QString, QString> virtualHeaderFiles;

inline GLuint vao, vbo;
enum FBOId
{
    FBO_TRANSMITTANCE,
    FBO_IRRADIANCE,
    FBO_SINGLE_SCATTERING,
    FBO_MULTIPLE_SCATTERING,

    FBO_COUNT
};
inline GLuint fbos[FBO_COUNT];
enum TextureId
{
    TEX_TRANSMITTANCE,
    TEX_DELTA_IRRADIANCE,
    TEX_FIRST_SCATTERING,
    TEX_DELTA_SCATTERING=TEX_FIRST_SCATTERING,
    TEX_DELTA_SCATTERING_DENSITY,

    TEX_COUNT
};
inline GLuint textures[TEX_COUNT];

inline std::string textureOutputDir=".";
inline GLint transmittanceTexW, transmittanceTexH;
inline GLint irradianceTexW, irradianceTexH;
inline glm::vec4 scatteringTextureSize;
inline GLint scatteringOrdersToCompute;
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
