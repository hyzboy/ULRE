#version 450 core

layout(location = 0) in vec3 Vertex;
layout(location = 1) in vec3 Color;
layout(location = 2) in vec3 Normal;
layout(location = 3) in vec3 Tangent;
layout(location = 4) in vec2 TexCoord;

layout(binding = 0) uniform WorldMatrix
{
    mat4 two_dim;
    mat4 projection;
    mat4 modelview;
    mat4 mvp;
    mat3 normal;
} world;

layout(push_constant) uniform Consts {
    mat4 local_to_world;
} pc;

layout(location = 0) out vec3 FragmentColor;
layout(location = 1) out vec3 FragmentNormal;
layout(location = 2) out vec3 FragmentTangent;
layout(location = 3) out vec3 FragmentWorldPos;
layout(location = 4) out vec2 FragmentTexCoord;

void main()
{
    FragmentColor=Color;
    FragmentWorldPos=vec3(vec4(Vertex,1.0)*pc.local_to_world);
    FragmentTexCoord=TexCoord;    
    FragmentTexCoord.t=1.0-TexCoord.t;

    mat3 mNormal=transpose(inverse(mat3(pc.local_to_world)));
    FragmentNormal=mNormal*normalize(Normal);
    FragmentTangent=mNormal*normalize(Tangent);

    gl_Position=vec4(Vertex,1.0)*world.mvp;
}
