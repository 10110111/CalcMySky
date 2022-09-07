#include <memory>
#include <iostream>
#include <QApplication>
#include <QSurfaceFormat>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions_3_3_Core>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QSurfaceFormat format;
    format.setMajorVersion(3);
    format.setMinorVersion(3);
    format.setProfile(QSurfaceFormat::CoreProfile);

    auto context = std::make_unique<QOpenGLContext>();
    context->setFormat(format);
    context->create();
    if(!context->isValid())
    {
        std::cerr << "Failed to create OpenGL "
            << format.majorVersion() << '.'
            << format.minorVersion() << " context\n";
        return 1;
    }

    auto surface = std::make_unique<QOffscreenSurface>();
    surface->setFormat(format);
    surface->create();
    if(!surface->isValid())
    {
        std::cerr << "Failed to create OpenGL "
            << format.majorVersion() << '.'
            << format.minorVersion() << " offscreen surface\n";
        return 1;
    }

    context->makeCurrent(surface.get());

    QOpenGLFunctions_3_3_Core gl;
    if(!gl.initializeOpenGLFunctions())
    {
        std::cerr << "Failed to initialize OpenGL "
            << format.majorVersion() << '.'
            << format.minorVersion() << " functions\n";
        return 1;
    }

    std::cerr << "OpenGL vendor  : " << gl.glGetString(GL_VENDOR) << "\n";
    std::cerr << "OpenGL renderer: " << gl.glGetString(GL_RENDERER) << "\n";

    const auto openglVersion = gl.glGetString(GL_VERSION);
    if(openglVersion)
    {
        std::cerr << "OpenGL version : " << openglVersion << "\n";
    }
    else
    {
        std::cerr << "Failed to obtain OpenGL version\n";
    }


    const auto glslVersion = gl.glGetString(GL_SHADING_LANGUAGE_VERSION);
    if(glslVersion)
    {
        std::cerr << " GLSL  version : " << glslVersion << "\n";
    }
    else
    {
        std::cerr << "Failed to obtain GLSL version\n";
    }

    constexpr char ext_GL_ARB_shading_language_420pack[] = "GL_ARB_shading_language_420pack";
    if(context->hasExtension(QByteArray(ext_GL_ARB_shading_language_420pack)))
        std::cerr << ext_GL_ARB_shading_language_420pack << " is supported\n";
    else
        std::cerr << ext_GL_ARB_shading_language_420pack << " is NOT supported\n";

    QOpenGLShaderProgram program;
    program.addShaderFromSourceCode(QOpenGLShader::Vertex, 1+R"(
#version 330
in vec3 vertex;
out vec3 position;
void main()
{
    position=vertex;
    gl_Position=vec4(position,1);
}
)");
    auto files = app.arguments();
    files.pop_front();
    for(const auto file : files)
    {
        if(!program.addShaderFromSourceFile(QOpenGLShader::Fragment, file))
        {
            std::cerr << "Failed to compile " << file.toStdString() << "\n";
            return 1;
        }
    }
    if(!program.link())
    {
        std::cerr << "Failed to link\n";
        return 1;
    }
    std::cout << "Success\n";
}
