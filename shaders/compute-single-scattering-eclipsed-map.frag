#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "phase-functions.h.glsl"
#include "common-functions.h.glsl"
#include "texture-coordinates.h.glsl"
#include "single-scattering-eclipsed.h.glsl"

uniform int cubeSideLength; // NOTE: must be even!
uniform int eclipsedAtmoMapAltitudeLayerCount;
uniform vec3 moonPos; // should be in the XZ plane; origin is at the subsolar point on the ground
uniform vec3 incidenceDir;
out vec4 mapOutput;

void main()
{
    vec3 zenith;
    float altitude;
    CONST vec3 pointInMap = computeEclipsedMultipleScatteringMapPoint(cubeSideLength,
                                                                      eclipsedAtmoMapAltitudeLayerCount,
                                                                      ivec2(gl_FragCoord.xy),
                                                                      zenith, altitude);
    const vec3 sunDir = vec3(0,0,1);
    CONST float cosIncZenithAngle = clampCosine(dot(zenith, incidenceDir));
    CONST bool incRayIntersectsGround=rayIntersectsGround(cosIncZenithAngle, altitude);
    CONST vec4 sample = computeSingleScatteringEclipsed(pointInMap, incidenceDir, sunDir, moonPos,
                                                        incRayIntersectsGround);

    // TODO: implement higher-order spherical harmonics for better results
    CONST float sphericalHarmonicY = 1 / (2 * sqrt(PI));

    CONST float dSolidAngle = sphereIntegrationSolidAngleDifferential(eclipseAngularIntegrationPoints);
    mapOutput = sphericalHarmonicY * sample * dSolidAngle;
}
