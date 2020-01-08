#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "common-functions.h.glsl"
#include "texture-coordinates.h.glsl"
#include "texture-sampling-functions.h.glsl"
#include "total-scattering-coefficient.h.glsl"

uniform sampler3D scatteringDensityTexture;

bool dbgDataPresent=false;
bool debugDataPresent()
{
    return dbgDataPresent;
}
vec3 dbgData;
vec3 debugData()
{
    return dbgData;
}
void setDebugData(float a,float b, float c)
{
    dbgDataPresent=true;
    dbgData=vec3(a,b,c);
}

vec4 computeScatteringDensity(const float cosSunZenithAngle, const float cosViewZenithAngle, const float dotViewSun,
                              const float altitude, const int scatteringOrder, const bool radiationIsFromGroundOnly)
{
    const vec3 zenith=vec3(0,0,1);
    const vec3 viewDir=vec3(sqrt(1-sqr(cosViewZenithAngle)), 0, cosViewZenithAngle);
    const float sunDirZ = cosSunZenithAngle;
    const float sunDirX = viewDir.x==0 ? 0 : (dotViewSun - cosViewZenithAngle*cosSunZenithAngle)/viewDir.x;
    const float sunDirY = sqrt(max(1-sqr(sunDirX)-sqr(cosSunZenithAngle), 0));
    const vec3 sunDir=vec3(sunDirX, sunDirY, sunDirZ);

    // XXX: Might be a good idea to increase sampling density near horizon and decrease near zenith&nadir.
    // XXX: Also sampling should be more dense near the light source, since there often is a strong forward
    //       scattering peak like that of Mie phase functions.
    // TODO:At the very least, the phase functions should be lowpass-filtered to avoid aliasing, before
    //       sampling them here.
    const float dAzimuth = PI/angularIntegrationPointsPerHalfRevolution;
    const float dZenAng  = PI/angularIntegrationPointsPerHalfRevolution;
    vec4 scatteringDensity = vec4(0);
    // Iterate over all incident directions
    for(int z=0; z<angularIntegrationPointsPerHalfRevolution; ++z)
    {
        const float zenithAngle=(z+0.5)*dZenAng;
        const float cosIncZenithAngle=cos(zenithAngle);
        const float sinIncZenithAngle=sin(zenithAngle);
        const bool incRayIntersectsGround=rayIntersectsGround(cosIncZenithAngle, altitude);

        float distToGround=0;
        vec4 transmittanceToGround=vec4(0);
        if(incRayIntersectsGround)
        {
            distToGround = distanceToGround(cosIncZenithAngle, altitude);
            transmittanceToGround = transmittance(cosIncZenithAngle, altitude, distToGround,
                                                  incRayIntersectsGround);
        }

        for(int a=0; a < 2*angularIntegrationPointsPerHalfRevolution; ++a)
        {
            const float azimuth = (a+0.5)*dAzimuth;
            // Direction to the source of incident ray
            const vec3 incDir = vec3(cos(azimuth)*sinIncZenithAngle,
                                     sin(azimuth)*sinIncZenithAngle,
                                     cosIncZenithAngle);
            const float dSolidAngle = dAzimuth*dZenAng*sinIncZenithAngle;

            vec4 incidentRadiance = vec4(0);
            // Only for scatteringOrder==2 we consider radiation from ground in a separate run
            if(radiationIsFromGroundOnly || scatteringOrder>2)
            {
                // Normal to ground at the point where incident light originates on the ground, with current incDir
                const vec3 groundNormal = normalize(zenith*(earthRadius+altitude)+incDir*distToGround);
                const vec4 groundIrradiance = irradiance(dot(groundNormal, sunDir), 0);
                // Radiation scattered by the ground
                const float groundBRDF = 1/PI; // Assuming Lambertian BRDF, which is constant
                incidentRadiance += transmittanceToGround*groundAlbedo*groundIrradiance*groundBRDF;
            }
            if(!radiationIsFromGroundOnly)
            {
                const float dotIncSun = dot(incDir,sunDir);
                // Radiation scattered by the atmosphere
                incidentRadiance += scattering(cosSunZenithAngle, incDir.z, dotIncSun, altitude,
                                              incRayIntersectsGround, scatteringOrder-1);
            }

            const float dotViewInc = dot(viewDir, incDir);
            scatteringDensity += dSolidAngle * incidentRadiance * totalScatteringCoefficient(altitude, dotViewInc);
        }
    }
    return scatteringDensity;
}

vec4 computeMultipleScattering(const float cosSunZenithAngle, const float cosViewZenithAngle, const float dotViewSun,
                               const float altitude, const bool viewRayIntersectsGround)
{
    const float r=earthRadius+altitude;
    const float dl = distanceToNearestAtmosphereBoundary(cosViewZenithAngle, altitude, viewRayIntersectsGround) /
                                                  radialIntegrationPoints;
    vec4 radiance=vec4(0);
    for(int n=0; n <= radialIntegrationPoints; ++n)
    {
        const float dist=n*dl;
        // Clamping only guards against rounding errors here, we don't try to handle here the case when the
        // endpoint of the view ray intentionally appears in outer space.
        const float altAtDist=clampAltitude(sqrt(sqr(dist)+sqr(r)+2*r*dist*cosViewZenithAngle)-earthRadius);
        const float cosVZAatDist=clampCosine((r*cosViewZenithAngle+dist)/(earthRadius+altAtDist));
        const float cosSZAatDist=clampCosine((r*cosSunZenithAngle+dist*dotViewSun)/(earthRadius+altAtDist));

        const vec4 scDensity=sample4DTexture(scatteringDensityTexture, cosSZAatDist, cosVZAatDist, altAtDist,
                                             dotViewSun, viewRayIntersectsGround);
        const vec4 xmittance=transmittance(cosViewZenithAngle, altitude, dist, viewRayIntersectsGround);
        const float weight = n==0||n==radialIntegrationPoints ? 0.5 : 1; // weight by trapezoidal rule
        radiance += scDensity*xmittance*weight*dl;
    }
    return radiance;
}
