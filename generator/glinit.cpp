#include "glinit.hpp"

#include <iostream>
#include "util.hpp"
#include "data.hpp"

void initBuffers()
{
	gl.glGenVertexArrays(1, &vao);
	gl.glBindVertexArray(vao);
	gl.glGenBuffers(1, &vbo);
	gl.glBindBuffer(GL_ARRAY_BUFFER, vbo);
	static constexpr GLfloat vertices[]=
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

void initTexturesAndFramebuffers()
{
    gl.glGenTextures(TEX_COUNT,textures);
    for(const auto tex : {TEX_TRANSMITTANCE,TEX_DELTA_IRRADIANCE})
    {
        gl.glBindTexture(GL_TEXTURE_2D,textures[tex]);
        gl.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        gl.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    }
    setupTexture(TEX_TRANSMITTANCE,transmittanceTexW,transmittanceTexH);
    setupTexture(TEX_DELTA_IRRADIANCE,irradianceTexW,irradianceTexH);
    setupTexture(TEX_IRRADIANCE,irradianceTexW,irradianceTexH);

    const auto width=scatTexWidth(), height=scatTexHeight(), depth=scatTexDepth();
    for(const auto tex : {TEX_DELTA_SCATTERING,TEX_DELTA_SCATTERING_DENSITY})
    {
        gl.glBindTexture(GL_TEXTURE_3D,textures[tex]);
        gl.glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        gl.glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
        setupTexture(tex,width,height,depth);
    }
    setupTexture(TEX_MULTIPLE_SCATTERING,width,height,depth);

    gl.glGenFramebuffers(FBO_COUNT,fbos);
}

void checkLimits()
{
    GLint max3DTexSize=-1;
    gl.glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &max3DTexSize);
    const unsigned max3DSize=max3DTexSize; // to silence compiler warning about comparisons
    if(scatTexWidth()>max3DSize || scatTexHeight()>max3DSize || scatTexDepth()>max3DSize)
    {
        std::cerr << "Scattering texture 3D size of " << scatTexWidth() << "x" << scatTexHeight() << "x" << scatTexDepth() << " is too large: GL_MAX_3D_TEXTURE_SIZE is " << max3DTexSize << "\n";
        throw MustQuit{};
    }
}

void init()
{
    initBuffers();
    initTexturesAndFramebuffers();
    checkLimits();
}

