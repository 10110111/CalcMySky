#version 330

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;
uniform int layer;

void main()
{
    for(int i=0; i<3; ++i)
    {
        gl_Position=gl_in[i].gl_Position;
        gl_Layer=layer;
        EmitVertex();
    }
    EndPrimitive();
}
