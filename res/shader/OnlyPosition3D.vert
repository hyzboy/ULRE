#version 450 core

layout(location = 0) in vec3 Vertex;

layout(binding = 0) uniform WorldConfig
{
    mat4 mvp;
} world;

layout(location = 0) out vec4 FragmentColor;

void main()
{
    FragmentColor=vec4(1.0);

    gl_Position=vec4(Vertex,1.0)*world.mvp;
}
