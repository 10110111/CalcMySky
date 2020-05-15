#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"

bool dbgDataPresent=false;
bool debugDataPresent()
{
    return dbgDataPresent;
}
vec4 dbgData=vec4(0);
vec4 debugData()
{
    return dbgData;
}
void setDebugData(float a)
{
    dbgDataPresent=true;
    dbgData=vec4(a,0,0,0);
}
void setDebugData(float a,float b)
{
    dbgDataPresent=true;
    dbgData=vec4(a,b,0,0);
}
void setDebugData(float a,float b, float c)
{
    dbgDataPresent=true;
    dbgData=vec4(a,b,c,0);
}
void setDebugData(float a,float b, float c, float d)
{
    dbgDataPresent=true;
    dbgData=vec4(a,b,c,d);
}

// Assumes that if its argument is negative, it's due to rounding errors and
// should instead be zero.
float safeSqrt(const float x)
{
    return sqrt(max(x,0.));
}

float clampCosine(const float x)
{
    return clamp(x, -1., 1.);
}

// Fixup for possible rounding errors resulting in altitude being outside of theoretical bounds
float clampAltitude(const float altitude)
{
    return clamp(altitude, 0., atmosphereHeight);
}

// Fixup for possible rounding errors resulting in distance being outside of theoretical bounds
float clampDistance(const float d)
{
    return max(d, 0.);
}

float distanceToAtmosphereBorder(const float cosZenithAngle, const float observerAltitude)
{
    const float Robs=earthRadius+observerAltitude;
    const float Ratm=earthRadius+atmosphereHeight;
    const float discriminant=sqr(Ratm)-sqr(Robs)*(1-sqr(cosZenithAngle));
    return clampDistance(safeSqrt(discriminant)-Robs*cosZenithAngle);
}

float distanceToGround(const float cosZenithAngle, const float observerAltitude)
{
    const float Robs=earthRadius+observerAltitude;
    const float discriminant=sqr(earthRadius)-sqr(Robs)*(1-sqr(cosZenithAngle));
    return clampDistance(-safeSqrt(discriminant)-Robs*cosZenithAngle);
}

bool rayIntersectsGround(const float cosViewZenithAngle, const float observerAltitude)
{
    const float Robs=earthRadius+observerAltitude;
    const float discriminant=sqr(earthRadius)-sqr(Robs)*(1-sqr(cosViewZenithAngle));
    return cosViewZenithAngle<0 && discriminant>0;
}

float distanceToNearestAtmosphereBoundary(const float cosZenithAngle, const float observerAltitude,
                                          const bool viewRayIntersectsGround)
{
    return viewRayIntersectsGround ? distanceToGround(cosZenithAngle, observerAltitude)
                                   : distanceToAtmosphereBorder(cosZenithAngle, observerAltitude);
}
