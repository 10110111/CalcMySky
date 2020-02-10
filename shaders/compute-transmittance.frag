#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "texture-coordinates.h.glsl"

in vec3 position;
out vec4 color;

#include "compute-transmittance-functions.h.glsl"

void main()
{
    const vec2 texCoord=0.5*position.xy+vec2(0.5);
    const TransmittanceTexVars vars=transmittanceTexCoordToTexVars(texCoord);
    color=computeTransmittanceToAtmosphereBorder(vars.cosViewZenithAngle, vars.altitude);
}
