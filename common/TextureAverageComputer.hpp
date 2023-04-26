#ifndef INCLUDE_ONCE_386B7A49_CC0D_40CF_AC50_73493DF4B289
#define INCLUDE_ONCE_386B7A49_CC0D_40CF_AC50_73493DF4B289

#include <memory>
#include <glm/glm.hpp>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>

class QOpenGLFunctions_3_3_Core;
class TextureAverageComputer
{
    QOpenGLFunctions_3_3_Core& gl;
    std::unique_ptr<QOpenGLShaderProgram> blitTexProgram;
    GLuint potFBO = 0;
    GLuint potTex = 0;
    GLuint vbo = 0, vao = 0;
    GLint npotWidth, npotHeight;
    static inline bool inited = false;
    static inline bool workaroundNeeded = false;

    void init(GLuint unusedTextureUnitNum);
    glm::vec4 getTextureAverageSimple(GLuint texture, int width, int height, GLuint unusedTextureUnitNum);
    glm::vec4 getTextureAverageWithWorkaround(GLuint texture, GLuint unusedTextureUnitNum);
public:
    glm::vec4 getTextureAverage(GLuint texture, GLuint unusedTextureUnitNum);
    TextureAverageComputer(QOpenGLFunctions_3_3_Core&, int texW, int texH,
                           GLenum internalFormat, GLuint unusedTextureUnitNum);
    ~TextureAverageComputer();
};

#endif
