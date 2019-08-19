#version 330
in vec3 vertex;
out vec3 position;
void main()
{
    position=vertex;
    gl_Position=vec4(position,1);
}
