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
    CONST LightPollutionTexVars vars=scatteringTexIndicesToLightPollutionTexVars(vec3(gl_FragCoord.xy-vec2(0.5),layer));
    CONST vec3 cameraPos = vec3(0,0,altitude);
    CONST vec3 viewDir = vec3(cos(vars.relativeAzimuthFromSource)*safeSqrt(1-sqr(vars.cosViewZenithAngle)),
                              sin(vars.relativeAzimuthFromSource)*safeSqrt(1-sqr(vars.cosViewZenithAngle)),
                              vars.cosViewZenithAngle);
    CONST vec3 emitterPos = vec3(
                                 cos(vars.relativeAzimuthFromSource)*sin(groundDistanceToEmitter/earthRadius),
                                 sin(vars.relativeAzimuthFromSource)*sin(groundDistanceToEmitter/earthRadius),
                                 cos(groundDistanceToEmitter/earthRadius)
                                )*earthRadius+earthCenter;
    scatteringTextureOutput=computeSingleScatteringForLightPollution(cameraPos, viewDir, emitterPos, vars.altitude,
                                                                     vars.cosViewZenithAngle, vars.viewRayIntersectsGround);
    deltaScatteringTextureOutput=scatteringTextureOutput;
}
