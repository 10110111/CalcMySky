#version 330

#definitions (ALL_SCATTERERS_AT_ONCE_WITH_PHASE_FUNCTION)

#include "version.h.glsl"
#include "const.h.glsl"
#include "densities.h.glsl"
#include "common-functions.h.glsl"
#include "texture-sampling-functions.h.glsl"
#include_if(ALL_SCATTERERS_AT_ONCE_WITH_PHASE_FUNCTION) "phase-functions.h.glsl"

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
    CONST float r=earthRadius+altitude;
    // Clamping only guards against rounding errors here, we don't try to handle here the case when the
    // endpoint of the view ray intentionally appears in outer space.
    CONST float altAtDist=clampAltitude(sqrt(sqr(dist)+sqr(r)+2*r*dist*cosViewZenithAngle)-earthRadius);
    CONST float cosSunZenithAngleAtDist=clampCosine((r*cosSunZenithAngle+dist*dotViewSun)/(earthRadius+altAtDist));

    CONST vec4 xmittance=transmittance(cosViewZenithAngle, altitude, dist, viewRayIntersectsGround)
                                                    *
                         transmittanceToAtmosphereBorder(cosSunZenithAngleAtDist, altAtDist)
                                                    *
                                    sunVisibilityDueToMoon(scatterer,sunDir,moonPos)
                                                    *
                            // FIXME: this ignores orientation of the crescent of eclipsed Sun WRT horizon
                                    sunVisibility(cosSunZenithAngleAtDist, altAtDist);
#if ALL_SCATTERERS_AT_ONCE_WITH_PHASE_FUNCTION
    COMPUTE_TOTAL_SCATTERING_COEFFICIENT;
    return xmittance * totalScatteringCoefficient;
#else
    return xmittance * scattererDensity(altAtDist);
#endif
}

vec4 computeSingleScatteringEclipsed(const vec3 camera, const vec3 viewDir, const vec3 sunDir, const vec3 moonPos,
                                     const bool viewRayIntersectsGround)
{
    CONST float cosViewZenithAngle=cosZenithAngle(camera,viewDir);
    CONST float cosSunZenithAngle=cosZenithAngle(camera,sunDir);
    CONST float altitude=pointAltitude(camera);
    CONST float dotViewSun=dot(viewDir,sunDir);
    CONST float integrInterval=distanceToNearestAtmosphereBoundary(cosViewZenithAngle, altitude,
                                                                   viewRayIntersectsGround);

    // Using the midpoint rule for quadrature
    vec4 spectrum=vec4(0);
    CONST float dl=integrInterval/radialIntegrationPoints;
    for(int n=0; n<radialIntegrationPoints; ++n)
    {
        CONST float dist=(n+0.5)*dl;
        spectrum += computeSingleScatteringIntegrandEclipsed(cosSunZenithAngle, cosViewZenithAngle, dotViewSun,
                                                             altitude, dist, viewRayIntersectsGround,
                                                             camera+viewDir*dist, sunDir, moonPos);
    }

    spectrum *= dl*solarIrradianceAtTOA
#if ALL_SCATTERERS_AT_ONCE_WITH_PHASE_FUNCTION
                                // the multiplier is already included
#else
                                        * scatteringCrossSection()
#endif
        ;

    return spectrum;
}

vec4 computeSingleScatteringEclipsedSample(const int depthIndex, const vec3 camera, const vec3 viewDir,
                                           const vec3 sunDir, const vec3 moonPos, const bool viewRayIntersectsGround)
{
    CONST float cosViewZenithAngle=cosZenithAngle(camera,viewDir);
    CONST float cosSunZenithAngle=cosZenithAngle(camera,sunDir);
    CONST float altitude=pointAltitude(camera);
    CONST float dotViewSun=dot(viewDir,sunDir);
    CONST float integrInterval=distanceToNearestAtmosphereBoundary(cosViewZenithAngle, altitude,
                                                                   viewRayIntersectsGround);

    // Using the midpoint rule for quadrature
    vec4 spectrum=vec4(0);
    CONST float dl=integrInterval/radialIntegrationPoints;
    CONST float dist=(depthIndex+0.5)*dl;
    spectrum += computeSingleScatteringIntegrandEclipsed(cosSunZenithAngle, cosViewZenithAngle, dotViewSun,
                                                         altitude, dist, viewRayIntersectsGround,
                                                         camera+viewDir*dist, sunDir, moonPos);

    spectrum *= dl*solarIrradianceAtTOA
#if ALL_SCATTERERS_AT_ONCE_WITH_PHASE_FUNCTION
                                // the multiplier is already included
#else
                                        * scatteringCrossSection()
#endif
        ;

    return spectrum;
}
