#version 450 core

layout(binding = 0) uniform WorldMatrix
{
    mat4 two_dim;
    mat4 projection;
    mat4 modelview;
    mat4 mvp;
    mat3 normal;
} world;

void main()
{
    gl_Position=vec4(1.0)*world.mvp;
}
