#include "util.hpp"

#include <memory>
#include <fstream>
#include <iostream>

#include "data.hpp"

std::string openglErrorString(const GLenum error)
{
    switch(error)
    {
#ifdef GL_NO_ERROR
    case GL_NO_ERROR: return "No error";
#endif
#ifdef GL_INVALID_ENUM
    case GL_INVALID_ENUM: return "Invalid enumerator";
#endif
#ifdef GL_INVALID_VALUE
    case GL_INVALID_VALUE: return "Invalid value";
#endif
#ifdef GL_INVALID_OPERATION
    case GL_INVALID_OPERATION: return "Invalid operation";
#endif
#ifdef GL_STACK_OVERFLOW
    case GL_STACK_OVERFLOW: return "Stack overflow";
#endif
#ifdef GL_STACK_UNDERFLOW
    case GL_STACK_UNDERFLOW: return "Stack underflow";
#endif
#ifdef GL_OUT_OF_MEMORY
    case GL_OUT_OF_MEMORY: return "Out of memory";
#endif
#ifdef GL_TABLE_TOO_LARGE
    case GL_TABLE_TOO_LARGE: return "Table too large";
#endif
#ifdef GL_INVALID_FRAMEBUFFER_OPERATION
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "Invalid framebuffer operation";
#endif
    }
    return "Error code " + std::to_string(error);
}

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

void renderQuad()
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

GLfloat* pixelsToSaveOrLoad()
{
    // Allocate this once at the beginning, so as to avoid std::bad_alloc when we have taken a fair
    // chunk of VRAM. Dunno why, but on my nvidia cards VRAM usage somehow makes the system deny us
    // even 1G of RAM.
    // As the scattering texture is the largest texture we are using here, allocating this size
    // would be enough for saving/loading any texture.
    static GLfloat* pixels=new GLfloat[scatTexWidth()*scatTexHeight()*scatTexDepth()*4];
    return pixels;
}
void saveTexture(const GLenum target, const GLuint texture, const std::string_view name,
                 const std::string_view path, std::vector<float> const& sizes)
{
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        std::cerr << "GL error on entry to saveTexture(): " << openglErrorString(err) << "\n";
        throw MustQuit{};
    }
    OutputIndentIncrease incr;
    std::cerr << indentOutput() << "Saving " << name << " to \"" << path << "\"... ";
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(target,texture);
    int w=1,h=1,d=1;
    gl.glGetTexLevelParameteriv(target,0,GL_TEXTURE_WIDTH,&w);
    if(target==GL_TEXTURE_2D || target==GL_TEXTURE_3D)
        gl.glGetTexLevelParameteriv(target,0,GL_TEXTURE_HEIGHT,&h);
    if(target==GL_TEXTURE_3D)
        gl.glGetTexLevelParameteriv(target,0,GL_TEXTURE_DEPTH,&d);
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        std::cerr << "GL error in saveTexture() on attempt to get texture dimensions: " << openglErrorString(err) << "\n";
        throw MustQuit{};
    }

    std::size_t pixelCount=1;
    for(const std::size_t s : sizes)
        pixelCount *= s;

    // Sanity check
    {
        const auto physicalSize = std::size_t(w)*h*d;
        if(physicalSize!=pixelCount)
        {
            std::cerr << "internal inconsistency detected: texture logical size " << pixelCount << " doesn't match physical size " << physicalSize << "\n";
            throw MustQuit{};
        }
    }

    const auto subpixelCount = 4*pixelCount;
    const auto subpixels=pixelsToSaveOrLoad();
    if(mustSaveTextureLayerByLayer && target==GL_TEXTURE_3D)
    {
        GLint origReadFramebuffer=0, origDrawFramebuffer=0;
        gl.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &origReadFramebuffer);
        gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &origDrawFramebuffer);
        if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
        {
            std::cerr << "GL error in saveTexture() on trying to save original framebuffer bindings: " << openglErrorString(err) << "\n";
            throw MustQuit{};
        }
        gl.glBindFramebuffer(GL_FRAMEBUFFER,fbos[FBO_FOR_TEXTURE_SAVING]);
        for(int layer=0; layer<d; ++layer)
        {
            gl.glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0, layer);
            checkFramebufferStatus("framebuffer for saving textures");
            gl.glReadPixels(0,0,w,h,GL_RGBA,GL_FLOAT, subpixels+layer*w*h*4);
            if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
            {
                std::cerr << "GL error in saveTexture() after glReadPixels() call: " << openglErrorString(err) << "\n";
                throw MustQuit{};
            }
        }
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, origReadFramebuffer);
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, origDrawFramebuffer);
    }
    else
    {
        gl.glGetTexImage(target, 0, GL_RGBA, GL_FLOAT, subpixels);
        if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
        {
            std::cerr << "GL error in saveTexture() after glGetTexImage() call: " << openglErrorString(err) << "\n";
            throw MustQuit{};
        }
    }
    std::ofstream out{std::string(path)};
    for(const uint16_t s : sizes)
        out.write(reinterpret_cast<const char*>(&s), sizeof s);
    out.write(reinterpret_cast<const char*>(subpixels), subpixelCount*sizeof subpixels[0]);
    out.close();
    std::cerr << "done\n";
}

void loadTexture(std::string const& path, const std::size_t width, const std::size_t height, const std::size_t depth)
{
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        std::cerr << "GL error on entry to loadTexture(" << width << "," << height << "," << depth << "): " << openglErrorString(err) << "\n";
        throw MustQuit{};
    }
    std::cerr << indentOutput() << "Loading texture from \"" << path << "\"... ";
    const std::size_t subpixelCount = 4*width*height*depth;
    const auto subpixels=pixelsToSaveOrLoad();
    std::ifstream file(path);
    if(!file)
    {
        std::cerr << "failed to open file\n";
        throw MustQuit{};
    }
    file.exceptions(std::ifstream::failbit);
    uint16_t sizes[4];
    file.read(reinterpret_cast<char*>(sizes), sizeof sizes);
    if(std::uintptr_t(sizes[0])*sizes[1]*sizes[2]*sizes[3] != std::uintptr_t(width)*height*depth)
        throw std::runtime_error("Bad texture size in file "+path);
    file.read(reinterpret_cast<char*>(subpixels), subpixelCount*sizeof subpixels[0]);
    gl.glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA32F_ARB,width,height,depth,0,GL_RGBA,GL_FLOAT,subpixels);
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        std::cerr << "GL error in loadTexture(" << width << "," << height << "," << depth << ") after glTexImage3D() call: " << openglErrorString(err) << "\n";
        throw MustQuit{};
    }
    std::cerr << "done\n";
}

void setupTexture(TextureId id, unsigned width, unsigned height)
{
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        std::cerr << "GL error on entry to setupTexture(" << width << "," << height << "): " << openglErrorString(err) << "\n";
        throw MustQuit{};
    }
    gl.glBindTexture(GL_TEXTURE_2D,textures[id]);
    gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F_ARB,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl.glBindTexture(GL_TEXTURE_2D,0);
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        std::cerr << "GL error in setupTexture(" << width << "," << height << "): " << openglErrorString(err) << "\n";
        throw MustQuit{};
    }
}
void setupTexture(TextureId id, unsigned width, unsigned height, unsigned depth)
{
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        std::cerr << "GL error on entry to setupTexture(" << width << "," << height << "," << depth << "): " << openglErrorString(err) << "\n";
        throw MustQuit{};
    }
    gl.glBindTexture(GL_TEXTURE_3D,textures[id]);
    gl.glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA32F_ARB,width,height,depth,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl.glBindTexture(GL_TEXTURE_3D,0);
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        std::cerr << "GL error in setupTexture(" << width << "," << height << "," << depth << "): " << openglErrorString(err) << "\n";
        throw MustQuit{};
    }
}
