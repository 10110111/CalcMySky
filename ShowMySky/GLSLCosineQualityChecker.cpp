#include "GLSLCosineQualityChecker.hpp"
#include <cmath>
#include <vector>
#include <cassert>
#include <iostream>
#include <QDebug>
#include <QOpenGLFunctions_3_3_Core>

#include "util.hpp"

void GLSLCosineQualityChecker::loadShaders()
{
    program = std::make_unique<QOpenGLShaderProgram>();
    addShaderCode(*program, QOpenGLShader::Vertex,
                  QObject::tr("GLSL cosine quality check vertex shader"), 1+R"(
#version 330
in vec4 vertex;
void main() { gl_Position=vertex; }
)");

    addShaderCode(*program, QOpenGLShader::Fragment,
                  QObject::tr("GLSL cosine quality check fragment shader"), 1+R"(
#version 330

out vec4 color;
uniform sampler1D xtex;

void main()
{
    float texCoord = gl_FragCoord.x / textureSize(xtex,0);
    float x = texture(xtex,texCoord).x;
    color = vec4(cos(x),x,0,1);
}
)");
    link(*program, QObject::tr("GLSL cosine quality check shader program"));
}

void GLSLCosineQualityChecker::setupBuffers()
{
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
    gl.glBindVertexArray(0);
}

void GLSLCosineQualityChecker::setupRenderTarget(const int width, const int height)
{
    gl.glGenTextures(1, &texFBO);
    gl.glGenFramebuffers(1,&fbo);

    gl.glBindTexture(GL_TEXTURE_2D,texFBO);
    gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl.glBindTexture(GL_TEXTURE_2D,0);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texFBO,0);
    [[maybe_unused]] const auto status=gl.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    assert(status==GL_FRAMEBUFFER_COMPLETE);

}

void GLSLCosineQualityChecker::setupInputTextures(const int width, const int /*height*/)
{
    gl.glGenTextures(1, &inputTexture);
    gl.glBindTexture(GL_TEXTURE_1D, inputTexture);
    gl.glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    std::vector<GLfloat> data;
    constexpr auto degree = M_PI/180;
    for(int n = 0; n < width; ++n)
        data.push_back(3*degree * n/(width-1.));
    gl.glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F, data.size(), 0, GL_RED, GL_FLOAT, data.data());
}

GLSLCosineQualityChecker::GLSLCosineQualityChecker(QOpenGLFunctions_3_3_Core& gl)
    : gl(gl)
{
    setupRenderTarget(width, height);
    setupInputTextures(width, height);
    loadShaders();
    setupBuffers();
}

bool GLSLCosineQualityChecker::isGood()
{
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    gl.glViewport(0,0,width,height);

    program->bind();
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_1D, inputTexture);
    program->setUniformValue("xtex",0);

    gl.glBindVertexArray(vao);
    gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl.glBindVertexArray(0);

    gl.glBindTexture(GL_TEXTURE_1D, 0);
    gl.glBindTexture(GL_TEXTURE_2D, 0);
    program->release();
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);

    gl.glBindTexture(GL_TEXTURE_2D, texFBO);

    constexpr int numPoints = width*height;
    std::vector<GLfloat> data(4*numPoints);
    gl.glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, data.data());

#if 0
    std::cerr.precision(9);
    std::cerr << "x/°,arccos(cos_{intel}(x)))\n";
    for(int i=0;i<numPoints;++i)
        std::cerr << data[4*i+1]*180/M_PI << "," << 180/M_PI*std::acos(data[4*i+0]) << "\n";
#endif

    float prevCos = data[0];
    for(int n=1; n<numPoints; ++n)
    {
        const auto currCos = data[4*n];
        if(currCos >= prevCos)
        {
            qWarning() << "WARNING: non-monotonic cosine detected in GLSL test, "
                          "results of computations might be unreliable";
            return false;
        }
        prevCos = currCos;
    }
    return true;
}

GLSLCosineQualityChecker::~GLSLCosineQualityChecker()
{
    gl.glDeleteVertexArrays(1, &vao);
    gl.glDeleteBuffers(1, &vbo);
    gl.glDeleteTextures(1, &texFBO);
    gl.glDeleteTextures(1, &inputTexture);
    gl.glDeleteFramebuffers(1,&fbo);
}
