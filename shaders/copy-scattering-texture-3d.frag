#version 330
#include "version.h.glsl"
uniform int layer;
uniform sampler3D tex;
out vec4 copy;

uniform mat4 radianceToLuminance=mat4(1);

void main()
{
    CONST vec3 coord=vec3(gl_FragCoord.xy,layer+0.5)/textureSize(tex,0);
    copy=radianceToLuminance*texture(tex, coord);
}
