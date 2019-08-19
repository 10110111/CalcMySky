#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"

uniform vec2 transmittanceTextureSize;

float texCoordToUnitRange(const float texCoord, const float texSize)
{
    return (texSize*texCoord-0.5)/(texSize-1);
}

float unitRangeToTexCoord(const float u, const float texSize)
{
    return (0.5+(texSize-1)*u)/texSize;
}

// Alt: altitude, Mu: cos(viewZenithAngle)
vec2 transmittanceTexCoordToMuAlt(vec2 texCoord)
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
    const float mu = d==0 ? 1 : (2*r*dMin+sqr(dMin)-sqr(d))/(2*r*d);
    return vec2(mu,altitude);
}
