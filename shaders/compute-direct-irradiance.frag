#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "texture-sampling-functions.h.glsl"
#include "texture-coordinates.h.glsl"

in vec3 position;
layout(location=0) out vec4 deltaIrradianceOutput;
layout(location=1) out vec4 irradianceOutput;

vec4 computeDirectGroundIrradiance(const float cosSunZenithAngle, const float altitude)
{
    // Several approximations are used:
    // * Radiance is assumed independent of position on the solar disk.
    // * Transmittance is assumed to not change much over the solar disk (i.e. we approximate it with a constant).
    // * Instead of true integration of the cosine factor in the integrand from the definition of irradiance we use its
    //   value in the center of the solar disk, assuming it to be a kind of "average".
    // * When the Sun is partially behind the astronomical horizon, we approximate the radiative view factor (i.e.
    //   cosine factor integrated over the solar disk) with a simple quadratic spline.
    const float averageCosFactor = cosSunZenithAngle < -sunAngularRadius ? 0
                                      : cosSunZenithAngle > sunAngularRadius ? cosSunZenithAngle
                                      : sqr(cosSunZenithAngle+sunAngularRadius)/(4*sunAngularRadius);
    return solarIrradianceAtTOA * transmittanceToAtmosphereBorder(cosSunZenithAngle, altitude) * averageCosFactor;
}

void main()
{
    const vec2 texCoord=0.5*position.xy+vec2(0.5);
    const IrradianceTexVars vars=irradianceTexCoordToTexVars(texCoord);
    const vec4 color=computeDirectGroundIrradiance(vars.cosSunZenithAngle, vars.altitude);
    deltaIrradianceOutput=color;
    irradianceOutput=color;
}
