#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "phase-functions.h.glsl"
#include "common-functions.h.glsl"
#include "texture-coordinates.h.glsl"
#include "eclipsed-direct-irradiance.h.glsl"
#include "single-scattering-eclipsed.h.glsl"
#include "texture-sampling-functions.h.glsl"
#include "total-scattering-coefficient.h.glsl"
#include "integrate-eclipsed-multiple-scattering-map.h.glsl"

uniform int cubeSideLength; // NOTE: must be even!
uniform int eclipsedAtmoMapAltitudeLayerCount;
uniform vec3 moonPos; // should be in the XZ plane; origin is at the subsolar point on the ground
uniform float lunarShadowAngleFromSubsolarPoint;
uniform vec3 incidenceDir;
out vec4 mapOutput;

void main()
{
    vec3 zenith;
    float altitude;
    CONST vec3 pointInMap = computeEclipsedMultipleScatteringMapPoint(cubeSideLength,
                                                                      eclipsedAtmoMapAltitudeLayerCount,
                                                                      ivec2(gl_FragCoord.xy),
                                                                      lunarShadowAngleFromSubsolarPoint,
                                                                      zenith, altitude);
    const vec3 sunDir = vec3(0,0,1);
    CONST float cosIncZenithAngle = clampCosine(dot(zenith, incidenceDir));
    CONST bool incRayIntersectsGround=rayIntersectsGround(cosIncZenithAngle, altitude);
    CONST vec4 incRadianceFromAtmo = integrateEclipsedMultipleScatteringMap(pointInMap, incidenceDir,
                                                                            cosIncZenithAngle, altitude, mat3(1),
                                                                            incRayIntersectsGround);

    float distToGround=0;
    vec4 transmittanceToGround=vec4(0);
    if(incRayIntersectsGround)
    {
        distToGround = distanceToGround(cosIncZenithAngle, altitude);
        transmittanceToGround = transmittance(cosIncZenithAngle, altitude, distToGround, incRayIntersectsGround);
    }

    // TODO: add radiance from ground
    vec4 incRadianceFromGround = vec4(0);

    // The following logic is the same as in single scattering computation

    // TODO: implement higher-order spherical harmonics for better results
    CONST float sphericalHarmonicY0 = 1 / (2 * sqrt(PI));
    CONST vec4 scatCoefSpherHarOrder0 = sphericalHarmonicY0 * totalScatteringCoefficientIsotropic(altitude);

    CONST float dSolidAngleOfIncDirs = sphereIntegrationSolidAngleDifferential(eclipseAngularIntegrationPoints);
    mapOutput = (incRadianceFromAtmo + incRadianceFromGround) * scatCoefSpherHarOrder0 * dSolidAngleOfIncDirs;
}
