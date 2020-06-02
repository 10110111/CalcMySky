#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "densities.h.glsl"
#include "common-functions.h.glsl"
#include "texture-sampling-functions.h.glsl"

vec3 normalToEarth(vec3 point)
{
    return normalize(point-earthCenter);
}

float pointAltitude(vec3 point)
{
    return length(point-earthCenter)-earthRadius;
}

float cosZenithAngle(vec3 origin, vec3 direction)
{
    return dot(direction, normalToEarth(origin));
}

// This function omits phase function and solar irradiance: these are to be applied somewhere in the calling code.
vec4 computeSingleScatteringIntegrandEclipsed(const float cosSunZenithAngle, const float cosViewZenithAngle,
                                              const float dotViewSun, const float altitude,
                                              const float dist, const bool viewRayIntersectsGround,
                                              const vec3 scatterer, const vec3 sunDir, const vec3 moonPos)
{
    const float r=earthRadius+altitude;
    // Clamping only guards against rounding errors here, we don't try to handle here the case when the
    // endpoint of the view ray intentionally appears in outer space.
    const float altAtDist=clampAltitude(sqrt(sqr(dist)+sqr(r)+2*r*dist*cosViewZenithAngle)-earthRadius);
    const float cosSunZenithAngleAtDist=clampCosine((r*cosSunZenithAngle+dist*dotViewSun)/(earthRadius+altAtDist));

    const vec4 xmittance=transmittance(cosViewZenithAngle, altitude, dist, viewRayIntersectsGround)
                                                    *
                         transmittanceToAtmosphereBorder(cosSunZenithAngleAtDist, altAtDist)
                                                    *
                                    sunVisibilityDueToMoon(scatterer,sunDir,moonPos)
                                                    *
                            // FIXME: this ignores orientation of the crescent of eclipsed Sun WRT horizon
                                    sunVisibility(cosSunZenithAngleAtDist, altAtDist);
    return xmittance*scattererDensity(altAtDist);
}

vec4 computeSingleScatteringEclipsed(const vec3 camera, const vec3 viewDir, const vec3 sunDir, const vec3 moonPos,
                                     const bool viewRayIntersectsGround)
{
    const float cosViewZenithAngle=cosZenithAngle(camera,viewDir);
    const float cosSunZenithAngle=cosZenithAngle(camera,sunDir);
    const float altitude=pointAltitude(camera);
    const float dotViewSun=dot(viewDir,sunDir);
    const float integrInterval=distanceToNearestAtmosphereBoundary(cosViewZenithAngle, altitude,
                                                                   viewRayIntersectsGround);

    // Using trapezoid rule on a uniform grid: f0/2+f1+f2+...+f(N-2)+f(N-1)/2.
    // Initializing with sum of values at endpoints, with weight of 0.5
    const vec4 end1=computeSingleScatteringIntegrandEclipsed(cosSunZenithAngle, cosViewZenithAngle, dotViewSun,
                                                             altitude, 0, viewRayIntersectsGround,
                                                             camera, sunDir, moonPos);
    const vec4 end2=computeSingleScatteringIntegrandEclipsed(cosSunZenithAngle, cosViewZenithAngle, dotViewSun,
                                                             altitude, integrInterval, viewRayIntersectsGround,
                                                             camera+viewDir*integrInterval, sunDir, moonPos);
    vec4 spectrum=(end1+end2)*0.5;

    const float dl=integrInterval/(radialIntegrationPoints-1);
    for(int n=1; n<radialIntegrationPoints-1; ++n)
    {
        const float dist=n*dl;
        spectrum += computeSingleScatteringIntegrandEclipsed(cosSunZenithAngle, cosViewZenithAngle, dotViewSun,
                                                             altitude, dist, viewRayIntersectsGround,
                                                             camera+viewDir*dist, sunDir, moonPos);
    }
    spectrum *= dl*solarIrradianceAtTOA*scatteringCrossSection();
    return spectrum;
}
