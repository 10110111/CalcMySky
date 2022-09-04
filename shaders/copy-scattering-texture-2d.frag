#version 330
#include "version.h.glsl"
uniform int layer;
uniform sampler2D tex;
out vec4 copy;

uniform mat4 radianceToLuminance=mat4(1);

void main()
{
    CONST vec2 coord=gl_FragCoord.xy/textureSize(tex,0);
    copy=radianceToLuminance*texture(tex, coord);
}
