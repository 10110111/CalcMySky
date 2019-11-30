#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "texture-coordinates.h.glsl"
#include "common-functions.h.glsl"

uniform vec2 transmittanceTextureSize;
uniform sampler2D transmittanceTexture;

uniform vec2 irradianceTextureSize;

float texCoordToUnitRange(const float texCoord, const float texSize)
{
    return (texSize*texCoord-0.5)/(texSize-1);
}

float unitRangeToTexCoord(const float u, const float texSize)
{
    return (0.5+(texSize-1)*u)/texSize;
}

TransmittanceTexVars transmittanceTexCoordToTexVars(const vec2 texCoord)
{
    const float R=earthRadius;

    const float lengthOfHorizRayFromGroundToBorderOfAtmo=sqrt(atmosphereHeight*(atmosphereHeight+2*earthRadius));
    const float distToHorizon=lengthOfHorizRayFromGroundToBorderOfAtmo *
                                texCoordToUnitRange(texCoord.t,transmittanceTextureSize.t);
    // Distance from Earth center to camera
    const float r=sqrt(sqr(distToHorizon)+sqr(R));
    const float altitude=r-R;

    const float dMin=atmosphereHeight-altitude; // distance to zenith
    const float dMax=lengthOfHorizRayFromGroundToBorderOfAtmo+distToHorizon;
    // distance to border of visible atmosphere from the view point
    const float d=dMin+(dMax-dMin)*texCoordToUnitRange(texCoord.s,transmittanceTextureSize.s);
    // d==0 can happen when altitude==atmosphereHeight
    const float cosVZA = d==0 ? 1 : (2*r*dMin+sqr(dMin)-sqr(d))/(2*r*d);
    return TransmittanceTexVars(cosVZA,altitude);
}

// cosVZA: cos(viewZenithAngle)
//  Instead of cosVZA itself, distance to the atmosphere border along the view ray is
// used as the texture parameter. This lets us make sure the function is sampled
// with decent resolution near true horizon and avoids useless oversampling near
// zenith.
//  Instead of altitude itself, ratio of distance-to-horizon to
// length-of-horizontal-ray-from-ground-to-atmosphere-border is used to improve
// resolution at low altitudes, where transmittance has noticeable but very thin
// dip near horizon.
//  NOTE: this function relies on transmittanceTexture sampler being defined
vec2 transmittanceTexVarsToTexCoord(const float cosVZA, float altitude)
{
    if(altitude<0)
        altitude=0;

    const float altMax=atmosphereHeight;
    const float R=earthRadius;

    const float lengthOfHorizRayFromGroundToBorderOfAtmo=sqrt(sqr(R+altMax)-sqr(R));
    const float distToHorizon=sqrt(sqr(altitude)+2*altitude*R);
    const float t=unitRangeToTexCoord(distToHorizon / lengthOfHorizRayFromGroundToBorderOfAtmo,
                                      textureSize(transmittanceTexture,0).t);
    const float dMin=altMax-altitude; // distance to zenith
    const float dMax=lengthOfHorizRayFromGroundToBorderOfAtmo+distToHorizon;
    const float d=distanceToAtmosphereBorder(cosVZA,altitude);
    const float s=unitRangeToTexCoord((d-dMin)/(dMax-dMin), textureSize(transmittanceTexture,0).s);
    return vec2(s,t);
}

// Output: vec2(cos(sunZenithAngle), altitude)
IrradianceTexVars irradianceTexCoordToTexVars(const vec2 texCoord)
{
    const float cosSZA=2*texCoordToUnitRange(texCoord.s, irradianceTextureSize.s)-1;
    const float alt=atmosphereHeight*texCoordToUnitRange(texCoord.t, irradianceTextureSize.t);
    return IrradianceTexVars(cosSZA,alt);
}
