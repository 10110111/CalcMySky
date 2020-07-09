#ifndef INCLUDE_ONCE_2BE961E4_6CF8_4E2F_B5E5_DE8EEEE510F9
#define INCLUDE_ONCE_2BE961E4_6CF8_4E2F_B5E5_DE8EEEE510F9

#include <memory>
#include <QOpenGLShader>
#include <glm/glm.hpp>
#include "../common/util.hpp"

DEFINE_EXPLICIT_BOOL(IgnoreCache);
QString getShaderSrc(QString const& fileName, IgnoreCache ignoreCache=IgnoreCache{false});
DEFINE_EXPLICIT_BOOL(UseGeomShader);
std::unique_ptr<QOpenGLShaderProgram> compileShaderProgram(QString const& mainSrcFileName,
                                                           const char* description,
                                                           UseGeomShader useGeomShader=UseGeomShader{false},
                                                           std::vector<std::pair<QString, QString>>* sourcesToSave=nullptr);
void initConstHeader(glm::vec4 const& wavelengths);
QString makeScattererDensityFunctionsSrc();
QString makeTransmittanceComputeFunctionsSrc(glm::vec4 const& wavelengths);
QString makeTotalScatteringCoefSrc();
QString makePhaseFunctionsSrc();
#endif
