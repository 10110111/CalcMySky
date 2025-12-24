/*
 * CalcMySky - a simulator of light scattering in planetary atmospheres
 * Copyright © 2025 Ruslan Kabatsayev
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

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
#include "../common/AtmosphereParameters.hpp"

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
    FBO_ECLIPSED_DOUBLE_SCATTERING,
    FBO_LIGHT_POLLUTION,

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
    TEX_ECLIPSED_DOUBLE_SCATTERING,
    TEX_LIGHT_POLLUTION_SCATTERING,
    TEX_LIGHT_POLLUTION_DELTA_SCATTERING,
    TEX_LIGHT_POLLUTION_SCATTERING_LUMINANCE,
    TEX_LIGHT_POLLUTION_SCATTERING_PREV_ORDER,

    TEX_COUNT
};
inline GLuint textures[TEX_COUNT];
// Accumulation of radiance to yield luminance
inline std::map<QString/*scatterer name*/, GLuint> accumulatedSingleScatteringTextures;

struct Options
{
    unsigned textureSavePrecision = 0; // 0 means not reduced
    bool openglDebug=false;
    bool openglDebugFull=false;
    bool printOpenGLInfoAndQuit=false;
    bool saveResultAsRadiance=false;
    bool dbgNoSaveTextures=false;
    bool dbgNoEDSTextures=false;
    bool dbgSaveGroundIrradiance=false;
    bool dbgSaveScatDensityOrder2FromGround=false;
    bool dbgSaveScatDensity=false;
    bool dbgSaveDeltaScattering=false;
    bool dbgSaveAccumScattering=false;
    bool dbgSaveLightPollutionIntermediateTextures=false;
};
inline Options opts;
inline AtmosphereParameters atmo;

#endif
