#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "single-scattering.h.glsl"
#include "texture-coordinates.h.glsl"

uniform int layer;
layout(location=0) out vec4 deltaRayleigh;
layout(location=1) out vec4 deltaMie;
layout(location=2) out vec4 rayleigh;
layout(location=3) out vec4 mie;

void main()
{
    const ScatteringTexVars vars=scatteringTexIndicesToTexVars(vec3(gl_FragCoord.xy-vec2(0.5),layer));
    const ScatteringSpectra scat=computeSingleScattering(vars.cosSunZenithAngle,vars.cosViewZenithAngle,vars.dotViewSun,
                                                         vars.altitude,vars.viewRayIntersectsGround);
    // These will be source data for 2nd scattering order
    deltaRayleigh=scat.rayleigh;
    deltaMie=scat.mie;
    // These will be the first part of multiple scattering series
    rayleigh=scat.rayleigh;
    mie=scat.mie;
}
