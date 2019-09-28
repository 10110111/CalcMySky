#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"

// Assumes that if its argument is negative, it's due to rounding errors and
// should instead be zero.
float safeSqrt(float x)
{
    return sqrt(max(x,0.));
}

float distanceToAtmosphereBorder(float cosZenithAngle, float observerAltitude)
{
    const float Robs=earthRadius+observerAltitude;
    const float Ratm=earthRadius+atmosphereHeight;
    const float discriminant=sqr(Ratm)-sqr(Robs)*(1-sqr(cosZenithAngle));
    return safeSqrt(discriminant)-Robs*cosZenithAngle;
}
