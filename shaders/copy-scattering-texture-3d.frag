#version 330
#extension GL_ARB_shading_language_420pack : require

uniform int layer;
uniform sampler3D tex;
out vec4 copy;

uniform mat4 radianceToLuminance=mat4(1);

void main()
{
    const vec3 coord=vec3(gl_FragCoord.xy,layer+0.5)/textureSize(tex,0);
    copy=radianceToLuminance*texture(tex, coord);
}
