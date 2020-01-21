#version 450 core

layout(location = 0) in vec4 FragmentNormal;
layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor=FragmentNormal;
}
