#version 450

layout(location = 0) in vec4 FragmentColor;
layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor=vec4(FragmentColor.rgb,1);
}
