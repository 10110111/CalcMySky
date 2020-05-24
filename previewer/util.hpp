#ifndef INCLUDE_ONCE_BCBE8DB3_A1E2_40C1_8E09_1DA9FE40B65D
#define INCLUDE_ONCE_BCBE8DB3_A1E2_40C1_8E09_1DA9FE40B65D

#include <filesystem>
#include <QOpenGLShaderProgram>
#include <QString>

QByteArray readFullFile(QString const& filename);
void addShaderCode(QOpenGLShaderProgram& program, QOpenGLShader::ShaderType type,
                   QString const& description, QByteArray const& sourceCode);
inline void addShaderFile(QOpenGLShaderProgram& program, QOpenGLShader::ShaderType type, QString const& filename)
{ addShaderCode(program, type, QObject::tr("shader file \"%1\"").arg(filename), readFullFile(filename)); }
inline void addShaderFile(QOpenGLShaderProgram& program, QOpenGLShader::ShaderType type, std::filesystem::path const& filename)
{ addShaderFile(program, type, QString::fromStdString(filename.u8string())); }
void link(QOpenGLShaderProgram& program, QString const& description);

#endif
