#version 450 core

layout(location=0) out vec4 FragColor;

layout(binding=1) uniform ColorMaterial
{
    vec4 color;
} color_material;

void main()
{
    FragColor=color_material.color;
}
