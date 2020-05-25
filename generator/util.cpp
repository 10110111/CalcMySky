#include "util.hpp"

#include <memory>
#include <cstring>
#include <iostream>
#include <QFile>

#include "data.hpp"

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

void saveTexture(const GLenum target, const GLuint texture, const std::string_view name,
                 const std::string_view path, std::vector<GLsizei> const& sizes)
{
    std::cerr << indentOutput() << "Saving " << name << " to \"" << path << "\"... ";
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        std::cerr << "GL error on entry to saveTexture(): " << openglErrorString(err) << "\n";
        throw MustQuit{};
    }

    OutputIndentIncrease incr;
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

    size_t pixelCount=1;
    for(const size_t s : sizes)
        pixelCount *= s;

    // Sanity check
    if(!sizes.empty())
    {
        const auto physicalSize = size_t(w)*h*d;
        if(physicalSize!=pixelCount)
        {
            std::cerr << "internal inconsistency detected: texture logical size " << pixelCount << " doesn't match physical size " << physicalSize << "\n";
            throw MustQuit{};
        }
    }

    const auto subpixelCount = 4*pixelCount;
    const std::unique_ptr<GLfloat[]> subpixels(new GLfloat[subpixelCount]);
    gl.glGetTexImage(target, 0, GL_RGBA, GL_FLOAT, subpixels.get());
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        std::cerr << "GL error in saveTexture() after glGetTexImage() call: " << openglErrorString(err) << "\n";
        throw MustQuit{};
    }

    QFile out(QByteArray::fromRawData(path.data(), path.size()));
    if(!out.open(QFile::WriteOnly))
    {
        std::cerr << "failed to open file: " << out.errorString().toStdString() << "\n";
        throw MustQuit{};
    }
    for(const uint16_t s : sizes)
        out.write(reinterpret_cast<const char*>(&s), sizeof s);
    out.write(reinterpret_cast<const char*>(subpixels.get()), subpixelCount*sizeof subpixels[0]);
    out.close();
    if(out.error())
    {
        std::cerr << "failed to write file: " << out.errorString().toStdString() << "\n";
        throw MustQuit{};
    }
    std::cerr << "done\n";
}

void setupTexture(TextureId id, const GLsizei width, const GLsizei height)
{
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        std::cerr << "GL error on entry to setupTexture(" << width << "," << height << "): " << openglErrorString(err) << "\n";
        throw MustQuit{};
    }
    gl.glBindTexture(GL_TEXTURE_2D,textures[id]);
    gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl.glBindTexture(GL_TEXTURE_2D,0);
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        std::cerr << "GL error in setupTexture(" << width << "," << height << "): " << openglErrorString(err) << "\n";
        throw MustQuit{};
    }
}
void setupTexture(TextureId id, const GLsizei width, const GLsizei height, const GLsizei depth)
{
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        std::cerr << "GL error on entry to setupTexture(" << width << "," << height << "," << depth << "): " << openglErrorString(err) << "\n";
        throw MustQuit{};
    }
    gl.glBindTexture(GL_TEXTURE_3D,textures[id]);
    gl.glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA32F,width,height,depth,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl.glBindTexture(GL_TEXTURE_3D,0);
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        std::cerr << "GL error in setupTexture(" << width << "," << height << "," << depth << "): " << openglErrorString(err) << "\n";
        throw MustQuit{};
    }
}

// ------------------------------------ KHR_debug support ----------------------------------------
std::string sourceToString(const GLenum source)
{
    switch(source)
    {
    case GL_DEBUG_SOURCE_API:             return "OpenGL API";
    case GL_DEBUG_SOURCE_SHADER_COMPILER: return "Shader compiler";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   return "Window system";
    case GL_DEBUG_SOURCE_THIRD_PARTY:     return "Third party";
    case GL_DEBUG_SOURCE_APPLICATION:     return "Application";
    case GL_DEBUG_SOURCE_OTHER:           return "Other";
    }
    return "Unknown source "+std::to_string(int(source));
}

std::string typeToString(const GLenum type)
{
    switch(type)
    {
    case GL_DEBUG_TYPE_ERROR:               return "Error";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "Deprecated behavior";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  return "Undefined behavior";
    case GL_DEBUG_TYPE_PERFORMANCE:         return "Performance warning";
    case GL_DEBUG_TYPE_PORTABILITY:         return "Portability warning";
    case GL_DEBUG_TYPE_OTHER:               return "Other";
    case GL_DEBUG_TYPE_MARKER:              return "Stream annotation";
    case GL_DEBUG_TYPE_PUSH_GROUP:          return "Entering a debug group";
    case GL_DEBUG_TYPE_POP_GROUP:           return "Leaving a debug group";
    }
    return "Unknown type "+std::to_string(int(type));
}

std::string severityToString(const GLenum severity)
{
    switch(severity)
    {
    case GL_DEBUG_SEVERITY_HIGH: return "High";
    case GL_DEBUG_SEVERITY_MEDIUM: return "Medium";
    case GL_DEBUG_SEVERITY_LOW: return "Low";
    case GL_DEBUG_SEVERITY_NOTIFICATION: return "Notification";
    }
    return "Unknown type "+std::to_string(int(severity));
}

void APIENTRY debugCallback(const GLenum source, const GLenum type, const GLuint /*id*/, const GLenum severity,
                            const GLsizei /*length*/, const GLchar*const message, const void*const /*userParam*/)
{
    if(severity==GL_DEBUG_SEVERITY_NOTIFICATION) return;
    if(source==GL_DEBUG_SOURCE_SHADER_COMPILER) return;
    if(severity==GL_DEBUG_SEVERITY_LOW && source==GL_DEBUG_SOURCE_API)
    {
        constexpr char unwantedEnding[]="is base level inconsistent. Check texture size.";
        if(std::strstr(message, unwantedEnding)) return;
    }

    std::cerr << "debug callback called: severity " << severityToString(severity)
                                     << ", source " << sourceToString(source)
                                       << ", type " << typeToString(type)
                                       << ", msg: " << message << "\n";
}


void setupDebugPrintCallback(QOpenGLContext& context)
{
    if(!context.hasExtension("GL_KHR_debug"))
    {
        std::cerr << "*** WARNING: debug extension is not supported, debug callback won't be set\n";
        return;
    }

    static const auto glDebugMessageCallback=reinterpret_cast<PFNGLDEBUGMESSAGECALLBACKPROC>
            (context.getProcAddress("glDebugMessageCallback"));

    static const auto glDebugMessageControl=reinterpret_cast<PFNGLDEBUGMESSAGECONTROLPROC>
            (context.getProcAddress("glDebugMessageControl"));

    glDebugMessageCallback(&debugCallback,NULL);
    glDebugMessageControl(GL_DONT_CARE,GL_DONT_CARE,GL_DONT_CARE,0,NULL,GL_TRUE);
    gl.glEnable(GL_DEBUG_OUTPUT);
}
