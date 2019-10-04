#ifndef INCLUDE_ONCE_72E237D7_42B6_462B_90E4_73AB6B6E4DE4
#define INCLUDE_ONCE_72E237D7_42B6_462B_90E4_73AB6B6E4DE4

float texCoordToUnitRange(const float texCoord, const float texSize);
float unitRangeToTexCoord(const float u, const float texSize);
struct TransmittanceTexVars
{
    float cosViewZenithAngle;
    float altitude;
};
TransmittanceTexVars transmittanceTexCoordToTexVars(const vec2 texCoord);
vec2 transmittanceTexVarsToTexCoord(const float cosVZA, float altitude);
struct IrradianceTexVars
{
    float cosSunZenithAngle;
    float altitude;
};
IrradianceTexVars irradianceTexCoordToTexVars(const vec2 texCoord);

struct ScatteringTexVars
{
    float cosSunZenithAngle;
    float cosViewZenithAngle;
    float dotViewSun;
    float altitude;
    bool viewRayIntersectsGround;
};
ScatteringTexVars scatteringTexIndicesToTexVars(const vec3 texIndices);
vec4 sample4DTexture(const sampler3D tex, const float cosSunZenithAngle, const float cosViewZenithAngle,
                     const float dotViewSun, const float altitude, const bool viewRayIntersectsGround);
#ifdef ENABLE_TESTING_CODE
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

ScatteringTexVars scatteringTex4DCoordsToTexVars(const Scattering4DCoords coords);
Scattering4DCoords scatteringTexVarsTo4DCoords(const float cosSunZenithAngle, const float cosViewZenithAngle,
                                               const float dotViewSun, const float altitude,
                                               const bool viewRayIntersectsGround);
TexCoordPair texVarsToScatteringTexCoords(const float cosSunZenithAngle, const float cosViewZenithAngle,
                                          const float dotViewSun, const float altitude,
                                          const bool viewRayIntersectsGround);
Scattering4DCoords scatteringTexIndicesTo4DCoords(const vec3 texIndices);
TexCoordPair scattering4DCoordsToTexCoords(const Scattering4DCoords coords);
#endif

#endif
