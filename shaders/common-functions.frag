#version 330
#include "version.h.glsl"
#include "const.h.glsl"

const float goldenRatio=1.6180339887499;

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

void swap(inout float x, inout float y)
{
    CONST float t = x;
    x = y;
    y = t;
}

// Assumes that if its argument is negative, it's due to rounding errors and
// should instead be zero.
float safeSqrt(const float x)
{
    return sqrt(max(x,0.));
}

float safeAtan(const float y, const float x)
{
    CONST float a = atan(y,x);
    return x==0 && y==0 ? 0 : a;
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
    CONST float Robs=earthRadius+observerAltitude;
    CONST float Ratm=earthRadius+atmosphereHeight;
    CONST float discriminant=sqr(Ratm)-sqr(Robs)*(1-sqr(cosZenithAngle));
    return clampDistance(safeSqrt(discriminant)-Robs*cosZenithAngle);
}

float distanceToGround(const float cosZenithAngle, const float observerAltitude)
{
    CONST float Robs=earthRadius+observerAltitude;
    CONST float discriminant=sqr(earthRadius)-sqr(Robs)*(1-sqr(cosZenithAngle));
    return clampDistance(-safeSqrt(discriminant)-Robs*cosZenithAngle);
}

float cosZenithAngleOfHorizon(const float altitude)
{
    CONST float R=earthRadius;
    CONST float h=max(0.,altitude); // negative values would result in sqrt(-|x|)
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
    CONST float sinHorizonZenithAngle = earthRadius/(earthRadius+altitude);
    CONST float cosHorizonZenithAngle = -sqrt(1-sqr(sinHorizonZenithAngle));
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
    // Note: R1^2-R2^2 must be computed before adding d^2 to the result,
    // otherwise catastrophic cancellation will occur when R1≈R2 and d≈0,
    // which will result in visual artifacts.
    CONST float dRR = sqr(R1)-sqr(R2);
    return sqr(R1)*acos(clampCosine( (sqr(d)+dRR)/(2*d*R1) )) +
           sqr(R2)*acos(clampCosine( (sqr(d)-dRR)/(2*d*R2) )) -
           0.5*sqrt(max( (-d+R1+R2)*(d+R1-R2)*(d-R1+R2)*(d+R1+R2) ,0.));
}

float angleBetween(const vec3 a, const vec3 b)
{
    CONST float d=dot(a,b);
    CONST float c=length(cross(a,b));
    // To avoid loss of precision, don't use dot product near the singularity
    // of acos, and cross product near the singularity of asin
    if(abs(d) < abs(c))
        return acos(d/(length(a)*length(b)));
    CONST float smallerAngle = asin(c/(length(a)*length(b)));
    if(d<0) return PI-smallerAngle;
    return smallerAngle;
}

float angleBetweenSunAndMoon(const vec3 camera, const vec3 sunDir, const vec3 moonPos)
{
    return angleBetween(sunDir, moonPos-camera);
}

float moonAngularRadius(const vec3 cameraPosition, const vec3 moonPosition)
{
    return moonRadius/length(moonPosition-cameraPosition);
}

float visibleSolidAngleOfSun(const vec3 camera, const vec3 sunDir, const vec3 moonPos)
{
    CONST float Rs=sunAngularRadius;
    CONST float Rm=moonAngularRadius(camera,moonPos);
    float visibleSolidAngle=PI*sqr(Rs);

    CONST float dSM=angleBetweenSunAndMoon(camera,sunDir,moonPos);
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

float sphereIntegrationSolidAngleDifferential(const int pointCountOnSphere)
{
    return 4*PI/pointCountOnSphere;
}
vec3 sphereIntegrationSampleDir(const int index, const int pointCountOnSphere)
{
    // The range of n is 0.5, 1.5, ..., pointCountOnSphere-0.5
    CONST float n=index+0.5;
    // Explanation of the Fibonacci grid generation can be seen at https://stackoverflow.com/a/44164075/673852
    CONST float zenithAngle=acos(clamp(1-(2.*n)/pointCountOnSphere, -1.,1.));
    CONST float azimuth=n*(2*PI*goldenRatio);
    return vec3(cos(azimuth)*sin(zenithAngle),
                sin(azimuth)*sin(zenithAngle),
                cos(zenithAngle));
}

float sphericalCapIntegrationSolidAngleDifferential(const int pointCount, const float zenithAngleOfBottom)
{
    // capArea = 2πr²(1-cosθ) = 4πr²sin²(θ/2)
    return 4*PI*sqr(sin(zenithAngleOfBottom/2))/pointCount;
}
vec3 sphericalCapIntegrationSampleDir(const int index, const int pointCount, const float zenithAngleOfBottom)
{
    // Special case for only one point: we want it to be in the center of the cap, for ease of use.
    if(pointCount==1) return vec3(0,0,1);

    // The range of n is 0.5, 1.5, ..., pointCount-0.5
    const float n=index+0.5;
    // Number of points covering the whole sphere, so that number of points in the range of zenithAngle∈[0,zenithAngleOfBottom]
    // was pointCount. In other words, when index+1==pointCount+0.5, we want zenithAngle==zenithAngleOfBottom. The +0.5 on the
    // RHS ensures that at zenithAngleOfBottom==π we reproduce the behavior of sphereIntegrationSampleDir().
    const float virtualPointCount = pointCount/sqr(sin(zenithAngleOfBottom/2));
    // Explanation of the Fibonacci grid generation can be seen at https://stackoverflow.com/a/44164075/673852
    const float sinHalfZenithAngle = sqrt(clampCosine(n/virtualPointCount));
    const float zenithAngle = 2*asin(sinHalfZenithAngle);
    const float azimuth=n*(2*PI*goldenRatio);
    return vec3(cos(azimuth)*sin(zenithAngle),
                sin(azimuth)*sin(zenithAngle),
                cos(zenithAngle));
}
