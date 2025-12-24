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

#include "ShowMySky/AtmosphereRenderer.hpp"
#include "../AtmosphereRenderer.hpp"

constexpr quint32 abi=ShowMySky_ABI_version;
#undef ShowMySky_ABI_version
extern "C"
{
extern SHOWMYSKY_DLL_PUBLIC const quint32 ShowMySky_ABI_version;
const quint32 ShowMySky_ABI_version=abi;
}

ShowMySky::AtmosphereRenderer* ShowMySky_AtmosphereRenderer_create(QOpenGLFunctions_3_3_Core* gl,
                                                                    QString const* pathToData,
                                                                    ShowMySky::Settings* tools,
                                                                    std::function<void(QOpenGLShaderProgram&)> const* drawSurface)
{
    return new AtmosphereRenderer(*gl,*pathToData,tools,*drawSurface);
}
