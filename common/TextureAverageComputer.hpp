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

#ifndef INCLUDE_ONCE_386B7A49_CC0D_40CF_AC50_73493DF4B289
#define INCLUDE_ONCE_386B7A49_CC0D_40CF_AC50_73493DF4B289

#include <memory>
#include <glm/glm.hpp>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>

class QOpenGLFunctions_3_3_Core;
class TextureAverageComputer
{
    QOpenGLFunctions_3_3_Core& gl;
    std::unique_ptr<QOpenGLShaderProgram> blitTexProgram;
    GLuint potFBO = 0;
    GLuint potTex = 0;
    GLuint vbo = 0, vao = 0;
    GLint npotWidth, npotHeight;
    static inline bool inited = false;
    static inline bool workaroundNeeded = false;

    void init(GLuint unusedTextureUnitNum);
    glm::vec4 getTextureAverageSimple(GLuint texture, int width, int height, GLuint unusedTextureUnitNum);
    glm::vec4 getTextureAverageWithWorkaround(GLuint texture, GLuint unusedTextureUnitNum);
public:
    glm::vec4 getTextureAverage(GLuint texture, GLuint unusedTextureUnitNum);
    TextureAverageComputer(QOpenGLFunctions_3_3_Core&, int texW, int texH,
                           GLenum internalFormat, GLuint unusedTextureUnitNum);
    ~TextureAverageComputer();
};

#endif
