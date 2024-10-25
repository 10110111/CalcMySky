#version 330
#include "version.h.glsl"
#include "phase-functions.h.glsl"
#include "common-functions.h.glsl"
#include "texture-coordinates.h.glsl"

uniform int layer;
uniform sampler3D tex;
uniform bool embedPhaseFunction;
out vec4 scatteringTextureOutput;

uniform mat4 radianceToLuminance=mat4(1);

void main()
{
    CONST vec3 coord=vec3(gl_FragCoord.xy,layer+0.5)/textureSize(tex,0);
    scatteringTextureOutput = radianceToLuminance * texture(tex, coord);
    if(embedPhaseFunction)
    {
        CONST ScatteringTexVars vars=scatteringTexIndicesToTexVars(vec3(gl_FragCoord.xy-vec2(0.5),layer));
        CONST float dotViewSun = calcDotViewSun(vars.azimuthRelativeToSun,
                                                vars.cosSunZenithAngle,
                                                vars.cosViewZenithAngle);
        scatteringTextureOutput *= currentPhaseFunction(dotViewSun);
    }
}
