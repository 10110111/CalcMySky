#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "single-scattering-light-pollution.h.glsl"
#include "texture-coordinates.h.glsl"

uniform int layer;
layout(location=0) out vec4 scatteringTextureOutput;
layout(location=1) out vec4 deltaScatteringTextureOutput;

void main()
{
    scatteringTextureOutput=vec4(0);
    deltaScatteringTextureOutput=scatteringTextureOutput;
}
