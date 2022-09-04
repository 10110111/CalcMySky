#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "densities.h.glsl"
#include "common-functions.h.glsl"
#include "single-scattering.h.glsl"
#include "texture-sampling-functions.h.glsl"

// This function omits phase function and solar irradiance: these are to be applied somewhere in the calling code.
vec4 computeSingleScatteringIntegrand(const float cosSunZenithAngle, const float cosViewZenithAngle,
                                      const float dotViewSun, const float altitude,
                                      const float dist, const bool viewRayIntersectsGround)
{
    CONST float r=earthRadius+altitude;
    // Clamping only guards against rounding errors here, we don't try to handle here the case when the
    // endpoint of the view ray intentionally appears in outer space.
    CONST float altAtDist=clampAltitude(sqrt(sqr(dist)+sqr(r)+2*r*dist*cosViewZenithAngle)-earthRadius);
    CONST float cosSunZenithAngleAtDist=clampCosine((r*cosSunZenithAngle+dist*dotViewSun)/(earthRadius+altAtDist));

    CONST vec4 xmittance=transmittance(cosViewZenithAngle, altitude, dist, viewRayIntersectsGround)
                                                    *
                         transmittanceToAtmosphereBorder(cosSunZenithAngleAtDist, altAtDist)
                                                    *
                                  sunVisibility(cosSunZenithAngleAtDist, altAtDist);
    return xmittance*scattererDensity(altAtDist);
}

vec4 computeSingleScattering(const float cosSunZenithAngle, const float cosViewZenithAngle,
                             const float dotViewSun, const float altitude,
                             const bool viewRayIntersectsGround)
{
    CONST float integrInterval=distanceToNearestAtmosphereBoundary(cosViewZenithAngle, altitude,
                                                                   viewRayIntersectsGround);
    // Using the midpoint rule for quadrature
    vec4 spectrum=vec4(0);
    CONST float dl=integrInterval/radialIntegrationPoints;
    for(int n=0; n<radialIntegrationPoints; ++n)
    {
        CONST float dist=(n+0.5)*dl;
        spectrum += computeSingleScatteringIntegrand(cosSunZenithAngle, cosViewZenithAngle, dotViewSun,
                                                     altitude, dist, viewRayIntersectsGround);
    }
    spectrum *= dl*solarIrradianceAtTOA*scatteringCrossSection();
    return spectrum;
}
