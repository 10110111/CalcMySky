#include "TextureAverageComputer.hpp"
#include "util.hpp"
#include <limits>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLFunctions_4_3_Core>

// Can happen on macOS
#ifndef GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS
# define GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS 0x90EB
#endif
#ifndef GL_TEXTURE_UPDATE_BARRIER_BIT
# define GL_TEXTURE_UPDATE_BARRIER_BIT     0x00000100
#endif

static bool isPOT(const int x)
{
    return roundDownToClosestPowerOfTwo(x) == x;
}

glm::vec4 TextureAverageComputerGL33::getTextureAverageSimple(const GLuint texture, const int width, const int height,
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
glm::vec4 TextureAverageComputerGL33::getTextureAverageWithWorkaround(const GLuint texture, const GLuint unusedTextureUnitNum)
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

glm::vec4 TextureAverageComputerGL33::getTextureAverage(const GLuint texture, const GLuint unusedTextureUnitNum)
{
    if(workaroundNeeded && !(isPOT(npotWidth) && isPOT(npotHeight)))
        return getTextureAverageWithWorkaround(texture, unusedTextureUnitNum);
    return getTextureAverageSimple(texture, npotWidth, npotHeight, unusedTextureUnitNum);
}

void TextureAverageComputerGL33::init(const GLuint unusedTextureUnitNum)
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
TextureAverageComputerGL33::TextureAverageComputerGL33(QOpenGLFunctions_3_3_Core& gl,
                                                       const int texWidth, const int texHeight,
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

TextureAverageComputerGL33::~TextureAverageComputerGL33()
{
    gl.glDeleteTextures(1, &potTex);
    gl.glDeleteFramebuffers(1, &potFBO);
    gl.glDeleteVertexArrays(1, &vao);
    gl.glDeleteBuffers(1, &vbo);
}

TextureAverageComputerGL43::TextureAverageComputerGL43(QOpenGLFunctions_4_3_Core& gl,
                                                       const int texW, const int texH,
                                                       const GLuint unusedTextureUnitNum)
    : gl(gl)
    , texW(texW)
    , texH(texH)
    , workGroupSizeX(32)
    , pixelsPerSideAtOnce(5)
{
    GLint maxInvocations = -1;
    gl.glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxInvocations);
    if(maxInvocations % workGroupSizeX)
    {
        std::cerr << "WARNING: Max work group invocations " << maxInvocations
                  << " is not a multiple of " << workGroupSizeX << "\n";
    }
    workGroupSizeY = maxInvocations / workGroupSizeX;

    buildShaderProgram();

    gl.glActiveTexture(GL_TEXTURE0 + unusedTextureUnitNum);
    gl.glGenTextures(1, &scratchTex);
    gl.glBindTexture(GL_TEXTURE_2D, scratchTex);
    GLint maxTexSize = -1;
    gl.glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    const int scratchTexW = (texW + workGroupSizeX - 1) / workGroupSizeX;
    const int scratchTexH = (texH + workGroupSizeY - 1) / workGroupSizeY;
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, scratchTexW, scratchTexH);

    gl.glGenTextures(1, &outputTex);
    gl.glBindTexture(GL_TEXTURE_2D, outputTex);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 1, 1);

    gl.glBindTexture(GL_TEXTURE_2D, 0);
}

// Clobbers:
// GL_ACTIVE_TEXTURE, GL_TEXTURE_BINDING_2D, GL_CURRENT_PROGRAM
glm::vec4 TextureAverageComputerGL43::getTextureAverage(const GLuint tex, const GLuint unusedTextureUnitNum)
{
    if(const auto error = gl.glGetError(); error != GL_NO_ERROR)
        std::cerr << "TextureAverageComputerGL43: error " << error << " at entry to getTextureAverage()\n";
    const glm::vec4 NaN(std::numeric_limits<float>::quiet_NaN());

    computeProgram->bind();
    gl.glBindImageTexture(0, tex, 0, false, 0, GL_READ_ONLY, GL_RGBA32F);
    if(const auto error = gl.glGetError(); error != GL_NO_ERROR)
    {
        std::cerr << "TextureAverageComputerGL43: glBindImageTexture(inTex, pass 1): error " << error << "\n";
        return NaN;
    }

    gl.glBindImageTexture(1, scratchTex, 0, false, 0, GL_WRITE_ONLY, GL_RGBA32F);
    if(const auto error = gl.glGetError(); error != GL_NO_ERROR)
    {
        std::cerr << "TextureAverageComputerGL43: glBindImageTexture(outTex, pass 1): error " << error << "\n";
        return NaN;
    }

    int numWGx = ((texW+pixelsPerSideAtOnce-1)/pixelsPerSideAtOnce + workGroupSizeX - 1) / workGroupSizeX;
    int numWGy = ((texH+pixelsPerSideAtOnce-1)/pixelsPerSideAtOnce + workGroupSizeY - 1) / workGroupSizeY;
    glm::uvec2 inputTexSubImageSize = glm::uvec2(texW, texH);

    for(int pass = 1; ; ++pass)
    {
        if(pass == 2)
        {
            // From now on we source the texture with the results of the previous pass
            gl.glBindImageTexture(0, scratchTex, 0, false, 0, GL_READ_ONLY, GL_RGBA32F);
            if(const auto error = gl.glGetError(); error != GL_NO_ERROR)
            {
                std::cerr << "TextureAverageComputerGL43: glBindImageTexture(inTex, pass " << pass << "): error " << error << "\n";
                return NaN;
            }
        }

        if(numWGx == 1 && numWGy == 1)
        {
            gl.glBindImageTexture(1, outputTex, 0, false, 0, GL_WRITE_ONLY, GL_RGBA32F);
            if(const auto error = gl.glGetError(); error != GL_NO_ERROR)
            {
                std::cerr << "TextureAverageComputerGL43: glBindImageTexture(outTex, pass " << pass << "): error " << error << "\n";
                return NaN;
            }
        }

        gl.glUniform2uiv(2, 1, &inputTexSubImageSize[0]);
        gl.glDispatchCompute(numWGx, numWGy, 1);
        if(const auto error = gl.glGetError(); error != GL_NO_ERROR)
        {
            std::cerr << "TextureAverageComputerGL43: glDispatchCompute at pass " << pass << ": error " << error << "\n";
            return NaN;
        }

        gl.glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);

        if(numWGx == 1 && numWGy == 1) break;

        inputTexSubImageSize = glm::uvec2(numWGx, numWGy);
        numWGx = (numWGx + workGroupSizeX - 1) / workGroupSizeX;
        numWGy = (numWGy + workGroupSizeY - 1) / workGroupSizeY;
    }

    glm::vec4 mean;
    gl.glActiveTexture(GL_TEXTURE0 + unusedTextureUnitNum);
    gl.glBindTexture(GL_TEXTURE_2D, outputTex);
    gl.glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &mean[0]);
    computeProgram->release();
    return mean / float(texW*texH);
}

TextureAverageComputerGL43::~TextureAverageComputerGL43()
{
    gl.glDeleteTextures(1, &scratchTex);
    gl.glDeleteTextures(1, &outputTex);
}

void TextureAverageComputerGL43::buildShaderProgram()
{
    const auto shaderCode = 1+R"(
#version 430 core

#define WORK_GROUP_SIZE_X )" + std::to_string(workGroupSizeX) + R"(
#define WORK_GROUP_SIZE_Y )" + std::to_string(workGroupSizeY) + R"(
#define PIXELS_PER_SIDE_AT_ONCE )" + std::to_string(pixelsPerSideAtOnce) + R"(

layout(local_size_x = WORK_GROUP_SIZE_X, local_size_y = WORK_GROUP_SIZE_Y, local_size_z = 1) in;

uniform layout(rgba32f, binding = 0) readonly image2D tex;
uniform layout(rgba32f, binding = 1) writeonly image2D outputTex;
uniform layout(location = 2) uvec2 inputSubImageSize;
const int sharedArraySize = WORK_GROUP_SIZE_X*WORK_GROUP_SIZE_Y;
shared vec4 sharedSums[sharedArraySize];

void synchronize()
{
    memoryBarrierShared();
    barrier();
}

void accumulateSum(const uint index, const int stride)
{
    synchronize();
    if(index % (stride*2) == 0 && index+stride < sharedArraySize)
        sharedSums[index] += sharedSums[index+stride];
}

void main()
{
    vec4 pixel = vec4(0);
    for(int dx = 0; dx < PIXELS_PER_SIDE_AT_ONCE; ++dx)
    {
            for(int dy = 0; dy < PIXELS_PER_SIDE_AT_ONCE; ++dy)
            {
                    const uvec2 imgPos = PIXELS_PER_SIDE_AT_ONCE*gl_GlobalInvocationID.xy + uvec2(dx, dy);
                    if(all(lessThan(imgPos, inputSubImageSize)))
                            pixel += imageLoad(tex, ivec2(imgPos));
            }
    }
    const uint index = gl_LocalInvocationIndex;
    sharedSums[index] = pixel;
    const int maxLevel = int(ceil(log2(gl_WorkGroupSize.x*gl_WorkGroupSize.y)));
    for(int level = 0; level < maxLevel; ++level)
        accumulateSum(index, 1 << level);
    synchronize();
    if(index == 0)
        imageStore(outputTex, ivec2(gl_WorkGroupID.xy), sharedSums[index]);
}
)";
    computeProgram.reset(new QOpenGLShaderProgram);
    computeProgram->addShaderFromSourceCode(QOpenGLShader::Compute, shaderCode.c_str());
    computeProgram->link();
}

TextureAverageComputer::TextureAverageComputer(QOpenGLFunctions_3_3_Core& gl33, QOpenGLFunctions_4_3_Core* gl43,
                                               const int texW, const int texH,
                                               const GLenum internalFormat, const GLuint unusedTextureUnitNum)
{
    if(gl43 && internalFormat == GL_RGBA32F)
        averagerGL43.reset(new TextureAverageComputerGL43(*gl43, texW, texH, unusedTextureUnitNum));
    else
        averagerGL33.reset(new TextureAverageComputerGL33(gl33, texW, texH, internalFormat, unusedTextureUnitNum));
}

glm::vec4 TextureAverageComputer::getTextureAverage(const GLuint texture, const GLuint unusedTextureUnitNum)
{
    if(averagerGL43)
        return averagerGL43->getTextureAverage(texture, unusedTextureUnitNum);
    else
        return averagerGL33->getTextureAverage(texture, unusedTextureUnitNum);
}
