#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "single-scattering-light-pollution.h.glsl"
#include "texture-coordinates.h.glsl"

layout(location=0) out vec4 scatteringTextureOutput;
layout(location=1) out vec4 deltaScatteringTextureOutput;

void main()
{
    CONST LightPollutionTexVars vars=scatteringTexIndicesToLightPollutionTexVars(gl_FragCoord.xy-vec2(0.5));
    scatteringTextureOutput=computeSingleScatteringForLightPollution(vars.cosViewZenithAngle, vars.altitude, vars.viewRayIntersectsGround);
    deltaScatteringTextureOutput=scatteringTextureOutput;
}
