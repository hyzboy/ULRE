#version 450 core

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

layout(push_constant) uniform Consts {
    mat4 local_to_world;
} pc;

layout(binding = 0) uniform sampler2D GB_Position;
layout(binding = 1) uniform sampler2D GB_Normal;
layout(binding = 2) uniform sampler2D GB_Color;

layout(location = 0) in vec2 FragmentPosition;
layout(location = 0) out vec4 FragColor;



void main()
{
    vec3 pos    =texture(GB_Position,   FragmentPosition).xyz;
    vec3 normal =texture(GB_Normal,     FragmentPosition).xyz;
    vec3 color  =texture(GB_Color,      FragmentPosition).xyz;

    vec3 sun_light_direction=vec3(1,1,1);

    float sun_light_intensity=max(dot(normal,sun_light_direction),0.0);

    FragColor=vec4(color*sun_light_intensity,1.0);
}
