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

inline std::vector<GLfloat> allWavelengths;
inline std::vector<GLfloat> solarIrradianceAtTOA;

inline std::map<QString, std::unique_ptr<QOpenGLShader>> allShaders;
inline QString constantsHeader;
inline QString densitiesHeader;
inline std::set<QString> internalShaders
{
    DENSITIES_SHADER_FILENAME,
    PHASE_FUNCTIONS_SHADER_FILENAME,
    COMPUTE_TRANSMITTANCE_SHADER_FILENAME,
};
inline GLuint vao, vbo;
enum
{
    FBO_TRANSMITTANCE,
    FBO_IRRADIANCE,
    FBO_SINGLE_SCATTERING,

    FBO_COUNT
};
inline GLuint fbos[FBO_COUNT];
enum
{
    TEX_TRANSMITTANCE,
    TEX_IRRADIANCE,
    TEX_FIRST_SCATTERING,

    TEX_COUNT
};
inline GLuint textures[TEX_COUNT];

inline std::string textureOutputDir=".";
inline GLint transmittanceTexW, transmittanceTexH;
inline GLint irradianceTexW, irradianceTexH;
inline glm::vec4 scatteringTextureSize;
inline GLint numTransmittanceIntegrationPoints;
inline GLint radialIntegrationPoints;
inline GLfloat earthRadius;
inline GLfloat atmosphereHeight;
inline double earthSunDistance;
inline GLfloat sunAngularRadius; // calculated from earthSunDistance
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
    std::vector<GLfloat> absorptionCrossSection;

    AbsorberDescription(QString const& name) : name(name) {}
    bool valid() const
    {
        return !numberDensity.isEmpty() &&
               absorptionCrossSection.size()==allWavelengths.size() &&
               !name.isEmpty();
    }
    glm::vec4 crossSection(glm::vec4 const wavelengths) const
    {
        const auto wlIt=std::find(allWavelengths.begin(), allWavelengths.end(), wavelengths[0]);
        assert(wlIt!=allWavelengths.end());
        const auto*const wl0=&*wlIt;
        assert(glm::vec4(wl0[0],wl0[1],wl0[2],wl0[3])==wavelengths);
        const auto i=wl0-&allWavelengths[0];
        return {absorptionCrossSection[i],absorptionCrossSection[i+1],absorptionCrossSection[i+2],absorptionCrossSection[i+3]};
    }
};
inline std::vector<AbsorberDescription> absorbers;

#endif
