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
#include "AtmosphereParameters.hpp"

inline bool saveResultAsRadiance=false;
inline bool dbgNoSaveTextures=false;
inline bool dbgSaveGroundIrradiance=false;
inline bool dbgSaveScatDensityOrder2FromGround=false;
inline bool dbgSaveScatDensity=false;
inline bool dbgSaveDeltaScattering=false;
inline bool dbgSaveAccumScattering=false;

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

inline AtmosphereParameters atmo;

#endif
