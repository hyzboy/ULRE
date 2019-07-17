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

layout(location = 0) in vec2 FragmentPosition;
layout(location = 0) out vec4 FragColor;



void main()
{
    vec3 pos    =texture(GB_Position,   FragmentPosition).xyz;
    vec3 normal =texture(GB_Normal,     FragmentPosition).xyz;
    vec3 color  =texture(GB_Color,      FragmentPosition).xyz;
    
    vec3 light_pos=vec3(1,1,1);
    vec3 light_halfVector=vec3(1,1,1);    
    
    
    float pf;
    
    float nDotVP=max(0.0,dot(normal,normalize(light_pos)));
    float nDotHV=max(0.0,dot(normal,normalize(light_halfVector)));
    
    if(nDotVP==0.0)
    {
        pf=0.0;
    }
    else
    {
        pf=pow(nDotHV,
    }
}
