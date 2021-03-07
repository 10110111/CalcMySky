#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "phase-functions.h.glsl"
#include "common-functions.h.glsl"
#include "texture-coordinates.h.glsl"

uniform sampler2D transmittanceTexture;
uniform sampler2D irradianceTexture;

uniform sampler3D firstScatteringTexture;
uniform sampler3D multipleScatteringTexture;

uniform sampler2D lightPollutionScatteringTexture;

vec4 irradiance(const float cosSunZenithAngle, const float altitude)
{
    const vec2 texCoords=irradianceTexVarsToTexCoord(cosSunZenithAngle, altitude);
    return texture(irradianceTexture, texCoords);
}

vec4 transmittanceToAtmosphereBorder(const float cosViewZenithAngle, const float altitude)
{
    const vec2 texCoords=transmittanceTexVarsToTexCoord(cosViewZenithAngle, altitude);
    return texture(transmittanceTexture, texCoords);
}

// Assumes that the endpoint of view ray doesn't intentionally exit atmosphere.
vec4 transmittance(const float cosViewZenithAngle, const float altitude, const float dist,
                   const bool viewRayIntersectsGround)
{
    const float r=earthRadius+altitude;
    // Clamping only guards against rounding errors here, we don't try to handle view ray endpoint
    // in space here.
    const float altAtDist=clampAltitude(sqrt(sqr(dist)+sqr(r)+2*r*dist*cosViewZenithAngle)-earthRadius);
    const float cosViewZenithAngleAtDist=clampCosine((r*cosViewZenithAngle+dist)/(earthRadius+altAtDist));

    // min() clamps transmittance to <=1, which could otherwise happen to be >1 due to rounding errors in coordinates.
    if(viewRayIntersectsGround)
    {
        return min(transmittanceToAtmosphereBorder(-cosViewZenithAngleAtDist, altAtDist)
                                                /
                   transmittanceToAtmosphereBorder(-cosViewZenithAngle, altitude)
                   ,
                   1.);
    }
    else
    {
        return min(transmittanceToAtmosphereBorder(cosViewZenithAngle, altitude)
                                                /
                   transmittanceToAtmosphereBorder(cosViewZenithAngleAtDist, altAtDist)
                   ,
                   1.);
    }
}

vec4 calcFirstScattering(const float cosSunZenithAngle, const float cosViewZenithAngle,
                         const float dotViewSun, const float altitude, const bool viewRayIntersectsGround)
{
    const vec4 scattering = sample4DTexture(firstScatteringTexture, cosSunZenithAngle, cosViewZenithAngle,
                                          dotViewSun, altitude, viewRayIntersectsGround);
    return scattering*currentPhaseFunction(dotViewSun);
}

vec4 scattering(const float cosSunZenithAngle, const float cosViewZenithAngle,
                const float dotViewSun, const float altitude, const bool viewRayIntersectsGround,
                const int scatteringOrder)
{
    if(scatteringOrder==1)
        return calcFirstScattering(cosSunZenithAngle, cosViewZenithAngle,
                                   dotViewSun, altitude, viewRayIntersectsGround);
    else
        return sample4DTexture(multipleScatteringTexture, cosSunZenithAngle, cosViewZenithAngle,
                               dotViewSun, altitude, viewRayIntersectsGround);
}

vec4 lightPollutionScattering(const float altitude, const float cosViewZenithAngle, const bool viewRayIntersectsGround)
{
    const vec2 coords = lightPollutionTexVarsToTexCoords(altitude, cosViewZenithAngle, viewRayIntersectsGround);
    return texture(lightPollutionScatteringTexture, coords);
}
