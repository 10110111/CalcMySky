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
    bool isGroundIrradianceLayer;
    CONST vec3 pointInMap = computeEclipsedMultipleScatteringMapPoint(cubeSideLength,
                                                                      eclipsedAtmoMapAltitudeLayerCount,
                                                                      ivec2(gl_FragCoord.xy),
                                                                      lunarShadowAngleFromSubsolarPoint,
                                                                      zenith, altitude, isGroundIrradianceLayer);

    const vec3 sunDir = vec3(0,0,1);
    CONST float cosIncZenithAngle = clampCosine(dot(zenith, incidenceDir));
    CONST bool incRayIntersectsGround=rayIntersectsGround(cosIncZenithAngle, altitude);
    CONST vec4 incRadianceFromAtmo = integrateEclipsedMultipleScatteringMap(pointInMap, incidenceDir,
                                                                            cosIncZenithAngle, altitude, mat3(1),
                                                                            incRayIntersectsGround);
    CONST float dSolidAngleOfIncDirs = sphereIntegrationSolidAngleDifferential(eclipseAngularIntegrationPoints);

    if(isGroundIrradianceLayer)
    {
        // Save ground irradiance
        CONST float lambertianFactor = max(0., cosIncZenithAngle);
        CONST vec4 irr = incRadianceFromAtmo * dSolidAngleOfIncDirs * lambertianFactor;
        // The 2sqrt(pi) factor compensates for the spherical harmonic that the value
        // will be multiplied by on sampling in sampleEclipseMultipleScatteringMap()
        mapOutput = irr * (2 * sqrt(PI));
    }
    else
    {

        vec4 incRadianceFromGround = vec4(0);
        if(incRayIntersectsGround)
        {
            CONST float distToGround = distanceToGround(cosIncZenithAngle, altitude);
            CONST vec4 transmittanceToGround = transmittance(cosIncZenithAngle, altitude, distToGround, incRayIntersectsGround);

            // The point where incident light originates on the ground, with current incidenceDir
            CONST vec3 pointOnGround = pointInMap+incidenceDir*distToGround;
            CONST vec4 groundIrradiance = sampleEclipseMultipleScatteringMap(cubeSideLength, eclipsedAtmoMapAltitudeLayerCount,
                                                                             lunarShadowAngleFromSubsolarPoint,
                                                                             incidenceDir, pointOnGround, mat3(1), true);
            // Radiation scattered by the ground
            CONST float groundBRDF = 1/PI; // Assuming Lambertian BRDF, which is constant
            incRadianceFromGround = transmittanceToGround*groundAlbedo*groundIrradiance*groundBRDF;
        }
        // The following logic is the same as in single scattering computation

        // TODO: implement higher-order spherical harmonics for better results
        CONST float sphericalHarmonicY0 = 1 / (2 * sqrt(PI));
        CONST vec4 scatCoefSpherHarOrder0 = sphericalHarmonicY0 * totalScatteringCoefficientIsotropic(altitude);

        mapOutput = (incRadianceFromAtmo + incRadianceFromGround) * scatCoefSpherHarOrder0 * dSolidAngleOfIncDirs;
    }
}
