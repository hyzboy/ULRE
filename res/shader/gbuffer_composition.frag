#version 450 core

layout(location = 0) in vec2 FragmentPosition;
layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor=vec4(normalize(FragmentPosition),0.0,1.0);
}
