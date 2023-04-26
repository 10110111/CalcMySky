#include "TextureAverageComputer.hpp"
#include "util.hpp"
#include <QOpenGLFunctions_3_3_Core>

static bool isPOT(const int x)
{
    return roundDownToClosestPowerOfTwo(x) == x;
}

glm::vec4 TextureAverageComputer::getTextureAverageSimple(const GLuint texture, const int width, const int height,
                                                          const GLuint unusedTextureUnitNum)
{
    // Get average value of the pixels as the value of the deepest mipmap level
    gl.glActiveTexture(GL_TEXTURE0 + unusedTextureUnitNum);
    gl.glBindTexture(GL_TEXTURE_2D, texture);
    gl.glGenerateMipmap(GL_TEXTURE_2D);

    using namespace std;
    // Formula from the glspec, "Mipmapping" subsection in section 3.8.11 Texture Minification
    const auto totalMipmapLevels = 1+floor(log2(max(width,height)));
    const auto deepestLevel=totalMipmapLevels-1;

#ifndef NDEBUG
    // Sanity check
    int deepestMipmapLevelWidth=-1, deepestMipmapLevelHeight=-1;
    gl.glGetTexLevelParameteriv(GL_TEXTURE_2D, deepestLevel, GL_TEXTURE_WIDTH, &deepestMipmapLevelWidth);
    gl.glGetTexLevelParameteriv(GL_TEXTURE_2D, deepestLevel, GL_TEXTURE_HEIGHT, &deepestMipmapLevelHeight);
    assert(deepestMipmapLevelWidth==1);
    assert(deepestMipmapLevelHeight==1);
#endif

    glm::vec4 pixel;
    gl.glGetTexImage(GL_TEXTURE_2D, deepestLevel, GL_RGBA, GL_FLOAT, &pixel[0]);
    return pixel;
}

// Clobbers:
// GL_ACTIVE_TEXTURE, GL_TEXTURE_BINDING_2D,
// input texture's minification filter
glm::vec4 TextureAverageComputer::getTextureAverageWithWorkaround(const GLuint texture, const GLuint unusedTextureUnitNum)
{
    GLint oldVAO=-1;
    gl.glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVAO);
    GLint oldProgram=-1;
    gl.glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    GLint oldViewport[4];
    gl.glGetIntegerv(GL_VIEWPORT, oldViewport);
    GLint oldFBO=-1;
    gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldFBO);

    // Play it safe: we don't want to make the GPU struggle with very large textures
    // if we happen to make them ~4 times larger. Instead round the dimensions down.
    const auto potWidth  = roundDownToClosestPowerOfTwo(npotWidth);
    const auto potHeight = roundDownToClosestPowerOfTwo(npotHeight);

    gl.glActiveTexture(GL_TEXTURE0 + unusedTextureUnitNum);
    gl.glBindTexture(GL_TEXTURE_2D, texture);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    blitTexProgram->bind();
    blitTexProgram->setUniformValue("tex", unusedTextureUnitNum);

    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, potFBO);
    gl.glViewport(0,0,potWidth,potHeight);

    gl.glBindVertexArray(vao);
    gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl.glBindVertexArray(oldVAO);

    gl.glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldFBO);

    gl.glUseProgram(oldProgram);

    return getTextureAverageSimple(potTex, potWidth, potHeight, unusedTextureUnitNum);
}

glm::vec4 TextureAverageComputer::getTextureAverage(const GLuint texture, const GLuint unusedTextureUnitNum)
{
    if(workaroundNeeded && !(isPOT(npotWidth) && isPOT(npotHeight)))
        return getTextureAverageWithWorkaround(texture, unusedTextureUnitNum);
    return getTextureAverageSimple(texture, npotWidth, npotHeight, unusedTextureUnitNum);
}

void TextureAverageComputer::init(const GLuint unusedTextureUnitNum)
{
    GLuint texture = -1;
    gl.glGenTextures(1, &texture);
    assert(texture>0);
    gl.glActiveTexture(GL_TEXTURE0 + unusedTextureUnitNum);
    gl.glBindTexture(GL_TEXTURE_2D, texture);

    std::vector<glm::vec4> data;
    for(int n=0; n<10; ++n)
        data.emplace_back(1,1,1,1);
    for(int n=0; n<10; ++n)
        data.emplace_back(1,1,1,0);
    for(int n=0; n<10; ++n)
        data.emplace_back(1,1,0,0);
    for(int n=0; n<10; ++n)
        data.emplace_back(1,0,0,0);

    constexpr int width = 63;
    for(int n=data.size(); n<width; ++n)
        data.emplace_back(0,0,0,0);

    gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,data.size(),1,0,GL_RGBA,GL_FLOAT,&data[0][0]);
    const auto mipmapAverage = getTextureAverageSimple(texture, width, 1, unusedTextureUnitNum);

    const auto sum = std::accumulate(data.begin(), data.end(), glm::vec4(0,0,0,0));
    const auto trueAverage = sum / float(data.size());
    std::cerr << "Test texture true average: "
              << trueAverage[0] << ", "
              << trueAverage[1] << ", "
              << trueAverage[2] << ", "
              << trueAverage[3] << "\n";
    std::cerr << "Test texture mipmap average: "
              << mipmapAverage[0] << ", "
              << mipmapAverage[1] << ", "
              << mipmapAverage[2] << ", "
              << mipmapAverage[3] << "\n";

    const auto diff = mipmapAverage - trueAverage;
    using std::abs;
    const auto maxDiff = std::max({abs(diff[0]),abs(diff[1]),abs(diff[2]),abs(diff[3])});
    workaroundNeeded = maxDiff >= 2./255.;

    if(workaroundNeeded)
    {
        std::cerr << "WARNING: Mipmap average is unusable, will resize textures to "
                     "power-of-two size when average value is required.\n";
    }
    else
    {
        std::cerr << "Mipmap average works correctly\n";
    }

    gl.glBindTexture(GL_TEXTURE_2D, 0);
    gl.glDeleteTextures(1, &texture);

    inited = true;
}

// Clobbers: GL_TEXTURE_BINDING_2D, GL_ARRAY_BUFFER_BINDING
TextureAverageComputer::TextureAverageComputer(QOpenGLFunctions_3_3_Core& gl, const int texWidth, const int texHeight,
                                               const GLenum internalFormat, const GLuint unusedTextureUnitNum)
    : gl(gl)
    , npotWidth(texWidth)
    , npotHeight(texHeight)
{
    if(!inited) init(unusedTextureUnitNum);
    if(!workaroundNeeded) return;
    const auto potWidth  = roundDownToClosestPowerOfTwo(npotWidth);
    const auto potHeight = roundDownToClosestPowerOfTwo(npotHeight);
    if(potWidth == npotWidth && potHeight == npotHeight)
        return;

    GLint oldVAO=-1;
    gl.glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVAO);
    GLint oldFBO=-1;
    gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldFBO);

    gl.glGenFramebuffers(1, &potFBO);
    gl.glGenTextures(1, &potTex);
    gl.glActiveTexture(GL_TEXTURE0 + unusedTextureUnitNum);
    gl.glBindTexture(GL_TEXTURE_2D, potTex);
    gl.glTexImage2D(GL_TEXTURE_2D,0,internalFormat,potWidth,potHeight,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl.glBindTexture(GL_TEXTURE_2D,0);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER,potFBO);
    gl.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,potTex,0);
    [[maybe_unused]] const auto status=gl.glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    assert(status==GL_FRAMEBUFFER_COMPLETE);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);

    gl.glGenVertexArrays(1, &vao);
    gl.glBindVertexArray(vao);
    gl.glGenBuffers(1, &vbo);
    gl.glBindBuffer(GL_ARRAY_BUFFER, vbo);
    const GLfloat vertices[]=
    {
        -1, -1,
         1, -1,
        -1,  1,
         1,  1,
    };
    gl.glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);
    constexpr GLuint attribIndex=0;
    constexpr int coordsPerVertex=2;
    gl.glVertexAttribPointer(attribIndex, coordsPerVertex, GL_FLOAT, false, 0, 0);
    gl.glEnableVertexAttribArray(attribIndex);

    blitTexProgram.reset(new QOpenGLShaderProgram);
    blitTexProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, 1+R"(
#version 330
layout(location=0) in vec4 vertex;
out vec2 texcoord;
void main()
{
    gl_Position = vertex;
    texcoord = vertex.st*0.5+vec2(0.5);
}
)");
    blitTexProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, 1+R"(
#version 330
in vec2 texcoord;
out vec4 color;
uniform sampler2D tex;
void main()
{
    color = texture(tex, texcoord);
}
)");
    blitTexProgram->link();

    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldFBO);
    gl.glBindVertexArray(oldVAO);
}

TextureAverageComputer::~TextureAverageComputer()
{
    gl.glDeleteTextures(1, &potTex);
    gl.glDeleteFramebuffers(1, &potFBO);
    gl.glDeleteVertexArrays(1, &vao);
    gl.glDeleteBuffers(1, &vbo);
}
