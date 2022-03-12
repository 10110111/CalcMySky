#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "common-functions.h.glsl"
#include "texture-sampling-functions.h.glsl"
#include "total-scattering-coefficient.h.glsl"

uniform int lightPollutionRadialIntegrationPoints=50; // TODO: remove default value, set it from C++ code

// This function omits ground luminance: it is to be applied somewhere in the calling code.
vec4 computeSingleScatteringForLightPollutionIntegrand(const vec3 emitter, const vec3 scatterer, const vec3 viewDir,
                                                       const float cameraAltitude, const float cameraScattererDistance,
                                                       const float cosViewZenithAngle, const bool viewRayIntersectsGround)
{
    CONST float R = earthRadius;
    CONST float r = length(scatterer-earthCenter);
    CONST float scattererAltitude = r-R;
    CONST float horizonZenithAngle=acos(clampCosine(cosZenithAngleOfHorizon(scattererAltitude)));

    CONST vec3 incDir = normalize(emitter-scatterer);
    CONST vec3 zenithAtScatterer = normalize(scatterer-earthCenter);
    CONST float cosIncZenithAngle = dot(zenithAtScatterer, incDir);
    CONST float distToEmitter=length(emitter-scatterer);
    CONST float dotViewInc = dot(incDir, viewDir);

    CONST vec3 zenithAtEmitter = normalize(emitter-earthCenter);
    // Directivity pattern of the emitter is taken to be cosinusoidal. This will make a grid of such emitters behave like a lambertian source.
    // FIXME: I don't know how close to reality this is. But in any case it MUST NOT shine downwards! Otherwise we'll see it from under the horizon.
    CONST float directivityFactor = max(0., dot(-incDir, zenithAtEmitter));

    CONST vec4 radiance = transmittance(cosIncZenithAngle, scattererAltitude, distToEmitter, true)
                                                        *
                                 totalScatteringCoefficient(scattererAltitude, dotViewInc)
                                                        *
                                                 directivityFactor
                                                        /
                                                 sqr(distToEmitter)
                                                        ;
    CONST vec4 xmittanceFromCameraToScatterer=transmittance(cosViewZenithAngle, cameraAltitude, cameraScattererDistance,
                                                            viewRayIntersectsGround);
    return xmittanceFromCameraToScatterer*radiance;
}

// altitude, cosViewZenithAngle are here just to prevent recalculation of already known quantities. These parameters are not crucial to have.
vec4 computeSingleScatteringForLightPollution(const vec3 cameraPos, const vec3 viewDir, const vec3 emitterPos,
                                              const float altitude, const float cosViewZenithAngle, const bool viewRayIntersectsGround)
{
    CONST float R=earthRadius;
    CONST float distIntegrInterval=distanceToNearestAtmosphereBoundary(cosViewZenithAngle, altitude, viewRayIntersectsGround);

    // Using the midpoint rule for quadrature over distance, splitting the interval into two regions
    // to sample near the source more densely. Dense sampling is done by a coordinate transformation:
    // $\int f(r)dr = \int f(\exp(t))\exp(t)dt$, where r is the distance from the source.

    CONST float distToClosestPointToSource = dot(emitterPos-cameraPos, viewDir);
    CONST float squaredSmallestDistToSource = dot(emitterPos-cameraPos,emitterPos-cameraPos) - sqr(distToClosestPointToSource);

    vec4 radiance=vec4(0);
    // Less than 1 cm is negligible, while it might lead to NaNs due to underflows in the callees below
    CONST float NEGLIGIBLE_DISTANCE = 1e-5*km;
    if(distToClosestPointToSource > NEGLIGIBLE_DISTANCE)
    {
        // First region: from camera to the closest distance to the source
        CONST float maxDist = min(distIntegrInterval, distToClosestPointToSource); // don't drill through the ground
        CONST float dt=log(maxDist)/lightPollutionRadialIntegrationPoints;
        vec4 sum=vec4(0);
        for(int n=0; n<lightPollutionRadialIntegrationPoints; ++n)
        {
            CONST float t=(n+0.5)*dt;
            CONST float dist = maxDist-exp(t);
            CONST vec3 scatterer = cameraPos+viewDir*dist;
            sum += exp(t)*computeSingleScatteringForLightPollutionIntegrand(emitterPos, scatterer, viewDir, altitude,
                                                                            dist, cosViewZenithAngle, viewRayIntersectsGround);
        }
        radiance += sum*dt;
    }
    CONST float secondRegionStartDist = distToClosestPointToSource>0 ? distToClosestPointToSource : 0;
    CONST float secondRegionLength = distIntegrInterval-secondRegionStartDist;
    if(secondRegionLength > NEGLIGIBLE_DISTANCE)
    {
        // Second region, from closest distance to the source to the atmosphere boundary far away
        CONST float dt=log(secondRegionLength)/lightPollutionRadialIntegrationPoints;
        vec4 sum=vec4(0);
        for(int n=0; n<lightPollutionRadialIntegrationPoints; ++n)
        {
            CONST float t = (n+0.5)*dt;
            CONST float dist = secondRegionStartDist + exp(t);
            CONST vec3 scatterer = cameraPos+viewDir*dist;
            sum += exp(t)*computeSingleScatteringForLightPollutionIntegrand(emitterPos, scatterer, viewDir, altitude,
                                                                            dist, cosViewZenithAngle, viewRayIntersectsGround);
        }
        radiance += sum*dt;
    }
    return radiance*lightPollutionRelativeRadiance;
}
