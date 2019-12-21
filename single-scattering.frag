#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "densities.h.glsl"
#include "common-functions.h.glsl"
#include "single-scattering.h.glsl"
#include "texture-sampling-functions.h.glsl"

uniform vec4 solarIrradianceAtTOA;

// This function omits phase function and solar irradiance: these are to be applied somewhere in the calling code.
vec4 computeSingleScatteringIntegrand(const float cosSunZenithAngle, const float cosViewZenithAngle,
                                      const float dotViewSun, const float altitude,
                                      const float dist, const bool viewRayIntersectsGround)
{
    const float r=earthRadius+altitude;
    // Clamping only guards against rounding errors here, we don't try to handle here the case when the
    // endpoint of the view ray intentionally appears in outer space.
    const float altAtDist=clampAltitude(sqrt(sqr(dist)+sqr(r)+2*r*dist*cosViewZenithAngle)-earthRadius);
    const float cosSunZenithAngleAtDist=clampCosine((r*cosSunZenithAngle+dist*dotViewSun)/(earthRadius+altAtDist));

    const vec4 xmittance=transmittance(cosViewZenithAngle, altitude, dist, viewRayIntersectsGround)
                                                    *
                         transmittanceToSun(cosSunZenithAngleAtDist, altAtDist);
    return xmittance*scattererDensity(altAtDist);
}

vec4 computeSingleScattering(const float cosSunZenithAngle, const float cosViewZenithAngle,
                             const float dotViewSun, const float altitude,
                             const bool viewRayIntersectsGround)
{
    const float integrInterval=distanceToNearestAtmosphereBoundary(cosViewZenithAngle, altitude,
                                                                   viewRayIntersectsGround);

    // Using trapezoid rule on a uniform grid: f0/2+f1+f2+...+f(N-2)+f(N-1)/2.
    // Initializing with sum of values at endpoints, with weight of 0.5
    const vec4 end1=computeSingleScatteringIntegrand(cosSunZenithAngle, cosViewZenithAngle, dotViewSun,
                                                     altitude, 0, viewRayIntersectsGround);
    const vec4 end2=computeSingleScatteringIntegrand(cosSunZenithAngle, cosViewZenithAngle, dotViewSun,
                                                     altitude, integrInterval, viewRayIntersectsGround);
    vec4 spectrum=(end1+end2)*0.5;

    const float dl=integrInterval/(radialIntegrationPoints-1);
    for(int n=1; n<radialIntegrationPoints-1; ++n)
    {
        const float dist=n*dl;
        spectrum += computeSingleScatteringIntegrand(cosSunZenithAngle, cosViewZenithAngle, dotViewSun,
                                                     altitude, dist, viewRayIntersectsGround);
    }
    spectrum *= dl*solarIrradianceAtTOA*scatteringCrossSection();
    return spectrum;
}
