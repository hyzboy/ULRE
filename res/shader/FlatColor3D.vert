#version 450 core

layout(location = 0) in vec3 Vertex;
layout(location = 1) in vec3 Color;
layout(location = 2) in vec3 Normal;

layout(binding = 0) uniform WorldMatrix
{
    mat4 ortho;
    mat4 projection;
    mat4 modelview;
    mat4 mvp;
    vec4 view_pos;
} world;

layout(push_constant) uniform Consts {
    mat4 local_to_world;
} pc;

layout(location = 0) out vec4 FragmentColor;

void main()
{
    FragmentColor=vec4(Color,1.0);

    gl_Position=vec4(Vertex,1.0)*world.mvp;
}
