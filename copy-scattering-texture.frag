#version 330
#extension GL_ARB_shading_language_420pack : require

uniform int layer;
uniform sampler3D tex;
out vec4 copy;

void main()
{
    const vec3 coord=vec3(gl_FragCoord.xy,layer+0.5)/textureSize(tex,0);
    copy=texture(tex, coord);
}
