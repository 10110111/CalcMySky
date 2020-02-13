#include "util.hpp"

#include <memory>
#include <fstream>
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
    case QtInfoMsg:
        std::cerr << "[INFO] " << message.toStdString() << "\n";
        break;
    }
}

void saveTexture(const GLenum target, const GLuint texture, const std::string_view name,
                 const std::string_view path, std::vector<float> const& sizes)
{
    OutputIndentIncrease incr;
    std::cerr << indentOutput() << "Saving " << name << " to \"" << path << "\"...";
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(target,texture);
    int w=1,h=1,d=1;
    gl.glGetTexLevelParameteriv(target,0,GL_TEXTURE_WIDTH,&w);
    if(target==GL_TEXTURE_2D || target==GL_TEXTURE_3D)
        gl.glGetTexLevelParameteriv(target,0,GL_TEXTURE_HEIGHT,&h);
    if(target==GL_TEXTURE_3D)
        gl.glGetTexLevelParameteriv(target,0,GL_TEXTURE_DEPTH,&d);
    // NOTE: not using glm::vec4[] because in older versions it initializes the components in default constructor
    const auto elemCount = 4*std::size_t(w)*h*d;
    std::unique_ptr<GLfloat[]> pixels(new GLfloat[elemCount]);
    gl.glGetTexImage(target, 0, GL_RGBA, GL_FLOAT, pixels.get());
    std::ofstream out{std::string(path)};
    for(const uint16_t s : sizes)
        out.write(reinterpret_cast<const char*>(&s), sizeof s);
    out.write(reinterpret_cast<const char*>(pixels.get()), elemCount*sizeof pixels[0]);
    std::cerr << " done\n";
}

void loadTexture(std::string const& path, const unsigned width, const unsigned height, const unsigned depth)
{
    // NOTE: not using glm::vec4[] because in older versions it initializes the components in default constructor
    const std::size_t elemCount = 4*std::size_t(width)*height*depth;
    std::unique_ptr<GLfloat[]> pixels(new GLfloat[elemCount]);
    std::ifstream file(path);
    file.exceptions(std::ifstream::failbit);
    uint16_t sizes[4];
    file.read(reinterpret_cast<char*>(sizes), sizeof sizes);
    if(std::uintptr_t(sizes[0])*sizes[1]*sizes[2]*sizes[3] != std::uintptr_t(width)*height*depth)
        throw std::runtime_error("Bad texture size in file "+path);
    file.read(reinterpret_cast<char*>(pixels.get()), elemCount*sizeof pixels[0]);
    gl.glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA32F_ARB,width,height,depth,0,GL_RGBA,GL_FLOAT,pixels.get());
}
