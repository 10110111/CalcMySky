#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "common-functions.h.glsl"
#include "direct-irradiance.h.glsl"
#include "texture-sampling-functions.h.glsl"
#include "texture-coordinates.h.glsl"

in vec3 position;
layout(location=0) out vec4 deltaIrradianceOutput;
layout(location=1) out vec4 irradianceOutput;

void main()
{
    const vec2 texCoord=0.5*position.xy+vec2(0.5);
    const IrradianceTexVars vars=irradianceTexCoordToTexVars(texCoord);
    const vec4 color=computeDirectGroundIrradiance(vars.cosSunZenithAngle, vars.altitude);
    // This output is not blended, so we can safely apply logarithm.
    deltaIrradianceOutput=safeLog(color);
    // This output will be blended, so to avoid turning addition into multiplication we don't
    // apply logarithm here. Instead it will be applied after all the accumulation of scattering
    // orders - during saving the texture in the C++ code.
    irradianceOutput=color;
}
