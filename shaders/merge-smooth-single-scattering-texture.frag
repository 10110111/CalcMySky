#version 330
#extension GL_ARB_shading_language_420pack : require

#include "phase-functions.h.glsl"
#include "texture-coordinates.h.glsl"

uniform int layer;
uniform sampler3D tex;
out vec4 result;

void main()
{
    const ScatteringTexVars vars=scatteringTexIndicesToTexVars(vec3(gl_FragCoord.xy-vec2(0.5),layer));
    const vec3 coord=vec3(gl_FragCoord.xy,layer+0.5)/textureSize(tex,0);
    result=texture(tex, coord)*currentPhaseFunction(vars.dotViewSun);
}
