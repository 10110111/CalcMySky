#include "util.hpp"

#include <iostream>

#include "data.hpp"

void dumpActiveUniforms(const GLuint program)
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

void checkFramebufferStatus(const char*const fboDescription)
{
    GLenum status=gl.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status!=GL_FRAMEBUFFER_COMPLETE)
    {
        std::string errorDescription;
        switch(status)
        {
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            errorDescription="incomplete attachment";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            errorDescription="missing attachment";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            errorDescription="invalid framebuffer operation";
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            errorDescription="framebuffer unsupported";
            break;
        default:
            errorDescription="Unknown error "+std::to_string(status);
            break;
        }
        std::cerr << "Error: " << fboDescription << " is incomplete: " << errorDescription << "\n";
        throw MustQuit{};
    }
}

void renderUntexturedQuad()
{
	gl.glBindVertexArray(vao);
	gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	gl.glBindVertexArray(0);
}

void qtMessageHandler(const QtMsgType type, QMessageLogContext const&, QString const& message)
{
    switch(type)
    {
    case QtDebugMsg:
        std::cerr << "[DEBUG] " << message.toStdString() << "\n";
        break;
    case QtWarningMsg:
        if(message.startsWith("*** Problematic Fragment shader source code ***"))
            break;
        if(message.startsWith("QOpenGLShader::compile("))
            break;
        std::cerr << "[WARN] " << message.toStdString() << "\n";
        break;
    case QtCriticalMsg:
        std::cerr << "[ERROR] " << message.toStdString() << "\n";
        break;
    case QtFatalMsg:
        std::cerr << "[FATAL] " << message.toStdString() << "\n";
        break;
    }
}
