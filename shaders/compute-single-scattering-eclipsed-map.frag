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
    CONST vec4 incRadianceFromAtmo = computeSingleScatteringEclipsed(pointInMap, incidenceDir, sunDir, moonPos,
                                                                  incRayIntersectsGround);

    float distToGround=0;
    vec4 transmittanceToGround=vec4(0);
    if(incRayIntersectsGround)
    {
        distToGround = distanceToGround(cosIncZenithAngle, altitude);
        transmittanceToGround = transmittance(cosIncZenithAngle, altitude, distToGround, incRayIntersectsGround);
    }

    vec4 incRadianceFromGround = vec4(0);
    // XXX: keep this ground-scattered radiation logic in sync with that in computeScatteringDensity().
    {
        // The point where incident light originates on the ground, with current incidenceDir
        CONST vec3 pointOnGround = pointInMap+incidenceDir*distToGround;
        CONST vec4 groundIrradiance = calcEclipsedDirectGroundIrradiance(pointOnGround, sunDir, moonPos);
        // Radiation scattered by the ground
        CONST float groundBRDF = 1/PI; // Assuming Lambertian BRDF, which is constant
        incRadianceFromGround = transmittanceToGround*groundAlbedo*groundIrradiance*groundBRDF;
    }

    /* Generally, double scattering eclipsed value dse(viewDir) would be, in terms of
       single scattering eclipsed value sse(incDir),

       dse(viewDir) = \int d\Omega_{inc} sse(incDir) scatCoef(dot(viewDir, incDir)).

       But we are computing the coefficient of the 0th spherical harmonic of dse:

       dse0 = \int d\Omega_{view} dse(viewDir) Y_0^0(viewDir).

       After switching the order of integration this results in phase functions being
       integrated away to 1, leaving totalScatteringCoefficientIsotropic() from the
       totalScatteringCoefficient (scatCoef above). So the final expression will be

       dse0 = scatCoefIsotr \; Y_0^0 \int d\Omega_{inc} sse(incDir).
     */
    // TODO: implement higher-order spherical harmonics for better results
    CONST float sphericalHarmonicY0 = 1 / (2 * sqrt(PI));
    CONST vec4 scatCoefSpherHarOrder0 = sphericalHarmonicY0 * totalScatteringCoefficientIsotropic(altitude);

    CONST float dSolidAngleOfIncDirs = sphereIntegrationSolidAngleDifferential(eclipseAngularIntegrationPoints);
    mapOutput = (incRadianceFromAtmo + incRadianceFromGround) * scatCoefSpherHarOrder0 * dSolidAngleOfIncDirs;
}
