#version 450 core

layout(location = 0) in vec2 Vertex;
layout(location = 1) in vec2 TexCoord;

layout(binding = 0) uniform WorldConfig
{
    mat4 mvp;
} world;

layout(location = 0) out vec2 FragmentTexCoord;

void main()
{
    FragmentTexCoord=TexCoord;

    gl_Position=vec4(Vertex,0.0,1.0)*world.mvp;
}
