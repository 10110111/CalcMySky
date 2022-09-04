#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "texture-coordinates.h.glsl"

in vec3 position;
out vec4 color;

#include "compute-transmittance-functions.h.glsl"

void main()
{
    CONST vec2 texCoord=0.5*position.xy+vec2(0.5);
    CONST TransmittanceTexVars vars=transmittanceTexCoordToTexVars(texCoord);
    color=computeTransmittanceToAtmosphereBorder(vars.cosViewZenithAngle, vars.altitude);
}
