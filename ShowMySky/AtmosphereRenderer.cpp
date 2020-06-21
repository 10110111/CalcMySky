#include "AtmosphereRenderer.hpp"

#include <set>
#include <cmath>
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
#include "../common/const.hpp"
#include "../common/util.hpp"
#include "ToolsWidget.hpp"

namespace fs=std::filesystem;

// XXX: keep in sync with those in CalcMySky
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

    for(unsigned wlSetIndex=0; wlSetIndex<params.wavelengthSetCount; ++wlSetIndex)
    {
        auto& tex=*transmittanceTextures.emplace_back(std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D));
        tex.setMinificationFilter(QOpenGLTexture::Linear);
        tex.setWrapMode(QOpenGLTexture::ClampToEdge);
        tex.bind();
        loadTexture2D(QString("%1/transmittance-wlset%2.f32").arg(pathToData).arg(wlSetIndex));
    }

    for(unsigned wlSetIndex=0; wlSetIndex<params.wavelengthSetCount; ++wlSetIndex)
    {
        auto& tex=*irradianceTextures.emplace_back(std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D));
        tex.setMinificationFilter(QOpenGLTexture::Linear);
        tex.setWrapMode(QOpenGLTexture::ClampToEdge);
        tex.bind();
        loadTexture2D(QString("%1/irradiance-wlset%2.f32").arg(pathToData).arg(wlSetIndex));
    }

    const auto texFilter = tools->textureFilteringEnabled() ? QOpenGLTexture::Linear : QOpenGLTexture::Nearest;
    multipleScatteringTexture.setMinificationFilter(texFilter);
    multipleScatteringTexture.setMagnificationFilter(texFilter);
    multipleScatteringTexture.setWrapMode(QOpenGLTexture::ClampToEdge);
    multipleScatteringTexture.bind();
    loadTexture4D(pathToData+"/multiple-scattering-xyzw.f32");

    singleScatteringTextures.clear();
    for(const auto& [scattererName, phaseFuncType] : params.scatterers)
    {
        auto& texturesPerWLSet=singleScatteringTextures[scattererName];
        switch(phaseFuncType)
        {
        case PhaseFunctionType::General:
            for(unsigned wlSetIndex=0; wlSetIndex<params.wavelengthSetCount; ++wlSetIndex)
            {
                auto& texture=*texturesPerWLSet.emplace_back(std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target3D));
                texture.setMinificationFilter(texFilter);
                texture.setMagnificationFilter(texFilter);
                texture.setWrapMode(QOpenGLTexture::ClampToEdge);
                texture.bind();
                loadTexture4D(QString("%1/single-scattering/%2/%3.f32").arg(pathToData).arg(wlSetIndex).arg(scattererName));
            }
            break;
        case PhaseFunctionType::Achromatic:
        {
            auto& texture=*texturesPerWLSet.emplace_back(std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target3D));
            texture.setMinificationFilter(texFilter);
            texture.setMagnificationFilter(texFilter);
            texture.setWrapMode(QOpenGLTexture::ClampToEdge);
            texture.bind();
            loadTexture4D(QString("%1/single-scattering/%2-xyzw.f32").arg(pathToData).arg(scattererName));
            break;
        }
        case PhaseFunctionType::Smooth:
            break;
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
    const QByteArray viewDirShaderSrc=1+R"(
#version 330
in vec3 position;
uniform float zoomFactor;
const float PI=3.1415926535897932;
vec3 calcViewDir()
{
    vec2 pos=position.xy/zoomFactor;
    return vec3(cos(pos.x*PI)*cos(pos.y*(PI/2)),
                sin(pos.x*PI)*cos(pos.y*(PI/2)),
                sin(pos.y*(PI/2)));
})";

    singleScatteringPrograms.clear();
    for(int renderMode=0; renderMode<SSRM_COUNT; ++renderMode)
    {
        auto& programsPerScatterer=*singleScatteringPrograms.emplace_back(std::make_unique<std::map<QString,std::vector<ShaderProgPtr>>>());

        for(const auto& [scattererName,phaseFuncType] : params.scatterers)
        {
            auto& programs=programsPerScatterer[scattererName];
            if(phaseFuncType==PhaseFunctionType::General || (phaseFuncType!=PhaseFunctionType::Smooth && renderMode==SSRM_ON_THE_FLY))
            {
                for(unsigned wlSetIndex=0; wlSetIndex<params.wavelengthSetCount; ++wlSetIndex)
                {
                    const auto scatDir=QString("%1/shaders/single-scattering/%2/%3/%4").arg(pathToData)
                                                                                       .arg(singleScatteringRenderModeNames[renderMode])
                                                                                       .arg(wlSetIndex)
                                                                                       .arg(scattererName);
                    auto& program=*programs.emplace_back(std::make_unique<QOpenGLShaderProgram>());

                    for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
                        addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

                    addShaderCode(program,QOpenGLShader::Fragment,"viewDir function shader",viewDirShaderSrc);

                    addShaderCode(program, QOpenGLShader::Vertex, tr("vertex shader for scatterer \"%1\"").arg(scattererName),
                                  commonVertexShaderSrc);

                    link(program, tr("shader program for scatterer \"%1\"").arg(scattererName));
                }
            }
            else if(phaseFuncType==PhaseFunctionType::Achromatic)
            {
                const auto scatDir=QString("%1/shaders/single-scattering/%2/%3").arg(pathToData)
                                                                                .arg(singleScatteringRenderModeNames[renderMode])
                                                                                .arg(scattererName);
                auto& program=*programs.emplace_back(std::make_unique<QOpenGLShaderProgram>());
                for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
                    addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

                addShaderCode(program,QOpenGLShader::Fragment,"viewDir function shader",viewDirShaderSrc);

                addShaderCode(program, QOpenGLShader::Vertex, tr("vertex shader for scatterer \"%1\"").arg(scattererName),
                              commonVertexShaderSrc);

                link(program, tr("shader program for scatterer \"%1\"").arg(scattererName));
            }
        }
    }

    {
        eclipsedSingleScatteringPrograms.clear();
        for(int renderMode=SSRM_ON_THE_FLY; renderMode<SSRM_COUNT; ++renderMode)
        {
            auto& programsPerScatterer=*eclipsedSingleScatteringPrograms.emplace_back(std::make_unique<ScatteringProgramsMap>());

            for(const auto& [scattererName,phaseFuncType] : params.scatterers)
            {
                auto& programs=programsPerScatterer[scattererName];
                if(phaseFuncType==PhaseFunctionType::General || (phaseFuncType!=PhaseFunctionType::Smooth && renderMode==SSRM_ON_THE_FLY))
                {
                    for(unsigned wlSetIndex=0; wlSetIndex<params.wavelengthSetCount; ++wlSetIndex)
                    {
                        const auto scatDir=QString("%1/shaders/single-scattering-eclipsed/%2/%3/%4").arg(pathToData)
                                                                                                    .arg(singleScatteringRenderModeNames[renderMode])
                                                                                                    .arg(wlSetIndex)
                                                                                                    .arg(scattererName);
                        auto& program=*programs.emplace_back(std::make_unique<QOpenGLShaderProgram>());

                        for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
                            addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

                        addShaderCode(program,QOpenGLShader::Fragment,"viewDir function shader",viewDirShaderSrc);

                        addShaderCode(program, QOpenGLShader::Vertex, tr("vertex shader for scatterer \"%1\"").arg(scattererName),
                                      commonVertexShaderSrc);

                        link(program, tr("shader program for scatterer \"%1\"").arg(scattererName));
                    }
                }
                else if(phaseFuncType==PhaseFunctionType::Achromatic)
                {
                    const auto scatDir=QString("%1/shaders/single-scattering-eclipsed/%2/%3").arg(pathToData)
                                                                                                .arg(singleScatteringRenderModeNames[renderMode])
                                                                                                .arg(scattererName);
                    auto& program=*programs.emplace_back(std::make_unique<QOpenGLShaderProgram>());

                    for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
                        addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

                    addShaderCode(program,QOpenGLShader::Fragment,"viewDir function shader",viewDirShaderSrc);

                    addShaderCode(program, QOpenGLShader::Vertex, tr("vertex shader for scatterer \"%1\"").arg(scattererName),
                                  commonVertexShaderSrc);

                    link(program, tr("shader program for scatterer \"%1\"").arg(scattererName));
                }
            }
        }
    }

    {
        eclipsedSingleScatteringPrecomputationPrograms=std::make_unique<ScatteringProgramsMap>();
        for(const auto& [scattererName,phaseFuncType] : params.scatterers)
        {
            auto& programs=(*eclipsedSingleScatteringPrecomputationPrograms)[scattererName];
            for(unsigned wlSetIndex=0; wlSetIndex<params.wavelengthSetCount; ++wlSetIndex)
            {
                const auto scatDir=QString("%1/shaders/single-scattering-eclipsed/precomputation/%3/%4").arg(pathToData)
                                                                                                        .arg(wlSetIndex)
                                                                                                        .arg(scattererName);
                auto& program=*programs.emplace_back(std::make_unique<QOpenGLShaderProgram>());

                for(const auto& shaderFile : fs::directory_iterator(fs::u8path(scatDir.toStdString())))
                    addShaderFile(program,QOpenGLShader::Fragment,shaderFile.path());

                addShaderCode(program, QOpenGLShader::Vertex, tr("vertex shader for scatterer \"%1\"").arg(scattererName),
                              commonVertexShaderSrc);

                link(program, tr("shader program for scatterer \"%1\"").arg(scattererName));
            }
        }
    }

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
        addShaderCode(program,QOpenGLShader::Fragment,"viewDir function shader",viewDirShaderSrc);
        addShaderCode(program, QOpenGLShader::Vertex, tr("vertex shader for multiple scattering"), commonVertexShaderSrc);
        link(program, tr("multiple scattering shader program"));
    }

    zeroOrderScatteringPrograms.clear();
    for(unsigned wlSetIndex=0; wlSetIndex<params.wavelengthSetCount; ++wlSetIndex)
    {
        auto& program=*zeroOrderScatteringPrograms.emplace_back(std::make_unique<QOpenGLShaderProgram>());
        const auto wlDir=QString("%1/shaders/zero-order-scattering/%2").arg(pathToData).arg(wlSetIndex);
        for(const auto& shaderFile : fs::directory_iterator(fs::u8path(wlDir.toStdString())))
            addShaderFile(program, QOpenGLShader::Fragment, shaderFile.path());
        addShaderCode(program,QOpenGLShader::Fragment,"viewDir function shader",viewDirShaderSrc);
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

glm::dvec3 AtmosphereRenderer::cameraPosition() const
{
    return glm::dvec3(0,0,tools->altitude());
}

glm::dvec3 AtmosphereRenderer::sunDirection() const
{
    return glm::dvec3(std::cos(tools->sunAzimuth())*std::sin(tools->sunZenithAngle()),
                      std::sin(tools->sunAzimuth())*std::sin(tools->sunZenithAngle()),
                      std::cos(tools->sunZenithAngle()));
}

glm::dvec3 AtmosphereRenderer::moonPosition() const
{
    const auto moonDir=glm::dvec3(std::cos(tools->moonAzimuth())*std::sin(tools->moonZenithAngle()),
                                  std::sin(tools->moonAzimuth())*std::sin(tools->moonZenithAngle()),
                                  std::cos(tools->moonZenithAngle()));
    return cameraPosition()+moonDir*cameraMoonDistance();
}

glm::dvec3 AtmosphereRenderer::moonPositionRelativeToSunAzimuth() const
{
    const auto moonDir=glm::dvec3(std::cos(tools->moonAzimuth() - tools->sunAzimuth())*std::sin(tools->moonZenithAngle()),
                                  std::sin(tools->moonAzimuth() - tools->sunAzimuth())*std::sin(tools->moonZenithAngle()),
                                  std::cos(tools->moonZenithAngle()));
    return cameraPosition()+moonDir*cameraMoonDistance();
}

double AtmosphereRenderer::cameraMoonDistance() const
{
    using namespace std;
    const auto hpR=tools->altitude()+params.earthRadius;
    const auto moonElevation=M_PI/2-tools->moonZenithAngle();
    return -hpR*sin(moonElevation)+sqrt(sqr(params.earthMoonDistance)-0.5*sqr(hpR)*(1+cos(2*moonElevation)));
}

double AtmosphereRenderer::moonAngularRadius() const
{
    return moonRadius/cameraMoonDistance();
}

void AtmosphereRenderer::renderZeroOrderScattering()
{
    for(unsigned wlSetIndex=0; wlSetIndex<params.wavelengthSetCount; ++wlSetIndex)
    {
        auto& prog=*zeroOrderScatteringPrograms[wlSetIndex];
        prog.bind();
        prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
        prog.setUniformValue("zoomFactor", tools->zoomFactor());
        prog.setUniformValue("sunDirection", toQVector(sunDirection()));
        transmittanceTextures[wlSetIndex]->bind(0);
        prog.setUniformValue("transmittanceTexture", 0);
        irradianceTextures[wlSetIndex]->bind(1);
        prog.setUniformValue("irradianceTexture",1);

        gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
}


void AtmosphereRenderer::precomputeEclipsedSingleScattering()
{
    // TODO: avoid redoing it if Sun elevation and Moon elevation and relative azimuth haven't changed
    for(const auto& [scattererName, phaseFuncType] : params.scatterers)
    {
        auto& textures=eclipsedSingleScatteringPrecomputationTextures[scattererName];
        const auto& programs=eclipsedSingleScatteringPrecomputationPrograms->at(scattererName);
        gl.glDisable(GL_BLEND); // First wavelength set overwrites old contents, regardless of subsequent blending modes
        const bool needBlending = phaseFuncType==PhaseFunctionType::Achromatic || phaseFuncType==PhaseFunctionType::Smooth;
        for(unsigned wlSetIndex=0; wlSetIndex<params.wavelengthSetCount; ++wlSetIndex)
        {
            auto& prog=*programs[wlSetIndex];
            prog.bind();
            prog.setUniformValue("altitude", float(tools->altitude()));
            prog.setUniformValue("moonAngularRadius", float(moonAngularRadius()));
            prog.setUniformValue("moonPositionRelativeToSunAzimuth", toQVector(moonPositionRelativeToSunAzimuth()));
            prog.setUniformValue("sunZenithAngle", float(tools->sunZenithAngle()));
            transmittanceTextures[wlSetIndex]->bind(0);
            prog.setUniformValue("transmittanceTexture", 0);

            auto& tex = needBlending ? *textures.front() : *textures[wlSetIndex];
            gl.glBindFramebuffer(GL_FRAMEBUFFER, eclipseSingleScatteringPrecomputationFBO);
            gl.glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex.textureId(),0);
            checkFramebufferStatus(gl, "Eclipsed single scattering precomputation FBO");
            gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            if(needBlending)
                gl.glEnable(GL_BLEND);
        }
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER,mainFBO);
    gl.glEnable(GL_BLEND);
}

void AtmosphereRenderer::renderSingleScattering()
{
    if(tools->usingEclipseShader())
        precomputeEclipsedSingleScattering();

    const auto renderMode = tools->onTheFlySingleScatteringEnabled() ? SSRM_ON_THE_FLY : SSRM_PRECOMPUTED;
    for(const auto& [scattererName,phaseFuncType] : params.scatterers)
    {
        if(!scatterersEnabledStates.at(scattererName))
            continue;

        if(renderMode==SSRM_ON_THE_FLY)
        {
            if(phaseFuncType==PhaseFunctionType::Smooth)
                continue;

            if(tools->usingEclipseShader())
            {
                for(unsigned wlSetIndex=0; wlSetIndex<params.wavelengthSetCount; ++wlSetIndex)
                {
                    auto& prog=*eclipsedSingleScatteringPrograms[renderMode]->at(scattererName)[wlSetIndex];
                    prog.bind();
                    prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
                    prog.setUniformValue("moonAngularRadius", float(moonAngularRadius()));
                    prog.setUniformValue("moonPosition", toQVector(moonPosition()));
                    prog.setUniformValue("zoomFactor", tools->zoomFactor());
                    prog.setUniformValue("sunDirection", toQVector(sunDirection()));
                    transmittanceTextures[wlSetIndex]->bind(0);
                    prog.setUniformValue("transmittanceTexture", 0);

                    gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                }
            }
            else
            {
                for(unsigned wlSetIndex=0; wlSetIndex<params.wavelengthSetCount; ++wlSetIndex)
                {
                    auto& prog=*singleScatteringPrograms[renderMode]->at(scattererName)[wlSetIndex];
                    prog.bind();
                    prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
                    prog.setUniformValue("zoomFactor", tools->zoomFactor());
                    prog.setUniformValue("sunDirection", toQVector(sunDirection()));
                    transmittanceTextures[wlSetIndex]->bind(0);
                    prog.setUniformValue("transmittanceTexture", 0);

                    gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                }
            }
        }
        else if(phaseFuncType==PhaseFunctionType::General)
        {
            if(tools->usingEclipseShader())
            {
                for(unsigned wlSetIndex=0; wlSetIndex<params.wavelengthSetCount; ++wlSetIndex)
                {
                    auto& prog=*eclipsedSingleScatteringPrograms[renderMode]->at(scattererName)[wlSetIndex];
                    prog.bind();
                    prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
                    prog.setUniformValue("zoomFactor", tools->zoomFactor());
                    prog.setUniformValue("sunDirection", toQVector(sunDirection()));
                    {
                        auto& tex=*eclipsedSingleScatteringPrecomputationTextures.at(scattererName)[wlSetIndex];
                        const auto texFilter = tools->textureFilteringEnabled() ? QOpenGLTexture::Linear : QOpenGLTexture::Nearest;
                        tex.setMinificationFilter(texFilter);
                        tex.setMagnificationFilter(texFilter);
                        tex.bind(0);
                        prog.setUniformValue("eclipsedScatteringTexture", 0);
                    }

                    gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                }
            }
            else
            {
                for(unsigned wlSetIndex=0; wlSetIndex<params.wavelengthSetCount; ++wlSetIndex)
                {
                    auto& prog=*singleScatteringPrograms[renderMode]->at(scattererName)[wlSetIndex];
                    prog.bind();
                    prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
                    prog.setUniformValue("zoomFactor", tools->zoomFactor());
                    prog.setUniformValue("sunDirection", toQVector(sunDirection()));
                    {
                        auto& tex=*singleScatteringTextures.at(scattererName)[wlSetIndex];
                        const auto texFilter = tools->textureFilteringEnabled() ? QOpenGLTexture::Linear : QOpenGLTexture::Nearest;
                        tex.setMinificationFilter(texFilter);
                        tex.setMagnificationFilter(texFilter);
                        tex.bind(0);
                        prog.setUniformValue("scatteringTexture", 0);
                    }

                    gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                }
            }
        }
        else if(phaseFuncType==PhaseFunctionType::Achromatic)
        {
            if(tools->usingEclipseShader())
            {
                auto& prog=*eclipsedSingleScatteringPrograms[renderMode]->at(scattererName).front();
                prog.bind();
                prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
                prog.setUniformValue("zoomFactor", tools->zoomFactor());
                prog.setUniformValue("sunDirection", toQVector(sunDirection()));
                {
                    auto& tex=*eclipsedSingleScatteringPrecomputationTextures.at(scattererName).front();
                    const auto texFilter = tools->textureFilteringEnabled() ? QOpenGLTexture::Linear : QOpenGLTexture::Nearest;
                    tex.setMinificationFilter(texFilter);
                    tex.setMagnificationFilter(texFilter);
                    tex.bind(0);
                    prog.setUniformValue("eclipsedScatteringTexture", 0);
                }

                gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
            else
            {
                auto& prog=*singleScatteringPrograms[renderMode]->at(scattererName).front();
                prog.bind();
                prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
                prog.setUniformValue("zoomFactor", tools->zoomFactor());
                prog.setUniformValue("sunDirection", toQVector(sunDirection()));
                {
                    auto& tex=*singleScatteringTextures.at(scattererName).front();
                    const auto texFilter = tools->textureFilteringEnabled() ? QOpenGLTexture::Linear : QOpenGLTexture::Nearest;
                    tex.setMinificationFilter(texFilter);
                    tex.setMagnificationFilter(texFilter);
                    tex.bind(0);
                }
                prog.setUniformValue("scatteringTexture", 0);

                gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
        }
    }
}

void AtmosphereRenderer::renderMultipleScattering()
{
    auto& prog=*multipleScatteringProgram;
    prog.bind();
    prog.setUniformValue("cameraPosition", toQVector(cameraPosition()));
    prog.setUniformValue("zoomFactor", tools->zoomFactor());
    prog.setUniformValue("sunDirection", toQVector(sunDirection()));
    {
        const auto texFilter = tools->textureFilteringEnabled() ? QOpenGLTexture::Linear : QOpenGLTexture::Nearest;
        multipleScatteringTexture.setMinificationFilter(texFilter);
        multipleScatteringTexture.setMagnificationFilter(texFilter);
    }
    multipleScatteringTexture.bind(0);
    prog.setUniformValue("scatteringTexture", 0);

    gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void AtmosphereRenderer::draw()
{
    GLint targetFBO=-1;
    gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &targetFBO);

    gl.glBindVertexArray(vao);
    {
        gl.glBindFramebuffer(GL_FRAMEBUFFER,mainFBO);
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
            mainFBOTexture.bind(0);
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
    gl.glGenFramebuffers(1,&mainFBO);
    mainFBOTexture.setMinificationFilter(QOpenGLTexture::Nearest);
    mainFBOTexture.setMagnificationFilter(QOpenGLTexture::Nearest);
    mainFBOTexture.setWrapMode(QOpenGLTexture::ClampToEdge);

    gl.glGenFramebuffers(1,&eclipseSingleScatteringPrecomputationFBO);
    eclipsedSingleScatteringPrecomputationTextures.clear();
    for(const auto& [scattererName, phaseFuncType] : params.scatterers)
    {
        auto& textures=eclipsedSingleScatteringPrecomputationTextures[scattererName];
        for(unsigned wlSetIndex=0; wlSetIndex<params.wavelengthSetCount; ++wlSetIndex)
        {
            auto& tex=*textures.emplace_back(std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D));
            tex.setMinificationFilter(QOpenGLTexture::Linear);
            tex.setMagnificationFilter(QOpenGLTexture::Linear);
            // relative azimuth; we don't rely on auto-repeater, since it'd shift the computed azimuths by half a texel
            tex.setWrapMode(QOpenGLTexture::DirectionS, QOpenGLTexture::ClampToEdge);
            // cosVZA
            tex.setWrapMode(QOpenGLTexture::DirectionT, QOpenGLTexture::ClampToEdge);
            tex.bind();
            const auto width=params.eclipseSingleScatteringTextureSizeForRelAzimuth;
            const auto height=params.eclipseSingleScatteringTextureSizeForCosVZA;
            gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);

            if(phaseFuncType==PhaseFunctionType::Achromatic || phaseFuncType==PhaseFunctionType::Smooth)
                break;
        }
    }

    GLint viewport[4];
    gl.glGetIntegerv(GL_VIEWPORT, viewport);
    const int width=viewport[2], height=viewport[3];
    resizeEvent(width,height);
}

AtmosphereRenderer::AtmosphereRenderer(QOpenGLFunctions_3_3_Core& gl, QString const& pathToData,
                                       Parameters const& params, ToolsWidget* tools)
    : gl(gl)
    , tools(tools)
    , params(params)
    , multipleScatteringTexture(QOpenGLTexture::Target3D)
    , bayerPatternTexture(QOpenGLTexture::Target2D)
    , mainFBOTexture(QOpenGLTexture::Target2D)
{
    for(const auto& [scattererName,_] : params.scatterers)
        scatterersEnabledStates[scattererName]=true;
    loadShaders(pathToData);
    loadTextures(pathToData);
    setupRenderTarget();
    setupBuffers();
}

AtmosphereRenderer::~AtmosphereRenderer()
{
    gl.glDeleteBuffers(1, &vbo);
    gl.glDeleteVertexArrays(1, &vao);
    gl.glDeleteFramebuffers(1, &mainFBO);
    gl.glDeleteFramebuffers(1, &eclipseSingleScatteringPrecomputationFBO);
}

void AtmosphereRenderer::resizeEvent(const int width, const int height)
{
    assert(mainFBO);
    mainFBOTexture.bind();
    gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,mainFBO);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,mainFBOTexture.textureId(),0);
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

void AtmosphereRenderer::setScattererEnabled(QString const& name, const bool enable)
{
    scatterersEnabledStates[name]=enable;
    emit needRedraw();
}

void AtmosphereRenderer::reloadShaders(QString const& pathToData)
{
    std::cerr << "Reloading shaders... ";
    loadShaders(pathToData);
    std::cerr << "done\n";
}
