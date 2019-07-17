#version 450 core

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

layout(binding = 0) uniform sampler2D GB_Position;
layout(binding = 1) uniform sampler2D GB_Normal;
layout(binding = 2) uniform sampler2D GB_Color;

layout(location = 0) flat in uint FragmentTexID;
layout(location = 1) in vec2 FragmentTexCoord;

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor=texture(GB_Normal,FragmentTexCoord);
}

