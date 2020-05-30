#include "util.hpp"
#include "../common/util.hpp"
#include <QFile>

QByteArray readFullFile(QString const& filename)
{
    QFile file(filename);
    if(!file.open(QFile::ReadOnly))
        throw DataLoadError{QObject::tr("Failed to open file \"%1\": %2").arg(filename).arg(file.errorString())};
    const auto data=file.readAll();
    if(file.error())
        throw DataLoadError{QObject::tr("Failed to read file \"%1\": %2").arg(filename).arg(file.errorString())};
    return data;
}

void addShaderCode(QOpenGLShaderProgram& program, const QOpenGLShader::ShaderType type,
                   QString const& description, QByteArray sourceCode,
                   std::vector<std::pair<QString,QString>> const& codeReplacements)
{
    for(const auto& [before,after] : codeReplacements)
        sourceCode.replace(before.toUtf8(), after.toUtf8());
    if(!program.addShaderFromSourceCode(type, sourceCode))
        throw DataLoadError{QObject::tr("Failed to compile %1:\n%2").arg(description).arg(program.log())};
}

void link(QOpenGLShaderProgram& program, QString const& description)
{
    if(!program.link())
        throw DataLoadError{QObject::tr("Failed to link %1:\n%2").arg(description).arg(program.log())};
}
