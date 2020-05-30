#ifndef INCLUDE_ONCE_BCBE8DB3_A1E2_40C1_8E09_1DA9FE40B65D
#define INCLUDE_ONCE_BCBE8DB3_A1E2_40C1_8E09_1DA9FE40B65D

#include <filesystem>
#include <QOpenGLShaderProgram>
#include <QString>

QByteArray readFullFile(QString const& filename);
void addShaderCode(QOpenGLShaderProgram& program, QOpenGLShader::ShaderType type,
                   QString const& description, QByteArray sourceCode,
                   std::vector<std::pair<QString,QString>> const& codeReplacements={});
inline void addShaderFile(QOpenGLShaderProgram& program, QOpenGLShader::ShaderType type, QString const& filename,
                          std::vector<std::pair<QString,QString>> const& codeReplacements={})
{ addShaderCode(program, type, QObject::tr("shader file \"%1\"").arg(filename), readFullFile(filename),codeReplacements); }
inline void addShaderFile(QOpenGLShaderProgram& program, QOpenGLShader::ShaderType type, std::filesystem::path const& filename,
                          std::vector<std::pair<QString,QString>> const& codeReplacements={})
{ addShaderFile(program, type, QString::fromStdString(filename.u8string()),codeReplacements); }
void link(QOpenGLShaderProgram& program, QString const& description);

#endif
