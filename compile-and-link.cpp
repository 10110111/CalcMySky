#include <iostream>
#include <QApplication>
#include <QSurfaceFormat>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions_3_3_Core>

QOpenGLFunctions_3_3_Core gl;

void runTest(const bool reproduceBug)
{
    constexpr const char* vertShaderSrc = 1+R"(
#version 330
in vec3 vertex;
out vec3 position;
void main()
{
    position=vertex;
    gl_Position=vec4(position,1);
}
)";
    constexpr const char* fragShaderMainSrc_notReproducing = 1+R"(
#version 330
const int globalConst=200;

float f()
{
    float copy=globalConst;
    return copy;
}

void main() {}
)";
    constexpr const char* fragShaderMainSrc_reproducing = 1+R"(
#version 330
const int globalConst=200;

float f()
{
    const float copy=globalConst;
    return copy;
}

void main() {}
)";
    constexpr const char* fragShaderConstSrc = 1+R"(
#version 330
const int globalConst=200;
)";

    const char*const fragShaderMainSrc = reproduceBug ? fragShaderMainSrc_reproducing
                                                      : fragShaderMainSrc_notReproducing;
    const auto program = gl.glCreateProgram();
    const auto vert      = gl.glCreateShader(GL_VERTEX_SHADER);
    const auto fragMain  = gl.glCreateShader(GL_FRAGMENT_SHADER);
    const auto fragConst = gl.glCreateShader(GL_FRAGMENT_SHADER);
    gl.glShaderSource(vert     , 1, &vertShaderSrc     , nullptr);
    gl.glShaderSource(fragMain , 1, &fragShaderMainSrc , nullptr);
    gl.glShaderSource(fragConst, 1, &fragShaderConstSrc, nullptr);
    gl.glCompileShader(vert);
    gl.glCompileShader(fragMain);
    gl.glCompileShader(fragConst);
    for(const auto shader : {vert, fragMain, fragConst})
    {
        GLint compileSuccess = GL_FALSE;
        gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compileSuccess);
        if(!compileSuccess)
        {
            std::cout << "failed to compile a shader\n";
            return;
        }
    }

    gl.glAttachShader(program, vert);
    gl.glAttachShader(program, fragMain);
    gl.glAttachShader(program, fragConst);

    gl.glLinkProgram(program);
    GLint linkSuccess = GL_FALSE;
    gl.glGetProgramiv(program, GL_LINK_STATUS, &linkSuccess);

    if(linkSuccess)
    {
        std::cout << "linked successfully\n";
    }
    else
    {
        std::cout << "failed to link:\n";
        GLint len = 0;
        gl.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        if(len <= 0) return;
        std::string log(len, '\0');
        gl.glGetProgramInfoLog(program, len-1, nullptr, log.data());
        std::cout << log << "\n";
    }
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QSurfaceFormat format;
    format.setMajorVersion(3);
    format.setMinorVersion(3);
    format.setProfile(QSurfaceFormat::CoreProfile);

    QOpenGLContext context;
    context.setFormat(format);
    context.create();
    if(!context.isValid())
    {
        std::cout << "Failed to create OpenGL "
            << format.majorVersion() << '.'
            << format.minorVersion() << " context\n";
        return 1;
    }

    QOffscreenSurface surface;
    surface.setFormat(format);
    surface.create();
    if(!surface.isValid())
    {
        std::cout << "Failed to create OpenGL "
            << format.majorVersion() << '.'
            << format.minorVersion() << " offscreen surface\n";
        return 1;
    }

    context.makeCurrent(&surface);

    if(!gl.initializeOpenGLFunctions())
    {
        std::cout << "Failed to initialize OpenGL "
            << format.majorVersion() << '.'
            << format.minorVersion() << " functions\n";
        return 1;
    }

    std::cout << "OpenGL vendor  : " << gl.glGetString(GL_VENDOR) << "\n";
    std::cout << "OpenGL renderer: " << gl.glGetString(GL_RENDERER) << "\n";
    std::cout << "OpenGL version : " << gl.glGetString(GL_VERSION) << "\n\n";

    std::cout << "Succeeds on Intel: ";
    runTest(false);
    std::cout << "Fails on Intel   : ";
    runTest(true);
}
