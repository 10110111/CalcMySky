#version 330
#extension GL_ARB_shading_language_420pack : require

uniform int layer;
uniform sampler2D tex;
out vec4 copy;

uniform mat4 radianceToLuminance=mat4(1);

void main()
{
    const vec2 coord=gl_FragCoord.xy/textureSize(tex,0);
    copy=radianceToLuminance*texture(tex, coord);
}
