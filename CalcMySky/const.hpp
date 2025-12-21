/*
 * CalcMySky - a simulator of light scattering in planetary atmospheres
 * Copyright © 2025 Ruslan Kabatsayev
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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

#ifndef INCLUDE_ONCE_8BF515BD_53FC_406E_BD32_71E1EF0C73CC
#define INCLUDE_ONCE_8BF515BD_53FC_406E_BD32_71E1EF0C73CC

#include <limits>
#include "../common/const.hpp"

constexpr char DENSITIES_SHADER_FILENAME[]="densities.frag";
constexpr char PHASE_FUNCTIONS_SHADER_FILENAME[]="phase-functions.frag";
constexpr char TOTAL_SCATTERING_COEFFICIENT_SHADER_FILENAME[]="total-scattering-coefficient.frag";
constexpr char COMPUTE_TRANSMITTANCE_SHADER_FILENAME[]="compute-transmittance-functions.frag";
constexpr char CONSTANTS_HEADER_FILENAME[]="const.h.glsl";
constexpr char DENSITIES_HEADER_FILENAME[]="densities.h.glsl";
constexpr char GLSL_EXTENSIONS_HEADER_FILENAME[]="version.h.glsl";
constexpr char RADIANCE_TO_LUMINANCE_HEADER_FILENAME[]="radiance-to-luminance.h.glsl";
constexpr char PHASE_FUNCTIONS_HEADER_FILENAME[]="phase-functions.h.glsl";
constexpr char TOTAL_SCATTERING_COEFFICIENT_HEADER_FILENAME[]="total-scattering-coefficient.h.glsl";
constexpr char COMPUTE_SCATTERING_DENSITY_FILENAME[]="compute-scattering-density.frag";
constexpr char COMPUTE_ECLIPSED_DOUBLE_SCATTERING_FILENAME[]="compute-eclipsed-double-scattering.frag";
constexpr char SINGLE_SCATTERING_ECLIPSED_FILENAME[]="single-scattering-eclipsed.frag";
constexpr char DOUBLE_SCATTERING_ECLIPSED_FILENAME[]="double-scattering-eclipsed.frag";
constexpr char COMPUTE_INDIRECT_IRRADIANCE_FILENAME[]="compute-indirect-irradiance.frag";

#endif
