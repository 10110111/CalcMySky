#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "common-functions.h.glsl"
#include "multiple-scattering.h.glsl"
#include "texture-coordinates.h.glsl"

uniform int layer;

out vec4 scatteringTextureOutput;

void main()
{
    CONST ScatteringTexVars vars=scatteringTexIndicesToTexVars(vec3(gl_FragCoord.xy-vec2(0.5),layer));
    CONST float dotViewSun = calcDotViewSun(vars.azimuthRelativeToSun,
                                            vars.cosSunZenithAngle,
                                            vars.cosViewZenithAngle);
    scatteringTextureOutput=computeMultipleScattering(vars.cosSunZenithAngle, vars.cosViewZenithAngle,
                                                      dotViewSun, vars.azimuthRelativeToSun,
                                                      vars.altitude, vars.viewRayIntersectsGround);
}
