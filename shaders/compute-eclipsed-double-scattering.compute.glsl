#version 430

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

vec4 computeDoubleScatteringEclipsedDensitySample(const uint directionIndex, const uint depthIndex,
                                                  const vec3 cameraViewDir, const vec3 scatterer,
                                                  const vec3 sunDir, const vec3 moonPos)
{
    const vec3 zenith=vec3(0,0,1);
    const float altitude=pointAltitude(scatterer);
    // XXX: Might be a good idea to increase sampling density near horizon and decrease near zenith&nadir.
    // XXX: Also sampling should be more dense near the light source, since there often is a strong forward
    //       scattering peak like that of Mie phase functions.
    // TODO:At the very least, the phase functions should be lowpass-filtered to avoid aliasing, before
    //       sampling them here.

    // Instead of iterating over all directions, we compute only one sample, for only one direction, to
    // facilitate parallelization. The summation will be done after this parallel computation of the samples.

    const float dSolidAngle = sphereIntegrationSolidAngleDifferential(eclipseAngularIntegrationPoints);
    // Direction to the source of incident ray
    const vec3 incDir = sphereIntegrationSampleDir(int(directionIndex), eclipseAngularIntegrationPoints);

    // NOTE: we don't recalculate sunDir as we do in computeScatteringDensity(), because it would also require
    // at least recalculating the position of the Moon. Instead we take into account scatterer's position to
    // calculate zenith direction and the direction to the incident ray.
    const vec3 zenithAtScattererPos=normalize(scatterer-earthCenter);
    const float cosIncZenithAngle=dot(incDir, zenithAtScattererPos);
    const bool incRayIntersectsGround=rayIntersectsGround(cosIncZenithAngle, altitude);

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
        const vec3 pointOnGround = scatterer+incDir*distToGround;
        const vec4 groundIrradiance = calcEclipsedDirectGroundIrradiance(pointOnGround, sunDir, moonPos);
        // Radiation scattered by the ground
        const float groundBRDF = 1/PI; // Assuming Lambertian BRDF, which is constant
        // We sum this for each of depthIndex values, so need to scale down, since this should only
        // be added once. FIXME: it would be better to simply separate this computation to avoid so
        // many repeated computations of the same value.
        const float scale = 1./radialIntegrationPoints;
        incidentRadiance += transmittanceToGround*groundAlbedo*groundIrradiance*groundBRDF*scale;
    }
    // Radiation scattered by the atmosphere
    incidentRadiance+=computeSingleScatteringEclipsedSample(int(depthIndex),scatterer,incDir,sunDir,moonPos,incRayIntersectsGround);

    const float dotViewInc = dot(cameraViewDir, incDir);
    return dSolidAngle * incidentRadiance * totalScatteringCoefficient(altitude, dotViewInc);
}

layout(local_size_x = WORK_GROUP_SIZE_X, local_size_y = WORK_GROUP_SIZE_Y, local_size_z = WORK_GROUP_SIZE_Z) in;
layout(std430, binding = 0) buffer sumBuf { vec4 sums[]; };
const int sharedArraySize = WORK_GROUP_SIZE_X*WORK_GROUP_SIZE_Y*WORK_GROUP_SIZE_Z;
shared vec4 sharedSums[sharedArraySize];

uniform int shiftInBuffer;
uniform float cameraAltitude;
uniform vec3 cameraViewDir;
uniform float sunZenithAngle;
uniform vec3 moonPositionRelativeToSunAzimuth;

void synchronize()
{
    memoryBarrierShared();
    barrier();
}

void accumulateSum(const uint index, const int stride)
{
    synchronize();
    if(index % (stride*2) == 0 && index+stride < sharedArraySize)
        sharedSums[index] += sharedSums[index+stride];
}

void main()
{
    const uvec3 maxGlobalID = gl_WorkGroupSize * gl_NumWorkGroups;
    const uint flatIndex = gl_GlobalInvocationID.x + maxGlobalID.x*(gl_GlobalInvocationID.y + gl_GlobalInvocationID.z*maxGlobalID.y);

    uint partialIndex = flatIndex;
    const uint directionIndex = partialIndex % eclipseAngularIntegrationPoints;
    partialIndex /= eclipseAngularIntegrationPoints;

    const uint radialDistFromCamIndex = partialIndex % radialIntegrationPoints;
    partialIndex /= radialIntegrationPoints;

    const uint radialDistFromScattererIndex = partialIndex;

    const bool indicesOK = radialDistFromScattererIndex < radialIntegrationPoints;
    vec4 partialRadiance = vec4(0);
    if(indicesOK)
    {
        const vec3 sunDir=vec3(sin(sunZenithAngle), 0, cos(sunZenithAngle));
        const vec3 cameraPos=vec3(0,0,cameraAltitude);
        const bool viewRayIntersectsGround=rayIntersectsGround(cameraViewDir.z, cameraAltitude);

        const float radialIntegrInterval=distanceToNearestAtmosphereBoundary(cameraViewDir.z, cameraAltitude,
                                                                             viewRayIntersectsGround);

        // Using midpoint rule for quadrature
        const float dl=radialIntegrInterval/radialIntegrationPoints;
        const float dist=(float(radialDistFromCamIndex)+0.5)*dl;
        const vec4 scDensity=computeDoubleScatteringEclipsedDensitySample(directionIndex, radialDistFromScattererIndex,
                                                                          cameraViewDir, cameraPos+cameraViewDir*dist,
                                                                          sunDir, moonPositionRelativeToSunAzimuth);
        const vec4 xmittance=transmittance(cameraViewDir.z, cameraAltitude, dist, viewRayIntersectsGround);
        partialRadiance = scDensity*xmittance*dl;
    }
    const uint index = gl_LocalInvocationIndex;
    sharedSums[index] = partialRadiance;
    const uint wgTotalSize = gl_WorkGroupSize.x*gl_WorkGroupSize.y*gl_WorkGroupSize.z;
    const int maxLevel = int(ceil(log2(wgTotalSize)));
    for(int level = 0; level < maxLevel; ++level)
        accumulateSum(index, 1 << level);
    synchronize();
    if(index == 0)
    {
        const uint wgFlatIndex = gl_WorkGroupID.x + gl_NumWorkGroups.x*(gl_WorkGroupID.y + gl_WorkGroupID.z*gl_NumWorkGroups.y);
        sums[wgTotalSize*shiftInBuffer+wgFlatIndex] = sharedSums[index];
    }
}
