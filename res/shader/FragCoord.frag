#version 450 core

layout(binding=1) uniform WorldMatrix     // hgl/math/Math.h
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
}fragment_world;

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor=vec4(gl_FragCoord.xy/fragment_world.resolution,0,1);
}
