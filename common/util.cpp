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

#include "util.hpp"
#include <cstring>
#include <qopengl.h>
#include "../common/cie-xyzw-functions.hpp"

std::string openglErrorString(const GLenum error)
{
    switch(error)
    {
    case GL_NO_ERROR: return "No error";
    case GL_INVALID_ENUM: return "Invalid enumerator";
    case GL_INVALID_VALUE: return "Invalid value";
    case GL_INVALID_OPERATION: return "Invalid operation";
    case GL_STACK_OVERFLOW: return "Stack overflow";
    case GL_STACK_UNDERFLOW: return "Stack underflow";
    case GL_OUT_OF_MEMORY: return "Out of memory";
    case GL_TABLE_TOO_LARGE: return "Table too large";
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "Invalid framebuffer operation";
    }
    return "Error code " + std::to_string(error);
}

void checkFramebufferStatus(QOpenGLFunctions_3_3_Core& gl, const char*const fboDescription)
{
    const auto status=gl.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status==GL_FRAMEBUFFER_COMPLETE) return;

    QString errorDescription;
    switch(status)
    {
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        errorDescription=QObject::tr("incomplete attachment");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        errorDescription=QObject::tr("missing attachment");
        break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        errorDescription=QObject::tr("invalid framebuffer operation");
        break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
        errorDescription=QObject::tr("framebuffer unsupported");
        break;
    default:
        errorDescription=QObject::tr("unknown error 0x%1").arg(status, 0, 16);
        break;
    }
    throw OpenGLError{QObject::tr("%1 is incomplete: %2").arg(fboDescription).arg(errorDescription)};
}

void dumpActiveUniforms(QOpenGLFunctions_3_3_Core& gl, const GLuint program)
{
    int uniformCount=0, maxLen=0;
    gl.glGetProgramiv(program,GL_ACTIVE_UNIFORMS,&uniformCount);
    gl.glGetProgramiv(program,GL_ACTIVE_UNIFORM_MAX_LENGTH,&maxLen);
    std::cerr << "Active uniforms:\n";
    for(int uniformIndex=0;uniformIndex<uniformCount;++uniformIndex)
    {
        std::vector<char> name(maxLen);
        GLsizei size;
        GLenum type;
        gl.glGetActiveUniform(program,uniformIndex,maxLen,nullptr,&size,&type,name.data());
        std::cerr << ' ' << name.data() << "\n";
    }
}

glm::mat4 radianceToLuminance(const unsigned texIndex, std::vector<glm::vec4> const& allWavelengths)
{
    using glm::mat4;
    const auto diag=[](GLfloat x, GLfloat y, GLfloat z, GLfloat w) { return mat4(x,0,0,0,
                                                                                 0,y,0,0,
                                                                                 0,0,z,0,
                                                                                 0,0,0,w); };
    const auto wlCount = 4*allWavelengths.size();
    // Weights for the trapezoidal quadrature rule
    const mat4 weights = wlCount==4            ? diag(0.5,1,1,0.5) :
                         texIndex==0           ? diag(0.5,1,1,1  ) :
                         texIndex+1==wlCount/4 ? diag(  1,1,1,0.5) :
                                                 diag(  1,1,1,1);
    const mat4 dlambda = weights * abs(allWavelengths.back()[3]-allWavelengths.front()[0]) / (wlCount-1.f);
    // Ref: Rapport BIPM-2019/05. Principles Governing Photometry, 2nd edition. Sections 6.2, 6.3.
    const mat4 maxLuminousEfficacy=diag(683.002f,683.002f,683.002f,1700.13f); // lm/W
    return maxLuminousEfficacy * mat4(wavelengthToXYZW(allWavelengths[texIndex][0]),
                                      wavelengthToXYZW(allWavelengths[texIndex][1]),
                                      wavelengthToXYZW(allWavelengths[texIndex][2]),
                                      wavelengthToXYZW(allWavelengths[texIndex][3])) * dlambda;
}

void roundTexData(GLfloat*const data, const size_t size, const int bitsOfPrecision)
{
    using Float = GLfloat;
    using FloatAsInt = uint32_t;
    static_assert(sizeof(FloatAsInt) == sizeof(Float));
    constexpr unsigned maxPrecision = std::numeric_limits<Float>::digits;

    const FloatAsInt mask = ~((1u << (maxPrecision - bitsOfPrecision)) - 1);
    std::cerr << "mask: 0x" << std::hex << mask << std::dec << " ... ";

    for(size_t i = 0; i < size; ++i)
    {
        uint32_t x;
        std::memcpy(&x, &data[i], sizeof x);
        x &= mask;
        std::memcpy(&data[i], &x, sizeof x);
    }
}
