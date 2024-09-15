#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "common-functions.h.glsl"
#include "texture-sampling-functions.h.glsl"
#include "texture-coordinates.h.glsl"

in vec3 position;
layout(location=0) out vec4 deltaIrradianceOutput;
layout(location=1) out vec4 irradianceOutput;

vec4 computeIndirectGroundIrradiance(const float cosSunZenithAngle, const float altitude, const int scatteringOrder)
{
    CONST float dSolidAngle = sphereIntegrationSolidAngleDifferential(angularIntegrationPoints);
    CONST vec3 sunDir = vec3(safeSqrt(1-sqr(cosSunZenithAngle)), 0, cosSunZenithAngle);
    vec4 radiance=vec4(0);
    // Our Fibonacci grid spiral goes from zenith to nadir monotonically. Halfway to the nadir it's (almost) on the horizon.
    // Beyond that it's under the horizon. We need only the upper part of the sphere, so we stop at k==N/2.
    for(int k=0; k<angularIntegrationPoints/2; ++k)
    {
        CONST vec3 incDir = sphereIntegrationSampleDir(k, angularIntegrationPoints);
        CONST float cosIncZenithAngle=incDir.z;

        CONST float lambertianFactor=cosIncZenithAngle;
        CONST float dotIncSun = dot(sunDir, incDir);
        radiance += dSolidAngle * lambertianFactor * scattering(cosSunZenithAngle, incDir.z, dotIncSun,
                                                                    altitude, false, scatteringOrder);
    }
    return radiance;
}

void main()
{
    CONST vec2 texCoord=0.5*position.xy+vec2(0.5);
    CONST IrradianceTexVars vars=irradianceTexCoordToTexVars(texCoord);
    CONST vec4 color=computeIndirectGroundIrradiance(vars.cosSunZenithAngle, vars.altitude, SCATTERING_ORDER);
    const vec4 AVOID_INFINITY = vec4(1e-37);
    // Delta gets the log value for subsequent sampling in next order scattering computation,
    // while the full texture will accumulate radiance, so it needs to be addable by blending,
    // which log scale doesn't allow for. This log conversion will be done on saving instead.
    deltaIrradianceOutput=log(color + AVOID_INFINITY);
    irradianceOutput=color;
}
