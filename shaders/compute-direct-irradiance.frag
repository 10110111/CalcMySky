#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "direct-irradiance.h.glsl"
#include "texture-sampling-functions.h.glsl"
#include "texture-coordinates.h.glsl"

in vec3 position;
layout(location=0) out vec4 deltaIrradianceOutput;
layout(location=1) out vec4 irradianceOutput;

void main()
{
    CONST vec2 texCoord=0.5*position.xy+vec2(0.5);
    CONST IrradianceTexVars vars=irradianceTexCoordToTexVars(texCoord);
    CONST vec4 color=computeDirectGroundIrradiance(vars.cosSunZenithAngle, vars.altitude);
    deltaIrradianceOutput=color;
    irradianceOutput=color;
}
