#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "single-scattering.h.glsl"
#include "texture-coordinates.h.glsl"

uniform int layer;
out vec4 scatteringTextureOutput;

void main()
{
    CONST ScatteringTexVars vars=scatteringTexIndicesToTexVars(vec3(gl_FragCoord.xy-vec2(0.5),layer));
    scatteringTextureOutput=computeSingleScattering(vars.cosSunZenithAngle,vars.cosViewZenithAngle,vars.dotViewSun,
                                                    vars.altitude,vars.viewRayIntersectsGround);
}
