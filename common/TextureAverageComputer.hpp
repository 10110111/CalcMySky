#ifndef INCLUDE_ONCE_386B7A49_CC0D_40CF_AC50_73493DF4B289
#define INCLUDE_ONCE_386B7A49_CC0D_40CF_AC50_73493DF4B289

#include <memory>
#include <glm/glm.hpp>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>

class QOpenGLFunctions_3_3_Core;
class QOpenGLFunctions_4_3_Core;
class TextureAverageComputerGL33
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
    TextureAverageComputerGL33(QOpenGLFunctions_3_3_Core&, int texW, int texH,
                               GLenum internalFormat, GLuint unusedTextureUnitNum);
    ~TextureAverageComputerGL33();
};

class TextureAverageComputerGL43
{
public:
    // NOTE: we require the internalFormat to be RGBA32F for now, because it's
    // simpler to manage the shader then (which hard-codes this format too).
    TextureAverageComputerGL43(QOpenGLFunctions_4_3_Core& gl, int texW, int texH, GLuint unusedTextureUnitNum);
    glm::vec4 getTextureAverage(GLuint tex, GLuint unusedTextureUnitNum);
    ~TextureAverageComputerGL43();
private:
    void buildShaderProgram();

    QOpenGLFunctions_4_3_Core& gl;
    int texW;
    int texH;
    int workGroupSizeX;
    int workGroupSizeY;
    int pixelsPerSideAtOnce;
    std::unique_ptr<QOpenGLShaderProgram> computeProgram;
    GLuint scratchTex = 0;
    GLuint outputTex = 0;
};

class TextureAverageComputer
{
    std::unique_ptr<TextureAverageComputerGL33> averagerGL33;
    std::unique_ptr<TextureAverageComputerGL43> averagerGL43;
public:
    TextureAverageComputer(QOpenGLFunctions_3_3_Core&, QOpenGLFunctions_4_3_Core*,
                           int texW, int texH, GLenum internalFormat, GLuint unusedTextureUnitNum);
    glm::vec4 getTextureAverage(GLuint texture, GLuint unusedTextureUnitNum);
};

#endif
