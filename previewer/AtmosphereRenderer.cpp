#include "AtmosphereRenderer.hpp"

#include <array>
#include <vector>
#include <cstring>
#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>

#include "../common/util.hpp"

static constexpr double degree=M_PI/180;
static constexpr double lengthUnitInMeters=1000;
static constexpr double earthRadius=6.36e6/lengthUnitInMeters;
static constexpr double sunAngularRadius=0.00935/2;

template<typename OutType=std::vector<char>>
OutType readFileData(std::string const& path)
{
    std::ifstream file(path);
    if(!file) throw std::runtime_error("Failed to open file \""+path+"\"");
    file.exceptions(std::ios_base::failbit|std::ios_base::badbit);

    file.seekg(0, std::ios_base::end);
    const auto fileSize=file.tellg();
    file.seekg(0);

    OutType data(fileSize,'\0');
    file.read(&data[0], fileSize);
    return data;
}

struct TextureSize4D
{
    std::int32_t sizes[4];
    int muS_size() const { return sizes[0]; }
    int mu_size() const { return sizes[1]; }
    int nu_size() const { return sizes[2]; }
    int r_size() const { return sizes[3]; }
    int width() const { return muS_size(); }
    int height() const { return mu_size(); }
    int depth() const { return nu_size()*r_size(); }
};
TextureSize4D getTextureSize4D(std::vector<char> const& data)
{
    std::int32_t dim;
    std::memcpy(&dim, &data.back()+1-sizeof dim, sizeof dim);
    if(dim!=4)
    {
        throw std::runtime_error("Bad dimension for 4D texture: "+std::to_string(dim)+
                                 " (texture size: "+std::to_string(data.size())+")");
    }

    TextureSize4D out;
    if(data.size()<=sizeof dim+sizeof out.sizes)
        throw std::runtime_error("Too small texture file");

    std::memcpy(out.sizes, &data.back()+1-(sizeof dim+sizeof out.sizes), sizeof out.sizes);

    if(data.size()!=out.r_size()*out.mu_size()*out.muS_size()*out.nu_size()*sizeof(GLfloat)*4+sizeof dim+sizeof out.sizes)
        throw std::runtime_error("Bad 4D texture size "+std::to_string(data.size()));

    return out;
}
struct TextureSize2D
{
    std::int32_t sizes[2];
    int width() const { return sizes[0]; }
    int height() const { return sizes[1]; }
};
TextureSize2D getTextureSize2D(std::vector<char> const& data)
{
    std::int32_t dim;
    std::memcpy(&dim, &data.back()+1-sizeof dim, sizeof dim);
    if(dim!=2)
    {
        throw std::runtime_error("Bad dimension for 2D texture: "+std::to_string(dim)+
                                 " (texture size: "+std::to_string(data.size())+")");
    }

    TextureSize2D out;
    if(data.size()<=sizeof dim+sizeof out.sizes)
        throw std::runtime_error("Too small texture file");

    std::memcpy(out.sizes, &data.back()+1-(sizeof dim+sizeof out.sizes), sizeof out.sizes);

    if(data.size()!=out.width()*out.height()*sizeof(GLfloat)*4+sizeof dim+sizeof out.sizes)
        throw std::runtime_error("Bad 2D texture size "+std::to_string(data.size()));

    return out;
}

TextureSize2D transmittanceTextureSize, irradianceTextureSize;
TextureSize4D scatteringTextureSize, mieScatteringTextureSize;

void AtmosphereRenderer::bindAndSetupTexture(const GLenum target, const GLuint texture)
{
    gl.glBindTexture(target, texture);
    gl.glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if(target==GL_TEXTURE_3D)
        gl.glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

void AtmosphereRenderer::loadTextures(std::string const& pathToData)
{
    while(gl.glGetError()!=GL_NO_ERROR);
    gl.glGenTextures(TEX_COUNT, textures);

    gl.glActiveTexture(GL_TEXTURE0);

    {
        bindAndSetupTexture(GL_TEXTURE_2D, textures[TRANSMITTANCE_TEXTURE]);
        const auto data=readFileData(pathToData+"/transmittance.dat");
        transmittanceTextureSize=getTextureSize2D(data);
        gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,
                     transmittanceTextureSize.width(), transmittanceTextureSize.height(),
                     0, GL_RGBA, GL_FLOAT, data.data());
    }
    {
        bindAndSetupTexture(GL_TEXTURE_3D, textures[SCATTERING_TEXTURE]);
        const auto data=readFileData(pathToData+"/scattering.dat");
        scatteringTextureSize=getTextureSize4D(data);
        gl.glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F,
                     scatteringTextureSize.width(), scatteringTextureSize.height(),
                     scatteringTextureSize.depth(),
                     0, GL_RGBA, GL_FLOAT, data.data());
    }
    {
        bindAndSetupTexture(GL_TEXTURE_2D, textures[IRRADIANCE_TEXTURE]);
        const auto data=readFileData(pathToData+"/irradiance.dat");
        irradianceTextureSize=getTextureSize2D(data);
        gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,
                     irradianceTextureSize.width(), irradianceTextureSize.height(),
                     0, GL_RGBA, GL_FLOAT, data.data());
    }
    try
    {
        bindAndSetupTexture(GL_TEXTURE_3D, textures[MIE_SCATTERING_TEXTURE]);
        const auto data=readFileData(pathToData+"/mie_scattering.dat");
        const auto mieScatteringTextureSize=getTextureSize4D(data);
        gl.glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F,
                     mieScatteringTextureSize.width(), mieScatteringTextureSize.height(),
                     mieScatteringTextureSize.depth(),
                     0, GL_RGBA, GL_FLOAT, data.data());
    }
    catch(std::runtime_error const&)
    {
        std::cerr << "Separate Mie scattering texture wasn't found, assuming the shader doesn't need it.\n";
    }

    gl.glBindTexture(GL_TEXTURE_2D, 0);
    assert(gl.glGetError()==GL_NO_ERROR);
}

void replaceConstant(std::string& s, std::string const& name, const int value)
{
    {
        const auto sphereSizeToFind=std::string("kSphereRadius = 1000.0 ");
        const auto foundSphereSize=s.find(sphereSizeToFind);
        if(foundSphereSize!=s.npos)
            s.replace(foundSphereSize, sphereSizeToFind.size(), "kSphereRadius = 1.");
    }

    const auto foundName=s.find(name);
    if(foundName==s.npos) return;
    const auto foundSemicolon=s.find(';',foundName+name.length());
    if(foundSemicolon==s.npos)
        throw std::runtime_error("Replacing constant "+name+": failed to find semicolon");
    if(foundSemicolon<foundName+name.length()+4) // at least 4 chars must be between name and ';': for " = [0-9]"
        throw std::runtime_error("Replacing constant "+name+": too small space between name and semicolon");
    if(s.substr(foundName+name.length(), 3)!=" = ")
        throw std::runtime_error("Replacing constant "+name+": unexpected format of constant initialization");
    s.replace(foundName+name.length()+3, foundSemicolon-(foundName+name.length()+3), std::to_string(value));
}

GLuint AtmosphereRenderer::makeShader(const GLenum type, std::string const& srcFilePath)
{
    while(gl.glGetError()!=GL_NO_ERROR);
    const auto shader=gl.glCreateShader(type);
    auto fileData=readFileData<std::string>(srcFilePath);
    replaceConstant(fileData, "TRANSMITTANCE_TEXTURE_WIDTH",  transmittanceTextureSize.width());
    replaceConstant(fileData, "TRANSMITTANCE_TEXTURE_HEIGHT", transmittanceTextureSize.height());
    replaceConstant(fileData, "IRRADIANCE_TEXTURE_WIDTH",  irradianceTextureSize.width());
    replaceConstant(fileData, "IRRADIANCE_TEXTURE_HEIGHT", irradianceTextureSize.height());
    replaceConstant(fileData, "SCATTERING_TEXTURE_R_SIZE", scatteringTextureSize.r_size());
    replaceConstant(fileData, "SCATTERING_TEXTURE_MU_SIZE", scatteringTextureSize.mu_size());
    replaceConstant(fileData, "SCATTERING_TEXTURE_MU_S_SIZE", scatteringTextureSize.muS_size());
    replaceConstant(fileData, "SCATTERING_TEXTURE_NU_SIZE", scatteringTextureSize.nu_size());
    const GLchar* src=fileData.data();
    const GLint srcLen=fileData.size();
    gl.glShaderSource(shader, 1, &src, &srcLen);
    gl.glCompileShader(shader);
    assert(gl.glGetError()==GL_NO_ERROR);
    GLint status=-1;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if(gl.glGetError()!=GL_NO_ERROR)
        throw std::runtime_error("glGetShaderiv failed");
    if(status==GL_FALSE)
    {
        std::string errorStr="Failed to compile shader \""+srcFilePath+"\":\n";
        GLint infoLogLen=-1;
        gl.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
        assert(infoLogLen>=0);
        auto infoLog=std::string(infoLogLen, '\0');
        gl.glGetShaderInfoLog(shader, infoLogLen, nullptr, &infoLog[0]);
        throw std::runtime_error(errorStr+infoLog);
    }
    return shader;
}

void AtmosphereRenderer::loadShaders(std::string const& pathToData)
{
    program=gl.glCreateProgram();

    const auto vertexShader=makeShader(GL_VERTEX_SHADER, pathToData+"/vertex_shader.txt");
    gl.glAttachShader(program, vertexShader);

    const auto fragmentShader=makeShader(GL_FRAGMENT_SHADER, pathToData+"/fragment_shader.txt");
    gl.glAttachShader(program, fragmentShader);

    const auto atmosphereShader=makeShader(GL_FRAGMENT_SHADER, pathToData+"/atmosphere_shader.txt");
    gl.glAttachShader(program, atmosphereShader);

    gl.glLinkProgram(program);
    assert(gl.glGetError()==GL_NO_ERROR);
    GLint status=-1;
    gl.glGetProgramiv(program, GL_LINK_STATUS, &status);
    if(gl.glGetError()!=GL_NO_ERROR)
        throw std::runtime_error("glGetProgramiv failed");
    if(status==GL_FALSE)
    {
        std::string errorStr="Failed to link shader program:\n";
        GLint infoLogLen=-1;
        gl.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLen);
        assert(infoLogLen>=0);
        std::string infoLog(infoLogLen, '\0');
        gl.glGetProgramInfoLog(program, infoLogLen, nullptr, &infoLog[0]);
        throw std::runtime_error(errorStr+infoLog);
    }

    gl.glDetachShader(program, atmosphereShader);
    gl.glDeleteShader(atmosphereShader);

    gl.glDetachShader(program, fragmentShader);
    gl.glDeleteShader(fragmentShader);

    gl.glDetachShader(program, vertexShader);
    gl.glDeleteShader(vertexShader);
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

auto AtmosphereRenderer::getMeanPixelValue(const int texW, const int texH) -> Vec4
{
    // Get average value of the rendered pixels as the value of the deepest mipmap level
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, textures[TEX_FBO]);
    gl.glGenerateMipmap(GL_TEXTURE_2D);

    using namespace std;
    // Formula from the glspec, "Mipmapping" subsection in section 3.8.11 Texture Minification
    const auto totalMipmapLevels = 1+floor(log2(max(texW,texH)));
    const auto deepestLevel=totalMipmapLevels-1;

#ifndef NDEBUG
    // Sanity check
    int deepestMipmapLevelWidth=-1, deepestMipmapLevelHeight=-1;
    gl.glGetTexLevelParameteriv(GL_TEXTURE_2D, deepestLevel, GL_TEXTURE_WIDTH, &deepestMipmapLevelWidth);
    gl.glGetTexLevelParameteriv(GL_TEXTURE_2D, deepestLevel, GL_TEXTURE_HEIGHT, &deepestMipmapLevelHeight);
    assert(deepestMipmapLevelWidth==1);
    assert(deepestMipmapLevelHeight==1);
#endif

    Vec4 pixel;
    gl.glGetTexImage(GL_TEXTURE_2D, deepestLevel, GL_RGBA, GL_FLOAT, &pixel[0]);
    return pixel;
}

void AtmosphereRenderer::draw()
{
    GLint viewport[4];
    gl.glGetIntegerv(GL_VIEWPORT, viewport);
    const int width=viewport[2], height=viewport[3];

    GLint targetFBO=-1;
    gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &targetFBO);

    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbo);

    gl.glBindVertexArray(vao);
    gl.glUseProgram(program);

    {
        const GLfloat tanFovY=std::tan(fovY/2);
        const auto aspect=GLfloat(width)/height;
        const GLfloat viewFromClip[]={tanFovY*aspect,  0,   0, 0,
                                           0,      tanFovY, 0, 0,
                                           0,          0,   0,-1,
                                           0,          0,   1, 1};
        gl.glUniformMatrix4fv(gl.glGetUniformLocation(program,"view_from_clip"), 1, true, viewFromClip);
    }
    {
        const GLfloat cosZ=std::cos(viewZenithAngle_);
        const GLfloat sinZ=std::sin(viewZenithAngle_);
        const GLfloat cosA=std::cos(viewAzimuth_);
        const GLfloat sinA=std::sin(viewAzimuth_);
        const GLfloat altitude=altitudeInMeters/lengthUnitInMeters;
        const GLfloat modelFromView[]={-sinA, -cosZ*cosA, sinZ*cosA, 0,
                                        cosA, -cosZ*sinA, sinZ*sinA, 0,
                                          0,      sinZ,     cosZ, altitude,
                                          0,        0,       0,      1};
        gl.glUniformMatrix4fv(gl.glGetUniformLocation(program,"model_from_view"), 1, true, modelFromView);
        gl.glUniform3f (gl.glGetUniformLocation(program,"camera"), modelFromView[3],modelFromView[7],modelFromView[11]);
    }
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, textures[TRANSMITTANCE_TEXTURE]);
    gl.glUniform1i(gl.glGetUniformLocation(program,"transmittance_texture"), 0);
    gl.glActiveTexture(GL_TEXTURE1);
    gl.glBindTexture(GL_TEXTURE_3D, textures[SCATTERING_TEXTURE]);
    gl.glUniform1i(gl.glGetUniformLocation(program,"scattering_texture"), 1);
    gl.glActiveTexture(GL_TEXTURE2);
    gl.glBindTexture(GL_TEXTURE_2D, textures[IRRADIANCE_TEXTURE]);
    gl.glUniform1i(gl.glGetUniformLocation(program,"irradiance_texture"), 2);
    gl.glActiveTexture(GL_TEXTURE3);
    gl.glBindTexture(GL_TEXTURE_3D, textures[MIE_SCATTERING_TEXTURE]);
    gl.glUniform1i(gl.glGetUniformLocation(program,"single_mie_scattering_texture"), 3);

    gl.glUniform3f(gl.glGetUniformLocation(program, "white_point"), 1,1,1);
    gl.glUniform1f(gl.glGetUniformLocation(program, "exposure"), exposure);
    gl.glUniform3f(gl.glGetUniformLocation(program, "earth_center"), 0,0, -earthRadius);
    gl.glUniform3f(gl.glGetUniformLocation(program, "sun_direction"),
                std::cos(sunAzimuth_)*std::sin(sunZenithAngle_),
                std::sin(sunAzimuth_)*std::sin(sunZenithAngle_),
                std::cos(sunZenithAngle_));
    gl.glUniform2f(gl.glGetUniformLocation(program, "sun_size"), std::tan(sunAngularRadius), std::cos(sunAngularRadius));

    gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    gl.glBindVertexArray(0);

    gl.glBindFramebuffer(GL_FRAMEBUFFER,0);

    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER,targetFBO);
    gl.glBlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);

    meanPixelValue_=getMeanPixelValue(width, height);
}

void AtmosphereRenderer::setupRenderTarget()
{
    gl.glGenFramebuffers(1,&fbo);
    gl.glBindTexture(GL_TEXTURE_2D,textures[TEX_FBO]);
    gl.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    gl.glBindTexture(GL_TEXTURE_2D,0);

    GLint viewport[4];
    gl.glGetIntegerv(GL_VIEWPORT, viewport);
    const int width=viewport[2], height=viewport[3];
    resizeEvent(width,height);
}

AtmosphereRenderer::AtmosphereRenderer(QOpenGLFunctions_3_3_Core& gl, std::string const& pathToData)
    : gl(gl)
{
    loadTextures(pathToData);
    setupRenderTarget();
    loadShaders(pathToData);
    setupBuffers();
}

AtmosphereRenderer::~AtmosphereRenderer()
{
    gl.glDeleteProgram(program);
    gl.glDeleteBuffers(1, &vbo);
    gl.glDeleteVertexArrays(1, &vao);
    gl.glDeleteTextures(TEX_COUNT, textures);
}

void AtmosphereRenderer::resizeEvent(const int width, const int height)
{
    assert(fbo);
    gl.glBindTexture(GL_TEXTURE_2D,textures[TEX_FBO]);
    gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gl.glBindTexture(GL_TEXTURE_2D,0);
    gl.glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[TEX_FBO],0);
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
        const auto oldZA=sunZenithAngle_, oldAz=sunAzimuth_;
        sunZenithAngle_ -= (prevMouseY-y)/scale;
        sunZenithAngle_ = std::max(0.,std::min(M_PI, sunZenithAngle_));
        sunAzimuth_ += (prevMouseX-x)/scale;
        if(oldZA!=sunZenithAngle_)
            emit sunElevationChanged(M_PI/2-sunZenithAngle_);
        if(oldAz!=sunAzimuth_)
            emit sunAzimuthChanged(sunAzimuth_);
        break;
    }
    case DragMode::Camera:
        viewZenithAngle_ -= (prevMouseY-y)/scale;
        viewZenithAngle_ = std::clamp(viewZenithAngle_, 0., M_PI);
        viewAzimuth_ -= (prevMouseX-x)/scale;
        break;
    default:
        break;
    }
    prevMouseX=x;
    prevMouseY=y;
}

double AtmosphereRenderer::sunElevation() const
{
    return M_PI/2-sunZenithAngle_;
}

void AtmosphereRenderer::setSunElevation(const float elevation)
{
    sunZenithAngle_=M_PI/2-elevation;
}

double AtmosphereRenderer::sunAzimuth() const
{
    return sunAzimuth_;
}

void AtmosphereRenderer::setSunAzimuth(const float azimuth)
{
    sunAzimuth_=azimuth;
}

void AtmosphereRenderer::setAltitude(const double altitude)
{
    altitudeInMeters=altitude;
}

void AtmosphereRenderer::setExposure(const double exposure)
{
    this->exposure=exposure;
}

void AtmosphereRenderer::setFovY(const double fovY)
{
    this->fovY=fovY*degree;
}
