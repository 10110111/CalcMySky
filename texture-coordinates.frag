#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "texture-coordinates.h.glsl"
#include "common-functions.h.glsl"

uniform vec2 transmittanceTextureSize;
uniform sampler2D transmittanceTexture;

uniform vec2 irradianceTextureSize;
uniform vec4 scatteringTextureSize;

struct Scattering4DCoords
{
    float cosSunZenithAngle;
    float cosViewZenithAngle;
    float dotViewSun;
    float altInTexSlice;
    bool viewRayIntersectsGround;
};
struct TexCoordPair
{
    vec3 lower;
    float alphaLower;
    vec3 upper;
    float alphaUpper;
};

// These two uniforms select the range of altitudes represented in the current block of the full texture.
// When sampling the texture, we need to map altMin -> texCoord==0 and altMax -> texCoord==1.
uniform float altitudeMin, altitudeMax;

float texCoordToUnitRange(const float texCoord, const float texSize)
{
    return (texSize*texCoord-0.5)/(texSize-1);
}

float unitRangeToTexCoord(const float u, const float texSize)
{
    return (0.5+(texSize-1)*u)/texSize;
}

vec2 unitRangeToTexCoord(const vec2 u, const float texSize)
{
    return vec2(unitRangeToTexCoord(u.s,texSize),
                unitRangeToTexCoord(u.t,texSize));
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

// dotViewSun: dot(viewDir,sunDir)
Scattering4DCoords scatteringTexVarsTo4DCoords(const float cosSunZenithAngle, const float cosViewZenithAngle,
                                               const float dotViewSun, const float altitude,
                                               const bool viewRayIntersectsGround)
{
    const float altMax=atmosphereHeight;
    const float R=earthRadius;
    const float r=R+altitude;

    const float lengthOfHorizRayFromGroundToBorderOfAtmo=sqrt(sqr(R+altMax)-sqr(R));
    const float distToHorizonMin = sqrt(sqr(altitudeMin)+2*altitudeMin*R);
    const float distToHorizonMax = sqrt(sqr(altitudeMax)+2*altitudeMax*R);
    const float distToHorizon    = sqrt(sqr(altitude)+2*altitude*R);
    const float altMinCoordInFullTexture = distToHorizonMin / lengthOfHorizRayFromGroundToBorderOfAtmo;
    const float altMaxCoordInFullTexture = distToHorizonMax / lengthOfHorizRayFromGroundToBorderOfAtmo;
    const float altCoordInFullTexture    = distToHorizon    / lengthOfHorizRayFromGroundToBorderOfAtmo;
    const float altCoordInTexSlice = (altCoordInFullTexture-altMinCoordInFullTexture)
                                                              /
                                      (altMaxCoordInFullTexture-altMinCoordInFullTexture);

    // ------------------------------------
    float cosVZACoord; // Coordinate for cos(viewZenithAngle)
    const float rCvza=r*cosViewZenithAngle;
    // Discriminant of the quadratic equation for the intersections of the ray (altitiude, cosViewZenithAngle) with the ground.
    const float discriminant=sqr(rCvza)-sqr(r)+sqr(R);
    if(viewRayIntersectsGround)
    {
        // Distance from camera to the ground along the view ray (altitude, cosViewZenithAngle)
        const float distToGround = -rCvza-safeSqrt(discriminant);
        // Minimum possible value of distToGround
        const float distMin = altitude;
        // Maximum possible value of distToGround
        const float distMax = distToHorizon;
        cosVZACoord = distMax==distMin ? 0. : (distToGround-distMin)/(distMax-distMin);
    }
    else
    {
        // Distance from camera to the atmosphere border along the view ray (altitude, cosViewZenithAngle)
        // sqr(lengthOfHorizRayFromGroundToBorderOfAtmo) added to sqr(R) term in discriminant changes sqr(R) to sqr(R+altMax),
        // so that we target the top atmosphere boundary instead of bottom.
        const float distToTopAtmoBorder = -rCvza+safeSqrt(discriminant+sqr(lengthOfHorizRayFromGroundToBorderOfAtmo));
        const float distMin = atmosphereHeight-altitude;
        const float distMax = distToHorizon+lengthOfHorizRayFromGroundToBorderOfAtmo;
        cosVZACoord = distMax==distMin ? 0. : (distToTopAtmoBorder-distMin)/(distMax-distMin);
    }
    // ------------------------------------

    const float dotVSCoord=(dotViewSun+1)/2;
    // ------------------------------------

    // Distance to top atmosphere border along the ray groundUnderCamera-sun: (altitude, cosSunZenithAngle)
    const float distFromGroundToTopAtmoBorder=distanceToAtmosphereBorder(cosSunZenithAngle, 0.);
    const float distMin=atmosphereHeight;
    const float distMax=lengthOfHorizRayFromGroundToBorderOfAtmo;
    // TODO: choose a more descriptive name
    const float a=(distFromGroundToTopAtmoBorder-distMin)/(distMax-distMin);
    // TODO: choose a more descriptive name
    const float A=R/(distMax-distMin);
    const float cosSZACoord=max(0.,1-a/A)/(a+1);

    return Scattering4DCoords(cosSZACoord, cosVZACoord, dotVSCoord, altCoordInTexSlice, viewRayIntersectsGround);
}

TexCoordPair scattering4DCoordsToTexCoords(const Scattering4DCoords coords)
{
    const float cosVZAtc = coords.viewRayIntersectsGround ?
                            // Coordinate is in ~[0,0.5]
                            0.5-0.5*unitRangeToTexCoord(coords.cosViewZenithAngle, scatteringTextureSize[0]/2) :
                            // Coordinate is in ~[0.5,1]
                            0.5+0.5*unitRangeToTexCoord(coords.cosViewZenithAngle, scatteringTextureSize[0]/2);

    // Width and height of the 2D subspace of the 4D texture - the subspace spanned by
    // the texture coordinates we combine into a single sampler3D coordinate.
    const float texW=scatteringTextureSize[1], texH=scatteringTextureSize[2];
    const float cosSZAIndex=coords.cosSunZenithAngle*(texH-1);
    const vec2 combiCoordUnitRange=vec2(floor(cosSZAIndex)*texW+coords.dotViewSun*(texW-1),
                                        ceil (cosSZAIndex)*texW+coords.dotViewSun*(texW-1)) / (texW*texH-1);
    const vec2 combinedCoord=unitRangeToTexCoord(combiCoordUnitRange, texW*texH);

    const float altitude=unitRangeToTexCoord(coords.altInTexSlice, scatteringTextureSize[3]);

    const float alphaUpper=fract(cosSZAIndex);
    return TexCoordPair(vec3(cosVZAtc, combinedCoord.x, altitude), float(1-alphaUpper),
                        vec3(cosVZAtc, combinedCoord.y, altitude), float(alphaUpper));
}

TexCoordPair texVarsToScatteringTexCoords(const float cosSunZenithAngle, const float cosViewZenithAngle,
                                          const float dotViewSun, const float altitude,
                                          const bool viewRayIntersectsGround)
{
    // Make sure altitude fits in the texture slice provided. This is the best we can do here.
    const float clampedAlt=clamp(altitude,altitudeMin,altitudeMax);

    const Scattering4DCoords coords=scatteringTexVarsTo4DCoords(cosSunZenithAngle,cosViewZenithAngle,
                                                                dotViewSun,clampedAlt,viewRayIntersectsGround);
    return scattering4DCoordsToTexCoords(coords);
}

vec4 sample4DTexture(const sampler3D tex, const float cosSunZenithAngle, const float cosViewZenithAngle,
                     const float dotViewSun, const float altitude, const bool viewRayIntersectsGround)
{
    const TexCoordPair coords=texVarsToScatteringTexCoords(cosSunZenithAngle,cosViewZenithAngle, dotViewSun,
                                                           altitude, viewRayIntersectsGround);
    return texture(tex, coords.lower) * coords.alphaLower +
           texture(tex, coords.upper) * coords.alphaUpper;
}

ScatteringTexVars scatteringTex4DCoordsToTexVars(const Scattering4DCoords coords)
{
    const float altMax=atmosphereHeight;
    const float R=earthRadius;

    const float lengthOfHorizRayFromGroundToBorderOfAtmo=sqrt(sqr(R+altMax)-sqr(R));
    const float distToHorizonMin = sqrt(sqr(altitudeMin)+2*altitudeMin*R);
    const float distToHorizonMax = sqrt(sqr(altitudeMax)+2*altitudeMax*R);
    const float altMinCoordInFullTexture = distToHorizonMin / lengthOfHorizRayFromGroundToBorderOfAtmo;
    const float altMaxCoordInFullTexture = distToHorizonMax / lengthOfHorizRayFromGroundToBorderOfAtmo;
    const float altCoordInFullTexture = coords.altInTexSlice*(altMaxCoordInFullTexture-altMinCoordInFullTexture)+altMinCoordInFullTexture;
    const float distToHorizon = altCoordInFullTexture*lengthOfHorizRayFromGroundToBorderOfAtmo;
    // Rounding errors can result in altitude>max, breaking the code after this calculation, so we have to clamp.
    const float altitude=clamp(sqrt(sqr(distToHorizon)+sqr(R))-R, 0., altMax);

    // ------------------------------------
    float cosViewZenithAngle;
    if(coords.viewRayIntersectsGround)
    {
        const float distMin=altitude;
        const float distMax=distToHorizon;
        const float distToGround=coords.cosViewZenithAngle*(distMax-distMin)+distMin;
        cosViewZenithAngle = distToGround==0 ? -1 :
            clampCosine(-(sqr(distToHorizon)+sqr(distToGround)) / (2*distToGround*(altitude+R)));
    }
    else
    {
        const float distMin=altMax-altitude;
        const float distMax=distToHorizon+lengthOfHorizRayFromGroundToBorderOfAtmo;
        const float distToTopAtmoBorder=coords.cosViewZenithAngle*(distMax-distMin)+distMin;
        cosViewZenithAngle = distToTopAtmoBorder==0 ? 1 :
            clampCosine((sqr(lengthOfHorizRayFromGroundToBorderOfAtmo)-sqr(distToHorizon)-sqr(distToTopAtmoBorder)) /
                        (2*distToTopAtmoBorder*(altitude+R)));
    }

    // ------------------------------------
    const float dotViewSun=coords.dotViewSun*2-1;

    // ------------------------------------
    const float distMin=atmosphereHeight;
    const float distMax=lengthOfHorizRayFromGroundToBorderOfAtmo;
    // TODO: choose a more descriptive name, same as in scatteringTexVarsTo4DCoords()
    const float A=R/(distMax-distMin);
    // TODO: choose a more descriptive name, same as in scatteringTexVarsTo4DCoords()
    const float a=(A-A*coords.cosSunZenithAngle)/(1+A*coords.cosSunZenithAngle);
    const float distFromGroundToTopAtmoBorder=distMin+min(a,A)*(distMax-distMin);
    const float cosSunZenithAngle = distFromGroundToTopAtmoBorder==0 ? 1 :
        clampCosine((sqr(lengthOfHorizRayFromGroundToBorderOfAtmo)-sqr(distFromGroundToTopAtmoBorder)) /
                    (2*R*distFromGroundToTopAtmoBorder));
    return ScatteringTexVars(cosSunZenithAngle, cosViewZenithAngle, dotViewSun, altitude, coords.viewRayIntersectsGround);
}

Scattering4DCoords scatteringTexIndicesTo4DCoords(const vec3 texIndices)
{
    const vec4 indexMax=scatteringTextureSize-vec4(1);
    Scattering4DCoords coords4d;
    coords4d.viewRayIntersectsGround = texIndices[0] < indexMax[0]/2;
    // The following formulas assume that scatteringTextureSize[0] is even. For odd sizes they would change.
    coords4d.cosViewZenithAngle = coords4d.viewRayIntersectsGround ?
                                   1-2*texIndices[0]/(indexMax[0]-1) :
                                   2*(texIndices[0]-1)/(indexMax[0]-1)-1;

    // Width and height of the 2D subspace of the 4D texture - the subspace spanned by
    // the texture indices we combine into a single sampler3D coordinate.
    const float texW=scatteringTextureSize[1], texH=scatteringTextureSize[2];
    const float combinedIndex=texIndices[1];
    coords4d.dotViewSun=mod(combinedIndex,texW)/(texW-1);
    coords4d.cosSunZenithAngle=floor(combinedIndex/texW)/(texH-1);

    // NOTE: Third texture coordinate must correspond to only one 4D coordinate, because GL_MAX_3D_TEXTURE_SIZE is
    // usually much smaller than GL_MAX_TEXTURE_SIZE. So we can safely pack two of the 4D coordinates into width or
    // height, but not into depth.
    coords4d.altInTexSlice=texIndices[2]/indexMax[3];

    return coords4d;
}

ScatteringTexVars scatteringTexIndicesToTexVars(const vec3 texIndices)
{
    const Scattering4DCoords coords4d=scatteringTexIndicesTo4DCoords(texIndices);
    ScatteringTexVars vars=scatteringTex4DCoordsToTexVars(coords4d);
    // Clamp dotViewSun to its valid range of values, given cosViewZenithAngle and cosSunZenithAngle. This is
    // needed to prevent NaNs when computing the scattering texture.
    const float cosVZA=vars.cosViewZenithAngle,
                cosSZA=vars.cosSunZenithAngle;
    vars.dotViewSun=clamp(vars.dotViewSun,
                          cosVZA*cosSZA-sqrt((1-sqr(cosVZA))*(1-sqr(cosSZA))),
                          cosVZA*cosSZA+sqrt((1-sqr(cosVZA))*(1-sqr(cosSZA))));
    return vars;
}
