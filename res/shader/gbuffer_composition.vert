#version 450 core

layout(location = 0) in vec2 Vertex;

layout(location = 0) out vec2 FragmentPosition;

void main()
{
    gl_Position=vec4(Vertex,0.0,1.0);

    FragmentPosition=(Vertex+1.0)/2.0;
}
