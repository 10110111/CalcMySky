#ifndef INCLUDE_ONCE_54FCB949_FBA5_49F1_AA53_E0503CC16A02
#define INCLUDE_ONCE_54FCB949_FBA5_49F1_AA53_E0503CC16A02

#include <iostream>
#include <iomanip>
#include <cassert>
#include <vector>

inline void dumpActiveUniforms(const GLuint programId, const QString programName)
{
    qDebug() << "Active uniforms for program" << programName << ":\n{";
    GLint uniformCount=0;
    gl.glGetProgramiv(programId, GL_ACTIVE_UNIFORMS, &uniformCount);
    GLint maxLen=0;
    gl.glGetProgramiv(programId, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLen);
    std::vector<GLchar> name(maxLen);
    for(int i=0;i<uniformCount;++i)
    {
        GLint size=0;
        GLenum type=0;
        gl.glGetActiveUniform(programId,i,maxLen,nullptr,&size,&type,name.data());
        std::cerr << " name: " << std::setfill(' ') << std::setw(maxLen) << name.data()
                  << ", size: " << size << ", type: 0x" << std::hex << type << std::dec << "\n";
    }
    qDebug() << "}";
}

#endif
