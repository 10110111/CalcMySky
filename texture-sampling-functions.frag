#version 330
#extension GL_ARB_shading_language_420pack : require

#include "const.h.glsl"
#include "texture-coordinates.h.glsl"

uniform sampler2D transmittanceTexture;

vec4 transmittanceToAtmosphereBorder(float cosViewZenithAngle, float altitude)
{
    const vec2 texCoords=transmittanceMuAltToTexCoord(cosViewZenithAngle, altitude);
    return texture(transmittanceTexture, texCoords);
}
