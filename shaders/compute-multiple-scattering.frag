#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "multiple-scattering.h.glsl"
#include "texture-coordinates.h.glsl"

uniform int layer;

out vec4 scatteringTextureOutput;

void main()
{
    const ScatteringTexVars vars=scatteringTexIndicesToTexVars(vec3(gl_FragCoord.xy-vec2(0.5),layer));
    scatteringTextureOutput=computeMultipleScattering(vars.cosSunZenithAngle,vars.cosViewZenithAngle,vars.dotViewSun,
                                                      vars.altitude,vars.viewRayIntersectsGround);
}
