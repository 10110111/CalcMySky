#version 330
#extension GL_ARB_shading_language_420pack : require
#extension GL_ARB_gpu_shader_fp64 : require

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

vec3 normalToEarth(vec3 point)
{
    return normalize(point-earthCenter);
}

float pointAltitude(vec3 point)
{
    return length(point-earthCenter)-earthRadius;
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

float cosZenithAngleOfHorizon(const float altitude)
{
    const float R=earthRadius;
    const float h=max(0.,altitude); // negative values would result in sqrt(-|x|)
    return -sqrt(2*h*R+sqr(h))/(R+h);
}

bool rayIntersectsGround(const float cosViewZenithAngle, const float observerAltitude)
{
    return cosViewZenithAngle<cosZenithAngleOfHorizon(observerAltitude);
}

float distanceToNearestAtmosphereBoundary(const float cosZenithAngle, const float observerAltitude,
                                          const bool viewRayIntersectsGround)
{
    return viewRayIntersectsGround ? distanceToGround(cosZenithAngle, observerAltitude)
                                   : distanceToAtmosphereBorder(cosZenithAngle, observerAltitude);
}

float sunVisibility(const float cosSunZenithAngle, float altitude)
{
    if(altitude<0) altitude=0;
    const float sinHorizonZenithAngle = earthRadius/(earthRadius+altitude);
    const float cosHorizonZenithAngle = -sqrt(1-sqr(sinHorizonZenithAngle));
    /* Approximating visible fraction of solar disk by smoothstep between the position of the Sun
     * touching the horizon by its upper part and the position with lower part touching the horizon.
     * The calculation assumes that solar angular radius is small and thus approximately equals its sine.
     * For details, see Bruneton's explanation before GetTransmittanceToSun() in the updated
     * Precomputed Atmospheric Scattering demo.
     */
     return smoothstep(-sinHorizonZenithAngle*sunAngularRadius,
                        sinHorizonZenithAngle*sunAngularRadius,
                        cosSunZenithAngle-cosHorizonZenithAngle);
}

/*
   R1,R2 - radii of the circles
   d - distance between centers of the circles
   returns area of intersection of these circles
 */
float circlesIntersectionArea(float R1, float R2, float d)
{
    if(d+min(R1,R2)<max(R1,R2)) return PI*sqr(min(R1,R2));
    if(d>=R1+R2) return 0.;

    // Return area of the lens with radii R1 and R2 and offset d
    return sqr(R1)*acos(clamp( (sqr(d)+sqr(R1)-sqr(R2))/(2*d*R1) ,-1.,1.)) +
           sqr(R2)*acos(clamp( (sqr(d)+sqr(R2)-sqr(R1))/(2*d*R2) ,-1.,1.)) -
           0.5*sqrt(max( (-d+R1+R2)*(d+R1-R2)*(d-R1+R2)*(d+R1+R2) ,0.));
}

float angleBetween(dvec3 a, dvec3 b)
{
    // NOTE: if we calculate dot(a,b) and only then divide by norms of a and b,
    // precision will be much worse. So normalize before calculation of dot.
    a=normalize(a);
    b=normalize(b);
    const double c=dot(a,b);
    // Don't let rounding errors lead to NaNs.
    if(c<=-1) return PI;
    if(c>=+1) return 0;
    // Don't lose precision for very small angles: they are the most precious.
    // Note that the precision loss we're concerned about here is not in the
    // acos or sqrt: it's instead in float(c), which can lead to high
    // granularity near c==1.0, which corresponds to the smallest and most
    // interesting angles. Computing 1-c*c in double precision gives us small
    // numbers, with which we'll not have any problems even after we convert
    // them to float.
    if(c>0.9) return asin(sqrt(float(1-c*c)));
    return acos(float(c));
}

float angleBetweenSunAndMoon(const vec3 camera, const vec3 sunDir, const vec3 moonPos)
{
    return angleBetween(dvec3(sunDir), dvec3(moonPos-camera));
}

float visibleSolidAngleOfSun(const vec3 camera, const vec3 sunDir, const vec3 moonPos)
{
    const float Rs=sunAngularRadius;
    const float Rm=moonAngularRadius;
    float visibleSolidAngle=PI*sqr(Rs);

    const float dSM=angleBetweenSunAndMoon(camera,sunDir,moonPos);
    if(dSM<Rs+Rm)
    {
        visibleSolidAngle -= circlesIntersectionArea(Rm,Rs,dSM);
    }

    return visibleSolidAngle;
}

float sunVisibilityDueToMoon(const vec3 camera, const vec3 sunDir, const vec3 moonPos)
{
    return visibleSolidAngleOfSun(camera,sunDir,moonPos)/(PI*sqr(sunAngularRadius));
}
