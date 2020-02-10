#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "common-functions.h.glsl"
#include "texture-sampling-functions.h.glsl"
#include "texture-coordinates.h.glsl"

in vec3 position;
layout(location=0) out vec4 deltaIrradianceOutput;
layout(location=1) out vec4 irradianceOutput;

vec4 computeIndirectGroundIrradiance(const float cosSunZenithAngle, const float altitude, const int scatteringOrder)
{
    const float dAzimuth = PI/angularIntegrationPointsPerHalfRevolution;
    const float dZenAng  = PI/angularIntegrationPointsPerHalfRevolution;
    const vec3 sunDir = vec3(safeSqrt(1-sqr(cosSunZenithAngle)), 0, cosSunZenithAngle);
    vec4 radiance=vec4(0);
    for(int z=0; z<angularIntegrationPointsPerHalfRevolution/2; ++z)
    {
        const float zenithAngle=(z+0.5)*dZenAng;
        const float cosIncZenithAngle=cos(zenithAngle);
        const float sinIncZenithAngle=sin(zenithAngle);
        const float lambertianFactor=cosIncZenithAngle;
        for(int a=0; a < 2*angularIntegrationPointsPerHalfRevolution; ++a)
        {
            const float azimuth = (a+0.5)*dAzimuth;
            const vec3 incDir = vec3(cos(azimuth)*sinIncZenithAngle,
                                     sin(azimuth)*sinIncZenithAngle,
                                     cosIncZenithAngle);
            const float dSolidAngle = dAzimuth*dZenAng*sinIncZenithAngle;
            const float dotIncSun = dot(sunDir, incDir);
            radiance += dSolidAngle * lambertianFactor * scattering(cosSunZenithAngle, incDir.z, dotIncSun,
                                                                    altitude, false, scatteringOrder);
        }
    }
    return radiance;
}

void main()
{
    const vec2 texCoord=0.5*position.xy+vec2(0.5);
    const IrradianceTexVars vars=irradianceTexCoordToTexVars(texCoord);
    const vec4 color=computeIndirectGroundIrradiance(vars.cosSunZenithAngle, vars.altitude, SCATTERING_ORDER);
    deltaIrradianceOutput=color;
    irradianceOutput=color;
}
