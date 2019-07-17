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
layout(location = 1) flat in uint FragmentTexID;
layout(location = 2) in vec2 FragmentTexCoord;

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor=vec4(normalize(FragmentTexCoord),0.0,1.0);
/*    vec3 components[3];
    
    components[0]=texture(GB_Position,   FragmentTexCoord).xyz;
    components[1]=texture(GB_Normal,     FragmentTexCoord).xyz;
    components[2]=texture(GB_Color,      FragmentTexCoord).xyz;
    
    if(FragmentTexID<3)
        FragColor=vec4(components[FragmentTexID],1.0);
    else
        FragColor=vec4(0.0,0.0,0.0,1.0);*/
}

