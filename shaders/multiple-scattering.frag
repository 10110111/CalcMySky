#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "common-functions.h.glsl"
#include "texture-coordinates.h.glsl"
#include "texture-sampling-functions.h.glsl"
#include "total-scattering-coefficient.h.glsl"

uniform sampler3D scatteringDensityTexture;

vec4 computeScatteringDensity(const float cosSunZenithAngle, const float cosViewZenithAngle, const float dotViewSun,
                              const float altitude, const int scatteringOrder, const bool radiationIsFromGroundOnly)
{
    CONST vec3 zenith=vec3(0,0,1);
    CONST vec3 viewDir=vec3(sqrt(1-sqr(cosViewZenithAngle)), 0, cosViewZenithAngle);
    CONST float sunDirZ = cosSunZenithAngle;
    CONST float sunDirX = viewDir.x==0 ? 0 : (dotViewSun - cosViewZenithAngle*cosSunZenithAngle)/viewDir.x;
    CONST float sunDirY = sqrt(max(1-sqr(sunDirX)-sqr(cosSunZenithAngle), 0));
    CONST vec3 sunDir=vec3(sunDirX, sunDirY, sunDirZ);

    // XXX: Might be a good idea to increase sampling density near horizon and decrease near zenith&nadir.
    // XXX: Also sampling should be more dense near the light source, since there often is a strong forward
    //       scattering peak like that of Mie phase functions.
    // TODO:At the very least, the phase functions should be lowpass-filtered to avoid aliasing, before
    //       sampling them here.

    CONST float dSolidAngle = sphereIntegrationSolidAngleDifferential(angularIntegrationPoints);

    vec4 scatteringDensity = vec4(0);
    // Iterate over all incident directions
    for(int k=0; k<angularIntegrationPoints; ++k)
    {
        // Direction to the source of incident ray
        CONST vec3 incDir = sphereIntegrationSampleDir(k, angularIntegrationPoints);
        CONST float cosIncZenithAngle=incDir.z;

        CONST bool incRayIntersectsGround=rayIntersectsGround(cosIncZenithAngle, altitude);

        float distToGround=0;
        vec4 transmittanceToGround=vec4(0);
        if(incRayIntersectsGround)
        {
            distToGround = distanceToGround(cosIncZenithAngle, altitude);
            transmittanceToGround = transmittance(cosIncZenithAngle, altitude, distToGround,
                                                  incRayIntersectsGround);
        }

        vec4 incidentRadiance = vec4(0);
        // Only for scatteringOrder==2 we consider radiation from ground in a separate run
        if(radiationIsFromGroundOnly || scatteringOrder>2)
        {
            // XXX: keep in sync with the same code in zeroth order rendering shader, but don't
            //      forget about the difference in the usage of viewDir vs incDir.

            // Normal to ground at the point where incident light originates on the ground, with current incDir
            CONST vec3 groundNormal = normalize(zenith*(earthRadius+altitude)+incDir*distToGround);
            CONST vec4 groundIrradiance = irradiance(dot(groundNormal, sunDir), 0);
            // Radiation scattered by the ground
            CONST float groundBRDF = 1/PI; // Assuming Lambertian BRDF, which is constant
            incidentRadiance += transmittanceToGround*groundAlbedo*groundIrradiance*groundBRDF;
        }
        if(!radiationIsFromGroundOnly)
        {
            CONST float dotIncSun = dot(incDir,sunDir);
            // Radiation scattered by the atmosphere
            incidentRadiance += scattering(cosSunZenithAngle, incDir.z, dotIncSun, altitude,
                                          incRayIntersectsGround, scatteringOrder-1);
        }

        CONST float dotViewInc = dot(viewDir, incDir);
        scatteringDensity += dSolidAngle * incidentRadiance * totalScatteringCoefficient(altitude, dotViewInc);
    }
    return scatteringDensity;
}

vec4 computeMultipleScattering(const float cosSunZenithAngle, const float cosViewZenithAngle, const float dotViewSun,
                               const float altitude, const bool viewRayIntersectsGround)
{
    CONST float r=earthRadius+altitude;
    // Using the midpoint rule for quadrature
    CONST float dl = distanceToNearestAtmosphereBoundary(cosViewZenithAngle, altitude, viewRayIntersectsGround) /
                                                  radialIntegrationPoints;
    vec4 radiance=vec4(0);
    for(int n=0; n<radialIntegrationPoints; ++n)
    {
        CONST float dist=(n+0.5)*dl;
        // Clamping only guards against rounding errors here, we don't try to handle here the case when the
        // endpoint of the view ray intentionally appears in outer space.
        CONST float altAtDist=clampAltitude(sqrt(sqr(dist)+sqr(r)+2*r*dist*cosViewZenithAngle)-earthRadius);
        CONST float cosVZAatDist=clampCosine((r*cosViewZenithAngle+dist)/(earthRadius+altAtDist));
        CONST float cosSZAatDist=clampCosine((r*cosSunZenithAngle+dist*dotViewSun)/(earthRadius+altAtDist));

        CONST vec4 scDensity=sample4DTexture(scatteringDensityTexture, cosSZAatDist, cosVZAatDist,
                                             dotViewSun, altAtDist, viewRayIntersectsGround);
        CONST vec4 xmittance=transmittance(cosViewZenithAngle, altitude, dist, viewRayIntersectsGround);
        radiance += scDensity*xmittance*dl;
    }
    return radiance;
}
