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

constexpr auto allWavelengths=[]() constexpr
{
    constexpr float wlMin=360, wlMax=830;
    std::array<float, 16> arr{};
    const auto step=(wlMax-wlMin)/(arr.size()-1);
    for(unsigned n=0; n<arr.size(); ++n)
        arr[n]=wlMin+step*n;
    return arr;
}();
/* Data taken from https://www.nrel.gov/grid/solar-resource/assets/data/astmg173.zip
 * which is linked to at https://www.nrel.gov/grid/solar-resource/spectra-am1.5.html .
 * Values are in W/(m^2*nm).
 */
constexpr decltype(allWavelengths) solarIrradianceAtTOA=
   {1.037,1.249,1.684,1.975,
    1.968,1.877,1.854,1.818,
    1.723,1.604,1.516,1.408,
    1.309,1.23,1.142,1.062};
/* Data taken from http://www.iup.uni-bremen.de/gruppen/molspec/downloads/serdyuchenkogorshelevversionjuly2013.zip
 * which is linked to at http://www.iup.uni-bremen.de/gruppen/molspec/databases/referencespectra/o3spectra2011/index.html .
 * Data are for 233K. Values are in m^2/molecule.
 */
constexpr decltype(allWavelengths) ozoneAbsCrossSection=
   {1.394e-26,6.052e-28,4.923e-27,2.434e-26,
    7.361e-26,1.831e-25,3.264e-25,4.514e-25,
    4.544e-25,2.861e-25,1.571e-25,7.902e-26,
    4.452e-26,2.781e-26,1.764e-26,5.369e-27};
static_assert(allWavelengths.size()%4==0,"Non-round number of wavelengths");

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
    std::array<float,allWavelengths.size()> absorptionCrossSection{};

    AbsorberDescription(QString const& name) : name(name) {}
    bool valid() const
    {
        return !numberDensity.isEmpty() &&
               std::accumulate(absorptionCrossSection.begin(),absorptionCrossSection.end(), 0.) != 0 &&
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
