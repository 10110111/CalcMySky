#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "common-functions.h.glsl"
#include "texture-coordinates.h.glsl"
#include "texture-sampling-functions.h.glsl"

uniform sampler3D eclipseMultipleScatteringMap;
uniform int cubeSideLength; // NOTE: must be even!
uniform int eclipsedAtmoMapAltitudeLayerCount;
uniform mat3 worldToMap;

vec4 sampleEclipseMultipleScatteringMap(const vec3 sunDir, const vec3 viewDir,
                                        const vec3 moonPos, const vec3 pointAtDist)
{
    // viewDir is not used here because our map currently only contains order-0 spherical harmonic of radiance

    CONST float altitudeAtDist = pointAltitude(pointAtDist);
    CONST vec3 zenithAtDist = worldToMap * normalize(pointAtDist - earthCenter);
    CONST vec3 tc = computeEclipsedMultipleScatteringMapTexCoords(cubeSideLength, eclipsedAtmoMapAltitudeLayerCount,
                                                                  zenithAtDist, altitudeAtDist);

    // TODO: implement higher-order spherical harmonics for better results
    CONST float sphericalHarmonicY = 1 / (2 * sqrt(PI));

    return sphericalHarmonicY * texture(eclipseMultipleScatteringMap, tc);
}

vec4 integrateEclipsedMultipleScatteringMap(const vec3 camera, const vec3 sunDir, const vec3 viewDir,
                                            const vec3 moonPos, const float cosViewZenithAngle,
                                            const float cameraAltitude, const bool viewRayIntersectsGround)
{
    CONST float integrInterval=distanceToNearestAtmosphereBoundary(cosViewZenithAngle, cameraAltitude,
                                                                   viewRayIntersectsGround);
    // Using the midpoint rule for quadrature
    vec4 spectrum=vec4(0);
    CONST float dl=integrInterval/radialIntegrationPoints;
    for(int n=0; n<radialIntegrationPoints; ++n)
    {
        CONST float dist=(n+0.5)*dl;
        spectrum += sampleEclipseMultipleScatteringMap(sunDir, viewDir, moonPos, camera+viewDir*dist)
                                                    *
                    transmittance(cosViewZenithAngle, cameraAltitude, dist, viewRayIntersectsGround);
    }
    spectrum *= dl;
    return spectrum;
}
