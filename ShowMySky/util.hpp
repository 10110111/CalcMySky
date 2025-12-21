/*
 * CalcMySky - a simulator of light scattering in planetary atmospheres
 * Copyright © 2025 Ruslan Kabatsayev
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

#ifndef INCLUDE_ONCE_BCBE8DB3_A1E2_40C1_8E09_1DA9FE40B65D
#define INCLUDE_ONCE_BCBE8DB3_A1E2_40C1_8E09_1DA9FE40B65D

#include <filesystem>
#include <QOpenGLShaderProgram>
#include <QString>

QByteArray readFullFile(QString const& filename);
void addShaderCode(QOpenGLShaderProgram& program, QOpenGLShader::ShaderType type,
                   QString const& description, QByteArray sourceCode);
inline void addShaderFile(QOpenGLShaderProgram& program, QOpenGLShader::ShaderType type, QString const& filename)
{ addShaderCode(program, type, QObject::tr("shader file \"%1\"").arg(filename), readFullFile(filename)); }
inline void addShaderFile(QOpenGLShaderProgram& program, QOpenGLShader::ShaderType type, std::filesystem::path const& filename)
{ addShaderFile(program, type, QString::fromStdString(filename.u8string())); }
void link(QOpenGLShaderProgram& program, QString const& description);

#endif
