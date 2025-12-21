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

#include <vector>
#include <string_view>
#include <glm/glm.hpp>

void generateInterpolationGuides2D(glm::vec4 const* data, unsigned vecIndex,
                                   unsigned width, unsigned height, unsigned rowStride,
                                   bool detailedSideIsWidth, uint8_t* angles);
void generateInterpolationGuidesForScatteringTexture(std::string_view filePath,
                                                     std::vector<glm::vec4> const& pixels,
                                                     std::vector<int> const& sizes);
