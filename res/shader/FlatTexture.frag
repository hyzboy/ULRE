#version 450 core

layout(binding = 2) uniform sampler2D tex;

layout(location = 0) in vec2 FragmentTexCoord;
layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor=texture(tex,FragmentTexCoord);
}
