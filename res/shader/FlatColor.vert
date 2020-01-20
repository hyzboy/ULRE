#version 450 core

layout(location = 0) in vec2 Vertex;
layout(location = 1) in vec3 Color;

layout(binding=0) uniform WorldMatrix     // hgl/math/Math.h
{
    mat4 ortho;

    mat4 projection;
    mat4 inverse_projection;

    mat4 modelview;
    mat4 inverse_modelview;

    mat4 mvp;
    mat4 inverse_mvp;

    vec4 view_pos;
    vec2 resolution;
} world;

layout(location = 0) out vec4 FragmentColor;

void main()
{
    FragmentColor=vec4(Color,1.0);

    gl_Position=vec4(Vertex,0.0,1.0)*world.ortho;
}
