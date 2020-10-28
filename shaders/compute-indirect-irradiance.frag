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
    const float dSolidAngle = sphereIntegrationSolidAngleDifferential(angularIntegrationPoints);
    const vec3 sunDir = vec3(safeSqrt(1-sqr(cosSunZenithAngle)), 0, cosSunZenithAngle);
    vec4 radiance=vec4(0);
    // Our Fibonacci grid spiral goes from zenith to nadir monotonically. Halfway to the nadir it's (almost) on the horizon.
    // Beyond that it's under the horizon. We need only the upper part of the sphere, so we stop at k==N/2.
    for(int k=0; k<angularIntegrationPoints/2; ++k)
    {
        // NOTE: directionIndex must be a half-integer: the range is [0.5, angularIntegrationPoints-0.5]
        const vec3 incDir = sphereIntegrationSampleDir(k, angularIntegrationPoints);
        const float cosIncZenithAngle=incDir.z;

        const float lambertianFactor=cosIncZenithAngle;
        const float dotIncSun = dot(sunDir, incDir);
        radiance += dSolidAngle * lambertianFactor * scattering(cosSunZenithAngle, incDir.z, dotIncSun,
                                                                    altitude, false, scatteringOrder);
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
