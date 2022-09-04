#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "common-functions.h.glsl"
#include "texture-sampling-functions.h.glsl"
#include "total-scattering-coefficient.h.glsl"

vec4 computeMultipleScatteringForLightPollutionIntegrand(const float cosViewZenithAngle, const float altitude,
                                                       const float dist, const bool viewRayIntersectsGround)
{
    CONST float r=earthRadius+altitude;
    // Clamping only guards against rounding errors here, we don't try to handle here the case when the
    // endpoint of the view ray intentionally appears in outer space.
    CONST float altAtDist=clampAltitude(sqrt(sqr(dist)+sqr(r)+2*r*dist*cosViewZenithAngle)-earthRadius);

    vec4 weightedIncomingRadiance=vec4(0);
    // We want to integrate over all the 4PI solid angle. But we know a priori that incoming radiance doesn't depend on azimuth.
    // Thus we've integrated out the azimuth, which gives us the 2PI multiplier before the integral.
    // We are using midpoint rule for quadrature.
    CONST float kMax=lightPollutionAngularIntegrationPoints;
    CONST float incZenithAngleStep=PI/kMax;
    CONST float sinViewZenithAngle=safeSqrt(1-sqr(cosViewZenithAngle));
    for(float k=0; k<kMax; ++k)
    {
        CONST float incZenithAngle=(k+0.5)*incZenithAngleStep;
        CONST float dotViewInc = sin(incZenithAngle)*sinViewZenithAngle+cos(incZenithAngle)*cosViewZenithAngle;
        CONST bool incRayIntersectsGround=rayIntersectsGround(cos(incZenithAngle), altAtDist);
        weightedIncomingRadiance += totalScatteringCoefficient(altAtDist, dotViewInc)
                                                          *
                                                 sin(incZenithAngle)
                                                          *
                      lightPollutionScattering(altAtDist, cos(incZenithAngle), incRayIntersectsGround) // sample previous scattering order
                                                          ;
    }

    CONST vec4 xmittanceToScatterer=transmittance(cosViewZenithAngle, altitude, dist, viewRayIntersectsGround);
    return 2*PI*xmittanceToScatterer*weightedIncomingRadiance*incZenithAngleStep;
}

vec4 computeMultipleScatteringForLightPollution(const float cosViewZenithAngle, const float altitude, const bool viewRayIntersectsGround)
{
    CONST float integrInterval=distanceToNearestAtmosphereBoundary(cosViewZenithAngle, altitude, viewRayIntersectsGround);

    // Using the midpoint rule for quadrature
    vec4 spectrum=vec4(0);
    CONST float dl=integrInterval/radialIntegrationPoints;
    for(int n=0; n<radialIntegrationPoints; ++n)
    {
        CONST float dist=(n+0.5)*dl;
        spectrum += computeMultipleScatteringForLightPollutionIntegrand(cosViewZenithAngle, altitude, dist, viewRayIntersectsGround);
    }
    return spectrum*dl*lightPollutionRelativeRadiance;
}
