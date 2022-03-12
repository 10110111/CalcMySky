#version 330
#include "version.h.glsl"
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
    CONST vec2 texCoords=irradianceTexVarsToTexCoord(cosSunZenithAngle, altitude);
    return texture(irradianceTexture, texCoords);
}

vec4 opticalDepthToAtmosphereBorder(const float cosViewZenithAngle, const float altitude)
{
    CONST vec2 texCoords=transmittanceTexVarsToTexCoord(cosViewZenithAngle, altitude);
    // We don't use mip mapping here, but for some reason, on my NVidia GTX 750 Ti with Linux-x86 driver 390.116 I get
    // an artifact when looking into nadir from TOA at some values of texture sizes (in particular, size of
    // transmittance texture for altitude being 4096). This happens when I simply call texture(eclipsedScatteringTexture,
    // texCoords) without specifying LOD.
    // Apparently, the driver uses the derivative for some reason, even though it shouldn't.
    return textureLod(transmittanceTexture, texCoords, 0);
}

vec4 transmittanceToAtmosphereBorder(const float cosViewZenithAngle, const float altitude)
{
    return exp(-opticalDepthToAtmosphereBorder(cosViewZenithAngle,altitude));
}

// Assumes that the endpoint of view ray doesn't intentionally exit atmosphere.
vec4 transmittance(const float cosViewZenithAngle, const float altitude, const float dist,
                   const bool viewRayIntersectsGround)
{
    CONST float r=earthRadius+altitude;
    // Clamping only guards against rounding errors here, we don't try to handle view ray endpoint
    // in space here.
    CONST float altAtDist=clampAltitude(sqrt(sqr(dist)+sqr(r)+2*r*dist*cosViewZenithAngle)-earthRadius);
    CONST float cosViewZenithAngleAtDist=clampCosine((r*cosViewZenithAngle+dist)/(earthRadius+altAtDist));

    vec4 depth;
    if(viewRayIntersectsGround)
    {
        depth=opticalDepthToAtmosphereBorder(-cosViewZenithAngleAtDist, altAtDist) -
              opticalDepthToAtmosphereBorder(-cosViewZenithAngle, altitude);
    }
    else
    {
        depth=opticalDepthToAtmosphereBorder(cosViewZenithAngle, altitude) -
              opticalDepthToAtmosphereBorder(cosViewZenithAngleAtDist, altAtDist);
    }
    return exp(-depth);
}

vec4 calcFirstScattering(const float cosSunZenithAngle, const float cosViewZenithAngle,
                         const float dotViewSun, const float altitude, const bool viewRayIntersectsGround)
{
    CONST vec4 scattering = sample4DTexture(firstScatteringTexture, cosSunZenithAngle, cosViewZenithAngle,
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
    CONST vec4 coords = lightPollutionTexVarsToTexCoords(altitude, cosViewZenithAngle, viewRayIntersectsGround);
    return texture(lightPollutionScatteringTexture, coords);
}
