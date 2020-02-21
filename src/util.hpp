#ifndef INCLUDE_ONCE_C49956E1_F7B6_4759_8745_711BBDFE6FE7
#define INCLUDE_ONCE_C49956E1_F7B6_4759_8745_711BBDFE6FE7

#include <string>
#include <QVector4D>
#include <QOpenGLFunctions_3_3_Core>
#include <glm/glm.hpp>
#include "data.hpp"

extern QOpenGLFunctions_3_3_Core gl;

struct MustQuit{};

inline QVector4D QVec(glm::vec4 v) { return QVector4D(v.x, v.y, v.z, v.w); }
inline QString toString(int x) { return QString::number(x); }
inline QString toString(double x) { return QString::number(x, 'g', 17); }
inline QString toString(float x) { return QString::number(x, 'g', 9); }
inline QString toString(glm::vec2 v) { return QString("vec2(%1,%2)").arg(double(v.x), 0,'g',9)
                                                             .arg(double(v.y), 0,'g',9); }
inline QString toString(glm::vec4 v) { return QString("vec4(%1,%2,%3,%4)").arg(double(v.x), 0,'g',9)
                                                                   .arg(double(v.y), 0,'g',9)
                                                                   .arg(double(v.z), 0,'g',9)
                                                                   .arg(double(v.w), 0,'g',9); }
void setupDebugPrintCallback(QOpenGLContext& context);
std::string openglErrorString(GLenum error);
void setupTexture(TextureId id, unsigned width, unsigned height);
void setupTexture(TextureId id, unsigned width, unsigned height, unsigned depth);
inline void setUniformTexture(QOpenGLShaderProgram& program, GLenum target, TextureId id, GLint sampler, const char* uniformName)
{
    gl.glActiveTexture(GL_TEXTURE0+sampler);
    gl.glBindTexture(target, textures[id]);
    program.setUniformValue(uniformName,sampler);
}

inline void setDrawBuffers(std::vector<GLenum> const& bufs)
{
    gl.glDrawBuffers(bufs.size(), bufs.data());
}

void renderQuad();
void checkFramebufferStatus(const char*const fboDescription);
void qtMessageHandler(const QtMsgType type, QMessageLogContext const&, QString const& message);
void saveTexture(GLenum target, GLuint texture, std::string_view name, std::string_view path,
                 std::vector<float> const& sizes);
void loadTexture(std::string const& path, std::size_t width, std::size_t height, std::size_t depth);
void loadTexture(GLfloat* data, std::size_t width, std::size_t height, std::size_t depth);

// Function useful only for debugging
void dumpActiveUniforms(const GLuint program);

#endif
