#version 450 core

layout(location = 0) in vec3 Vertex;

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

layout(location = 0) out vec4 FragmentVertex;

void main()
{
    FragmentVertex=vec4(Vertex,1.0)*pc.local_to_world*world.mvp;
    gl_Position=FragmentVertex;
}
