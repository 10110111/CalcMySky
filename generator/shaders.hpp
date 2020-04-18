#ifndef INCLUDE_ONCE_2BE961E4_6CF8_4E2F_B5E5_DE8EEEE510F9
#define INCLUDE_ONCE_2BE961E4_6CF8_4E2F_B5E5_DE8EEEE510F9

#include <memory>
#include <QOpenGLShader>
#include <glm/glm.hpp>

#define DEFINE_EXPLICIT_BOOL(Type)          \
struct Type                                 \
{                                           \
    bool on=true;                           \
    explicit Type()=default;                \
    explicit Type(bool on) : on(on) {}      \
    operator bool() const { return on; }    \
}

DEFINE_EXPLICIT_BOOL(IgnoreCache);
QString getShaderSrc(QString const& fileName, IgnoreCache ignoreCache=IgnoreCache{false});
std::unique_ptr<QOpenGLShader> compileShader(QOpenGLShader::ShaderType type,
                                             QString source,
                                             QString const& description);
DEFINE_EXPLICIT_BOOL(UseGeomShader);
std::unique_ptr<QOpenGLShaderProgram> compileShaderProgram(QString const& mainSrcFileName,
                                                           const char* description,
                                                           UseGeomShader useGeomShader=UseGeomShader{false});
void initConstHeader(glm::vec4 const& wavelengths);
QString makeScattererDensityFunctionsSrc();
QString makeTransmittanceComputeFunctionsSrc(glm::vec4 const& wavelengths);
QString makeTotalScatteringCoefSrc();
QString makePhaseFunctionsSrc();
#endif
