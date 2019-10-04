#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "single-scattering.h.glsl"
#include "texture-coordinates.h.glsl"

uniform int layer;
layout(location=0) out vec4 rayleigh;
layout(location=1) out vec4 mie;

void main()
{
    const ScatteringTexVars vars=scatteringTexIndicesToTexVars(vec3(gl_FragCoord.xy-vec2(0.5),layer));
    const ScatteringSpectra scat=computeSingleScattering(vars.cosSunZenithAngle,vars.altitude,vars.dotViewSun,
                                                         vars.cosViewZenithAngle,vars.viewRayIntersectsGround);
    rayleigh=scat.rayleigh;
    mie=scat.mie;
}
