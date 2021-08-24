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

uniform sampler1D opticalHorizonsTexture;
uniform sampler2D refractionAnglesForwardTexture;
uniform sampler2D refractionAnglesBackwardTexture;

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
    CONST vec2 coords = lightPollutionTexVarsToTexCoords(altitude, cosViewZenithAngle, viewRayIntersectsGround);
    return texture(lightPollutionScatteringTexture, coords);
}

float opticalHorizonElevation(const float altitude)
{
	const float altURTexCoord  = safeSqrt(altitude/atmosphereHeight);
    const float altStepCount = textureSize(opticalHorizonsTexture,0);
    const float coord = unitRangeToTexCoord(altURTexCoord, altStepCount);
    const float delta = texture(opticalHorizonsTexture, coord).r;
    const float geometricHorizon = -acos(earthRadius/(earthRadius+altitude));
    return geometricHorizon + delta;
}

float refractionAngleApparentToGeometric(const float altitude, const float cosApparentZenithAngle)
{
	const float altURTexCoord = safeSqrt(altitude/atmosphereHeight);

	const float optHorizon = opticalHorizonElevation(altitude);
	const float viewElevation = asin(cosApparentZenithAngle);
    const float elevStepCount = textureSize(refractionAnglesForwardTexture,0).s;
	const float elevStep = safeSqrt((viewElevation - optHorizon)/(PI/2-optHorizon)) * elevStepCount;
    if(elevStep >= elevStepCount-1)
        return 0; // Don't bother interpolating refraction angle, even linearly: it's almost zero anyway.
    const float elevURTexCoord = elevStep / (elevStepCount-1);

    const float altStepCount = textureSize(refractionAnglesForwardTexture,0).t;
	const vec2 coords = vec2(unitRangeToTexCoord(elevURTexCoord, elevStepCount),
                             unitRangeToTexCoord(altURTexCoord, altStepCount));
    // We don't use mip mapping here, but for some reason, on my NVidia GTX 750 Ti with Linux-x86 driver 390.116 I get
    // an odd result where every other row samples the texture wrongly near altitude==120km, view direction = horizon.
    // This happens when I simply call texture(refractionAnglesForwardTexture, coords) without specifying LOD.
    // Apparently, the driver uses the derivative for some reason, even though it shouldn't.
	const float tex = textureLod(refractionAnglesForwardTexture, coords, 0).r;
	return -exp(tex);
}

vec3 apparentDirToGeometric(const vec3 cameraPosition, const vec3 viewDir)
{
    const vec3 zenith=normalize(cameraPosition-earthCenter);
    const float cosViewZenithAngle=dot(zenith,viewDir);
    const float sinViewZenithAngle=safeSqrt(1-sqr(cosViewZenithAngle));
	const float deltaElev = refractionAngleApparentToGeometric(pointAltitude(cameraPosition), cosViewZenithAngle);
	const float projToZenith = sin(deltaElev)/(sinViewZenithAngle*cos(deltaElev)-cosViewZenithAngle*sin(deltaElev));
	return normalize(viewDir + projToZenith*zenith);
}
