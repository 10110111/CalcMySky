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

#ifndef INCLUDE_ONCE_5041B5F1_BF78_4C88_B28F_A06F80CB073A
#define INCLUDE_ONCE_5041B5F1_BF78_4C88_B28F_A06F80CB073A

#include <memory>

class QOpenGLContext;
class QOffscreenSurface;
std::pair<std::unique_ptr<QOffscreenSurface>, std::unique_ptr<QOpenGLContext>> initOpenGL();

#endif
