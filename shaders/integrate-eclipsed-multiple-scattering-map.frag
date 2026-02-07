#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "common-functions.h.glsl"
#include "texture-coordinates.h.glsl"
#include "texture-sampling-functions.h.glsl"

uniform int cubeSideLength; // NOTE: must be even!
uniform int eclipsedAtmoMapAltitudeLayerCount;
uniform float lunarShadowAngleFromSubsolarPoint;

// sunDir is not used because worldToMap transforms it to (0,0,1)
// moonPos is not used because we use uniforms describing shadow position WRT the subsolar point
vec4 integrateEclipsedMultipleScatteringMap(const vec3 camera, const vec3 viewDir, const float cosViewZenithAngle,
                                            const float cameraAltitude, const mat3 worldToMap,
                                            const bool viewRayIntersectsGround)
{
    CONST float integrInterval=distanceToNearestAtmosphereBoundary(cosViewZenithAngle, cameraAltitude,
                                                                   viewRayIntersectsGround);
    // Using the midpoint rule for quadrature
    vec4 spectrum=vec4(0);
    CONST float dl=integrInterval/radialIntegrationPoints;
    for(int n=0; n<radialIntegrationPoints; ++n)
    {
        CONST float dist=(n+0.5)*dl;
        spectrum += sampleEclipseMultipleScatteringMap(cubeSideLength, eclipsedAtmoMapAltitudeLayerCount,
                                                       lunarShadowAngleFromSubsolarPoint,
                                                       viewDir, camera+viewDir*dist, worldToMap, false)
                                                    *
                    transmittance(cosViewZenithAngle, cameraAltitude, dist, viewRayIntersectsGround);
    }
    spectrum *= dl;
    return spectrum;
}
