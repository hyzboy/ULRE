#version 450 core

layout(location = 0) in vec2 Vertex;
layout(location = 1) in vec3 Color;

layout(location = 0) out vec4 FragmentColor;

void main()
{
    FragmentColor=vec4(Color,1.0);

    gl_Position=vec4(Vertex,0.0,1.0);
}
