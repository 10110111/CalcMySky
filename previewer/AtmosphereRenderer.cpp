#include "AtmosphereRenderer.hpp"

#include <set>
#include <array>
#include <vector>
#include <cstring>
#include <cassert>
#include <iterator>
#include <iostream>
#include <filesystem>
#include <QFile>
#include <QRegularExpression>

#include "util.hpp"
#include "../common/util.hpp"
#include "ToolsWidget.hpp"

namespace fs=std::filesystem;

static constexpr double degree=M_PI/180;
static constexpr double lengthUnitInMeters=1000;
static constexpr double earthRadius=6.36e6/lengthUnitInMeters;
static constexpr double sunAngularRadius=0.00935/2;

// XXX: keep in sync with those in generator
static GLsizei scatTexWidth(glm::ivec4 sizes) { return sizes[0]; }
static GLsizei scatTexHeight(glm::ivec4 sizes) { return sizes[1]*sizes[2]; }
static GLsizei scatTexDepth(glm::ivec4 sizes) { return sizes[3]; }

void AtmosphereRenderer::makeBayerPatternTexture()
{
    bayerPatternTexture.setMinificationFilter(QOpenGLTexture::Nearest);
    bayerPatternTexture.setMagnificationFilter(QOpenGLTexture::Nearest);
    bayerPatternTexture.setWrapMode(QOpenGLTexture::Repeat);
	bayerPatternTexture.bind();
	static constexpr float bayerPattern[8*8] =
	{
		// 8x8 Bayer ordered dithering pattern.
		0/64.f, 32/64.f,  8/64.f, 40/64.f,  2/64.f, 34/64.f, 10/64.f, 42/64.f,
		48/64.f, 16/64.f, 56/64.f, 24/64.f, 50/64.f, 18/64.f, 58/64.f, 26/64.f,
		12/64.f, 44/64.f,  4/64.f, 36/64.f, 14/64.f, 46/64.f,  6/64.f, 38/64.f,
		60/64.f, 28/64.f, 52/64.f, 20/64.f, 62/64.f, 30/64.f, 54/64.f, 22/64.f,
		3/64.f, 35/64.f, 11/64.f, 43/64.f,  1/64.f, 33/64.f,  9/64.f, 41/64.f,
		51/64.f, 19/64.f, 59/64.f, 27/64.f, 49/64.f, 17/64.f, 57/64.f, 25/64.f,
		15/64.f, 47/64.f,  7/64.f, 39/64.f, 13/64.f, 45/64.f,  5/64.f, 37/64.f,
		63/64.f, 31/64.f, 55/64.f, 23/64.f, 61/64.f, 29/64.f, 53/64.f, 21/64.f
	};
	gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 8, 8, 0, GL_RED, GL_FLOAT, bayerPattern);
}

QVector3D AtmosphereRenderer::rgbMaxValue() const
{
	switch(tools->ditheringMode())
	{
		default:
		case DitheringMode::Disabled:
			return QVector3D(0,0,0);
		case DitheringMode::Color666:
			return QVector3D(63,63,63);
		case DitheringMode::Color565:
			return QVector3D(31,63,31);
		case DitheringMode::Color888:
			return QVector3D(255,255,255);
		case DitheringMode::Color101010:
			return QVector3D(1023,1023,1023);
	}
}

glm::ivec4 AtmosphereRenderer::loadTexture4D(QString const& path)
{
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        throw DataLoadError{tr("GL error on entry to loadTexture4D(\"%1\"): %2")
                            .arg(path).arg(openglErrorString(err).c_str())};
    }
    std::cerr << "Loading texture from \"" << path << "\"... ";
    QFile file(path);
    if(!file.open(QFile::ReadOnly))
        throw DataLoadError{tr("Failed to open file \"%1\": %2").arg(path).arg(file.errorString())};

    uint16_t sizes[4];
    {
        const qint64 sizeToRead=sizeof sizes;
        if(file.read(reinterpret_cast<char*>(sizes), sizeToRead) != sizeToRead)
        {
            throw DataLoadError{tr("Failed to read header from file \"%1\": %2")
                                .arg(path).arg(file.errorString())};
        }
    }
    const auto subpixelCount = 4*uint64_t(sizes[0])*sizes[1]*sizes[2]*sizes[3];
    std::cerr << "dimensions from header: " << sizes[0] << "×" << sizes[1] << "×" << sizes[2] << "×" << sizes[3] << "... ";
    const std::unique_ptr<GLfloat[]> subpixels(new GLfloat[subpixelCount]);
    {
        const qint64 sizeToRead=subpixelCount*sizeof subpixels[0];
        if(file.read(reinterpret_cast<char*>(subpixels.get()), sizeToRead) != sizeToRead)
        {
            throw DataLoadError{tr("Failed to read texture data from file \"%1\": %2")
                                .arg(path).arg(file.errorString())};
        }
    }
    const glm::ivec4 size(sizes[0],sizes[1],sizes[2],sizes[3]);
    gl.glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA32F,scatTexWidth(size),scatTexHeight(size),scatTexDepth(size),
                    0,GL_RGBA,GL_FLOAT,subpixels.get());
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        throw DataLoadError{tr("GL error in loadTexture4D(\"%1\") after glTexImage3D() call: %2")
                            .arg(path).arg(openglErrorString(err).c_str())};
    }
    std::cerr << "done\n";
    return size;
}

glm::ivec2 AtmosphereRenderer::loadTexture2D(QString const& path)
{
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        throw DataLoadError{tr("GL error on entry to loadTexture2D(\"%1\"): %2")
                            .arg(path).arg(openglErrorString(err).c_str())};
    }
    std::cerr << "Loading texture from \"" << path << "\"... ";
    QFile file(path);
    if(!file.open(QFile::ReadOnly))
        throw DataLoadError{tr("Failed to open file \"%1\": %2").arg(path).arg(file.errorString())};

    uint16_t sizes[2];
    {
        const qint64 sizeToRead=sizeof sizes;
        if(file.read(reinterpret_cast<char*>(sizes), sizeToRead) != sizeToRead)
        {
            throw DataLoadError{tr("Failed to read header from file \"%1\": %2")
                                .arg(path).arg(file.errorString())};
        }
    }
    const auto subpixelCount = 4*uint64_t(sizes[0])*sizes[1];
    std::cerr << "dimensions from header: " << sizes[0] << "×" << sizes[1] << "... ";
    const std::unique_ptr<GLfloat[]> subpixels(new GLfloat[subpixelCount]);
    {
        const qint64 sizeToRead=subpixelCount*sizeof subpixels[0];
        if(file.read(reinterpret_cast<char*>(subpixels.get()), sizeToRead) != sizeToRead)
        {
            throw DataLoadError{tr("Failed to read texture data from file \"%1\": %2")
                                .arg(path).arg(file.errorString())};
        }
    }
    gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,sizes[0],sizes[1],0,GL_RGBA,GL_FLOAT,subpixels.get());
    if(const auto err=gl.glGetError(); err!=GL_NO_ERROR)
    {
        throw DataLoadError{tr("GL error in loadTexture2D(\"%1\") after glTexImage2D() call: %2")
                            .arg(path).arg(openglErrorString(err).c_str())};
    }
    std::cerr << "done\n";
    return {sizes[0], sizes[1]};
}

void AtmosphereRenderer::loadTextures(QString const& pathToData)
{
    while(gl.glGetError()!=GL_NO_ERROR);

    gl.glActiveTexture(GL_TEXTURE0);

    for(unsigned wlSetIndex=0; wlSetIndex<wavelengthSetCount; ++wlSetIndex)
    {
        auto& tex=*transmittanceTextures.emplace_back(std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D));
        tex.setMinificationFilter(QOpenGLTexture::Linear);
        tex.setWrapMode(QOpenGLTexture::ClampToEdge);
        tex.bind();
        loadTexture2D(QString("%1/transmittance-wlset%2.f32").arg(pathToData).arg(wlSetIndex));
    }

    for(unsigned wlSetIndex=0; wlSetIndex<wavelengthSetCount; ++wlSetIndex)
    {
        auto& tex=*irradianceTextures.emplace_back(std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D));
        tex.setMinificationFilter(QOpenGLTexture::Linear);
        tex.setWrapMode(QOpenGLTexture::ClampToEdge);
        tex.bind();
        loadTexture2D(QString("%1/irradiance-wlset%2.f32").arg(pathToData).arg(wlSetIndex));
    }

    multipleScatteringTexture.setMinificationFilter(QOpenGLTexture::Linear);
    multipleScatteringTexture.setWrapMode(QOpenGLTexture::ClampToEdge);
    multipleScatteringTexture.bind();
    loadTexture4D(pathToData+"/multiple-scattering-xyzw.f32");

    singleScatteringTextures.resize(wavelengthSetCount);
    for(unsigned wlSetIndex=0; wlSetIndex<wavelengthSetCount; ++wlSetIndex)
    {
        auto& texturesPerScatterer=singleScatteringTextures[wlSetIndex];
        for(const auto& scatterer : scattererNames_)
        {
            texturesPerScatterer.emplace_back(std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target3D));
            auto& texture=*texturesPerScatterer.back();
            texture.setMinificationFilter(QOpenGLTexture::Linear);
            texture.setWrapMode(QOpenGLTexture::ClampToEdge);
            texture.bind();
            loadTexture4D(QString("%1/single-scattering/%2/%3.f32").arg(pathToData).arg(wlSetIndex).arg(scatterer));
        }
    }

    makeBayerPatternTexture();

    assert(gl.glGetError()==GL_NO_ERROR);
}

void AtmosphereRenderer::loadShaders(QString const& pathToData)
{
    constexpr char commonVertexShaderSrc[]=R"(
#version 330
in vec3 vertex;
out vec3 position;
void main()
{
    position=vertex;
    gl_Position=vec4(position,1);
}
)";

    scattererNames_.clear();
    singleScatteringPrograms.clear();
    singleScatteringPrograms.resize(SSRM_COUNT);
    for(int renderMode=0; renderMode<SSRM_COUNT; ++renderMode)
    {
        singleScatteringPrograms[renderMode].resize(wavelengthSetCount);
        auto& programsPerWLSet=singleScatteringPrograms[renderMode];

        const bool addScattererNames=scattererNames_.empty();
        for(unsigned wlSetIndex=0; wlSetIndex<wavelengthSetCount; ++wlSetIndex)
        {
            auto& programsPerScatterer=programsPerWLSet[wlSetIndex];
            const auto wlDir=QString("%1/shaders/single-scattering/%2/%3").arg(pathToData)
                                                                          .arg(singleScatteringRenderModeNames[renderMode])
                                                                          .arg(wlSetIndex);
            for(const auto& scatDir : fs::directory_iterator(fs::u8path(wlDir.toStdString())))
            {
                auto& program=*programsPerScatterer.emplace_back(std::make_unique<QOpenGLShaderProgram>());

                for(const auto& shaderFile : fs::directory_iterator(scatDir))
                    addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

                const QString scattererName=scatDir.path().filename().u8string().c_str();
                addShaderCode(program, QOpenGLShader::Vertex, tr("vertex shader for scatterer \"%1\"").arg(scattererName),
                              commonVertexShaderSrc);

                link(program, tr("shader program for scatterer \"%1\"").arg(scattererName));

                if(addScattererNames && wlSetIndex==0)
                    scattererNames_.push_back(scattererName);
            }
        }
    }
    scatterersEnabledStates=QVector(scattererNames_.size(), true);
    tools->updateScattererSet(scattererNames_);

    luminanceToScreenRGB=std::make_unique<QOpenGLShaderProgram>();
    addShaderCode(*luminanceToScreenRGB, QOpenGLShader::Fragment, tr("luminanceToScreenRGB fragment shader"), 1+R"(
#version 330
uniform float exposure;
uniform sampler2D luminanceXYZW;
in vec2 texCoord;
out vec4 color;

uniform vec3 rgbMaxValue;
uniform sampler2D bayerPattern;
vec3 dither(vec3 c)
{
    if(rgbMaxValue.r==0.) return c;
    float bayer=texture2D(bayerPattern,gl_FragCoord.xy/8.).r;

    vec3 rgb=c*rgbMaxValue;
    vec3 head=floor(rgb);
    vec3 tail=rgb-head;
    return (head+1.-step(tail,vec3(bayer)))/rgbMaxValue;
}

void main()
{
    const mat3 XYZ2sRGBl=mat3(vec3(3.2406,-0.9689,0.0557),
                              vec3(-1.5372,1.8758,-0.204),
                              vec3(-0.4986,0.0415,1.057));
    vec3 XYZ=texture(luminanceXYZW, texCoord).xyz;
    vec3 rgb=XYZ2sRGBl*XYZ;
    vec3 srgb=pow(rgb*exposure, vec3(1/2.2));
    color=vec4(dither(srgb),1);
}
)");
    addShaderCode(*luminanceToScreenRGB, QOpenGLShader::Vertex, tr("luminanceToScreenRGB vertex shader"), 1+R"(
#version 330
in vec3 vertex;
out vec2 texCoord;
void main()
{
    texCoord=(vertex.xy+vec2(1))/2;
    gl_Position=vec4(vertex,1);
}
)");
    link(*luminanceToScreenRGB, tr("luminanceToScreenRGB shader program"));

    {
        multipleScatteringProgram=std::make_unique<QOpenGLShaderProgram>();
        auto& program=*multipleScatteringProgram;
        const auto wlDir=pathToData+"/shaders/multiple-scattering/";
        for(const auto& shaderFile : fs::directory_iterator(fs::u8path(wlDir.toStdString())))
            addShaderFile(program, QOpenGLShader::Fragment, shaderFile.path());
        addShaderCode(program, QOpenGLShader::Vertex, tr("vertex shader for multiple scattering"), commonVertexShaderSrc);
        link(program, tr("multiple scattering shader program"));
    }

    for(unsigned wlSetIndex=0; wlSetIndex<wavelengthSetCount; ++wlSetIndex)
    {
        auto& program=*zeroOrderScatteringPrograms.emplace_back(std::make_unique<QOpenGLShaderProgram>());
        const auto wlDir=QString("%1/shaders/zero-order-scattering/%2").arg(pathToData).arg(wlSetIndex);
        for(const auto& shaderFile : fs::directory_iterator(fs::u8path(wlDir.toStdString())))
            addShaderFile(program, QOpenGLShader::Fragment, shaderFile.path());
        addShaderCode(program, QOpenGLShader::Vertex, tr("vertex shader for zero-order scattering"), commonVertexShaderSrc);
        link(program, tr("zero-order scattering shader program"));
    }
}

void AtmosphereRenderer::setupBuffers()
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

void AtmosphereRenderer::renderZeroOrderScattering()
{
    for(unsigned wlSetIndex=0; wlSetIndex<wavelengthSetCount; ++wlSetIndex)
    {
        const auto& prog=zeroOrderScatteringPrograms[wlSetIndex];
        prog->bind();
        prog->setUniformValue("cameraPosition", QVector3D(0,0,tools->altitude()));
        prog->setUniformValue("sunDirection", QVector3D(
                    std::cos(tools->sunAzimuth())*std::sin(tools->sunZenithAngle()),
                    std::sin(tools->sunAzimuth())*std::sin(tools->sunZenithAngle()),
                    std::cos(tools->sunZenithAngle())));
        transmittanceTextures[wlSetIndex]->bind(0);
        prog->setUniformValue("transmittanceTexture", 0);
        irradianceTextures[wlSetIndex]->bind(1);
        prog->setUniformValue("irradianceTexture",1);

        gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
}

void AtmosphereRenderer::renderSingleScattering()
{
    const auto renderMode = tools->onTheFlySingleScatteringEnabled() ? SSRM_ON_THE_FLY : SSRM_PRECOMPUTED;
    for(unsigned wlSetIndex=0; wlSetIndex<wavelengthSetCount; ++wlSetIndex)
    {
        for(int scattererIndex=0; scattererIndex<scatterersEnabledStates.size(); ++scattererIndex)
        {
            if(!scatterersEnabledStates[scattererIndex])
                continue;

            const auto& prog=singleScatteringPrograms[renderMode][wlSetIndex][scattererIndex];
            prog->bind();
            prog->setUniformValue("cameraPosition", QVector3D(0,0,tools->altitude()));
            prog->setUniformValue("sunDirection", QVector3D(
                        std::cos(tools->sunAzimuth())*std::sin(tools->sunZenithAngle()),
                        std::sin(tools->sunAzimuth())*std::sin(tools->sunZenithAngle()),
                        std::cos(tools->sunZenithAngle())));
            transmittanceTextures[wlSetIndex]->bind(0);
            prog->setUniformValue("transmittanceTexture", 0);
            singleScatteringTextures[wlSetIndex][scattererIndex]->bind(1);
            prog->setUniformValue("scatteringTexture", 1);

            gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }
}

void AtmosphereRenderer::renderMultipleScattering()
{
    const auto& prog=multipleScatteringProgram;
    prog->bind();
    prog->setUniformValue("cameraPosition", QVector3D(0,0,tools->altitude()));
    prog->setUniformValue("sunDirection", QVector3D(
                std::cos(tools->sunAzimuth())*std::sin(tools->sunZenithAngle()),
                std::sin(tools->sunAzimuth())*std::sin(tools->sunZenithAngle()),
                std::cos(tools->sunZenithAngle())));
    multipleScatteringTexture.bind(0);
    prog->setUniformValue("scatteringTexture", 0);

    gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void AtmosphereRenderer::draw()
{
    GLint targetFBO=-1;
    gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &targetFBO);

    gl.glBindVertexArray(vao);
    {
        gl.glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        gl.glClearColor(0,0,0,0);
        gl.glClear(GL_COLOR_BUFFER_BIT);
        gl.glEnable(GL_BLEND);
        {
            gl.glBlendFunc(GL_ONE, GL_ONE);
            if(tools->zeroOrderScatteringEnabled())
                renderZeroOrderScattering();
            if(tools->singleScatteringEnabled())
                renderSingleScattering();
            if(tools->multipleScatteringEnabled())
                renderMultipleScattering();
        }
        gl.glDisable(GL_BLEND);

        gl.glBindFramebuffer(GL_FRAMEBUFFER,targetFBO);
        {
            luminanceToScreenRGB->bind();
            texFBO.bind(0);
            luminanceToScreenRGB->setUniformValue("luminanceXYZW", 0);
            bayerPatternTexture.bind(1);
            luminanceToScreenRGB->setUniformValue("bayerPattern", 1);
            luminanceToScreenRGB->setUniformValue("rgbMaxValue", rgbMaxValue());
            luminanceToScreenRGB->setUniformValue("exposure", tools->exposure());
            gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }
    gl.glBindVertexArray(0);
}

void AtmosphereRenderer::setupRenderTarget()
{
    gl.glGenFramebuffers(1,&fbo);
    texFBO.setMinificationFilter(QOpenGLTexture::Nearest);
    texFBO.setMagnificationFilter(QOpenGLTexture::Nearest);
    texFBO.setWrapMode(QOpenGLTexture::ClampToEdge);

    GLint viewport[4];
    gl.glGetIntegerv(GL_VIEWPORT, viewport);
    const int width=viewport[2], height=viewport[3];
    resizeEvent(width,height);
}

AtmosphereRenderer::AtmosphereRenderer(QOpenGLFunctions_3_3_Core& gl, QString const& pathToData,
                                       Parameters const& params, ToolsWidget* tools)
    : gl(gl)
    , tools(tools)
    , wavelengthSetCount(params.wavelengthSetCount)
    , multipleScatteringTexture(QOpenGLTexture::Target3D)
    , bayerPatternTexture(QOpenGLTexture::Target2D)
    , texFBO(QOpenGLTexture::Target2D)
{
    loadShaders(pathToData);
    loadTextures(pathToData);
    setupRenderTarget();
    setupBuffers();
}

AtmosphereRenderer::~AtmosphereRenderer()
{
    gl.glDeleteBuffers(1, &vbo);
    gl.glDeleteVertexArrays(1, &vao);
    gl.glDeleteFramebuffers(1, &fbo);
}

void AtmosphereRenderer::resizeEvent(const int width, const int height)
{
    assert(fbo);
    texFBO.bind();
    gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texFBO.textureId(),0);
    checkFramebufferStatus(gl, "Atmosphere renderer FBO");
    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void AtmosphereRenderer::mouseMove(const int x, const int y)
{
    constexpr double scale=500;
    switch(dragMode)
    {
    case DragMode::Sun:
    {
        const auto oldZA=tools->sunZenithAngle(), oldAz=tools->sunAzimuth();
        tools->setSunZenithAngle(std::clamp(oldZA - (prevMouseY-y)/scale, 0., M_PI));
        tools->setSunAzimuth(std::clamp(oldAz - (prevMouseX-x)/scale, -M_PI, M_PI));
        break;
    }
    default:
        break;
    }
    prevMouseX=x;
    prevMouseY=y;
}

void AtmosphereRenderer::setScattererEnabled(const int scattererIndex, const bool enable)
{
    scatterersEnabledStates[scattererIndex]=enable;
    emit needRedraw();
}

void AtmosphereRenderer::reloadShaders(QString const& pathToData)
{
    std::cerr << "Reloading shaders... ";
    loadShaders(pathToData);
    std::cerr << "done\n";
}
