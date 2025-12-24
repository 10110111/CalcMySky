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

#ifndef INCLUDE_ONCE_3D249CBD_E858_4DD8_B420_066C5821EF30
#define INCLUDE_ONCE_3D249CBD_E858_4DD8_B420_066C5821EF30

#include <memory>
#include <QOpenGLShaderProgram>

class QOpenGLFunctions_3_3_Core;
class GLSLCosineQualityChecker
{
    static constexpr int width = 128, height = 1;

    QOpenGLFunctions_3_3_Core& gl;
    GLuint vao, vbo;
    GLuint texFBO;
    GLuint inputTexture;
    std::unique_ptr<QOpenGLShaderProgram> program;
    GLuint fbo;

    void loadShaders();
    void setupBuffers();
    void setupRenderTarget(int width, int height);
    void setupInputTextures(int width, int height);
public:
    GLSLCosineQualityChecker(QOpenGLFunctions_3_3_Core& gl);
    bool isGood();
    ~GLSLCosineQualityChecker();
};

#endif
