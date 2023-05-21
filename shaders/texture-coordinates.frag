#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "texture-coordinates.h.glsl"
#include "common-functions.h.glsl"

const float LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA=sqrt(atmosphereHeight*(atmosphereHeight+2*earthRadius));

uniform sampler2D transmittanceTexture;
uniform vec3 eclipsedDoubleScatteringTextureSize;

struct Scattering4DCoords
{
    float cosSunZenithAngle;
    float cosViewZenithAngle;
    float dotViewSun;
    float altitude;
    bool viewRayIntersectsGround;
};
struct TexCoordPair
{
    vec3 lower;
    float alphaLower;
    vec3 upper;
    float alphaUpper;
};

struct EclipseScattering2DCoords
{
    float azimuth;
    float cosViewZenithAngle;
};

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

float texCoordToIndex(const float texCoord, const float texSize)
{
    return texSize*texCoord-0.5;
}
float indexToTexCoord(const float index, const float texSize)
{
    return (index+0.5)/texSize;
}
vec3 texCoordsToIndices(const vec3 texCoords, const vec3 texSizes)
{
    return vec3(texCoordToIndex(texCoords[0], texSizes[0]),
                texCoordToIndex(texCoords[1], texSizes[1]),
                texCoordToIndex(texCoords[2], texSizes[2]));
}
vec3 indicesToTexCoords(const vec3 indices, const vec3 texSizes)
{
    return vec3(indexToTexCoord(indices[0], texSizes[0]),
                indexToTexCoord(indices[1], texSizes[1]),
                indexToTexCoord(indices[2], texSizes[2]));
}

TransmittanceTexVars transmittanceTexCoordToTexVars(const vec2 texCoord)
{
    CONST float distToHorizon=LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA *
                                texCoordToUnitRange(texCoord.t,transmittanceTextureSize.t);
    // Distance from Earth center to camera
    CONST float r=sqrt(sqr(distToHorizon)+sqr(earthRadius));
    CONST float altitude=r-earthRadius;

    CONST float dMin=atmosphereHeight-altitude; // distance to zenith
    CONST float dMax=LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA+distToHorizon;
    // distance to border of visible atmosphere from the view point
    CONST float d=dMin+(dMax-dMin)*texCoordToUnitRange(texCoord.s,transmittanceTextureSize.s);
    // d==0 can happen when altitude==atmosphereHeight
    CONST float cosVZA = d==0 ? 1 : (2*r*dMin+sqr(dMin)-sqr(d))/(2*r*d);
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

    CONST float distToHorizon=sqrt(sqr(altitude)+2*altitude*earthRadius);
    CONST float t=unitRangeToTexCoord(distToHorizon / LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA,
                                      transmittanceTextureSize.t);
    CONST float dMin=atmosphereHeight-altitude; // distance to zenith
    CONST float dMax=LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA+distToHorizon;
    CONST float d=distanceToAtmosphereBorder(cosVZA,altitude);
    CONST float s=unitRangeToTexCoord((d-dMin)/(dMax-dMin), transmittanceTextureSize.s);
    return vec2(s,t);
}

// Output: vec2(cos(sunZenithAngle), altitude)
IrradianceTexVars irradianceTexCoordToTexVars(const vec2 texCoord)
{
    CONST float cosSZA=2*texCoordToUnitRange(texCoord.s, irradianceTextureSize.s)-1;
    CONST float alt=atmosphereHeight*texCoordToUnitRange(texCoord.t, irradianceTextureSize.t);
    return IrradianceTexVars(cosSZA,alt);
}

vec2 irradianceTexVarsToTexCoord(const float cosSunZenithAngle, const float altitude)
{
    CONST float s=unitRangeToTexCoord((cosSunZenithAngle+1)/2, irradianceTextureSize.s);
    CONST float t=unitRangeToTexCoord(altitude/atmosphereHeight, irradianceTextureSize.t);
    return vec2(s,t);
}

float cosSZAToUnitRangeTexCoord(const float cosSunZenithAngle)
{
    // Distance to top atmosphere border along the ray groundUnderCamera-sun: (altitude, cosSunZenithAngle)
    CONST float distFromGroundToTopAtmoBorder=distanceToAtmosphereBorder(cosSunZenithAngle, 0.);
    CONST float distMin=atmosphereHeight;
    CONST float distMax=LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA;
    // TODO: choose a more descriptive name
    CONST float a=(distFromGroundToTopAtmoBorder-distMin)/(distMax-distMin);
    // TODO: choose a more descriptive name
    CONST float A=2*earthRadius/(distMax-distMin);
    return max(0.,1-a/A)/(a+1);
}

float unitRangeTexCoordToCosSZA(const float texCoord)
{
    CONST float distMin=atmosphereHeight;
    CONST float distMax=LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA;
    // TODO: choose a more descriptive name, same as in cosSZAToUnitRangeTexCoord()
    CONST float A=2*earthRadius/(distMax-distMin);
    // TODO: choose a more descriptive name, same as in cosSZAToUnitRangeTexCoord()
    CONST float a=(A-A*texCoord)/(1+A*texCoord);
    CONST float distFromGroundToTopAtmoBorder=distMin+min(a,A)*(distMax-distMin);
    return distFromGroundToTopAtmoBorder==0 ? 1 :
        clampCosine((sqr(LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA)-sqr(distFromGroundToTopAtmoBorder)) /
                    (2*earthRadius*distFromGroundToTopAtmoBorder));
}

// dotViewSun: dot(viewDir,sunDir)
Scattering4DCoords scatteringTexVarsTo4DCoords(const float cosSunZenithAngle, const float cosViewZenithAngle,
                                               const float dotViewSun, const float altitude,
                                               const bool viewRayIntersectsGround)
{
    CONST float r=earthRadius+altitude;

    CONST float distToHorizon    = sqrt(sqr(altitude)+2*altitude*earthRadius);
    CONST float altCoord = distToHorizon / LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA;

    // ------------------------------------
    float cosVZACoord; // Coordinate for cos(viewZenithAngle)
    CONST float rCvza=r*cosViewZenithAngle;
    // Discriminant of the quadratic equation for the intersections of the ray (altitiude, cosViewZenithAngle) with the ground.
    CONST float discriminant=sqr(rCvza)-sqr(r)+sqr(earthRadius);
    if(viewRayIntersectsGround)
    {
        // Distance from camera to the ground along the view ray (altitude, cosViewZenithAngle)
        CONST float distToGround = -rCvza-safeSqrt(discriminant);
        // Minimum possible value of distToGround
        CONST float distMin = altitude;
        // Maximum possible value of distToGround
        CONST float distMax = distToHorizon;
        cosVZACoord = distMax==distMin ? 0. : (distToGround-distMin)/(distMax-distMin);
    }
    else
    {
        // Distance from camera to the atmosphere border along the view ray (altitude, cosViewZenithAngle)
        // sqr(LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA) added to sqr(earthRadius) term in discriminant changes
        // sqr(earthRadius) to sqr(earthRadius+atmosphereHeight), so that we target the top atmosphere boundary instead of bottom.
        CONST float distToTopAtmoBorder = -rCvza+safeSqrt(discriminant+sqr(LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA));
        CONST float distMin = atmosphereHeight-altitude;
        CONST float distMax = distToHorizon+LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA;
        cosVZACoord = distMax==distMin ? 0. : (distToTopAtmoBorder-distMin)/(distMax-distMin);
    }

    // ------------------------------------
    CONST float dotVSCoord=(dotViewSun+1)/2;

    // ------------------------------------
    CONST float cosSZACoord=cosSZAToUnitRangeTexCoord(cosSunZenithAngle);

    return Scattering4DCoords(cosSZACoord, cosVZACoord, dotVSCoord, altCoord, viewRayIntersectsGround);
}

TexCoordPair scattering4DCoordsToTexCoords(const Scattering4DCoords coords)
{
    CONST float cosVZAtc = coords.viewRayIntersectsGround ?
                            // Coordinate is in ~[0,0.5]
                            0.5-0.5*unitRangeToTexCoord(coords.cosViewZenithAngle, scatteringTextureSize[0]/2) :
                            // Coordinate is in ~[0.5,1]
                            0.5+0.5*unitRangeToTexCoord(coords.cosViewZenithAngle, scatteringTextureSize[0]/2);

    // Width and height of the 2D subspace of the 4D texture - the subspace spanned by
    // the texture coordinates we combine into a single sampler3D coordinate.
    CONST float texW = scatteringTextureSize[1];
    CONST float texH = scatteringTextureSize[2];
    CONST float cosSZAIndex=coords.cosSunZenithAngle*(texH-1);
    CONST vec2 combiCoordUnitRange=vec2(floor(cosSZAIndex)*texW+coords.dotViewSun*(texW-1),
                                        ceil (cosSZAIndex)*texW+coords.dotViewSun*(texW-1)) / (texW*texH-1);
    CONST vec2 combinedCoord=unitRangeToTexCoord(combiCoordUnitRange, texW*texH);

    CONST float altitude = unitRangeToTexCoord(coords.altitude, scatteringTextureSize[3]);

    CONST float alphaUpper=fract(cosSZAIndex);
    return TexCoordPair(vec3(cosVZAtc, combinedCoord.x, altitude), float(1-alphaUpper),
                        vec3(cosVZAtc, combinedCoord.y, altitude), float(alphaUpper));
}

vec4 sample4DTexture(const sampler3D tex, const float cosSunZenithAngle, const float cosViewZenithAngle,
                     const float dotViewSun, const float altitude, const bool viewRayIntersectsGround)
{
    CONST Scattering4DCoords coords4d = scatteringTexVarsTo4DCoords(cosSunZenithAngle,cosViewZenithAngle,
                                                                    dotViewSun,altitude,viewRayIntersectsGround);
    CONST TexCoordPair texCoords=scattering4DCoordsToTexCoords(coords4d);
    return texture(tex, texCoords.lower) * texCoords.alphaLower +
           texture(tex, texCoords.upper) * texCoords.alphaUpper;
}

// 3D texture is the 3D single-altitude slice of a 4D texture
vec3 scattering4DCoordsToTex3DCoords(const Scattering4DCoords coords)
{
    CONST float cosVZAtc = coords.viewRayIntersectsGround ?
                            // Coordinate is in ~[0,0.5]
                            0.5-0.5*unitRangeToTexCoord(coords.cosViewZenithAngle, scatteringTextureSize[0]/2) :
                            // Coordinate is in ~[0.5,1]
                            0.5+0.5*unitRangeToTexCoord(coords.cosViewZenithAngle, scatteringTextureSize[0]/2);

    CONST float dvsSize = scatteringTextureSize[1];
    CONST float dotVStc = unitRangeToTexCoord(coords.dotViewSun, dvsSize);

    CONST float cszaSize = scatteringTextureSize[2];
    CONST float cosSZAtc = unitRangeToTexCoord(coords.cosSunZenithAngle, cszaSize);

    return vec3(cosVZAtc, dotVStc, cosSZAtc);
}

// Sample interpolation guides texture at the given coordinate
float sampleGuide(const sampler3D guides, const vec3 coords)
{
    return texture(guides, coords).r;
}
float findGuide01Angle(const sampler3D guides, const vec3 indices)
{
    // Row is the line along VZA coordinate.
    // Position between rows means the position along dotViewSun coordinate.

    CONST float texCoordVerbatim = indexToTexCoord(indices[2], scatteringTextureSize[2]);
    CONST float rowLen = scatteringTextureSize[0];
    CONST float numRows = scatteringTextureSize[1];
    CONST float posOfRow = indices[1];
    CONST float posInRow = indices[0];
    CONST float currRow = floor(posOfRow);
    CONST float posBetweenRows = posOfRow - currRow;

    // A & B are the endpoints of binary search inside the row
    float posInRow_A = 0;
    float posInRow_B = rowLen-1;

    CONST float angle_A = PI/2*sampleGuide(guides, vec3(indexToTexCoord(posInRow_A, rowLen),
                                                        indexToTexCoord(currRow, numRows-1),
                                                        texCoordVerbatim));
    CONST float posInRowAtPosBetweenRows_A = posInRow_A + (posBetweenRows-0.5) * tan(angle_A);
    if(posInRowAtPosBetweenRows_A > posInRow)
    {
        swap(posInRow_A, posInRow_B);
    }
    for(int n=0; n<8; ++n)
    {
        CONST float currPosInRow = (posInRow_A + posInRow_B)/2;
        CONST float currAngle = PI/2*sampleGuide(guides, vec3(indexToTexCoord(currPosInRow, rowLen),
                                                              indexToTexCoord(currRow, numRows-1),
                                                              texCoordVerbatim));
        CONST float currPosInRowAtPosBetweenRows = currPosInRow + (posBetweenRows-0.5) * tan(currAngle);
        if(currPosInRowAtPosBetweenRows < posInRow)
            posInRow_A = currPosInRow;
        else
            posInRow_B = currPosInRow;
    }
    CONST float finalPosInRow = (posInRow_A + posInRow_B)/2;
    return PI/2*sampleGuide(guides, vec3(indexToTexCoord(finalPosInRow, rowLen),
                                         indexToTexCoord(currRow, numRows-1),
                                         texCoordVerbatim));
}
vec4 sample3DTextureGuided01_log(const sampler3D tex, const sampler3D interpolationGuides01Tex,
                                 const vec3 indices)
{
    CONST float interpAngle = findGuide01Angle(interpolationGuides01Tex, indices);

    CONST float cosVZAIndex = indices[0];
    CONST float dotVSIndex = indices[1];
    CONST float currRow = floor(dotVSIndex);
    CONST float posBetweenRows = dotVSIndex - currRow;
    CONST float cvzaPosInCurrRow = cosVZAIndex - posBetweenRows*tan(interpAngle);
    CONST float cvzaPosInNextRow = cosVZAIndex + (1-posBetweenRows)*tan(interpAngle);

    vec3 indicesCurrRow = indices, indicesNextRow = indices;
    indicesCurrRow[0] = clamp(cvzaPosInCurrRow, 0., scatteringTextureSize[0]-1.);
    indicesNextRow[0] = clamp(cvzaPosInNextRow, 0., scatteringTextureSize[0]-1.);
    indicesCurrRow[1] =     currRow;
    indicesNextRow[1] = min(currRow+1, scatteringTextureSize[1]-1.);

    CONST vec3 coordsNextRow = indicesToTexCoords(indicesNextRow, scatteringTextureSize.stp);
    CONST vec3 coordsCurrRow = indicesToTexCoords(indicesCurrRow, scatteringTextureSize.stp);

    CONST vec4 valueCurrRow = texture(tex, coordsCurrRow);
    CONST vec4 valueNextRow = texture(tex, coordsNextRow);
    CONST float epsilon = 1e-37; // Prevents passing zero to log
    CONST vec4 logValNextRow = log(max(valueNextRow, vec4(epsilon)));
    CONST vec4 logValCurrRow = log(max(valueCurrRow, vec4(epsilon)));
    return (logValNextRow-logValCurrRow) * posBetweenRows + logValCurrRow;
}

float findGuide02Angle(const sampler3D guides, const vec3 indices)
{
    // Row is the line along VZA coordinate.
    // Position between rows means the position along SZA coordinate.

    CONST float texCoordVerbatim = indexToTexCoord(indices[1], scatteringTextureSize[1]);
    CONST float rowLen = scatteringTextureSize[0];
    CONST float numRows = scatteringTextureSize[2];
    CONST float posOfRow = indices[2];
    CONST float posInRow = indices[0];
    CONST float currRow = floor(posOfRow);
    CONST float posBetweenRows = posOfRow - currRow;

    // A & B are the endpoints of binary search inside the row
    float posInRow_A = 0;
    float posInRow_B = rowLen-1;

    CONST float angle_A = PI/2*sampleGuide(guides, vec3(indexToTexCoord(posInRow_A, rowLen),
                                                        texCoordVerbatim, indexToTexCoord(currRow, numRows-1)));
    CONST float posInRowAtPosBetweenRows_A = posInRow_A + (posBetweenRows-0.5) * tan(angle_A);
    if(posInRowAtPosBetweenRows_A > posInRow)
    {
        swap(posInRow_A, posInRow_B);
    }
    for(int n=0; n<8; ++n)
    {
        CONST float currPosInRow = (posInRow_A + posInRow_B)/2;
        CONST float currAngle = PI/2*sampleGuide(guides, vec3(indexToTexCoord(currPosInRow, rowLen),
                                                              texCoordVerbatim, indexToTexCoord(currRow, numRows-1)));
        CONST float currPosInRowAtPosBetweenRows = currPosInRow + (posBetweenRows-0.5) * tan(currAngle);
        if(currPosInRowAtPosBetweenRows < posInRow)
            posInRow_A = currPosInRow;
        else
            posInRow_B = currPosInRow;
    }
    CONST float finalPosInRow = (posInRow_A + posInRow_B)/2;
    return PI/2*sampleGuide(guides, vec3(indexToTexCoord(finalPosInRow, rowLen),
                                         texCoordVerbatim, indexToTexCoord(currRow, numRows-1)));
}

vec4 sample3DTextureGuided(const sampler3D tex,
                           const sampler3D interpolationGuides01Tex, const sampler3D interpolationGuides02Tex,
                           const float cosSunZenithAngle, const float cosViewZenithAngle,
                           const float dotViewSun, const float altitude, const bool viewRayIntersectsGround)
{
    CONST Scattering4DCoords coords4d = scatteringTexVarsTo4DCoords(cosSunZenithAngle, cosViewZenithAngle,
                                                                    dotViewSun, altitude, viewRayIntersectsGround);
    // Handle the external interpolation guides: the guides between a pair of VZA-dotViewSun 2D "pictures".
    CONST vec3 tc = scattering4DCoordsToTex3DCoords(coords4d);
    CONST vec3 indices = texCoordsToIndices(tc, scatteringTextureSize.stp);
    CONST float interpAngle = findGuide02Angle(interpolationGuides02Tex, indices);

    CONST float cvzaIndex = indices[0];
    CONST float cszaIndex = indices[2];
    CONST float currRow = floor(cszaIndex);
    CONST float posBetweenRows = cszaIndex - currRow;
    CONST float cvzaPosInCurrRow = cvzaIndex - posBetweenRows*tan(interpAngle);
    CONST float cvzaPosInNextRow = cvzaIndex + (1-posBetweenRows)*tan(interpAngle);

    vec3 indicesCurrRow = indices, indicesNextRow = indices;
    indicesCurrRow[0] = clamp(cvzaPosInCurrRow, 0., scatteringTextureSize[0]-1.);
    indicesNextRow[0] = clamp(cvzaPosInNextRow, 0., scatteringTextureSize[0]-1.);
    indicesCurrRow[2] =     currRow;
    indicesNextRow[2] = min(currRow+1, scatteringTextureSize[2]-1);

    // The caller will handle the internal interpolation guides: the ones between rows in each 2D "picture".
    CONST vec4 logValCurrRow = sample3DTextureGuided01_log(tex, interpolationGuides01Tex, indicesCurrRow);
    CONST vec4 logValNextRow = sample3DTextureGuided01_log(tex, interpolationGuides01Tex, indicesNextRow);
    return exp((logValNextRow-logValCurrRow) * posBetweenRows + logValCurrRow);
}

vec4 sample3DTexture(const sampler3D tex, const float cosSunZenithAngle, const float cosViewZenithAngle,
                     const float dotViewSun, const float altitude, const bool viewRayIntersectsGround)
{
    CONST Scattering4DCoords coords4d = scatteringTexVarsTo4DCoords(cosSunZenithAngle,cosViewZenithAngle,
                                                                    dotViewSun,altitude,viewRayIntersectsGround);
    CONST vec3 texCoords=scattering4DCoordsToTex3DCoords(coords4d);
    return texture(tex, texCoords);
}

ScatteringTexVars scatteringTex4DCoordsToTexVars(const Scattering4DCoords coords)
{
    CONST float distToHorizon = coords.altitude*LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA;
    // Rounding errors can result in altitude>max, breaking the code after this calculation, so we have to clamp.
    CONST float altitude=clampAltitude(sqrt(sqr(distToHorizon)+sqr(earthRadius))-earthRadius);

    // ------------------------------------
    float cosViewZenithAngle;
    if(coords.viewRayIntersectsGround)
    {
        CONST float distMin=altitude;
        CONST float distMax=distToHorizon;
        CONST float distToGround=coords.cosViewZenithAngle*(distMax-distMin)+distMin;
        cosViewZenithAngle = distToGround==0 ? -1 :
            clampCosine(-(sqr(distToHorizon)+sqr(distToGround)) / (2*distToGround*(altitude+earthRadius)));
    }
    else
    {
        CONST float distMin=atmosphereHeight-altitude;
        CONST float distMax=distToHorizon+LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA;
        CONST float distToTopAtmoBorder=coords.cosViewZenithAngle*(distMax-distMin)+distMin;
        cosViewZenithAngle = distToTopAtmoBorder==0 ? 1 :
            clampCosine((sqr(LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA)-sqr(distToHorizon)-sqr(distToTopAtmoBorder)) /
                        (2*distToTopAtmoBorder*(altitude+earthRadius)));
    }

    // ------------------------------------
    CONST float dotViewSun=coords.dotViewSun*2-1;

    // ------------------------------------
    CONST float cosSunZenithAngle=unitRangeTexCoordToCosSZA(coords.cosSunZenithAngle);

    return ScatteringTexVars(cosSunZenithAngle, cosViewZenithAngle, dotViewSun, altitude, coords.viewRayIntersectsGround);
}

Scattering4DCoords scatteringTexIndicesTo4DCoords(const vec3 texIndices)
{
    CONST vec4 indexMax=scatteringTextureSize-vec4(1);
    Scattering4DCoords coords4d;
    coords4d.viewRayIntersectsGround = texIndices[0] < indexMax[0]/2;
    // The following formulas assume that scatteringTextureSize[0] is even. For odd sizes they would change.
    coords4d.cosViewZenithAngle = coords4d.viewRayIntersectsGround ?
                                   1-2*texIndices[0]/(indexMax[0]-1) :
                                   2*(texIndices[0]-1)/(indexMax[0]-1)-1;
    // Although the above formula, when compiled as written above, should produce exact result for zenith and nadir,
    // aggressive optimizations of NVIDIA driver can and do result in inexact coordinate. And this is bad, since our
    // further calculations in scatteringTex4DCoordsToTexVars are sensitive to these values when altitude==atmosphereHeight,
    // when looking into zenith. So let's fixup this special case.
    if(texIndices[0]==scatteringTextureSize[0]/2)
        coords4d.cosViewZenithAngle=0;

    // Width and height of the 2D subspace of the 4D texture - the subspace spanned by
    // the texture indices we combine into a single sampler3D coordinate.
    CONST float texW=scatteringTextureSize[1], texH=scatteringTextureSize[2];
    CONST float combinedIndex=texIndices[1];
    coords4d.dotViewSun=mod(combinedIndex,texW)/(texW-1);
    coords4d.cosSunZenithAngle=floor(combinedIndex/texW)/(texH-1);

    // NOTE: Third texture coordinate must correspond to only one 4D coordinate, because GL_MAX_3D_TEXTURE_SIZE is
    // usually much smaller than GL_MAX_TEXTURE_SIZE. So we can safely pack two of the 4D coordinates into width or
    // height, but not into depth.
    coords4d.altitude=texIndices[2]/indexMax[3];

    return coords4d;
}

ScatteringTexVars scatteringTexIndicesToTexVars(const vec3 texIndices)
{
    CONST Scattering4DCoords coords4d=scatteringTexIndicesTo4DCoords(texIndices);
    ScatteringTexVars vars=scatteringTex4DCoordsToTexVars(coords4d);
    // Clamp dotViewSun to its valid range of values, given cosViewZenithAngle and cosSunZenithAngle. This is
    // needed to prevent NaNs when computing the scattering texture.
    CONST float cosVZA=vars.cosViewZenithAngle,
                cosSZA=vars.cosSunZenithAngle;
    vars.dotViewSun=clamp(vars.dotViewSun,
                          cosVZA*cosSZA-safeSqrt((1-sqr(cosVZA))*(1-sqr(cosSZA))),
                          cosVZA*cosSZA+safeSqrt((1-sqr(cosVZA))*(1-sqr(cosSZA))));
    return vars;
}

EclipseScatteringTexVars eclipseTexCoordsToTexVars(const vec2 texCoords, const float altitude)
{
    CONST float distToHorizon = sqrt(sqr(altitude)+2*altitude*earthRadius);

    CONST bool viewRayIntersectsGround = texCoords.t<0.5;
    float cosViewZenithAngle;
    if(viewRayIntersectsGround)
    {
        CONST float cosVZACoord = texCoordToUnitRange(1-2*texCoords.t, eclipsedSingleScatteringTextureSize.t/2);
        CONST float distMin=altitude;
        CONST float distMax=distToHorizon;
        CONST float distToGround=cosVZACoord*(distMax-distMin)+distMin;
        cosViewZenithAngle = distToGround==0 ? -1 :
            clampCosine(-(sqr(distToHorizon)+sqr(distToGround)) / (2*distToGround*(altitude+earthRadius)));
    }
    else
    {
        CONST float cosVZACoord = texCoordToUnitRange(2*texCoords.t-1, eclipsedSingleScatteringTextureSize.t/2);
        CONST float distMin=atmosphereHeight-altitude;
        CONST float distMax=distToHorizon+LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA;
        CONST float distToTopAtmoBorder=cosVZACoord*(distMax-distMin)+distMin;
        cosViewZenithAngle = distToTopAtmoBorder==0 ? 1 :
            clampCosine((sqr(LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA)-sqr(distToHorizon)-sqr(distToTopAtmoBorder)) /
                        (2*distToTopAtmoBorder*(altitude+earthRadius)));
    }

    CONST float azimuthRelativeToSun = 2*PI*(texCoords.s - 1/(2*eclipsedSingleScatteringTextureSize.s));
    return EclipseScatteringTexVars(azimuthRelativeToSun, cosViewZenithAngle, viewRayIntersectsGround);
}

EclipseScattering2DCoords eclipseTexVarsTo2DCoords(const float azimuthRelativeToSun, const float cosViewZenithAngle,
                                                   const float altitude, const bool viewRayIntersectsGround)
{
    CONST float r=earthRadius+altitude;

    CONST float distToHorizon    = sqrt(sqr(altitude)+2*altitude*earthRadius);

    // ------------------------------------
    float cosVZACoord; // Coordinate for cos(viewZenithAngle)
    CONST float rCvza=r*cosViewZenithAngle;
    // Discriminant of the quadratic equation for the intersections of the ray (altitiude, cosViewZenithAngle) with the ground.
    CONST float discriminant=sqr(rCvza)-sqr(r)+sqr(earthRadius);
    if(viewRayIntersectsGround)
    {
        // Distance from camera to the ground along the view ray (altitude, cosViewZenithAngle)
        CONST float distToGround = -rCvza-safeSqrt(discriminant);
        // Minimum possible value of distToGround
        CONST float distMin = altitude;
        // Maximum possible value of distToGround
        CONST float distMax = distToHorizon;
        cosVZACoord = distMax==distMin ? 0. : (distToGround-distMin)/(distMax-distMin);
    }
    else
    {
        // Distance from camera to the atmosphere border along the view ray (altitude, cosViewZenithAngle)
        // sqr(LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA) added to sqr(earthRadius) term in discriminant changes
        // sqr(earthRadius) to sqr(earthRadius+atmosphereHeight), so that we target the top atmosphere boundary instead of bottom.
        CONST float distToTopAtmoBorder = -rCvza+safeSqrt(discriminant+sqr(LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA));
        CONST float distMin = atmosphereHeight-altitude;
        CONST float distMax = distToHorizon+LENGTH_OF_HORIZ_RAY_FROM_GROUND_TO_TOA;
        cosVZACoord = distMax==distMin ? 0. : (distToTopAtmoBorder-distMin)/(distMax-distMin);
    }

    CONST float azimuthCoord = azimuthRelativeToSun/(2*PI);

    return EclipseScattering2DCoords(azimuthCoord, cosVZACoord);
}

vec2 eclipseTexVarsToTexCoords(const float azimuthRelativeToSun, const float cosViewZenithAngle,
                               const float altitude, const bool viewRayIntersectsGround, const vec2 texSize)
{
    CONST EclipseScattering2DCoords coords=eclipseTexVarsTo2DCoords(azimuthRelativeToSun, cosViewZenithAngle, altitude,
                                                                    viewRayIntersectsGround);
    CONST float cosVZAtc = viewRayIntersectsGround ?
                            // Coordinate is in ~[0,0.5]
                            0.5-0.5*unitRangeToTexCoord(coords.cosViewZenithAngle, texSize.t/2) :
                            // Coordinate is in ~[0.5,1]
                            0.5+0.5*unitRangeToTexCoord(coords.cosViewZenithAngle, texSize.t/2);
    CONST float azimuthTC = coords.azimuth + 1/(2*texSize.s);

    return vec2(azimuthTC, cosVZAtc);
}

vec4 sampleEclipseDoubleScattering3DTexture(const sampler3D tex, const float cosSunZenithAngle,
                                            const float cosViewZenithAngle, const float azimuthRelativeToSun,
                                            const float altitude, const bool viewRayIntersectsGround)
{
    CONST vec2 coords2d=eclipseTexVarsToTexCoords(azimuthRelativeToSun, cosViewZenithAngle, altitude, viewRayIntersectsGround,
                                                  eclipsedDoubleScatteringTextureSize.st);
    CONST float cosSZACoord=unitRangeToTexCoord(cosSZAToUnitRangeTexCoord(cosSunZenithAngle), eclipsedDoubleScatteringTextureSize[2]);
    CONST vec3 texCoords=vec3(coords2d, cosSZACoord);

    return texture(tex, texCoords);
}
