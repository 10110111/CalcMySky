#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "phase-functions.h.glsl"
#include "common-functions.h.glsl"
#include "texture-coordinates.h.glsl"
#include "radiance-to-luminance.h.glsl"
#include "eclipsed-direct-irradiance.h.glsl"
#include "texture-sampling-functions.h.glsl"
#include "single-scattering-eclipsed.h.glsl"
#include "total-scattering-coefficient.h.glsl"

in vec3 position;
out vec4 partialRadiance;

vec4 computeDoubleScatteringEclipsedDensitySample(const int directionIndex, const vec3 cameraViewDir, const vec3 scatterer,
                                                  const vec3 sunDir, const vec3 moonPos)
{
    CONST vec3 zenith=vec3(0,0,1);
    CONST float altitude=pointAltitude(scatterer);
    // XXX: Might be a good idea to increase sampling density near horizon and decrease near zenith&nadir.
    // XXX: Also sampling should be more dense near the light source, since there often is a strong forward
    //       scattering peak like that of Mie phase functions.
    // TODO:At the very least, the phase functions should be lowpass-filtered to avoid aliasing, before
    //       sampling them here.

    // Instead of iterating over all directions, we compute only one sample, for only one direction, to
    // facilitate parallelization. The summation will be done after this parallel computation of the samples.

    CONST float dSolidAngle = sphereIntegrationSolidAngleDifferential(eclipseAngularIntegrationPoints);
    // Direction to the source of incident ray
    CONST vec3 incDir = sphereIntegrationSampleDir(directionIndex, eclipseAngularIntegrationPoints);

    // NOTE: we don't recalculate sunDir as we do in computeScatteringDensity(), because it would also require
    // at least recalculating the position of the Moon. Instead we take into account scatterer's position to
    // calculate zenith direction and the direction to the incident ray.
    CONST vec3 zenithAtScattererPos=normalize(scatterer-earthCenter);
    CONST float cosIncZenithAngle=dot(incDir, zenithAtScattererPos);
    CONST bool incRayIntersectsGround=rayIntersectsGround(cosIncZenithAngle, altitude);

    float distToGround=0;
    vec4 transmittanceToGround=vec4(0);
    if(incRayIntersectsGround)
    {
        distToGround = distanceToGround(cosIncZenithAngle, altitude);
        transmittanceToGround = transmittance(cosIncZenithAngle, altitude, distToGround, incRayIntersectsGround);
    }

    vec4 incidentRadiance = vec4(0);
    // XXX: keep this ground-scattered radiation logic in sync with that in computeScatteringDensity().
    {
        // The point where incident light originates on the ground, with current incDir
        CONST vec3 pointOnGround = scatterer+incDir*distToGround;
        CONST vec4 groundIrradiance = calcEclipsedDirectGroundIrradiance(pointOnGround, sunDir, moonPos);
        // Radiation scattered by the ground
        CONST float groundBRDF = 1/PI; // Assuming Lambertian BRDF, which is constant
        incidentRadiance += transmittanceToGround*groundAlbedo*groundIrradiance*groundBRDF;
    }
    // Radiation scattered by the atmosphere
    incidentRadiance+=computeSingleScatteringEclipsed(scatterer,incDir,sunDir,moonPos,incRayIntersectsGround);

    CONST float dotViewInc = dot(cameraViewDir, incDir);
    return dSolidAngle * incidentRadiance * totalScatteringCoefficient(altitude, dotViewInc);
}

uniform float cameraAltitude;
uniform vec3 cameraViewDir;
uniform float sunZenithAngle;
uniform vec3 moonPositionRelativeToSunAzimuth;

void main()
{
    CONST vec3 sunDir=vec3(sin(sunZenithAngle), 0, cos(sunZenithAngle));
    CONST vec3 cameraPos=vec3(0,0,cameraAltitude);
    CONST bool viewRayIntersectsGround=rayIntersectsGround(cameraViewDir.z, cameraAltitude);

    CONST float radialIntegrInterval=distanceToNearestAtmosphereBoundary(cameraViewDir.z, cameraAltitude,
                                                                         viewRayIntersectsGround);

    CONST int directionIndex=int(gl_FragCoord.x);
    CONST float radialDistIndex=gl_FragCoord.y;

    // Using midpoint rule for quadrature
    CONST float dl=radialIntegrInterval/radialIntegrationPoints;
    CONST float dist=(radialDistIndex+0.5)*dl;
    CONST vec4 scDensity=computeDoubleScatteringEclipsedDensitySample(directionIndex, cameraViewDir, cameraPos+cameraViewDir*dist,
                                                                      sunDir, moonPositionRelativeToSunAzimuth);
    CONST vec4 xmittance=transmittance(cameraViewDir.z, cameraAltitude, dist, viewRayIntersectsGround);
    partialRadiance = scDensity*xmittance*dl;

}
