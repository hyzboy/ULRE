#version 450 core

layout(location = 0) in vec2 Vertex;
layout(location = 1) in vec2 TexCoord;

layout(location = 0) out vec2 FragmentTexCoord;

layout(push_constant) uniform Consts {
    mat4 local_to_world;
} pc;

void main()
{
    gl_Position=vec4(Vertex,0.0,1.0)*pc.local_to_world;
    FragmentTexCoord=TexCoord;
}
