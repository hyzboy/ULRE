#version 450 core

layout(location = 0) in vec3 Vertex;
layout(location = 1) in vec3 Normal;

layout(push_constant) uniform MatrixConstants {
    mat4 projection;
    mat4 modelview;
    mat4 mvp;
    mat3 normal;
}matrix;

layout(location = 0) out vec4 FragmentColor;
layout(location = 1) out vec3 FragmentNormal;

void main()
{
    FragmentColor=vec4(Color,1.0);
    FragmentNormal=Normal;

    gl_Position=vec4(Vertex,1.0)*matrix.mvp;
}
