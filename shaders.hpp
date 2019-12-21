#ifndef INCLUDE_ONCE_2BE961E4_6CF8_4E2F_B5E5_DE8EEEE510F9
#define INCLUDE_ONCE_2BE961E4_6CF8_4E2F_B5E5_DE8EEEE510F9

#include <memory>
#include <QOpenGLShader>
#include <glm/glm.hpp>

std::unique_ptr<QOpenGLShader> compileShader(QOpenGLShader::ShaderType type,
                                             QString source,
                                             QString const& description);
std::unique_ptr<QOpenGLShaderProgram> compileShaderProgram(QString const& mainSrcFileName,
                                                           const char* description,
                                                           const bool useGeomShader=false);
void initConstHeader();
QString makeScattererDensityFunctionsSrc(glm::vec4 const& wavelengths);
QString makeTransmittanceComputeFunctionsSrc(glm::vec4 const& wavelengths);
#endif
