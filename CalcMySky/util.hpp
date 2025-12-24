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

#ifndef INCLUDE_ONCE_C49956E1_F7B6_4759_8745_711BBDFE6FE7
#define INCLUDE_ONCE_C49956E1_F7B6_4759_8745_711BBDFE6FE7

#include <string>
#include <iostream>
#include <string_view>
#include <QVector4D>
#include <QOpenGLFunctions_3_3_Core>
#include <glm/glm.hpp>
#include "data.hpp"
#include "../common/util.hpp"

extern QOpenGLFunctions_3_3_Core gl;

inline QVector4D QVec(glm::vec4 v) { return QVector4D(v.x, v.y, v.z, v.w); }
inline QString toString(int x) { return QString::number(x); }
inline QString toString(double x) { return QString::number(x, 'g', 17); }
inline QString toString(float x) { return QString::number(x, 'g', 9); }
inline QString toString(glm::vec2 v) { return QString("vec2(%1,%2)").arg(double(v.x), 0,'g',9)
                                                                    .arg(double(v.y), 0,'g',9); }
inline QString toString(glm::vec3 v) { return QString("vec3(%1,%2,%3)").arg(double(v.x), 0,'g',9)
                                                                       .arg(double(v.y), 0,'g',9)
                                                                       .arg(double(v.z), 0,'g',9); }
inline QString toString(glm::vec4 v) { return QString("vec4(%1,%2,%3,%4)").arg(double(v.x), 0,'g',9)
                                                                          .arg(double(v.y), 0,'g',9)
                                                                          .arg(double(v.z), 0,'g',9)
                                                                          .arg(double(v.w), 0,'g',9); }
inline QString toString(glm::mat4 const& m) { return QString("mat4(%1,%2,%3,%4,  %5,%6,%7,%8,  %9,%10,%11,%12,  %13,%14,%15,%16)")
          .arg(double(m[0][0]),0,'g',9).arg(double(m[0][1]),0,'g',9).arg(double(m[0][2]),0,'g',9).arg(double(m[0][3]),0,'g',9)
          .arg(double(m[1][0]),0,'g',9).arg(double(m[1][1]),0,'g',9).arg(double(m[1][2]),0,'g',9).arg(double(m[1][3]),0,'g',9)
          .arg(double(m[2][0]),0,'g',9).arg(double(m[2][1]),0,'g',9).arg(double(m[2][2]),0,'g',9).arg(double(m[2][3]),0,'g',9)
          .arg(double(m[3][0]),0,'g',9).arg(double(m[3][1]),0,'g',9).arg(double(m[3][2]),0,'g',9).arg(double(m[3][3]),0,'g',9); }
inline QMatrix4x4 toQMatrix(glm::mat4 const& m) { return QMatrix4x4(&m[0][0]).transposed(); }
void setupDebugPrintCallback(QOpenGLContext& context, bool needFullDebugOutput);
void setupTexture(TextureId id, GLsizei width, GLsizei height);
void setupTexture(TextureId id, GLsizei width, GLsizei height, GLsizei depth);
void setupTexture(GLuint tex, GLsizei width, GLsizei height, GLsizei depth);
inline void setUniformTexture(QOpenGLShaderProgram& program, GLenum target, GLuint texture, GLint sampler, const char* uniformName)
{
    gl.glActiveTexture(GL_TEXTURE0+sampler);
    gl.glBindTexture(target, texture);
    program.setUniformValue(uniformName,sampler);
}
inline void setUniformTexture(QOpenGLShaderProgram& program, GLenum target, TextureId id, GLint sampler, const char* uniformName)
{ setUniformTexture(program, target, textures[id], sampler, uniformName); }

inline void setDrawBuffers(std::vector<GLenum> const& bufs)
{
    gl.glDrawBuffers(GLsizei(bufs.size()), bufs.data());
}

void renderQuad();
inline void checkFramebufferStatus(const char*const fboDescription) { return checkFramebufferStatus(gl, fboDescription); }
void qtMessageHandler(const QtMsgType type, QMessageLogContext const&, QString const& message);
DEFINE_EXPLICIT_BOOL(ReturnTextureData);
std::vector<glm::vec4> saveTexture(GLenum target, GLuint texture, std::string_view name, std::string_view path,
                                   std::vector<int> const& sizes, ReturnTextureData=ReturnTextureData{false});
void createDirs(std::string const& path);

class OutputIndentIncrease
{
    inline static unsigned outputIndent=0;
    friend std::string indentOutput();
public:
     OutputIndentIncrease() { ++outputIndent; }
    ~OutputIndentIncrease() { --outputIndent; }
};
inline std::string indentOutput()
{
    return std::string(OutputIndentIncrease::outputIndent, ' ');
}

#define OPENGL_DEBUG_CHECK_ERROR(errorMessage)                                  \
do {                                                                            \
    if(opts.openglDebug)                                                        \
    {                                                                           \
        if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)                    \
        {                                                                       \
            std::cerr << errorMessage << ":" << openglErrorString(err) << "\n"; \
            throw MustQuit{};                                                   \
        }                                                                       \
    }                                                                           \
} while(0)

#endif
