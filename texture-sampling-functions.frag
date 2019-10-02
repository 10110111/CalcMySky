#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "texture-coordinates.h.glsl"

uniform sampler2D transmittanceTexture;

vec4 transmittanceToAtmosphereBorder(const float cosViewZenithAngle, const float altitude)
{
    const vec2 texCoords=transmittanceTexVarsToTexCoord(cosViewZenithAngle, altitude);
    return texture(transmittanceTexture, texCoords);
}
