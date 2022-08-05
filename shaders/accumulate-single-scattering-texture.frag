#version 330
#extension GL_ARB_shading_language_420pack : require

#include "phase-functions.h.glsl"
#include "texture-coordinates.h.glsl"

uniform int layer;
uniform sampler3D tex;
uniform bool embedPhaseFunction;
out vec4 scatteringTextureOutput;

uniform mat4 radianceToLuminance=mat4(1);

void main()
{
    const vec3 coord=vec3(gl_FragCoord.xy,layer+0.5)/textureSize(tex,0);
    scatteringTextureOutput = radianceToLuminance * texture(tex, coord);
    if(embedPhaseFunction)
    {
        const ScatteringTexVars vars=scatteringTexIndicesToTexVars(vec3(gl_FragCoord.xy-vec2(0.5),layer));
        scatteringTextureOutput *= currentPhaseFunction(vars.dotViewSun);
    }
}
