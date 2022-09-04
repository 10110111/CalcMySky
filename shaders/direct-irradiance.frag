#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "texture-sampling-functions.h.glsl"

vec4 computeDirectGroundIrradiance(const float cosSunZenithAngle, const float altitude)
{
    // Several approximations are used:
    // * Radiance is assumed independent of position on the solar disk.
    // * Transmittance is assumed to not change much over the solar disk (i.e. we approximate it with a constant).
    // * Instead of true integration of the cosine factor in the integrand from the definition of irradiance we use its
    //   value in the center of the solar disk, assuming it to be a kind of "average".
    // * When the Sun is partially behind the astronomical horizon, we approximate the radiative view factor (i.e.
    //   cosine factor integrated over the solar disk) with a simple quadratic spline.
    CONST float averageCosFactor = cosSunZenithAngle < -sunAngularRadius ? 0
                                      : cosSunZenithAngle > sunAngularRadius ? cosSunZenithAngle
                                      : sqr(cosSunZenithAngle+sunAngularRadius)/(4*sunAngularRadius);
    return solarIrradianceAtTOA * transmittanceToAtmosphereBorder(cosSunZenithAngle, altitude) * averageCosFactor;
}
