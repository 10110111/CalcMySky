#ifndef INCLUDE_ONCE_E28E88C6_7992_4205_828C_8E04CC339B83
#define INCLUDE_ONCE_E28E88C6_7992_4205_828C_8E04CC339B83

#include <iostream>
#include <glm/glm.hpp>
#include <QString>
#include <QVector3D>
#include <QVector4D>
#include <QGenericMatrix>
#include <QOpenGLFunctions_3_3_Core>
#include "../ShowMySky/api/ShowMySky/Exception.hpp"

#define DEFINE_EXPLICIT_BOOL(Type)          \
struct Type                                 \
{                                           \
    bool on=true;                           \
    explicit Type()=default;                \
    explicit Type(bool on) : on(on) {}      \
    operator bool() const { return on; }    \
}

template<typename T> auto sqr(T const& x) { return x*x; }
inline QVector3D toQVector(glm::dvec3 const& v) { return QVector3D(v.x, v.y, v.z); }
inline QVector3D toQVector(glm::vec3 const& v) { return QVector3D(v.x, v.y, v.z); }
inline QVector4D toQVector(glm::dvec4 const& v) { return QVector4D(v.x, v.y, v.z, v.w); }
inline QVector4D toQVector(glm::vec4 const& v) { return QVector4D(v.x, v.y, v.z, v.w); }
inline QMatrix3x3 toQMatrix(glm::mat3 const& m) { return QMatrix3x3(&transpose(m)[0][0]); }

struct MustQuit{ int exitCode=1; };

class InitializationError : public ShowMySky::Error
{
    QString message;
public:
    InitializationError(QString const& message) : message(message) {}
    QString errorType() const override { return QObject::tr("Initialization error"); }
    QString what() const override { return message; }
};

class OpenGLError : public ShowMySky::Error
{
    QString message;
public:
    OpenGLError(QString const& message) : message(message) {}
    QString errorType() const override { return QObject::tr("OpenGL error"); }
    QString what() const override { return message; }
};

class DataLoadError : public ShowMySky::Error
{
    QString message;
public:
    DataLoadError(QString const& message) : message(message) {}
    QString errorType() const override { return QObject::tr("Error loading data"); }
    QString what() const override { return message; }
};

class BadCommandLine : public ShowMySky::Error
{
    QString message;
public:
    BadCommandLine(QString const& message) : message(message) {}
    QString errorType() const override { return QObject::tr("Bad command line"); }
    QString what() const override { return message; }
};

class ParsingError : public ShowMySky::Error
{
    QString message;
    QString filename;
    int lineNumber;
public:
    ParsingError(QString const& filename, int lineNumber, QString const& message)
        : message(message), filename(filename), lineNumber(lineNumber) {}
    QString errorType() const override { return QObject::tr("Parsing error"); }
    QString what() const override { return QString("%1:%2: %3").arg(filename).arg(lineNumber).arg(message); }
};

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

// XXX: keep in sync with the same function in texture-coordinates.frag
inline float unitRangeToTexCoord(const float u, const int texSize)
{
    return (0.5+(texSize-1)*u)/texSize;
}

// XXX: keep in sync with the same function in texture-coordinates.frag
inline float texCoordToUnitRange(const float texCoord, const float texSize)
{
    return (texSize*texCoord-0.5)/(texSize-1);
}

// XXX: keep in sync with the same function in common-functions.frag
template<typename Number>
Number clampCosine(const Number x)
{
    return std::clamp(x, Number(-1), Number(1));
}

glm::mat4 radianceToLuminance(unsigned texIndex, std::vector<glm::vec4> const& allWavelengths);

// Rounds each float to \p precision bits.
void roundTexData(GLfloat* data, size_t size, int precision);

inline int roundDownToClosestPowerOfTwo(const int x)
{
    if(x==0) return 1;
    int shift=0;
    for(auto v=x;v;v>>=1)
        ++shift;
    return 1<<(shift-1);
}

#endif
