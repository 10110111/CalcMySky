#ifndef INCLUDE_ONCE_E28E88C6_7992_4205_828C_8E04CC339B83
#define INCLUDE_ONCE_E28E88C6_7992_4205_828C_8E04CC339B83

#include <iostream>
#include <QString>
#include <QOpenGLFunctions_3_3_Core>

struct MustQuit{ int exitCode=1; };

inline std::ostream& operator<<(std::ostream& os, QString const& s)
{
    os << s.toStdString();
    return os;
}

void checkFramebufferStatus(QOpenGLFunctions_3_3_Core& gl, const char* fboDescription);

// Function useful only for debugging
void dumpActiveUniforms(QOpenGLFunctions_3_3_Core& gl, GLuint program);

std::string openglErrorString(GLenum error);

class UTF8Console
{
#ifdef Q_OS_WIN
    UINT oldConsoleCP;
public:
    UTF8Console()
        : oldConsoleCP(GetConsoleOutputCP())
    {
        SetConsoleOutputCP(65001); // UTF-8 console
    }
    ~UTF8Console()
    {
        restore();
    }
    void restore()
    {
        SetConsoleOutputCP(oldConsoleCP);
    }
#else
    void restore() {}
#endif
};

#endif
