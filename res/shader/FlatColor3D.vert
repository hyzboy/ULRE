#version 450 core

layout(location = 0) in vec3 Vertex;
layout(location = 1) in vec3 Color;

layout(binding = 0) uniform SunLightConfig
{
    vec4 color;
    vec4 direction;
} sun;

layout(push_constant) uniform Consts {
    mat4 projection;
    mat4 modelview;
    mat4 mvp;
    mat3 normal;
} matrix;

layout(location = 0) out vec4 FragmentColor;

void main()
{
    FragmentColor=vec4(Color,1.0);

    gl_Position=vec4(Vertex,1.0)*world.mvp;
}
