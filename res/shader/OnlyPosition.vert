#version 450 core

layout(location = 0) in vec2 Vertex;

layout(binding = 0) uniform WorldMatrix
{
    mat4 ortho;
    mat4 projection;
    mat4 modelview;
    mat4 mvp;
    vec4 view_pos;
} world;

layout(location = 0) out vec4 FragmentColor;

void main()
{
    FragmentColor=vec4(1.0);

    gl_Position=vec4(Vertex,0.0,1.0)*world.ortho;
}
